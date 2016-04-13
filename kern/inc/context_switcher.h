/** @file context_switcher.h
 *  @brief This file contains definition of operations of context switcher and 
 *         function interfaces of context_switcher.c.
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _CONTEXT_SWITCHER_H_
#define _CONTEXT_SWITCHER_H_

#include <stdint.h>

#define OP_CONTEXT_SWITCH 0
#define OP_FORK 1
#define OP_THREAD_FORK 2
#define OP_BLOCK 3
#define OP_MAKE_RUNNABLE 4
#define OP_RESUME 5
#define OP_YIELD 6

int context_switcher_init();

void context_switch(int op, uint32_t arg);

#endif