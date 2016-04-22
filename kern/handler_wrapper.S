/** @file handler_wrapper.S 
 *
 *  @brief Contains wrappers for syscall handlers and exception handlers
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs
 */

#include <idt.h>

.global keyboard_wrapper
.global timer_wrapper
.global gettid_wrapper
.global fork_wrapper
.global exec_wrapper
.global print_wrapper
.global new_pages_wrapper
.global remove_pages_wrapper
.global swexn_wrapper
.global halt_wrapper
.global readline_wrapper
.global set_term_color_wrapper
.global set_cursor_pos_wrapper
.global sleep_wrapper
.global get_ticks_wrapper
.global vanish_wrapper
.global wait_wrapper
.global set_status_wrapper
.global yield_wrapper
.global thread_fork_wrapper
.global deschedule_wrapper
.global make_runnable_wrapper
.global readfile_wrapper
.global get_cursor_pos_wrapper

.global apic_timer_wrapper

.global de_wrapper
.global db_wrapper
.global nmi_wrapper
.global bp_wrapper
.global of_wrapper
.global br_wrapper
.global ud_wrapper
.global nm_wrapper
.global df_wrapper
.global cso_wrapper
.global ts_wrapper
.global np_wrapper
.global ss_wrapper
.global gp_wrapper
.global pf_wrapper
.global mf_wrapper
.global ac_wrapper
.global mc_wrapper
.global xf_wrapper

keyboard_wrapper:
    pusha
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    # The following code will save this_thr->k_stack_esp on stack
    pushl   %esp                    # push arg for tcb_get_entry()
    call    tcb_get_entry           # %eax = *this_thr
    addl    $4, %esp                # 'pop' arg
    pushl   (%eax)                  # push this_thr->k_stack_esp

    movl    %cr2, %eax
    pushl   %eax

    call    keyboard_interrupt_handler

    popl    %eax
    movl    %eax, %cr2

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

    movl    %cr2, %eax
    pushl   %eax

    call    pic_timer_interrupt_handler

    popl    %eax
    movl    %eax, %cr2

    # The following code will restore this_thr->k_stack_esp from stack
    pushl   %esp                    # push arg for tcb_get_entry()
    call    tcb_get_entry           # %eax = *this_thr
    addl    $4, %esp                # 'pop' arg
    popl    (%eax)                  # pop this_thr->k_stack_esp
    
    call    asm_pop_ss              # restore all data segment selectors
    popa
    iret


apic_timer_wrapper:
    pusha
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    # The following code will save this_thr->k_stack_esp on stack
    pushl   %esp                    # push arg for tcb_get_entry()
    call    tcb_get_entry           # %eax = *this_thr
    addl    $4, %esp                # 'pop' arg
    pushl   (%eax)                  # push this_thr->k_stack_esp

    movl    %cr2, %eax
    pushl   %eax

    call    apic_timer_interrupt_handler

    popl    %eax
    movl    %eax, %cr2

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

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

print_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    pushL   $0                      # push dummy arguments, since kernel can 
                                    # call print as well, use this argument to
                                    # distinguish.
    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    print_syscall_handler
    addl    $12, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value
    
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

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

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

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

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

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

get_cursor_pos_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    get_cursor_pos_syscall_handler
    addl    $8, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret


sleep_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   %esi                    # push arg1  
    call    sleep_syscall_handler
    addl    $4, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

vanish_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    pushl   $0                      # Push dummy first argument, not used by 
                                    # user called vanish.
    call    vanish_syscall_handler
    addl    $4, %esp                # Pop dummy first argument

    # BELOW CODE SHOULDN'T BE EXECUTED
    iret

get_ticks_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS
    
    call    get_ticks_syscall_handler
    pushl   %eax                    # temporary save return value

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

wait_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   %esi                    # push arg1  
    call    wait_syscall_handler
    addl    $4, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret


set_status_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   %esi                    # push arg1  
    call    set_status_syscall_handler
    addl    $4, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

yield_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   %esi                    # push arg1  
    call    yield_syscall_handler
    addl    $4, %esp                # "pop" arguments
    pushl   %eax                    # temporary save return value

    popl    %eax                    # restore return value
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret


thread_fork_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    call    thread_fork_syscall_handler
    
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

deschedule_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   %esi                    # push arg1  
    call    deschedule_syscall_handler
    addl    $4, %esp                # "pop" arguments
    
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret


make_runnable_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   %esi                    # push arg1  
    call    make_runnable_syscall_handler
    addl    $4, %esp                # "pop" arguments
    
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret

readfile_wrapper:
    call    asm_push_generic        # save all generic registers except %esp and %eax
    call    asm_push_ss             # save all data segment selectors
    call    asm_set_ss              # set all data segment selectors to SEGSEL_KERNEL_DS

    pushl   12(%esi)                # push arg4
    pushl   8(%esi)                 # push arg3
    pushl   4(%esi)                 # push arg2
    pushl   (%esi)                  # push arg1  
    call    readfile_syscall_handler
    addl    $16, %esp               # "pop" arguments
    
    call    asm_pop_ss              # restore all data segment selectors
    call    asm_pop_generic         # restore all generic registers except %esp and %eax
    iret
    

/* Exception wrappers */

de_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_DE             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    iret


db_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_DB             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    iret

nmi_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_NMI            # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    iret

bp_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_BP            # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    iret

of_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_OF             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    iret

br_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_BR             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    iret

ud_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_UD             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    iret

nm_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_NM             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    iret

df_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_DF             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

cso_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_CSO            # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

ts_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_TS             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

np_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_NP             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

ss_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_SS             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

gp_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_GP             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

pf_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_PF             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

mf_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_MF             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

ac_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_AC             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

mc_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_MC             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

xf_wrapper:
    pusha
    call    asm_push_ss         # save all data segment selectors
    call    asm_set_ss          # set segment selectors to SEGSEL_KERNEL_DS
    pushl   $IDT_XF             # push exception type
    call    exception_handler   # call generic exception handler
    addl    $4, %esp            # "pop" argument
    call    asm_pop_ss          # restore all data segment selectors
    popa
    addl    $4, %esp            # "pop" error code
    iret

