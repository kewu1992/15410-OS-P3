/** @file asm_helper.S
 *
 *  @brief This file contains implementation of some assembly helper functions 
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#include <seg.h>

/* define function labels */
.globl asm_get_ebp
.globl asm_get_esp
.globl asm_get_cs
.globl asm_pop_generic
.globl asm_push_generic
.globl asm_pop_ss
.globl asm_push_ss
.globl asm_set_ss
.globl asm_bsf
.globl asm_hlt


asm_get_ebp:
    movl    %ebp,%eax       # Get current %ebp as return value
    ret

asm_get_esp:
    movl    %esp, %eax      # Get current %esp value
    addl    $4, %eax        # add 4 for bytes of ret addr
    ret                     # Return current %esp value

asm_get_cs:
    movl    %cs, %eax       # Get current %cs as return value
    ret

asm_pop_generic:
    popl    24(%esp)         # move ret addr
    popl    %ebp             # restore all generic registers except %esp, %eax
    popl    %edi
    popl    %esi
    popl    %edx
    popl    %ecx
    popl    %ebx
    ret

asm_push_generic:
    pushl   %ebx            # save all generic registers except %esp and %eax
    pushl   %ecx
    pushl   %edx
    pushl   %esi
    pushl   %edi
    pushl   %ebp
    pushl   24(%esp)        # push ret addr
    ret

asm_pop_ss:
    popl    16(%esp)         # move ret addr
    popl    %ds              # restore all data segment selectors
    popl    %es
    popl    %fs
    popl    %gs
    ret

asm_push_ss:
    pushl   %gs              # save all data segment selectors
    pushl   %fs
    pushl   %es
    pushl   %ds
    pushl   16(%esp)         # push ret addr
    ret

asm_set_ss:
    pushl   $SEGSEL_KERNEL_DS
    pushl   $SEGSEL_KERNEL_DS
    pushl   $SEGSEL_KERNEL_DS
    pushl   $SEGSEL_KERNEL_DS
    popl    %ds
    popl    %es
    popl    %fs
    popl    %gs
    ret

asm_bsf:
    bsf 4(%esp), %eax
    ret

asm_hlt:
    cli
    hlt