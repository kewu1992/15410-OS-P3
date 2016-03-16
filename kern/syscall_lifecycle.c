#include <context_switcher.h>
#include <control_block.h>
#include <asm_helper.h>

int fork_syscall_handler() {
    context_switch(-2);
    return tcb_get_entry((void*)asm_get_esp())->fork_result;
}