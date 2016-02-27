
/* k-stack size is 8192 */
#define K_STACK_BITS    13
#define K_STACK_SIZE    (1<<13) 

#define GET_K_STACK_INDEX(x)    (((unsigned int)(x)) >> K_STACK_BITS)

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
