/** @file get_ticks.S
 *  @brief Asm wrapper for get_ticks syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

.globl get_ticks

get_ticks:
int $GET_TICKS_INT       
ret              

