#include <control_block.h>
#include <malloc.h>
#include <malloc_internal.h>
#include <common_kern.h>
#include <cr.h>
#include <asm_atomic.h>
#include <list.h>

#include <simics.h>

#include <syscall_lifecycle.h>

/** @brief The lowest 13 bits of kernel memory are within the same k-stack */
#define K_STACK_BITS    13

/** @brief Kernel stack size for each thread is 8192 */
#define K_STACK_SIZE    (1<<13) 

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
pcb_t* tcb_create_process_only(tcb_t* thread) {

    lprintf("tcb_create_process_only called for tid %d", thread->tid);

    pcb_t *process = malloc(sizeof(pcb_t));
    if (process == NULL)
        return NULL;
    process->pid = thread->tid;
    process->page_table_base = thread->new_page_table_base;
    process->ppid = thread->pthr->pcb->pid;
    // ****************************Consider lock this operation
    // There's no thread fork now, all fork.
    // Parent task has one more child
    // thread->pthr->pcb->cur_child_num++;

    // Initially have one thread
    process->cur_thr_num = 1;
    // Initially exit status is 0
    process->exit_status = 0;

    if(list_init(&process->child_exit_status_list) < 0) {
        lprintf("list_init failed");
        panic("list_init failed");
    }


    // Initialize task wait struct
    task_wait_t *task_wait = &process->task_wait_struct;
    // Initially 0 alive child task
    task_wait->num_alive = 0;
    // Initially 0 zombie child task
    task_wait->num_zombie = 0;
    if(simple_queue_init(&task_wait->wait_queue) < 0) {
        lprintf("simple_queue_init failed");
        panic("simple_queue_init failed");
    }
    if(mutex_init(&task_wait->lock) < 0) {
        lprintf("mutex_init failed");
        panic("mutex_init failed");
    }


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
 *          return NULL on error
 */
tcb_t* tcb_create_thread_only(pcb_t* process, thread_state_t state) {
    tcb_t *thread = malloc(sizeof(tcb_t));
    if (thread == NULL) {
        return NULL;
    }
    thread->tid = atomic_add(&id_count, 1);
    thread->pcb = process;
    thread->state = state;
    thread->k_stack_esp = smemalign(K_STACK_SIZE, K_STACK_SIZE) + K_STACK_SIZE;
    if (thread->k_stack_esp == NULL) {
        free(thread);
        return NULL;
    }
    
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
tcb_t* tcb_create_process(thread_state_t state) {
    tcb_t *thread = tcb_create_thread_only(NULL, state);
    if (thread == NULL) {
        return NULL;
    }
    
    pcb_t *process = tcb_create_process_only(thread);
    if (process == NULL) {
        tcb_free_thread(thread);
        return NULL;
    }

    return thread;
}

void tcb_free_thread(tcb_t *thr) {

    lprintf("free tid: %d", thr->tid);
    // Free stack
    void *stack_esp = thr->k_stack_esp;
    void *stack_low = tcb_get_low_addr(stack_esp);
    if(stack_low == NULL) {
        lprintf("The stack to free is NULL");
        panic("The stack to free is NULL");
    }
    sfree(stack_low, K_STACK_SIZE);

    // Free tcb
    free(thr);
    tcb_table[GET_K_STACK_INDEX(stack_esp)] = NULL;

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
