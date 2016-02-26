/** @file gettid.S
 *  @brief Asm wrapper for gettid syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

.globl gettid

gettid:
int $GETTID_INT       
ret              

