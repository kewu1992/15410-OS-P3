
# void asm_context_switch(int mode, tcb_t *this_thr);

.globl asm_context_switch

asm_context_switch:  
    pushl   %ebp            # setup 
    movl    %esp, %ebp      # setup

    pusha                   # push all generic registers
    pushf                   # push EFLAGS
    movl    8(%ebp), %ebx   # %ebx = mode
    movl    12(%ebp), %esi  # %esi = *this_thr
    movl    %esp, (%esi)    # this_thr->k_stack_esp = %esp

    pushl   %esi            # push this_thr
    pushl   %ebx            # push mode
    call    context_switch_get_next  # %eax = *next_thr

    movl    (%eax), %esp    # %esp = next_thr->k_stack_esp
    popf                    # pop EFLAGS
    popa                    # pop all generic registers

    popl    %ebp
    ret
