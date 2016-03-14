/** @file asm_helper.S
 *  @brief This file contains some assembly helper functions 
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

/* define function labels */
.globl asm_get_ebp
.globl asm_get_esp
.globl asm_get_cs
.globl asm_set_esp_w_ret
.globl asm_pusha
.globl asm_popa

asm_get_ebp:
    movl    %ebp,%eax       # Get current %ebp as return value
    ret

asm_get_esp:
    movl    %esp, %eax      # Get current %esp value
    addl    $4, %eax        # add 4 to ignore ret addr of asm_get_esp() 
    ret                     # Return current %esp value

asm_get_cs:
    movl    %cs, %eax       # Get current %cs as return value
    ret

asm_set_esp_w_ret:
    movl    (%esp), %eax    # save ret addr to %eax
    movl    4(%esp), %esp   # set new esp
    pushl   %eax            # push ret addr
    ret  

asm_popa:
    addl    $4, %esp        # skip ret addr of asm_popa()
    popa
    pushl   -36(%esp)        # push ret addr of asm_popa()
    ret

asm_pusha:
    popl    -36(%esp)       # move return addr
    pusha
    subl    $4, %esp        # esp -= 4 so that it points to ret addr
    ret