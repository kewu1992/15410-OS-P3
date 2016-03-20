# int atomic_add(int* num)

.globl atomic_add

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