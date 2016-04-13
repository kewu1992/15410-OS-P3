/** @file asm_atomic.S
 *
 *  @brief This file contains implementation of atomic_add() and asm_xchg().
 *  
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs
 */

# int atomic_add(int* num, int val)
.globl atomic_add

# int asm_xchg(int *lock_available, val);
.globl asm_xchg

atomic_add:
    pushl   %ebx                    # save old %ebx
    movl    12(%esp), %ebx          # %ebx = val
    movl    8(%esp), %ecx           # %ecx = num
    movl    (%ecx), %eax            # %eax = *num
.L1:
    leal    (%eax, %ebx, 1), %edx   # %edx = *num + val
    lock cmpxchgl   %edx, (%ecx)    # Compare %eax with (%ecx), 
                                    # If equal (%ecx) = %edx
                                    # If not equal, %eax = (%ecx)
    jnz     .L1
    addl    %ebx, %eax              # %eax = *num (after atomic add)
    popl    %ebx                    # restore %ebx
    ret

asm_xchg:
    movl    4(%esp), %ecx   # Get lock_available
    movl    8(%esp), %eax   # Get val
    xchg    (%ecx), %eax    # atomically exchange *lock_available with val
    ret                     # Return old (*lock_availble)