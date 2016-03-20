
.global keyboard_wrapper
.global timer_wrapper
.global gettid_wrapper
.global fork_wrapper
.global exec_wrapper
.global pf_wrapper

keyboard_wrapper:
    pusha
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    call    keyboard_interrupt_handler
    
    pushl   %esp                    # push argument: %esp
    pushl   $56                     # push argument: offset of cs
    call    context_switch_set_esp0 # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments
    
    call    asm_pop_ss              # restore all data segment selectors
    popa
    iret

timer_wrapper:
    pusha
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    call    timer_interrupt_handler
    
    pushl   %esp                    # push argument: %esp
    pushl   $56                     # push argument: offset of cs
    call    context_switch_set_esp0 # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments
    
    call    asm_pop_ss              # restore all data segment selectors
    popa
    iret


gettid_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    call    gettid_syscall_handler
    pushl   %eax                    # temporary save return value

    pushl   %esp                    # push argument: %esp
    pushl   $56                     # push argument: offset of cs
    call    context_switch_set_esp0 # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

fork_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    call    fork_syscall_handler
    pushl   %eax                    # temporary save return value

    pushl   %esp                    # push argument: %esp
    pushl   $56                     # push argument: offset of cs
    call    context_switch_set_esp0 # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

exec_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    exec_syscall_handler
    addl    $8, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    pushl   %esp                    # push argument: %esp
    pushl   $56                     # push argument: offset of cs
    call    context_switch_set_esp0 # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

pf_wrapper:
    pusha
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    call    pf_handler
    
    pushl   %esp                    # push argument: %esp
    pushl   $56                     # push argument: offset of cs
    call    context_switch_set_esp0 # call context_switch_set_esp0()
    addl    $8, %esp                # "pop" arguments
    
    call    asm_pop_ss              # restore all data segment selectors
    popa
    iret