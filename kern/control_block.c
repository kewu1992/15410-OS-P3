#include <control_block.h>
#include <malloc.h>
#include <malloc_internal.h>
#include <common_kern.h>
#include <cr.h>
#include <asm_atomic.h>
#include <simple_queue.h>

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
 *  @param state The state for created process
 *  @param thread The (first) thread for created process
 *
 *  @return Process control block data structure of the newly created process, 
 *          return NULL on error
 */
pcb_t* tcb_create_process_only(process_state_t state, tcb_t* thread) {
    pcb_t *process = malloc(sizeof(pcb_t));
    if (process == NULL)
        return NULL;
    process->pid = thread->tid;
    process->page_table_base = get_cr3();
    process->state = state;

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
tcb_t* tcb_create_thread_only(pcb_t* process) {
    tcb_t *thread = malloc(sizeof(tcb_t));
    if (thread == NULL) {
        return NULL;
    }
    thread->tid = atomic_add(&id_count);
    thread->pcb = process;
    thread->k_stack_esp = smemalign(K_STACK_SIZE, K_STACK_SIZE) + 
                            K_STACK_SIZE - sizeof(simple_node_t);
    if (thread->k_stack_esp == NULL) {
        free(thread);
        return NULL;
    }
    
    // set tcb table entry
    tcb_set_entry(thread->k_stack_esp, thread);

    return thread;
}

/** @brief Create a process with a thread
 *
 *  The current cr3 will be used as the page table base of the new process.
 *  A kernel stack(K_STACK_SIZE) will be allocated for the new thread.
 *
 *  @param state The state for created process
 *
 *  @return Thread control block data structure of the newly created thread, 
 *          return NULL on error
 */
tcb_t* tcb_create_process(process_state_t state) {
    tcb_t *thread = tcb_create_thread_only(NULL);
    if (thread == NULL) {
        return NULL;
    }
    
    pcb_t *process = tcb_create_process_only(state, thread);
    if (process == NULL) {
        tcb_free_thread(thread);
        return NULL;
    }

    return thread;
}

void tcb_free_thread(tcb_t *thr) {

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
    // the very top of kernel stack for each thread is used for context switch
    // scheduler to store queue node
    return (void*)((GET_K_STACK_INDEX(addr) + 1) * K_STACK_SIZE 
                                                    - sizeof(simple_node_t));
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