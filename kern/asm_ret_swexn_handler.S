
.global asm_ret_swexn_handler

# asm_ret_swexn_handler(eip, cs, eflags, esp, ss);

# Before return to user space, on entry to this funtion
# The kernel space exception handler's stack looks like:
# SS
# ESP
# EFLAGS
# CS
# EIP   
# ret addr  <- %esp

asm_ret_swexn_handler:
    addl    $4, %esp        # Pop ret addr, %esp now points to eip

    movl 16(%esp), %eax       # set ds, es, fs, gs same as ss       
    movw    %ax, %ds         
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    subl    $0, %eax        # Set general purpose registers to default value
    movl    $0, %ebx        
    movl    $0, %ecx       
    movl    $0, %edx      
    movl    $0, %esi        
    movl    $0, %edi        
    movl    $0, %ebp        
    iret

