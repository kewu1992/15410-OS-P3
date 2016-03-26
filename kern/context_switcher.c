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
#include <stdio.h>

extern void asm_context_switch(int op, uint32_t arg, tcb_t *this_thr);

static tcb_t* internal_thread_fork(tcb_t* this_thr);

static void* get_last_ebp(void* ebp);

/*  op  arg             meaning
 *  0   -1 or 0-N       context switch (yield) to -1 or a given tid
 *
 *  1   0               fork
 *
 *  2   0               thread_fork
 *
 *  3   0               block the calling thread and yield -1
 *
 *  4   tcb_t*          resume to a given thread identified by its tcb
 */
void context_switch(int op, uint32_t arg) {
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

    asm_context_switch(op, arg, this_thr);

    this_thr = tcb_get_entry((void*)asm_get_esp());

    // lprintf("after esp: %p", (void*)asm_get_esp());
    // lprintf("after: %p", this_thr);

    if (op == 1 && this_thr->result == 0) {
        // new task (fork)
        tcb_create_process_only(RUNNING, this_thr);

        set_cr3(clone_pd());
    }
}

tcb_t* context_switch_get_next(int op, uint32_t arg, tcb_t* this_thr) {
    tcb_t* new_thr;

    switch(op) {
    case 0:
        // context switch (yield)
        if (scheduler_enqueue_tail(this_thr) < 0) {
            printf("scheduler_enqueue_tail() failed, context switch \
                    failed for thread %d", this_thr->tid);
            return this_thr;
        }
        // let sheduler to choose the next thread to run
        new_thr = scheduler_get_next((int)arg);
        if (new_thr == NULL) {
            // yield error
            this_thr->result = -1;
        } else
            return new_thr;
        
    case 1:    // fork and context switch to new thread
    case 2:    // thread_fork and context switch to new thread
        new_thr = internal_thread_fork(this_thr);
        if (new_thr != NULL) {
            // fork success
            this_thr->result = new_thr->tid;
            new_thr->result = 0;
        } else {
            // fork error
            this_thr->result = -1;
            return this_thr;
        }
        
        if (scheduler_enqueue_tail(this_thr) < 0) {
            printf("scheduler_enqueue_tail() failed, context switch \
                    failed for thread %d", this_thr->tid);
            return this_thr;
        }
        return new_thr;
    default:
        return this_thr;
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