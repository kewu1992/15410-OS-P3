/** @file deschedule.S
 *  @brief Asm wrapper for deschedule syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */
#include <syscall_int.h>

.globl deschedule

deschedule:
pushl %esi         
movl 8(%esp), %esi  
int $DESCHEDULE_INT       
popl %esi         
ret              

