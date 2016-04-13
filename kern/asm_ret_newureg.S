
.global asm_ret_newureg

# asm_ret_newureg(ureg_t *newureg);

# Before return to user space, on entry to this funtion
# The kernel space exception handler's stack looks like:
# newureg pointer
# ret addr  <- %esp

asm_ret_newureg:

    addl    $4, %esp        # Pop ret addr, %esp now points to newureg pointer

    movl    (%esp), %eax    # Put the pointer to newureg in %eax
    addl    $8, %eax        # %eax gets &newureg->ds
    movl    %eax, %esp      # %esp points to &newureg->ds

    popl    %ds             # Set segment registers
    popl    %es                 
    popl    %fs                
    popl    %gs                

    popl    %edi            # Set generic registers
    popl    %esi            
    popl    %ebp            
    addl    $4, %esp        # Skip dummy %esp
    popl    %ebx            
    popl    %edx            
    popl    %ecx            
    popl    %eax            

    addl    $4, %esp        # Skip error code
    iret                    # Return to user space

