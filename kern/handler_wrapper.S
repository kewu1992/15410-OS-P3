
.global keyboard_wrapper
.global timer_wrapper
.global gettid_wrapper
.global fork_wrapper
.global exec_wrapper
.global pf_wrapper
.global print_wrapper
.global new_pages_wrapper
.global remove_pages_wrapper
.global swexn_wrapper
.global halt_wrapper
.global readline_wrapper
.global set_term_color_wrapper
.global set_cursor_pos_wrapper

keyboard_wrapper:
    pusha
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    # The following code will save this_thr->k_stack_esp on stack
    pushl   %esp                    # push arg for tcb_get_entry()
    call    tcb_get_entry           # %eax = *this_thr
    addl    $4, %esp                # 'pop' arg
    pushl   (%eax)                  # push this_thr->k_stack_esp

    call    keyboard_interrupt_handler
    
    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments

    # The following code will restore this_thr->k_stack_esp from stack
    pushl   %esp                    # push arg for tcb_get_entry()
    call    tcb_get_entry           # %eax = *this_thr
    addl    $4, %esp                # 'pop' arg
    popl    (%eax)                  # pop this_thr->k_stack_esp
    
    call    asm_pop_ss              # restore all data segment selectors
    popa
    iret

timer_wrapper:
    pusha
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    # The following code will save this_thr->k_stack_esp on stack
    pushl   %esp                    # push arg for tcb_get_entry()
    call    tcb_get_entry           # %eax = *this_thr
    addl    $4, %esp                # 'pop' arg
    pushl   (%eax)                  # push this_thr->k_stack_esp

    call    timer_interrupt_handler
    
    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments

    # The following code will restore this_thr->k_stack_esp from stack
    pushl   %esp                    # push arg for tcb_get_entry()
    call    tcb_get_entry           # %eax = *this_thr
    addl    $4, %esp                # 'pop' arg
    popl    (%eax)                  # pop this_thr->k_stack_esp
    
    call    asm_pop_ss              # restore all data segment selectors
    popa
    iret


gettid_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    call    gettid_syscall_handler
    pushl   %eax                    # temporary save return value

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
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

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
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

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

pf_wrapper:
    pusha
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    movl    52(%esp), %eax          # place error code in %eax
    pushl   %eax                    # push error code as first argument
    call    pf_handler              
    addl    $4, %esp                # "pop" argument
    
    #pushl   %esp                    # push argument: %esp
    #pushl   $60                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
    call    asm_pop_ss              # restore all data segment selectors
    popa
    addl    $4, %esp                # "pop" error code
    iret

print_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    print_syscall_handler
    addl    $8, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

new_pages_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    new_pages_syscall_handler
    addl    $8, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

remove_pages_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    pushl   %esi                    # push arg1  
    call    remove_pages_syscall_handler
    addl    $4, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret
/*
swexn_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    pushl   12(%esi)                # push arg4
    pushl   8(%esi)                 # push arg3
    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    swexn_syscall_handler
    addl    $16, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret
*/

halt_wrapper:
    call    halt_syscall_handler
    # should never read here
    iret


readline_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    readline_syscall_handler
    addl    $8, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments
    
    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

set_term_color_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   %esi                    # push arg1  
    call    set_term_color_syscall_handler
    addl    $4, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

set_cursor_pos_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    set_cursor_pos_syscall_handler
    addl    $8, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    #pushl   %esp                    # push argument: %esp
    #pushl   $56                     # push argument: offset of cs
    #call    context_switch_set_esp0 # call context_switch_set_esp0()
    #addl    $8, %esp                # "pop" arguments

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret



