/** @file swexn.S
 *  @brief Asm wrapper for swexn syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

# int swexn(void *esp3, swexn_handler_t eip, void *arg, ureg_t *newureg);

.global swexn

swexn:
pushl   %esi
movl    %esp, %esi
addl    $8, %esi
int     $SWEXN_INT
popl    %esi
ret

