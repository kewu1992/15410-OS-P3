#include <control_block.h>
#include <malloc.h>
#include <common_kern.h>
#include <cr.h>
#include <asm_atomic.h>

/* k-stack size is 8192 */
#define K_STACK_BITS    13

#define K_STACK_SIZE    (1<<13) 

#define GET_K_STACK_INDEX(x)    (((unsigned int)(x)) >> K_STACK_BITS)

static tcb_t **tcb_table;

// need lock!!!!!!!!!!!!!!
static int id_count = 0;

static void tcb_set_entry(void *addr, tcb_t *thr);

int tcb_init() {
    tcb_table = calloc(USER_MEM_START/K_STACK_SIZE, sizeof(tcb_t*));
    return 0;
}

pcb_t* tcb_create_process_only(process_state_t state, tcb_t* thread) {
    pcb_t *process = malloc(sizeof(pcb_t));
    if (process == NULL)
        return NULL;
    process->pid = thread->tid;
    process->page_table_base = get_cr3();
    process->state = state;

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
    thread->k_stack_esp = smemalign(K_STACK_SIZE, K_STACK_SIZE) + K_STACK_SIZE;
    if (thread->k_stack_esp == NULL) {
        free(thread);
        return NULL;
    }
    
    // set tcb table entry
    tcb_set_entry(thread->k_stack_esp-1, thread);

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

}

void tcb_set_entry(void *addr, tcb_t *thr) {
    tcb_table[GET_K_STACK_INDEX(addr)] = thr;
}

tcb_t* tcb_get_entry(void *addr) {
    return tcb_table[GET_K_STACK_INDEX(addr)];
}

void* tcb_get_high_addr(void *addr) {
    return (void*)((GET_K_STACK_INDEX(addr) + 1) * K_STACK_SIZE);
}

void* tcb_get_low_addr(void *addr) {
    return (void*)(GET_K_STACK_INDEX(addr) * K_STACK_SIZE);
}