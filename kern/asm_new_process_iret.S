/** @file asm_new_process_iret.S
 *  @brief This file contains code that will be used by loader when load a new
 *         process.
 *
 *  It will set esp to the new value, set data segment selectors to 
 *  SEGSEL_USER_DS and execute iret.
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#include <seg.h>

.global asm_new_process_iret
.global asm_idle_process_iret

asm_new_process_iret:
    movl    4(%esp), %esp    # put new esp to %esp
    popl    %ds              # set ds for new process
    movw    %ds, %ax         # set es, fs, gs same as ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    iret


asm_idle_process_iret:
    movl    4(%esp), %esp    # put new esp to %esp
    call    idle_process_init
    popl    %ds              # set ds for new process
    movw    %ds, %ax         # set es, fs, gs same as ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    iret