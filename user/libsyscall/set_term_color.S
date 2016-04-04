/** @file set_term_color.S
 *  @brief Asm wrapper for set_term_color syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

# int set_term_color(int color);

.globl set_term_color

set_term_color:
pushl   %esi          
movl    8(%esp), %esi  
int     $SET_TERM_COLOR_INT 
popl    %esi           
ret 
