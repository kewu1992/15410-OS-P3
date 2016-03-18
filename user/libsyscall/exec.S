/** @file exec.S
 *
 *  @brief Syscall stub for exec
 *  
 *  @author Jian Wang (jianwan3)
 *  @bug No known bugs
 */

#include <syscall_int.h>

/** @brief Syscall stub for exec
 *
 *  @return What exec syscall returns
 */ 
.globl exec 

exec:
pushl %esi          # Save callee save registers that will be used here
movl %esp, %esi     # Get current %esp
addl $8, %esi       # Place the addr of the 1st argument in %esi
int $EXEC_INT       # Do syscall
popl %esi           # Restore callee save registers
ret                 # Return from stub

