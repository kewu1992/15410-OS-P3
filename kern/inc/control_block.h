#ifndef _CONTROL_BLOCK_H_
#define _CONTROL_BLOCK_H_

#include <stdint.h>

typedef enum {
    RUNNING,
    RUNNABLE,
    BLOCKED
} process_state_t;

typedef struct {
    int pid;
    uint32_t page_table_base;
    process_state_t state;
} pcb_t;

typedef struct {
    void *k_stack_esp;
    int tid;
    pcb_t *pcb;
    int fork_result;
} tcb_t;


int tcb_init();

pcb_t* tcb_create_process_only(process_state_t state, tcb_t* thread);

tcb_t* tcb_create_thread_only(pcb_t* process);

tcb_t* tcb_create_process(process_state_t state);

void tcb_free_thread(tcb_t *thr);

tcb_t* tcb_get_entry(void *addr);

void* tcb_get_high_addr(void *addr);

void* tcb_get_low_addr(void *addr);

#endif