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
#include <syscall_lifecycle.h>

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
 *  4   tcb_t*          make_runable a given thread identified by its tcb
 *
 *  5   tcb_t*          
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

    lprintf("context switch for tid: %d", this_thr->tid);

    // before context switch, save cr3
    this_thr->pcb->page_table_base = get_cr3();

    // lprintf("before esp: %p", (void*)asm_get_esp());
    // lprintf("before: %p", this_thr);
    // lprintf("before: %d", this_thr->tid);

    asm_context_switch(op, arg, this_thr);

    this_thr = tcb_get_entry((void*)asm_get_esp());

    // lprintf("after esp: %p", (void*)asm_get_esp());
    // lprintf("after: %p", this_thr);
    // lprintf("after: %d", this_thr->tid);

    lprintf("context switch to tid: %d", this_thr->tid);

    if (op == 1 && this_thr->result == 0) {
        // new task (fork)
        tcb_create_process_only(RUNNING, this_thr);

        lprintf("thread create process finished");

        // Can't wait until this point to clone pd, because parent process 
        // may have executed and changed its user space stack or even 
        // vanished and the old address space has gone.

        // set_cr3(clone_pd());
        set_cr3(this_thr->new_page_table_base);
        this_thr->pcb->page_table_base = this_thr->new_page_table_base;
    }

    // after context switch, restore cr3
    if (this_thr->pcb->page_table_base != get_cr3())
        set_cr3(this_thr->pcb->page_table_base);

    // reset esp0
    set_esp0((uint32_t)tcb_get_high_addr(this_thr->k_stack_esp));

    // Check if there's any thread to destroy
    tcb_t *thread_zombie;
    if(get_next_zombie(&thread_zombie) == 0) {
        // After putting self to zombie list, and on the way to
        // get next thread to run, timer interrupt is likely to 
        // happen, so zombie thread is likely to have a chance
        // to execute the following code, must let other thread
        // to free its resource.
        if(this_thr->tid == thread_zombie->tid || 
                scheduler_is_exist(thread_zombie->tid)) {
            // Put it back
            put_next_zombie(thread_zombie);
        } 
        
        else {
            // Zombie is ready to be reaped
            if(vanish_wipe_thread(thread_zombie) < 0) {
                lprintf("reap thread failed");
                MAGIC_BREAK;
            } else {
                lprintf("reap thread succeeded");
            }
            lprintf("some useless words");
        }
    }
}


tcb_t* context_switch_get_next(int op, uint32_t arg, tcb_t* this_thr) {
    tcb_t* new_thr;

    switch(op) {
        case 0: // context switch (yield)
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
                return this_thr;
            } else {
                return new_thr;
            }

        case 1:    // fork and context switch to new thread
            this_thr->pcb->cur_child_num++;
        case 2:    // thread_fork and context switch to new thread
            new_thr = internal_thread_fork(this_thr);

            if (new_thr != NULL) {
                // fork success
                this_thr->result = new_thr->tid;
                new_thr->result = 0;
                // Set parent thread, can be in the same task or different task
                new_thr->pthr = this_thr;
                new_thr->new_page_table_base = clone_pd();
                lprintf("thread %d page table base: %x", new_thr->tid, 
                        (unsigned)new_thr->new_page_table_base);
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
        case 3: // block
            // let sheduler to choose the next thread to run
            new_thr = scheduler_get_next(-1);
            if (new_thr == NULL) {
                panic("no other process is running, %d can not be blocked", this_thr->tid);
            } else
                return new_thr;
        case 4: // make_runnable a thread
            new_thr = (tcb_t*)arg;
            if (new_thr != NULL && scheduler_enqueue_tail(new_thr) == 0) {
                this_thr->result = 0;
            } else {
                this_thr->result = -1;
            }

            if(this_thr->result == 0) {
                lprintf("make runnable tid %d succeeded", new_thr->tid);
            } else {
                lprintf("make runnable tid %d failed", new_thr->tid);
            }

            return this_thr;
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
