/** @file context_switcher.c
 *  @brief This file contains implementation of a context switcher 
 *
 *  There are multiple operations that are supported by context_switcher:
 *  op  arg             meaning
 *  0   -1              Normal context switch driven by timer interupt. the 
 *                      invoking thread will be put to the tail of the queue of 
 *                      scheduler, scheduler will choose the next thread (which
 *                      is the head of the queue of scheduler) to run.
 *
 *  1   0               Fork and context switch to the new process. Old thread
 *                      will be put to the queue of scheduler.
 *
 *  2   0               Thread_fork and context switch to the new thread. Old 
 *                      thread will be put to the queue of scheduler.
 *
 *  3   0               Block the calling thread and let scheduler to choose
 *                      the next thread to run. The 'block' is done by simply
 *                      don't put the calling thread to the queue of scheduler.
 *                      Note that if a thread calls context_switch(OP_BLOCK), 
 *                      its tcb must be stored in another queue which belongs to 
 *                      the object that the thread is blocking (e.g queue of 
 *                      mutex, queue of sleep, queue of readline).
 *
 *  4   tcb_t*          Make runable a blocked thread identified by its tcb. 
 *                      Notice that this is not really a context switch. It will
 *                      just put the tcb of the thread that will be made 
 *                      runnable to the queue of scheduler without any context
 *                      switching.
 *
 *  5   tcb_t*          Resume (wake up) a blocked thread (make runnable and
 *                      context switch to that thread immediately).  
 *
 *  6   -1 or 0-N       yield to -1 or a given tid.
 *
 *
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug context_switch(OP_YIELD, tid) needs to search the queue of scheduler to
 *       find if the thread is in the queue. We realize this is an O(n) 
 *       operation and it violates the requirement in the handout that any 
 *       operation of scheduler should be done in constant time. But we don't
 *       have time to fix this issue. 
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
#include <syscall_inter.h>
#include <asm_atomic.h>
#include <syscall_errors.h>
#include <simple_queue.h>
#include <context_switcher.h>

extern void asm_context_switch(int op, uint32_t arg, tcb_t *this_thr);

static tcb_t* internal_thread_fork(tcb_t* this_thr);

static void* get_last_ebp(void* ebp);

extern mutex_t *get_malloc_lib_lock();

static spinlock_t spinlock;

extern tcb_t* idle_thr;

/** @brief Context switch from a thread to another thread. 
 *
 *  @param op The operation for context_switch()
 *  @param arg The argument for context_switch(). With different operation, this 
 *             argument has different meannings.
 *
 *  @return void
 */
void context_switch(int op, uint32_t arg) {

    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if (this_thr == NULL)
        return;

    //lprintf("context switch for tid: %d by op %d with cr3:%x (pd:%x)", this_thr->tid, op, (unsigned int)get_cr3(), (unsigned int)this_thr->pcb->page_table_base);

    // lprintf("before esp: %p", (void*)asm_get_esp());
    // lprintf("before: %p", this_thr);
    // lprintf("before: %d", this_thr->tid);

    asm_context_switch(op, arg, this_thr);

    this_thr = tcb_get_entry((void*)asm_get_esp());

    // lprintf("after esp: %p", (void*)asm_get_esp());
    // lprintf("after: %p", this_thr);
    // lprintf("after: %d", this_thr->tid);

    if (op == OP_FORK && this_thr->result == 0) {        
        lprintf("fork process %d with thread %d (pd:%x)", this_thr->pcb->pid, 
                this_thr->tid, (unsigned int)(this_thr->pcb->page_table_base));

        set_cr3((uint32_t)(this_thr->pcb->page_table_base));
    }

    // after context switch, restore cr3
    if (this_thr->pcb->page_table_base != get_cr3())
        set_cr3(this_thr->pcb->page_table_base);

    //lprintf("context switch to tid: %d with cr3:%x (pd:%x)", this_thr->tid, (unsigned int)get_cr3(), (unsigned int)this_thr->pcb->page_table_base);


    // reset esp0
    set_esp0((uint32_t)tcb_get_high_addr(this_thr->k_stack_esp-1));

    // Check if there's any thread to destroy

    // Check mutext lib lock holder
    if(op == OP_MAKE_RUNNABLE) {
        return; 
    } else {
        // try to grab the first lock
        if (mutex_try_lock(get_zombie_list_lock()) < 0) {
            return;
        } else {
            // try to grab the second lock
            if (mutex_try_lock(get_malloc_lib_lock()) < 0) {
                mutex_unlock(get_zombie_list_lock());
                return;
            } else {
                // get two locks!
                simple_node_t *node;
                if((node = get_next_zombie()) != NULL) {
                    // After putting self to zombie list, and on the way to
                    // get next thread to run, timer interrupt is likely to 
                    // happen, so zombie thread is likely to have a chance
                    // to execute the following code, must let other thread
                    // to free its resource.
                    tcb_t* zombie_thr = (tcb_t*)(node->thr);
                    if(this_thr->tid == zombie_thr->tid || 
                            //scheduler_is_exist(zombie_thr->tid)) {
                            zombie_thr->state != BLOCKED) {
                        // Put it back
                        put_next_zombie(node);
                    } else {
                        // Zombie thread is ready to be freed
                        tcb_vanish_thread(zombie_thr);
                    }
                }
                // free locks
                mutex_unlock(get_malloc_lib_lock());
                mutex_unlock(get_zombie_list_lock());
            }
        }
    }
}

/** @brief Get the next thread for context switch
 *  
 *  The next thread might be:
 *      1) Choosen by scheduler (regular context switch or yield -1)
 *      2) A specific thread because of yield or resume
 *      3) A newly created thread because of fork or thread_fork
 *      4) The original thread to call this function (mostly it is becuase some
 *         errors happen)    
 *
 *  @param op The option for context_switch()
 *  @param arg The argument for context_switch(). With different options, this 
 *             argument has different meannings.
 *  @param this_thr The thread before context switch
 *
 *  @return The next thread for context switch
 */
tcb_t* context_switch_get_next(int op, uint32_t arg, tcb_t* this_thr) {
    tcb_t* new_thr;

    int is_syscall = (op == 1 || op == 2 || op == 6)
                        ? 1 : 0;

    switch(op) {
        case OP_CONTEXT_SWITCH: // normal context switch 
        case OP_YIELD:  // yield -1 or yield to a specific thread
            // let sheduler choose the next thread to run
            new_thr = scheduler_get_next((int)arg);
            if (new_thr == NULL) {
                if ((int)arg == -1)
                    // no other thread to yield to, just return this thread
                    this_thr->result = (is_syscall) ? 0 : this_thr->result;
                else
                    // The requested thread doesn't exist
                    this_thr->result = ETHREAD;
                return this_thr;
            } 

            this_thr->result = (is_syscall) ? 0 : this_thr->result;

            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(&spinlock);
            // decide to enqueue this thread, should not be interrupted until 
            // context switch to the next thread successfully
            if (this_thr != idle_thr)
                scheduler_make_runnable(this_thr);
            return new_thr;
        case OP_FORK:    // fork and context switch to new thread
            if (this_thr->pcb->cur_thr_num > 1) {
                //the invoking task contains more than one thread,reject fork()
                lprintf("fork() with more than one thread");
                // MAGIC_BREAK;

                printf("fork() failed because more than one thread\n");
                this_thr->result = EMORETHR;
                return this_thr;
            }
            new_thr = internal_thread_fork(this_thr);

            if (new_thr == NULL) {
                // thread_fork error
                lprintf("internal_thread_fork() failed");
                // MAGIC_BREAK;

                printf("internal_thread_fork() failed when fork()");
                this_thr->result = ENOMEM;
                return this_thr;
            }
            
            // clone page table
            uint32_t new_page_table_base = clone_pd();
            if (new_page_table_base == ERROR_MALLOC_LIB ||
                new_page_table_base == ERROR_NOT_ENOUGH_MEM) {

                printf("clone_pd() failed when fork()");
                tcb_free_thread(new_thr);
                this_thr->result = ENOMEM;
                return this_thr;
            }

            // clone swexn handler
            if(this_thr->swexn_struct != NULL) {
                new_thr->swexn_struct = malloc(sizeof(swexn_t));
                if(new_thr->swexn_struct == NULL) {
                    lprintf("malloc failed");
                    int need_unreserve_frames = 1;
                    free_entire_space(new_page_table_base, 
                            need_unreserve_frames);
                    tcb_free_thread(new_thr);
                    this_thr->result = ENOMEM;
                    return this_thr;
                }
                memcpy(new_thr->swexn_struct, this_thr->swexn_struct, 
                        sizeof(swexn_t));
            }
            
            // create new process
            if (tcb_create_process_only(new_thr, this_thr, 
                                                new_page_table_base) == NULL) {
                lprintf("tcb_create_process_only() failed");
                MAGIC_BREAK;

                printf("tcb_create_process_only() failed when fork()\n");
                free(new_thr->swexn_struct);
                int need_unreserve_frames = 1;
                free_entire_space(new_page_table_base, need_unreserve_frames);
                tcb_free_thread(new_thr);
                this_thr->result = ENOMEM;
                return this_thr;
            }

            // add num_alive of child process for parent process
            mutex_lock(&((this_thr->pcb->task_wait_struct).lock));
            (this_thr->pcb->task_wait_struct).num_alive++;
            mutex_unlock(&((this_thr->pcb->task_wait_struct).lock));

            // fork success
            this_thr->result = new_thr->tid;
            new_thr->result = 0;

            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(&spinlock);
            // decide to enqueue this thread, should not be interrupted until 
            // context switch to the next thread successfully
            scheduler_make_runnable(this_thr);
            return new_thr;

        case OP_THREAD_FORK:    // thread_fork and context switch to new thread
            new_thr = internal_thread_fork(this_thr);

            if (new_thr != NULL) {
                // thread fork success
                this_thr->result = new_thr->tid;
                new_thr->result = 0;
                atomic_add(&this_thr->pcb->cur_thr_num, 1);
                lprintf("process %d get a new thread %d", 
                                            this_thr->pcb->pid, new_thr->tid);
            } else {
                // thread fork error
                lprintf("internal_thread_fork() failed");
                MAGIC_BREAK;

                printf("internal_thread_fork() failed when thread_fork()");
                this_thr->result = ENOMEM;
                return this_thr;
            }

            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(&spinlock);
            // decide to enqueue this thread, should not be interrupted until 
            // context switch to the next thread successfully
            scheduler_make_runnable(this_thr);
            return new_thr;

        case OP_BLOCK: // block itself
            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(&spinlock);
            if (this_thr->state == WAKEUP || this_thr->state == MADE_RUNNABLE) {
                // already be waked up or made runnable, should not block
                this_thr->state = NORMAL;
                return this_thr;
            } else {
                // decide to block itself, can not be waked up (resume) or made
                // runnable by other thread unitl context switch to the next 
                // thread successfully
                if (this_thr->state == NORMAL) {
                    this_thr->state = BLOCKED;
                } else  {
                    lprintf("strange state in context_switch(3,0): %d", 
                                                            this_thr->state);
                    MAGIC_BREAK;
                }

                // let sheduler to choose the next thread to run
                new_thr = scheduler_block();
                if (new_thr == NULL) {
                    if (this_thr == idle_thr)
                        panic("idle thread try to block itself, something goes wrong!");
                    else if (idle_thr == NULL)
                        panic("no other process is running, %d can not be blocked", this_thr->tid);
                    else
                        return idle_thr;
                } else
                    return new_thr;
            }            
        case OP_MAKE_RUNNABLE: // make_runnable a thread
            new_thr = (tcb_t*)arg;
            if (new_thr == NULL)
                return this_thr;

            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(&spinlock);
            if (new_thr->state == BLOCKED) {
                // the thread has already blocked, put it to the queue of scheduler
                new_thr->state = NORMAL;
                scheduler_make_runnable(new_thr);
            } else if (new_thr->state == NORMAL) {
                // the thread hasn't blocked, set state to tell it do not block
                new_thr->state = MADE_RUNNABLE;
            } else {
                lprintf("strange state in scheduler_make_runnable()");
                MAGIC_BREAK;
            }

            return this_thr;
        case OP_RESUME: // resume a thread
            new_thr = (tcb_t*)arg;
            
            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(&spinlock);
            scheduler_make_runnable(this_thr);

            if (new_thr->state == BLOCKED)
                // the thread has already blocked, context switch to it directly
                new_thr->state = NORMAL;
            else if (new_thr->state == NORMAL)
                 // the thread hasn't blocked, set state to tell it not block
                new_thr->state = WAKEUP;
            else {
                lprintf("strange state in context_switch(5,thr)");
                MAGIC_BREAK;
            }

            return new_thr;
        default:
            return this_thr;
    }
}

/** @brief Implement the kernel part of thread_fork()
 *  
 *  Create a new thread and copy the entire kernel stack of this thread
 *  (from very top to this_thr->k_stack_esp) to the kernel stack of the 
 *  newly created thread
 *
 *  @param this_thr The thread that will be forked
 *
 *  @return On success a new thread that is the result of thread_fork() 
 *          of this_thr. On error return NULL (because of out of memory)         
 */
tcb_t* internal_thread_fork(tcb_t* this_thr) {
    tcb_t* new_thr = tcb_create_thread_only(this_thr->pcb, NORMAL);
    if (new_thr == NULL)
        return NULL;

    void* high_addr = tcb_get_high_addr(this_thr->k_stack_esp);
    int len = (uint32_t)high_addr - (uint32_t)this_thr->k_stack_esp;

    // set k_stack_esp value
    void* init_k_esp = new_thr->k_stack_esp;
    new_thr->k_stack_esp = (void*)((uint32_t)new_thr->k_stack_esp - len);

    // copy kernel stack
    memcpy(new_thr->k_stack_esp, this_thr->k_stack_esp, len);

    // modify pushed k_esp value
    void *k_esp = (void*)((unsigned int)new_thr->k_stack_esp + 12);
    memcpy(k_esp, &init_k_esp, 4);

    // modify all %ebp values in the new thread's kernel stack so that all %ebp
    // values point to the new stack instead of the original stack
    uint32_t diff = (uint32_t)new_thr->k_stack_esp - (uint32_t)this_thr->k_stack_esp;
    void* ebp = (void*)((uint32_t)new_thr->k_stack_esp + 56);
    *((uint32_t*) ebp) = *((uint32_t*) ebp) + diff;
    ebp = get_last_ebp(ebp);
    *((uint32_t*) ebp) = *((uint32_t*) ebp) + diff;

    return new_thr;
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

int context_switcher_init() {
    return spinlock_init(&spinlock);
}

void context_switch_unlock() {
    spinlock_unlock(&spinlock);
}
