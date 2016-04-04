/** @file set_cursor_pos.S
 *  @brief Asm wrapper for set_cursor_pos syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

# int set_cursor_pos(int row, int col);

.global set_cursor_pos

set_cursor_pos:
pushl   %esi
movl    %esp, %esi
addl    $8, %esi
int     $SET_CURSOR_POS_INT
popl    %esi
ret
