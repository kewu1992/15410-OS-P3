/** @file asm_helper.S
 *  @brief This file contains some assembly helper functions 
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <seg.h>

/* define function labels */
.globl asm_get_ebp
.globl asm_get_esp
.globl asm_get_cs
.globl asm_set_esp_w_ret
.globl asm_pop_generic
.globl asm_push_generic
.globl asm_pop_ss
.globl asm_push_ss
.globl asm_set_ss


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

asm_set_esp_w_ret:
    movl    (%esp), %eax    # save ret addr to %eax
    movl    4(%esp), %esp   # set new esp
    pushl   %eax            # push ret addr
    ret  

asm_pop_generic:
    addl    $4, %esp         # skip ret addr
    popl    %ebp             # restore all generic registers except %esp and %eax
    popl    %edi
    popl    %esi
    popl    %edx
    popl    %ecx
    popl    %ebx
    pushl   -28(%esp)        # push ret addr
    ret

asm_push_generic:
    popl    -28(%esp)       # move return addr
    pushl   %ebx            # save all generic registers except %esp and %eax
    pushl   %ecx
    pushl   %edx
    pushl   %esi
    pushl   %edi
    pushl   %ebp
    subl    $4, %esp        # esp -= 4 so that it points to ret addr
    ret

asm_pop_ss:
    addl    $4, %esp         # skip ret addr
    popl    %gs              # restore all data segment selectors
    popl    %fs
    popl    %es
    popl    %ds
    pushl   -20(%esp)        # push ret addr
    ret

asm_push_ss:
    popl    -20(%esp)        # move return addr
    pushl   %ds              # save all data segment selectors
    pushl   %es
    pushl   %fs
    pushl   %gs
    subl    $4, %esp         # esp -= 4 so that it points to ret addr
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