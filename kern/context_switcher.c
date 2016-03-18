/*  When context switch will happen?
 *      1. timer interrupt (context_switch(-1))
 *      2. yield(-1), yield(tid)
 *      3. I/O interrupt (context_switch(tid))
 *      4. system call that cause thread fork/blocked
 */
#include <scheduler.h>
#include <asm_helper.h>
#include <control_block.h>
#include <cr.h>
#include <stdint.h>
#include <vm.h>
#include <loader.h>
#include <simics.h>

static tcb_t* first_task;

extern void asm_context_switch(int mode, tcb_t *this_thr);

static tcb_t* internal_thread_fork(tcb_t* this_thr);

static void* get_last_ebp(void* ebp);

static char* secondTask = "switched_program";

void context_switch(int mode) {
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
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if (this_thr == NULL)
        return;

    // lprintf("before esp: %p", (void*)asm_get_esp());
    // lprintf("before: %p", this_thr);

    asm_context_switch(mode, this_thr);

    this_thr = tcb_get_entry((void*)asm_get_esp());

    // lprintf("after esp: %p", (void*)asm_get_esp());
    // lprintf("after: %p", this_thr);


    // deal with VM
    if (mode == -2 && this_thr->fork_result == 0) {
        // new task (fork)
        set_cr3(clone_pd());
        
        tcb_create_process_only(RUNNING, this_thr);
    } else if (mode == -4 && this_thr->fork_result == 0) {
        // new task (load)
        set_cr3(create_pd());

        tcb_create_process_only(RUNNING, this_thr);

        const char *argv[1] = {secondTask};
        void* usr_esp;
        void* my_program = loadTask(secondTask, 1, argv, &usr_esp);

        void* eip = (void*)(((uint32_t)tcb_get_high_addr((void*)asm_get_esp())) - 20);
        memcpy(eip, &my_program, 4);
        void* esp = (void*)(((uint32_t)tcb_get_high_addr((void*)asm_get_esp())) - 8);
        memcpy(esp, &usr_esp, 4);

        set_esp0((uint32_t)tcb_get_high_addr((void*)asm_get_esp()));
    }

    if (this_thr->pcb->page_table_base != get_cr3())
        set_cr3(this_thr->pcb->page_table_base);

}

tcb_t* context_switch_get_next(int mode, tcb_t* this_thr) {
    tcb_t* new_thr;

    switch(mode){
    case -2:    // fork and context switch to new thread
    case -3:    // thread_fork and context switch to new thread
    case -4:    // thread_fork and don't context switch, also save the 
                // new thread as the first task (used for load)
        new_thr = internal_thread_fork(this_thr);
        if (new_thr != NULL) {
            this_thr->fork_result = new_thr->tid;
            new_thr->fork_result = 0;
        } else {
            this_thr->fork_result = -1;
            return this_thr;
        }
        
        if (mode == -4) {
            first_task = new_thr;
            return this_thr;
        } else  {
            scheduler_enqueue_tail(this_thr);
            return new_thr;
        }

    case -5:    // clone the first task and context switch to new thread 
                // (used for load)
        new_thr = internal_thread_fork(first_task);
        if (new_thr == NULL) {
            this_thr->fork_result = -1;
            return this_thr;
        } 
        else {
            this_thr->fork_result = new_thr->tid;
            new_thr->fork_result = 0;
            scheduler_enqueue_tail(this_thr);
            return new_thr;
        }
    default:
        // let scheduler to choose the next thread to run
        scheduler_enqueue_tail(this_thr);
        return scheduler_get_next(mode);
    }
}

tcb_t* internal_thread_fork(tcb_t* this_thr) {
    tcb_t* new_thr = tcb_create_thread_only(this_thr->pcb);
    if (new_thr == NULL)
        return NULL;

    void* high_addr = tcb_get_high_addr(this_thr->k_stack_esp);
    int len = (uint32_t)high_addr - (uint32_t)this_thr->k_stack_esp;

    new_thr->k_stack_esp = (void*)((uint32_t)new_thr->k_stack_esp - len);

    // copy kernel stack
    memcpy(new_thr->k_stack_esp, this_thr->k_stack_esp, len);

    uint32_t diff = (uint32_t)new_thr->k_stack_esp - (uint32_t)this_thr->k_stack_esp;
    void* ebp = (void*)((uint32_t)new_thr->k_stack_esp + 36);
    *((uint32_t*) ebp) = *((uint32_t*) ebp) + diff;
    ebp = get_last_ebp(ebp);
    *((uint32_t*) ebp) = *((uint32_t*) ebp) + diff;
    
    return new_thr;
}

void context_switch_load() {
    context_switch(-4);
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



/** @brief get old %ebp value based on current %ebp
 *
 *  @param ebp Value of current %ebp
 *
 *  @return Value of old %ebp
 */
void* get_last_ebp(void* ebp) {
    uint32_t last_ebp = *((uint32_t*) ebp);
    return (void*) last_ebp;
}