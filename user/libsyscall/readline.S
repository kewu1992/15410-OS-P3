/** @file readline.S
 *  @brief Asm wrapper for readline syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

# int readline(int size, char *buf);

.global readline

readline:
pushl   %esi
movl    %esp, %esi
addl    $8, %esi
int     $READLINE_INT
popl    %esi
ret
