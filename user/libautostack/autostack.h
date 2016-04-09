/** @file autostack.h
 *  @brief Declarations for the autostack lib
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug None known
 */
#ifndef _AUTOSTACK_H
#define _AUTOSTACK_H

#include <stdint.h>
#include <simics.h> 

#include <syscall.h>
#include <stddef.h>
#include <stdlib.h> 

#include <lib_public.h>

/** @brief Exception stack size 
 *
 * The size is set to be sufficient enough, the exception handler doesn't
 * do something requires substantial stack space.
 *
 */
#define EXCEPTION_STACK_SIZE (PAGE_SIZE/16)

int allocate_pages(uint32_t range_high, uint32_t range_low);
void swexn_handler(void *arg, ureg_t *ureg);
void install_autostack(void *stack_high, void *stack_low);

#endif /* _AUTOSTACK_H */

