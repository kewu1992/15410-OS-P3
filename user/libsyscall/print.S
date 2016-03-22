/** @file print.S
 *  @brief Asm wrapper for print syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

# int print(int size, char *buf)

.global print

print:
    pushl   %esi
    movl    %esp, %esi
    addl    $8, %esi
    int     $PRINT_INT
    popl    %esi
    ret
