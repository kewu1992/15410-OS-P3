/** @file control_block.c
 *  @brief This file contains functions related to thread control block (tcb)
 *         and process control block (pcb).
 *
 *  The kernel will allocate a tcb struct and a kernel stack for each thread 
 *  when it is created. When each thread is running in kernel mode, it run
 *  on its kernel stack. The size of each kernel stack is defined as 
 *  K_STACK_SIZE. The start address of each kernel stack algins to K_STACK_SIZE.
 *  To quickly find its tcb struct for each thread, an array tcb_table is used.
 *  The size of the array is KERNEL_MEM_SIZE/K_STACK_SIZE. So each kernel stack
 *  is corresponding to an unique index in the array. The elements in the array
 *  are tcb for all threads created by kernel. So a thread can get its tcb
 *  easily by calculating its index (a thread can get its kernel stack address
 *  by using the current %esp value) in the array and get the tcb from array.
 *
 *  All threads belong to the same task (process) share a same pcb struct. 
 *  In each tcb, there is a pointer points to the pcb. Pcb is used to store
 *  the task-level resources, such as page table address, parent task's pid.  
 *
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <control_block.h>
#include <malloc.h>
#include <malloc_internal.h>
#include <common_kern.h>
#include <cr.h>
#include <asm_atomic.h>

#include <simics.h>

#include <syscall_inter.h>
#include <stdio.h>

/** @brief Get the index in tcb_table array based on kernel stack address */
#define GET_K_STACK_INDEX(x)    (((unsigned int)(x)) >> K_STACK_BITS)

/** @brief The limit that a kernel stack can grow. The stack is considered
 *         overflow if it exceeds this limit and should be killed */
#define STACK_OVERFLOW_LIMIT    0x1C00

/** @brief This tcb table contains thread control block data structure for
 *         all threads created by kernel. */
static tcb_t **tcb_table;

/** @brief Counter that will be used for assigning tid and pid */
static int id_count = -1;

static void tcb_set_entry(void *addr, tcb_t *thr);

/** @brief Initialize tcb table data structure
 *  
 *  @return On success return 0, or error return -1
 */
int tcb_init() {
    tcb_table = _calloc(USER_MEM_START/K_STACK_SIZE, sizeof(tcb_t*));
    return tcb_table ? 0 : -1;
}

/** @brief Create a process without creating a thread
 *
 *  @param thread The (first) thread for the newly created process
 *  @param pthr The partent thread(process) that create this process
 *  @param new_page_table_base The page table base that will be used for the 
 *                             new process
 *
 *  @return Process control block data structure of the newly created process, 
 *          return NULL on error (because of out of memory)
 */
pcb_t* tcb_create_process_only(tcb_t* thread, tcb_t* pthr, 
                                                uint32_t new_page_table_base) {

    pcb_t *process = malloc(sizeof(pcb_t));
    if (process == NULL) {
        printf("malloc() failed in tcb_create_process_only()\n");
        return NULL;
    }
    process->pid = thread->tid;
    process->page_table_base = new_page_table_base;
    if (pthr)
        process->ppid = pthr->pcb->pid;
    else
        process->ppid = -1;

    // Put pid to pcb mapping in hashtable
    if (ht_put_task(process->pid, process) < 0) {
        printf("ht_put_task() failed in tcb_create_process_only()\n");
        free(process);
        return NULL;
    }

    process->exit_status = malloc(sizeof(exit_status_t));
    if (process->exit_status == NULL) {
        printf("malloc() failed in tcb_create_process_only()\n");
        ht_remove_task(process->pid);
        free(process);
        return NULL;
    }
    process->exit_status->pid = process->pid;
    // Initially exit status is 0
    process->exit_status->status = 0;


    process->exit_status_node = malloc(sizeof(simple_node_t));
    if (process->exit_status_node == NULL) {
        printf("malloc() failed in tcb_create_process_only()\n");
        free(process->exit_status);
        ht_remove_task(process->pid);
        free(process);
        return NULL;
    }
    process->exit_status_node->thr = (void*)process->exit_status;

    // Initially have one thread
    process->cur_thr_num = 1;
    
    if(simple_queue_init(&process->child_exit_status_list) < 0) {
        printf("simple_queue_init() failed in tcb_create_process_only()\n");
        free(process->exit_status);
        free(process->exit_status_node);
        ht_remove_task(process->pid);
        free(process);
        return NULL;
    }

    // Initialize task wait struct
    task_wait_t *task_wait = &process->task_wait_struct;
    if(simple_queue_init(&task_wait->wait_queue) < 0) {
        printf("simple_queue_init() failed in tcb_create_process_only()\n");
        simple_queue_destroy(&process->child_exit_status_list);
        free(process->exit_status);
        free(process->exit_status_node);
        ht_remove_task(process->pid);
        free(process);
        return NULL;
    }
    if(mutex_init(&task_wait->lock) < 0) {
        printf("mutex_init() failed in tcb_create_process_only()\n");
        simple_queue_destroy(&task_wait->wait_queue);
        simple_queue_destroy(&process->child_exit_status_list);
        free(process->exit_status);
        free(process->exit_status_node);
        ht_remove_task(process->pid);
        free(process);
        return NULL;
    }
    // Initially 0 alive child task
    task_wait->num_alive = 0;
    // Initially 0 zombie child task
    task_wait->num_zombie = 0;

    // Init page table lock
    int i;
    for(i = 0; i < NUM_PT_LOCKS_PER_PD; i++) {
        if(mutex_init(&process->pt_locks[i]) < 0) {
            printf("mutex_init() failed in tcb_create_process_only()\n");
            simple_queue_destroy(&task_wait->wait_queue);
            simple_queue_destroy(&process->child_exit_status_list);
            free(process->exit_status);
            free(process->exit_status_node);
            mutex_destroy(&task_wait->lock);
            int j;
            for(j = 0; j < i - 1; j++) {
                mutex_destroy(&process->pt_locks[j]);
            }
            ht_remove_task(process->pid);
            free(process);
            return NULL;
        }
    }

    // must be last step
    thread->pcb = process;

    return process;
}


/** @brief Create a thread without creating a process
 *
 *  A kernel stack(K_STACK_SIZE) will be allocated for the new thread.
 *
 *  @param process The process that the created thread belongs to
 *  @param state The initial state of the newly created thread
 *
 *  @return Thread control block data structure of the newly created thread, 
 *          return NULL on error (because of out of memory)
 */
tcb_t* tcb_create_thread_only(pcb_t* process, thread_state_t state) {
    tcb_t *thread = malloc(sizeof(tcb_t));
    if (thread == NULL) {
        return NULL;
    }
    thread->tid = atomic_add(&id_count, 1);
    thread->pcb = process;
    thread->state = state;

    thread->k_stack_esp = smemalign(K_STACK_SIZE, K_STACK_SIZE);
    if (thread->k_stack_esp == NULL) {
        free(thread);
        return NULL;
    } else 
        thread->k_stack_esp += K_STACK_SIZE;

    // Initially no swexn handler registered
    thread->swexn_struct = NULL;
    
    // set tcb table entry
    tcb_set_entry(thread->k_stack_esp-1, thread);

    return thread;
}

/** @brief Create a process with a thread
 *
 *  @param state The state for created thread
 *  @param new_page_table_base The page table base that will be used for the 
 *                             new process
 *
 *  @return Thread control block data structure of the newly created thread, 
 *          return NULL on error
 */
tcb_t* tcb_create_process(thread_state_t state, uint32_t new_page_table_base) {
    tcb_t *thread = tcb_create_thread_only(NULL, state);
    if (thread == NULL) {
        return NULL;
    }
    
    pcb_t *process = tcb_create_process_only(thread, NULL, new_page_table_base);
    if (process == NULL) {
        tcb_free_thread(thread);
        return NULL;
    }

    return thread;
}

/** @brief Release resources used by a thread
 *
 *  @param thread The thread to release resources
 *
 *  @return Void
 */
void tcb_free_thread(tcb_t *thr) {

    // Free stack
    void *stack_esp = thr->k_stack_esp;
    void *stack_low = tcb_get_low_addr(stack_esp);
    if(tcb_get_entry(stack_esp) == NULL) {
        panic("The stack to free is NULL");
    }
    tcb_table[GET_K_STACK_INDEX(stack_esp)] = NULL;
    sfree(stack_low, K_STACK_SIZE);

    // Free swexn struct
    if(thr->swexn_struct != NULL) {
        free(thr->swexn_struct);
        thr->swexn_struct = NULL;
    }

    // Free tcb
    free(thr);
}

/** @brief Release resources used by a thread
 *
 *  Unlike tcb_free_thread(), this function is not thread-safe (it calls 
 *  _free() and _sfree()). It is the caller's responsibility to make sure thread
 *  safe.
 *
 *  @param thread The thread to release resources
 *
 *  @return Void
 */
void tcb_vanish_thread(tcb_t *thr) {

    // Free stack
    void *stack_esp = thr->k_stack_esp;
    void *stack_low = tcb_get_low_addr(stack_esp);
    if(tcb_get_entry(stack_esp) == NULL) {
        panic("The stack to free is NULL");
    }
    tcb_table[GET_K_STACK_INDEX(stack_esp)] = NULL;
    _sfree(stack_low, K_STACK_SIZE);

    // Free swexn struct
    if(thr->swexn_struct != NULL) {
        _free(thr->swexn_struct);
        thr->swexn_struct = NULL;
    }

    // Free tcb
    _free(thr);
}

/** @brief Free pcb and all resources that are associated with it */
void tcb_free_process(pcb_t *process) {
    mutex_destroy(&process->task_wait_struct.lock);
    simple_queue_destroy(&process->task_wait_struct.wait_queue);
    simple_queue_destroy(&process->child_exit_status_list);

    // destroy page table lock
    int i;
    for(i = 0; i < NUM_PT_LOCKS_PER_PD; i++) {
        mutex_destroy(&process->pt_locks[i]);
    }

    free(process);
}

/** @brief set the tcb entry in tcb_table for a thread
 *
 *  @param addr An address of kernel stack of the thread
 *  @param thr The tcb struct to set
 *
 *  @return Void
 */
static void tcb_set_entry(void *addr, tcb_t *thr) {
    tcb_table[GET_K_STACK_INDEX(addr)] = thr;
}

/** @brief Get the tcb entry of a thread given its kernel stack address
 *
 *  @param addr An address of kernel stack of the thread
 *
 *  @return Thread control block data structure of the thread
 */
tcb_t* tcb_get_entry(void *addr) {
    return tcb_table[GET_K_STACK_INDEX(addr)];
}

/** @brief Get the highest kernel stack address of a thread 
 *
 *  @param addr An address of kernel stack of the thread
 *
 *  @return The highest kernel stack address of the thread
 */
void* tcb_get_high_addr(void *addr) {
    return (void*)((GET_K_STACK_INDEX(addr) + 1) * K_STACK_SIZE);
}

/** @brief Get the lowest kernel stack address of a thread 
 *
 *  @param addr An address of kernel stack of the thread
 *
 *  @return The lowest kernel stack address of the thread
 */
void* tcb_get_low_addr(void *addr) {
    return (void*)(GET_K_STACK_INDEX(addr) * K_STACK_SIZE);
}

/** @brief Detect if a thread is stack overflow. 
 *
 *  Note that the thread might not really stack overflow. It just exceeds the
 *  stack growth limit. But the kernel will think this is a dangerous sign and
 *  will kill the thread as soon as possible before it really stack overflow and
 *  overwrite data of other thread.
 *
 *  @param esp The current esp of a thread stack
 *
 *  @return Return 1 if it is considered as stack overflow, return 0 otherwise. 
 */
int tcb_is_stack_overflow(void *esp) {
    return ((tcb_get_high_addr(esp) - esp) > STACK_OVERFLOW_LIMIT);
}
