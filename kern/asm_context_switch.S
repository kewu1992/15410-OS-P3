/** @file asm_context_switch.S
 *  @brief The assembly part (kernel part) of context switch
 *  
 *  Function prototype: 
 *      void asm_context_switch(int op, uint32_t arg, tcb_t *this_thr);
 *
 *  Basically, this code does the following things:
 *      1. save registers on stack
 *      2. this_thr->esp = get_esp()
 *      3. next_thr = context_switch_get_next()
 *      4. set_esp(next_thr->esp)
 *      5. restore registers from stack
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

.globl asm_context_switch

asm_context_switch:  
    pushl   %ebp            # setup 
    movl    %esp, %ebp      # setup

    pusha                   # push all generic registers
    pushf                   # push EFLAGS
    movl    16(%ebp), %esi  # %esi = *this_thr
    pushl   (%esi)          # push this_thr->k_stack_esp
    movl    %esp, (%esi)    # this_thr->k_stack_esp = %esp

    pushl   16(%ebp)          # push this_thr
    pushl   12(%ebp)          # push arg
    pushl   8(%ebp)           # push op
    call    context_switch_get_next  # %eax = *next_thr

    movl    (%eax), %esp    # %esp = next_thr->k_stack_esp
    popl    (%eax)          # pop this_thr->k_stack_esp
    popf                    # pop EFLAGS
    popa                    # pop all generic registers

    popl    %ebp
    ret
