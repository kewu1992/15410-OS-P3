
# void asm_context_switch(int op, uint32_t arg, tcb_t *this_thr);

.globl asm_context_switch

asm_context_switch:  
    pushl   %ebp            # setup 
    movl    %esp, %ebp      # setup

    pusha                   # push all generic registers
    pushf                   # push EFLAGS
    #movl    8(%ebp), %ebx   # %ebx = mode
    movl    16(%ebp), %esi  # %esi = *this_thr
    movl    %esp, (%esi)    # this_thr->k_stack_esp = %esp

    pushl   16(%ebp)          # push this_thr
    pushl   12(%ebp)          # push arg
    pushl   8(%ebp)           # push op
    call    context_switch_get_next  # %eax = *next_thr

    movl    (%eax), %esp    # %esp = next_thr->k_stack_esp
    popf                    # pop EFLAGS
    popa                    # pop all generic registers

    popl    %ebp
    ret
