/* define function labels */
.globl asm_vanish

# asm_vanish(uint32_t safe_stack_high, tcb_t *this_thr, 
#   int is_only_thread_in_task);

# vanish_wipe_thread(tcb_t *this_thr, int is_only_thread_in_task);

asm_vanish:
    cli                                 # Disable interrupt
    movl    4(%esp), %eax               # Put safe stack's position in %eax
    movl    8(%esp), %ebx               # Put this_thr in %ebx
    movl    12(%esp), %ecx              # Put is_only_thread_in_task in %ecx
    movl    %eax, %esp                  # Switch to a safe stack
    pushl   %ecx                        # Push is_only_thread_in_task parameter
    pushl   %ebx                        # Push this_thr parameter
    call    vanish_wipe_thread          # Free resources and get next thread
    movl    (%eax), %esp                # %esp = next_thr->k_stack_esp
    popf                                # Pop EFLAGS, interrupt is enabled
    popa                                # Pop all generic registers
    popl    %ebp
    ret

