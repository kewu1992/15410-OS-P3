/** @file asm_xchg.S
 *
 *  @brief Atomically exchange two values.
 *  
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs
 */
# int asm_xchg(int *lock_available, int val);

.globl asm_xchg

asm_xchg:
movl    4(%esp), %ecx   # Get lock_available
movl    8(%esp), %eax   # Get val
xchg    (%ecx), %eax    # atomically exchange *lock_available with val
ret                     # Return old (*lock_availble)

