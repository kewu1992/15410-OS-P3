#include <control_block.h>
#include <malloc.h>
#include <common_kern.h>


#define GET_K_STACK_INDEX(x)    (((unsigned int)(x)) >> K_STACK_BITS)

static tcb_t **tcb_table;

static int id_count = 0;

int tcb_init() {
    tcb_table = calloc(USER_MEM_START/K_STACK_SIZE, sizeof(tcb_t*));
    return 0;
}

int tcb_next_id() {
    return id_count++;
}

void tcb_set_entry(void *addr, tcb_t *thr) {
    tcb_table[GET_K_STACK_INDEX(addr)] = thr;
}

tcb_t* tcb_get_entry(void *addr) {
    return tcb_table[GET_K_STACK_INDEX(addr)];
}