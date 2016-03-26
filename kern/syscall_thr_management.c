#include <control_block.h>
#include <asm_helper.h>
#include <simics.h>

int gettid_syscall_handler() {
    return tcb_get_entry((void*)asm_get_esp())->tid;
}