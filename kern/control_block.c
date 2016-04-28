/** @file control_block.c
 *  @brief This file contains functions related to thread control block (tcb)
 *         and process control block (pcb).
 *
 *  The kernel will allocate a tcb struct and a kernel stack for each thread 
 *  when it is created. When each thread is running in kernel mode, it run
 *  on its kernel stack. The size of each kernel stack is defined as 
 *  K_STACK_SIZE. The start address of each kernel stack algins to K_STACK_SIZE.
 *
 *  All threads belong to the same task (process) share a same pcb struct. 
 *  In each tcb, there is a pointer points to the pcb. Pcb is used to store
 *  the task-level resources, such as page table address, parent task's pid. 
 *
 *  **************************************
 *  *           P4 new desigin           *
 *  **************************************
 *  Unlike P3 where we put all tcbs in an array and find a tcb by indexing. In
 *  P4 we put the tcb data structure of each thread on the *top* of its kernel
 *  stack space. So the very top space with the size sizeof(tcb_t) of kernel
 *  stack of each thread is its tcb. The thread can still get its tcb very 
 *  quickly. It just need to know the highest address of its kernel stack.
 *  The reason we do so is to avoid allocating tcb_table array for each core 
 *  which is memory consuming. 
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
#include <smp.h>

/** @brief Get the index in tcb_table array based on kernel stack address */
#define GET_K_STACK_INDEX(x)    (((unsigned int)(x)) >> K_STACK_BITS)

/** @brief The limit that a kernel stack can grow. The stack is considered
 *         overflow if it exceeds this limit and should be killed */
#define STACK_OVERFLOW_LIMIT    0x1C00

/** @brief Counter that will be used for assigning tid and pid */
static int id_count = -1;

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
        // out of memory
        return NULL;
    }
    process->pid = thread->tid;
    process->page_table_base = new_page_table_base;
    if (pthr)
        process->ppid = pthr->pcb->pid;
    else
        process->ppid = -1;

    // Initially exit status is 0
    process->status = 0;

    // Initially have one thread
    process->cur_thr_num = 1;    

    // Init page table lock
    int i;
    for(i = 0; i < NUM_PT_LOCKS_PER_PD; i++) {
        if(mutex_init(&process->pt_locks[i]) < 0) {
            int j;
            for(j = 0; j < i - 1; j++) {
                mutex_destroy(&process->pt_locks[j]);
            }
           
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
    void* k_stack_esp = smemalign(K_STACK_SIZE, K_STACK_SIZE);
    if (k_stack_esp == NULL)
        return NULL;
        
    tcb_t *thread = (tcb_t*)tcb_get_entry(k_stack_esp);

    thread->my_msg = malloc(sizeof(msg_t));
    if (thread->my_msg == NULL) {
        sfree(k_stack_esp, K_STACK_SIZE);
        return NULL;
    }
    thread->my_msg->node.thr = (void*)(thread->my_msg);

    thread->k_stack_esp = tcb_get_high_addr(k_stack_esp);
    thread->tid = atomic_add(&id_count, 1);
    thread->pcb = process;
    thread->state = state;

    // Initially no swexn handler registered
    thread->swexn_struct = NULL;

    thread->ori_cpu = smp_get_cpu();

    return thread;
}

/** @brief Create a idle process with a thread
 *
 *  @param state The state for created thread
 *  @param new_page_table_base The page table base that will be used for the 
 *                             new process
 *
 *  @return Thread control block data structure of the newly created thread, 
 *          return NULL on error
 */
tcb_t* tcb_create_idle_process(thread_state_t state, 
                                            uint32_t new_page_table_base) {
    tcb_t *thread = tcb_create_thread_only(NULL, state);
    if (thread == NULL) {
        return NULL;
    }

    pcb_t *process = malloc(sizeof(pcb_t));
    if (process == NULL) {
        return NULL;
    }
    process->pid = thread->tid;
    process->page_table_base = new_page_table_base;


    // Initially have one thread
    process->cur_thr_num = 1;


    // Init page table lock
    int i;
    for(i = 0; i < NUM_PT_LOCKS_PER_PD; i++) {
        if(mutex_init(&process->pt_locks[i]) < 0) {
            return NULL;
        }
    }

    thread->pcb = process;

    return thread;
}

/** @brief Release resources used by a thread
 *
 *  @param thr The thread to release resources
 *
 *  @return Void
 */
void tcb_free_thread(tcb_t *thr) {

    // Free swexn struct
    if(thr->swexn_struct != NULL) {
        free(thr->swexn_struct);
        thr->swexn_struct = NULL;
    }

    // Free stack
    void *stack_esp = thr->k_stack_esp;
    void *stack_low = tcb_get_low_addr(stack_esp);
    if(tcb_get_entry(stack_esp) == NULL) {
        panic("The stack to free is NULL");
    }
    sfree(stack_low, K_STACK_SIZE);

}

/** @brief Release resources used by a thread
 *
 *  Unlike tcb_free_thread(), this function is not thread-safe (it calls 
 *  _free() and _sfree()). It is the caller's responsibility to make sure thread
 *  safe.
 *
 *  @param thr The thread to release resources
 *
 *  @return Void
 */
void tcb_vanish_thread(tcb_t *thr) {

    // Free swexn struct
    if(thr->swexn_struct != NULL) {
        _free(thr->swexn_struct);
        thr->swexn_struct = NULL;
    }

    // Free stack
    void *stack_esp = thr->k_stack_esp;
    void *stack_low = tcb_get_low_addr(stack_esp);
    if(tcb_get_entry(stack_esp) == NULL) {
        panic("The stack to free is NULL");
    }
    _sfree(stack_low, K_STACK_SIZE);

}

/** @brief Free pcb and all resources that are associated with it 
 *  
 *  @param process The process to free
 * 
 *  @return void
 */
void tcb_free_process(pcb_t *process) {

    // destroy page table lock
    int i;
    for(i = 0; i < NUM_PT_LOCKS_PER_PD; i++) {
        mutex_destroy(&process->pt_locks[i]);
    }

    free(process);
}

/** @brief Get the tcb entry of a thread given its kernel stack address
 *
 *  @param addr An address of kernel stack of the thread
 *
 *  @return Thread control block data structure of the thread
 */
tcb_t* tcb_get_entry(void *addr) {
    return (tcb_t*)tcb_get_high_addr(addr);
}

/** @brief Get the highest kernel stack address of a thread 
 *
 *  @param addr An address of kernel stack of the thread
 *
 *  @return The highest kernel stack address of the thread
 */
void* tcb_get_high_addr(void *addr) {
    return (void*)((GET_K_STACK_INDEX(addr) + 1) * K_STACK_SIZE - 
                                                                sizeof(tcb_t));
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
