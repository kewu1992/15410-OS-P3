#include <seg.h>

.global asm_new_process_iret

asm_new_process_iret:
    movl    4(%esp), %esp    # put new esp to %esp
    
    #call    context_switch_load 

    popl    %ds              # set ds for new process
    movw    %ds, %ax         # set es, fs, gs same as ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    iret
