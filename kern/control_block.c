#include <control_block.h>
#include <malloc.h>
#include <malloc_internal.h>
#include <common_kern.h>
#include <cr.h>
#include <asm_atomic.h>
#include <list.h>

#include <simics.h>

#include <syscall_inter.h>
#include <stdio.h>

/** @brief Get index of tcb table based on kernel stack address */
#define GET_K_STACK_INDEX(x)    (((unsigned int)(x)) >> K_STACK_BITS)

/** @brief This tcb table contains thread control block data structure for
 *         all threads created by kernel.
 */
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
 *  The current cr3 will be used as the page table base of the new process
 *
 *  @param thread The (first) thread for created process
 *
 *  @return Process control block data structure of the newly created process, 
 *          return NULL on error
 */
pcb_t* tcb_create_process_only(tcb_t* thread, tcb_t* pthr, uint32_t new_page_table_base) {

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

    process->exit_status = malloc(sizeof(exit_status_t));
    if (process->exit_status == NULL) {
        printf("malloc() failed in tcb_create_process_only()\n");
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
        free(process);
        return NULL;
    }
    process->exit_status_node->thr = (void*)process->exit_status;

    // Initially have one thread
    process->cur_thr_num = 1;
    
    if(simple_queue_init(&process->child_exit_status_list) < 0) {
        printf("list_init() failed in tcb_create_process_only()\n");
        free(process->exit_status);
        free(process->exit_status_node);
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
        free(process);
        return NULL;
    }
    if(mutex_init(&task_wait->lock) < 0) {
        printf("mutex_init() failed in tcb_create_process_only()\n");
        simple_queue_destroy(&task_wait->wait_queue);
        simple_queue_destroy(&process->child_exit_status_list);
        free(process->exit_status);
        free(process->exit_status_node);
        free(process);
        return NULL;
    }
    // Initially 0 alive child task
    task_wait->num_alive = 0;
    // Initially 0 zombie child task
    task_wait->num_zombie = 0;

    // Put pid to pcb mapping in hashtable
    ht_put_task(process->pid, process);

    // must be last step
    thread->pcb = process;

    return process;
}


/** @brief Create a thread without creating a process
 *
 *  A kernel stack(K_STACK_SIZE) will be allocated for the new thread.
 *
 *  @param process The process that the created thread belongs to
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
 *  The current cr3 will be used as the page table base of the new process.
 *  A kernel stack(K_STACK_SIZE) will be allocated for the new thread.
 *
 *  @param state The state for created thread
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

void tcb_free_thread(tcb_t *thr) {

    lprintf("free tcb and stack for thr %d", thr->tid);
    // Free stack
    void *stack_esp = thr->k_stack_esp;
    void *stack_low = tcb_get_low_addr(stack_esp);
    if(tcb_get_entry(stack_esp) == NULL) {
        lprintf("The stack to free is NULL");
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


void tcb_set_entry(void *addr, tcb_t *thr) {
    tcb_table[GET_K_STACK_INDEX(addr)] = thr;
}

/** @brief Get tcb entry of a thread given its kernel stack address
 *
 *  @param addr Any kernel stack address of the thread
 *
 *  @return Thread control block data structure of the thread
 */
tcb_t* tcb_get_entry(void *addr) {
    return tcb_table[GET_K_STACK_INDEX(addr)];
}

/** @brief Get the highest kernel stack address of a thread 
 *
 *  @param addr Any kernel stack address of the thread
 *
 *  @return The highest kernel stack address of the thread
 */
void* tcb_get_high_addr(void *addr) {
    return (void*)((GET_K_STACK_INDEX(addr) + 1) * K_STACK_SIZE);
}

/** @brief Get the lowest kernel stack address of a thread 
 *
 *  @param addr Any kernel stack address of the thread
 *
 *  @return The lowest kernel stack address of the thread
 */
void* tcb_get_low_addr(void *addr) {
    return (void*)(GET_K_STACK_INDEX(addr) * K_STACK_SIZE);
}
