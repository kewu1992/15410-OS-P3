#include <control_block.h>
#include <malloc.h>
#include <malloc_internal.h>
#include <common_kern.h>
#include <cr.h>
#include <asm_atomic.h>
#include <list.h>

#include <simics.h>

#include <syscall_lifecycle.h>

/* k-stack size is 8192 */
#define K_STACK_BITS    13

#define K_STACK_SIZE    (1<<13) 

#define GET_K_STACK_INDEX(x)    (((unsigned int)(x)) >> K_STACK_BITS)

static tcb_t **tcb_table;

static int id_count = -1;

static void tcb_set_entry(void *addr, tcb_t *thr);

int tcb_init() {
    tcb_table = _calloc(USER_MEM_START/K_STACK_SIZE, sizeof(tcb_t*));
    return 0;
}

pcb_t* tcb_create_process_only(process_state_t state, tcb_t* thread) {

    lprintf("tcb_create_process_only called for tid %d", thread->tid);

    pcb_t *process = malloc(sizeof(pcb_t));
    if (process == NULL)
        return NULL;
    process->pid = thread->tid;
    process->page_table_base = thread->new_page_table_base;
    process->state = state;
    process->ppid = thread->pthr->pcb->pid;
    // ****************************Consider lock this operation
    // There's no thread fork now, all fork.
    // Parent task has one more child
    // thread->pthr->pcb->cur_child_num++;

    // Initially have one thread
    process->cur_thr_num = 1;
    // Initially exit status is 0
    process->exit_status = 0;
    // Initially no children task
    process->cur_child_num = 0;

    
    if(spinlock_init(&process->lock_cur_thr_num) < 0) {
        lprintf("spinlock_init failed");
        panic("spinlock_init failed");
    }

    if(spinlock_init(&process->lock_cur_child_num) < 0) {
        lprintf("spinlock_init failed");
        panic("spinlock_init failed");
    }
    

    if(list_init(&process->child_exit_status_list) < 0) {
        lprintf("list_init failed");
        panic("list_init failed");
    }

    if(list_init(&process->wait_list) < 0) {
        lprintf("list_init failed");
        panic("list_init failed");
    }

    // Put pid to pcb mapping in hashtable
    ht_put_task(process->pid, process);

    // must be last step
    thread->pcb = process;

    return process;
}

tcb_t* tcb_create_thread_only(pcb_t* process) {
    tcb_t *thread = malloc(sizeof(tcb_t));
    if (thread == NULL) {
        return NULL;
    }
    thread->tid = atomic_add(&id_count);
    thread->pcb = process;
    thread->k_stack_esp = smemalign(K_STACK_SIZE, K_STACK_SIZE) + K_STACK_SIZE - sizeof(simple_node_t);
    if (thread->k_stack_esp == NULL) {
        free(thread);
        return NULL;
    }
    
    // set tcb table entry
    tcb_set_entry(thread->k_stack_esp, thread);

    return thread;
}

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

tcb_t* tcb_get_entry(void *addr) {
    return tcb_table[GET_K_STACK_INDEX(addr)];
}

void* tcb_get_high_addr(void *addr) {
    // the very top of kernel stack for each thread is used for context switch
    // scheduler to store queue node
    return (void*)((GET_K_STACK_INDEX(addr) + 1) * K_STACK_SIZE - sizeof(simple_node_t));
}

void* tcb_get_low_addr(void *addr) {
    return (void*)(GET_K_STACK_INDEX(addr) * K_STACK_SIZE);
}
