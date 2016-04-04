/** @file set_status.S
 *
 *  @brief Syscall stub for set_status
 *  
 *  @author Jian Wang (jianwan3)
 *  @bug No known bugs
 */

#include <syscall_int.h>

/** @brief Syscall stub for set_status
 *
 *  @return void
 */ 
.globl set_status

set_status:
pushl %esi          # Save callee save registers that will be used here
movl 8(%esp), %esi  # Place the only argument in %esi
int $SET_STATUS_INT # Do Syscal
popl %esi           # Restore callee save registers
ret                 # Return from stub

