/** @file asm_get_ebp.S
 *  @brief Get current %ebp value
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

/* define asm_get_ebp label */
.global asm_get_ebp

asm_get_ebp:
movl    %ebp,%eax # Get current %ebp as return value
ret
