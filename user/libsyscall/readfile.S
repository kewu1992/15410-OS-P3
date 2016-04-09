/** @file readfile.S
 *  @brief Asm wrapper for readfile syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

# int readfile(char *filename, char *buf, int count, int offset);

.global readfile

readfile:
pushl   %esi
movl    %esp, %esi
addl    $8, %esi
int     $READFILE_INT
popl    %esi
ret
