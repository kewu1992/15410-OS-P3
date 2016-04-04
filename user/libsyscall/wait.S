/** @file wait.S
 *  @brief Asm wrapper for wait syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

.globl wait

wait:
pushl %esi         
movl 8(%esp), %esi  
int $WAIT_INT       
popl %esi         
ret              

