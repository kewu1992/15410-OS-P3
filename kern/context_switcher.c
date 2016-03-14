
#include <scheduler.h>
#include <asm_helper.h>
#include <control_block.h>
#include <cr.h>
#include <stdint.h>
#include <vm.h>
#include <loader.h>
#include <simics.h>

void context_switch() {
    /*  The following are pseudocode 
     *  
     *  scheduler.enqueue_tail(this_thr);
     *  next_thr = scheduler.getNextThread();
     *
     *  save registers on stack
     *  this_thr->esp = get_esp();
     *  
     *  set_cr3(tcb->page_table_base);
     *  set_esp(next_thr->esp);
     *  restore registers on stack
     *
     *  *****************
     *  What if interrupt during context switch???
     *  *****************
     */
    tcb_t *next_thr = scheduler_get_next();
    if (next_thr == NULL)
        return;

    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    scheduler_enqueue_tail(this_thr);

    //lprintf("this:%p", this_thr); 
    //lprintf("this esp:%p", (void*)asm_get_esp());
    //lprintf("next:%p", next_thr);
    //lprintf("next esp:%p", next_thr->k_stack_esp);

    asm_pusha();
    this_thr->k_stack_esp = (void*)asm_get_esp();

    if (next_thr->pcb->page_table_base != get_cr3())
        set_cr3(next_thr->pcb->page_table_base);
    asm_set_esp_w_ret((uint32_t)next_thr->k_stack_esp);
    asm_popa();
}

void context_switch_load(const char *filename) {
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    scheduler_enqueue_tail(this_thr);

    //lprintf("this:%p", this_thr); 
    //lprintf("this esp:%p", (void*)asm_get_esp());

    // EFLAGS!!
    asm_pusha();
    this_thr->k_stack_esp = (void*)asm_get_esp();

    set_cr3(create_pd());
    loadTask(filename);
}

/* Any syscall/interrupt need to call this function before iret.
 * Context switch (change of esp0) can happen in anywhere
 */
void context_switch_set_esp0(int offset, uint32_t esp) {
    uint32_t cs;
    memcpy(&cs, (void*)(esp + offset), 4);
    if (cs != asm_get_cs()) {
        // kernel --> user, privilege change, SS, ESP, EFLAGS, CS, EIP
        set_esp0(esp + offset + 16);
    } else {
        // kernel --> kernel, don't need to set esp0
    }
}