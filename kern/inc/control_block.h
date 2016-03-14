#ifndef _CONTROL_BLOCK_H_
#define _CONTROL_BLOCK_H_

#include <stdint.h>

/* k-stack size is 8192 */
#define K_STACK_BITS    13
#define K_STACK_SIZE    (1<<13) 

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
    int tid;
    pcb_t *pcb;
    void *k_stack_esp;
} tcb_t;


int tcb_init();

int tcb_next_id();

void tcb_set_entry(void *addr, tcb_t *thr);

tcb_t* tcb_get_entry(void *addr);

#endif