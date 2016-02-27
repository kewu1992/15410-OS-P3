
.global keyboard_wrapper
.global timer_wrapper
.global gettid_wrapper

/*
keyboard_wrapper:
    pusha   #save all general purpose registers
    call keyboard_interrupt_handler #call interrupt handler
    popa #restore registers
    iret #return from interrupt

timer_wrapper:
    pusha
    call timer_interrupt_handler
    popa
    iret
*/

gettid_wrapper:
    pushl   %ebx    # save all generic registers except %esp and %eax
    pushl   %ecx
    pushl   %edx
    pushl   %esi
    pushl   %edi
    pushl   %ebp
    pushl   %ds     # save %ds segment register
    call gettid_syscall_handler
    popl    %ds     # restore %ds segment register
    popl    %ebp    # restore all generic registers except %esp and %eax
    popl    %edi
    popl    %esi
    popl    %edx
    popl    %ecx
    popl    %ebx
    iret