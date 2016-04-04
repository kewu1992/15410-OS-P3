#ifndef _CONTROL_BLOCK_H_
#define _CONTROL_BLOCK_H_

#include <stdint.h>
#include <list.h>

typedef enum {
    RUNNING,
    RUNNABLE,
    BLOCKED
} process_state_t;

typedef struct pcb_t {
    int pid;
    uint32_t page_table_base;
    process_state_t state;
    /** @brief Parent task's pcb, there's no information regarding if it's 
      * dead, don't use it, instead, look it up in hashtable, 
      * key pid, value pcb.
      */
    // struct pcb_t *ppcb;
    /** @brief Parent task's pid */
    int ppid;
    /** @brief Children task exit status list */
    list_t child_exit_status_list;
    /** @brief Exit status of the task */
    int exit_status;
    /** @brief Current number of alive threads in the task */
    int cur_thr_num;
    spinlock_t lock_cur_thr_num;
    /** @brief Current number of alive children task, if there's no child 
      * child task, return directly with an negative integer error.
      */
    int cur_child_num;
    spinlock_t lock_cur_child_num;
    /** @brief List of threads that are waiting to reap children */
    list_t wait_list;
} pcb_t;

typedef struct tcb_t {
    void *k_stack_esp;
    int tid;
    pcb_t *pcb;
    int result;
    /** @brief Parent thread's tcb, this field cannot be used to identify
      * which thread is waiting, because the thread that is waiting isn't 
      * necessarily the thread that creates the current vanishing thread.
      */
    struct tcb_t *pthr;
    /** @brief New address space for forked process */
    uint32_t new_page_table_base;
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
