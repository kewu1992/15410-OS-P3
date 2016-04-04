/** @file sleep.S
 *  @brief Asm wrapper for sleep syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

.globl sleep

sleep:
pushl %esi         
movl 8(%esp), %esi  
int $SLEEP_INT       
popl %esi         
ret              

