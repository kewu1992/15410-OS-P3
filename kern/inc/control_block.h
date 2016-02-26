
// how to define a proper size?
#define K_STACK_SIZE    1024

typedef enum {
    RUNNING,
    RUNNABLE,
    BLOCKED
} process_state_t;

typedef struct {
    int pid;
    void *page_table_base;
    process_state_t state;
} pcb_t;

typedef struct {
    int tid;
    pcb_t *pcb;
    void *k_stack_esp;
} tcb_t;