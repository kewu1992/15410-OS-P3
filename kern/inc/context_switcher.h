/** @file context_switcher.h
 *  @brief This file contains definition of operations of context switcher and 
 *         function interfaces of context_switcher.c.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _CONTEXT_SWITCHER_H_
#define _CONTEXT_SWITCHER_H_

#include <stdint.h>

/** @brief Normal context switch driven by timer interupt. */
#define OP_CONTEXT_SWITCH 0
 /** @brief Fork and context switch to the new process. */
#define OP_FORK 1
/** @brief Thread_fork and context switch to the new thread. */
#define OP_THREAD_FORK 2
/** @brief Block the calling thread and let scheduler to 
 *  choose the next thread to run. 
 */
#define OP_BLOCK 3
/** @brief Make runable a blocked thread identified by its tcb. */
#define OP_MAKE_RUNNABLE 4
/** @brief Resume (wake up) a blocked thread (make runnable 
 *  and context switch to that thread immediately) 
 */
#define OP_RESUME 5
/** @brief Yield to -1 or a given tid. */
#define OP_YIELD 6
/** @brief Send message and block the calling thread. */
#define OP_SEND_MSG 7


int context_switcher_init();

void context_switch(int op, uint32_t arg);

#endif