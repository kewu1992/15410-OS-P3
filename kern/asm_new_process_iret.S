#include <seg.h>

.global asm_new_process_iret

asm_new_process_iret:
    movl    4(%esp), %esp    # put new esp to %esp
    popl    %ds              # set ds for new process
    iret
