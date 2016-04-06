/** @file yield.S
 *
 *  @brief Syscall stub for yield
 *  
 *  @author Jian Wang (jianwan3)
 *  @bug No known bugs
 */

#include <syscall_int.h>

.globl yield

yield:
pushl %esi          # Save callee save registers that will be used here
movl 8(%esp), %esi  # Place the only argument in %esi
int $YIELD_INT      # Do syscall
popl %esi           # Restore callee save registers
ret                 # Return from stub

