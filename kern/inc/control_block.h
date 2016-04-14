/** @file control_block.h
 *  @brief This file contains structure definitions for thread control block 
 *         (tcb) and process control block (pcb) and function prototypes for 
 *         control_block.c.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _CONTROL_BLOCK_H_
#define _CONTROL_BLOCK_H_

#include <stdint.h>
#include <simple_queue.h>
#include <ureg.h>
#include <mutex.h>
#include <vm.h>

/** @brief The lowest 13 bits of kernel memory are within the same k-stack */
#define K_STACK_BITS    13

/** @brief Kernel stack size for each thread is 8192 bytes */
#define K_STACK_SIZE    (1<<K_STACK_BITS) 

/** @brief The state of a thread can be normal (running or runnable), blocked
 *         (due to OP_BLOCK), MADE_RUNNABLE (due to OP_MAKE_RUNNABLE) or 
 *          WAKEUP (due to OP_RESUME)  */
typedef enum {
    NORMAL,
    BLOCKED,
    MADE_RUNNABLE,
    WAKEUP
} thread_state_t;

/** @brief The struct to store exit status for a task */
typedef struct {
    /** @brief The vanished task's pid */
    int pid;
    /** @brief The vanished task's exit status */
    int status;
} exit_status_t;

/* @brief Data structure for wait() syscall. Each task (pcb) has one if 
 *        this struct */
typedef struct {
    /** @brief The number of alive child tasks and zombie child tasks */
    int num_alive, num_zombie;
    /** @brief A queue for threads that invoke wait() to block on */
    simple_queue_t wait_queue;

    mutex_t lock;
} task_wait_t;

typedef struct pcb_t {
    int pid;
    uint32_t page_table_base;
    /** @brief Parent task's pid */
    int ppid;
    /** @brief Child tasks exit status list. When a child task dies, it will put
     *         its exit_status_node to the parent's child_exit_status_list */
    simple_queue_t child_exit_status_list;
    /** @brief Exit status of the task */
    exit_status_t *exit_status;
    /** @brief Exit status node that will be inserted to parent task's
     *         child_exit_status_list when the task dies */
    simple_node_t *exit_status_node;

    /** @brief Current number of alive threads in the task, determine if 
      *        report task exit status and free task resources */
    int cur_thr_num;

    task_wait_t task_wait_struct;

    /** @brief The locks for page table */
    mutex_t pt_locks[NUM_PT_LOCKS_PER_PD];

} pcb_t;


/** @brief The swexn handler type */
typedef void (*swexn_handler_t)(void *arg, ureg_t *ureg);

/** @brief The parameters for registered swexn handler */
typedef struct swexn_t {
    void *esp3;
    swexn_handler_t eip;
    void *arg;
} swexn_t;

typedef struct tcb_t {
    void *k_stack_esp;
    int tid;
    pcb_t *pcb;
    /** @brief This will be used to store result of system call */
    int result;
    thread_state_t state;
    
    /** @brief The parameters for registered swexn handler */
    swexn_t *swexn_struct;
} tcb_t;


int tcb_init();

pcb_t* tcb_create_process_only(tcb_t* thread, tcb_t* pthr, 
                                                  uint32_t new_page_table_base);

tcb_t* tcb_create_thread_only(pcb_t* process, thread_state_t state);

tcb_t* tcb_create_process(thread_state_t state, uint32_t new_page_table_base);

void tcb_free_thread(tcb_t *thr);

void tcb_vanish_thread(tcb_t *thr);

void tcb_free_process(pcb_t *process);

tcb_t* tcb_get_entry(void *addr);

void* tcb_get_high_addr(void *addr);

void* tcb_get_low_addr(void *addr);

int tcb_is_stack_overflow(void *addr);

#endif
