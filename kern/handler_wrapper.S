
.global keyboard_wrapper
.global timer_wrapper
.global gettid_wrapper

#  DS????????????????????

keyboard_wrapper:
    pusha                           # save all general purpose registers
    call keyboard_interrupt_handler # call interrupt handler

    pushl   %esp                    # push argument: %esp
    pushl   $36                     # push argument: offset of cs
    call context_switch_set_esp0    # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments

    popa                            # restore registers
    iret                            # return from interrupt

timer_wrapper:
    pusha
    
    call timer_interrupt_handler
    
    pushl   %esp                    # push argument: %esp
    pushl   $36                     # push argument: offset of cs
    call context_switch_set_esp0    # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments
    
    popa
    iret


gettid_wrapper:
    pushl   %ebx    # save all generic registers except %esp and %eax
    pushl   %ecx
    pushl   %edx
    pushl   %esi
    pushl   %edi
    pushl   %ebp
    pushl   %ds     # save %ds segment register
    
    call gettid_syscall_handler
    pushl   %eax    # temporary save return value

    pushl   %esp                    # push argument: %esp
    pushl   $36                     # push argument: offset of cs
    call context_switch_set_esp0    # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments
    
    popl    %eax    # restore return value
    popl    %ds     # restore %ds segment register
    popl    %ebp    # restore all generic registers except %esp and %eax
    popl    %edi
    popl    %esi
    popl    %edx
    popl    %ecx
    popl    %ebx
    iret