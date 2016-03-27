/** @file halt.S
 *  @brief Asm wrapper for halt syscall
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <syscall_int.h>

# void halt();

.globl halt

halt:
int     $HALT_INT    # Syscall of halt
ret                  # Return from stub
