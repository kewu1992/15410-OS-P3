/** @file fork.S
 *
 *  @brief Syscall stub for fork
 *  
 *  @author Jian Wang (jianwan3)
 *  @bug No known bugs
 */

#include <syscall_int.h>

/** @brief Syscall stub for fork
 *
 *  @return What fork syscall returns
 */ 
.globl fork

fork:
int $FORK_INT       # Do syscall
ret                 # Return from stub

