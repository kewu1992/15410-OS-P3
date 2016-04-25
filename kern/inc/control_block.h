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
#include <smp_message.h>

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

/** @brief Process control block */
typedef struct pcb_t {
    /** @brief pid */
    int pid;
    
    /** @brief The page table base of the task */
    uint32_t page_table_base;
    
    /** @brief Parent task's pid */
    int ppid;
    
    int status;
    
    /** @brief Current number of alive threads in the task, determine if 
      *        report task exit status and free task resources */
    int cur_thr_num;

    /** @brief The locks for page table */
    mutex_t pt_locks[NUM_PT_LOCKS_PER_PD];

} pcb_t;


/** @brief The swexn handler type */
typedef void (*swexn_handler_t)(void *arg, ureg_t *ureg);

/** @brief The parameters for registered swexn handler */
typedef struct swexn_t {
    /** @brief Swexn handler stack */
    void *esp3;
    /** @brief Swexn handler address */
    swexn_handler_t eip;
    /** @brief Argument to pass to swexn handler */
    void *arg;
} swexn_t;

/** @brief Thread control block */
typedef struct tcb_t {
    /** @brief Kernel stack position for this thread */
    void *k_stack_esp;
    /** @brief Thread id */
    int tid;
    /** @brief thread's task's pcb */
    pcb_t *pcb;
    /** @brief This will be used to store result of system call */
    int result;
    /** @brief The current state of thread */
    thread_state_t state;
    /** @brief The parameters for registered swexn handler */
    swexn_t *swexn_struct;

    msg_t* my_msg;

    /** @brief Stores which cpu malloc() the kernel stack for this thread */
    int ori_cpu;
} tcb_t;



pcb_t* tcb_create_process_only(tcb_t* thread, tcb_t* pthr, 
                                            uint32_t new_page_table_base);

tcb_t* tcb_create_thread_only(pcb_t* process, thread_state_t state);

tcb_t* tcb_create_idle_process(thread_state_t state, uint32_t new_page_table_base);

void tcb_free_thread(tcb_t *thr);

void tcb_vanish_thread(tcb_t *thr);

void tcb_free_process(pcb_t *process);

tcb_t* tcb_get_entry(void *addr);

void* tcb_get_high_addr(void *addr);

void* tcb_get_low_addr(void *addr);

int tcb_is_stack_overflow(void *addr);

#endif
