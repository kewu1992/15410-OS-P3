/** @file vanish.S
 *
 *  @brief Syscall stub for vanish
 *  
 *  @author Jian Wang (jianwan3)
 *  @bug No known bugs
 */

#include <syscall_int.h>

/** @brief Syscall stub for vanish
 *
 *  @return No return
 */ 
.globl vanish

vanish:
int $VANISH_INT # Syscall of vanish
ret             # Return from stub, should never reach here though

