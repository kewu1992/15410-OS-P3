#include <control_block.h>
#include <asm_helper.h>

int gettid_syscall_handler() {
    //context_switch(-5);
    return tcb_get_entry((void*)asm_get_esp())->tid;
}