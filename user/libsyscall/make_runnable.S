/** @file make_runnable.S
 *  @brief Asm wrapper for make_runnable syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

.globl make_runnable

make_runnable:
pushl %esi         
movl 8(%esp), %esi  
int $MAKE_RUNNABLE_INT       
popl %esi         
ret              

