
# int atomic_add(int* num)
.globl atomic_add

# int asm_xchg(int *lock_available, int val);
.globl asm_xchg

atomic_add:
    movl    4(%esp), %ecx           # %ecx = num
    movl    (%ecx), %eax            # %eax = *num
.L1:
    leal    1(%eax), %edx           # %edx = *num + 1
    lock cmpxchgl   %edx, (%ecx)    # Compare %eax with (%ecx), 
                                    # If equal (%ecx) = %edx
                                    # If not equal, %eax = (%ecx)
    jnz     .L1
    addl    $1, %eax                # %eax = *num (after atomic add)
    ret

asm_xchg:
    movl    4(%esp), %ecx   # Get lock_available
    movl    8(%esp), %eax   # Get val
    xchg    (%ecx), %eax    # atomically exchange *lock_available with val
    ret                     # Return old (*lock_availble)