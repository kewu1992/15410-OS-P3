/** @file context_switcher.c
 *  @brief This file contains implementation of a context switcher 
 *
 *  There are multiple operations that are supported by context_switcher:
 *  op                  arg meaning
 *  OP_CONTEXT_SWITCH   -1  Normal context switch driven by timer interupt. 
 *                          The invoking thread will be put to the tail of the
 *                          queue of scheduler, scheduler will choose the next
 *                          thread (which is the head of the queue of 
 *                          scheduler) to run.
 *
 *  OP_FORK             0   Fork and context switch to the new process. Old 
 *                          thread will be put to the queue of scheduler.
 *
 *  OP_THREAD_FORK      0   Thread_fork and context switch to the new thread.
 *                          Old thread will be put to the queue of scheduler.
 *
 *  OP_BLOCK            0   Block the calling thread and let scheduler to 
 *                          choose the next thread to run. The 'block' is done
 *                          by simply don't put the calling thread to the 
 *                          queue of scheduler. Note that if a thread calls 
 *                          context_switch(OP_BLOCK), its tcb must be stored 
 *                          in another queue which belongs to the object that 
 *                          the thread is blocked on (e.g queue of mutex, 
 *                          queue of sleep, queue of deschedule).
 *
 *  OP_MAKE_RUNNABLE tcb_t* Make runable a blocked thread identified by its 
 *                          tcb.  Notice that this is not really a context 
 *                          switch. It will just put the tcb of the thread 
 *                          that will be made runnable to the queue of 
 *                          scheduler without any context switching.
 *
 *  OP_RESUME        tcb_t* Resume (wake up) a blocked thread (make runnable 
 *                          and context switch to that thread immediately).  
 *
 *  OP_YIELD     -1 or 0-N  Yield to -1 or a given tid.
 *
 *  OP_SEND_MSG         0   Send the message associated with the calling thread 
 *                          to the manager core. Block the calling thread and 
 *                          let scheduler to choose the next thread to run. The
 *                          tcb of the blocking thread is stored in the message
 *                          which stored in the send queue of this core.
 *
 *
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug context_switch(OP_YIELD, tid) needs to search the queue of scheduler 
 *       to find if the thread is in the queue. We realize this is an O(n) 
 *       operation and it violates the requirement in the handout that any 
 *       operation of scheduler should be done in constant time.
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
#include <smp.h>

/** @brief The assembly part (the most important part) of context switch.
 *         Please refer to asm_context_switch.S for more details. */
extern void asm_context_switch(int op, uint32_t arg, tcb_t *this_thr);

static tcb_t* internal_thread_fork(tcb_t* this_thr);

static void* get_last_ebp(void* ebp);

extern mutex_t *get_malloc_lib_lock();

/** @brief Spinlock to be used to protect queue of scheduler. We can not use
 *         mutex to protext queue of sheduler. Mutex may block the thread that
 *         can not get the lock, which will result in a context switch and 
 *         another operation of queue of scheduler... */
static spinlock_t* spinlocks[MAX_CPUS];

/** @brief The idle thread(task), this will be scheduled by scheduler when 
 *         there is no other thread to run */
extern tcb_t* idle_thr[MAX_CPUS];

/** @brief Stores the current running thread on each core. Note that it is
 *         an array of tcb_t** so that each core will malloc a tcb_t* using
 *         its own heap to avoid false sharing. */
static tcb_t** cur_running_thr[MAX_CPUS];

/** @brief Context switch from a thread to another thread. 
 *
 *  @param op   The operation for context_switch()
 *  @param arg  The argument for context_switch(). With different operation, 
 *              this argument has different meannings.
 *
 *  @return void
 */
void context_switch(int op, uint32_t arg) {

    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());

    // if this_thr == NULL, it means the first task hasn't been loaded
    if (this_thr == NULL)
        return;

    asm_context_switch(op, arg, this_thr);

    this_thr = tcb_get_entry((void*)asm_get_esp());


    /* for child process of fork(), finish the rest part of fork(), including
     * create pcb, clone page table and clone swexn. */ 
    if (op == OP_FORK && this_thr->result == 0) {
        if (this_thr->my_msg->type == FORK_RESPONSE) {
            // set cr3 to partent task's page table
            set_cr3((uint32_t)(this_thr->pcb->page_table_base));

            msg_t* req_msg = this_thr->my_msg->data.fork_response_data.req_msg;

            // finish the rest part of fork()
            int rv = fork_create_process(this_thr, req_msg->req_thr);

            // construct FORK_RESPONSE message
            this_thr->my_msg->req_thr = this_thr;
            this_thr->my_msg->req_cpu = smp_get_cpu();
            this_thr->my_msg->data.fork_response_data.result = rv;

            // send FORK_RESPONSE message back to the manager core, so the 
            // manager core may send the result of fork() to the old task
            context_switch(OP_SEND_MSG, 0);
        }
        
        set_cr3((uint32_t)(this_thr->pcb->page_table_base));
    }

    // after context switch, restore cr3
    if (this_thr->pcb->page_table_base != get_cr3())
        set_cr3(this_thr->pcb->page_table_base);



    // reset esp0
    set_esp0((uint32_t)tcb_get_high_addr(this_thr->k_stack_esp-1));

    
    /* ====== The following code is used to free any zombie thread ======= */

    // for OP_MAKE_RUNNABLE, because it is not really a context switch, it
    // should not try to free a zombie thread, otherwise stack overflow might
    // happen 
    if(op == OP_MAKE_RUNNABLE) {
        return; 
    } else {
        /* Here the thread should only try to get the locks of malloc and zombie
         * list. If it can't get the locks, it should not block. Because this
         * code will be executed in *every* context switch, so a thread will 
         * block when it can't get the locks, self-deadlock might happen 
         */
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
                        // if the state of zombie_thr is not BLOCKED, it means
                        // the zombie_thr is not really blocked yet
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

/** @brief Get the next thread to context switch to
 *  
 *  The next thread might be:
 *      1) Choosen by scheduler (regular context switch, yield -1 or block)
 *      2) A specific thread because of yield or resume
 *      3) A newly created thread because of fork or thread_fork
 *      4) The original thread to call this function (it may because some errors
 *         happen, or a blocking thread find itself has been made runnable or 
 *         wakend up by other thraeds, or there is no other thread to context 
 *         switch to)
 *      5) idle thread. It is because the last thread is calling OP_BLOCK and 
 *         there is no runnable thread in the queue of scheduler.    
 *
 *  @param op The operation for context_switch()
 *  @param arg The argument for context_switch(). With different options, this 
 *             argument has different meannings.
 *  @param this_thr The thread before context switch
 *
 *  @return The next thread to context switch to
 */
tcb_t* context_switch_get_next(int op, uint32_t arg, tcb_t* this_thr) {
    tcb_t* new_thr;

    int is_syscall = (op == 1 || op == 2 || op == 6)
                        ? 1 : 0;

    switch(op) {
        case OP_CONTEXT_SWITCH: // normal context switch 
        case OP_YIELD:  // yield -1 or yield to a specific thread
            // let sheduler choose the next thread to run
            spinlock_lock(spinlocks[smp_get_cpu()], 1);
            new_thr = scheduler_get_next((int)arg);
            spinlock_unlock(spinlocks[smp_get_cpu()], 1);
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
            spinlock_lock(spinlocks[smp_get_cpu()], 1);
            // decide to enqueue this thread, should not be interrupted until 
            // context switch to the next thread successfully
            if (this_thr != idle_thr[smp_get_cpu()])
                scheduler_make_runnable(this_thr);

            *cur_running_thr[smp_get_cpu()] = new_thr;
            return new_thr;
        
        case OP_FORK:    // fork and context switch to new thread
        
            if (this_thr->pcb->cur_thr_num > 1) {
                //the invoking task contains more than one thread,reject fork()
                this_thr->result = EMORETHR;
                return this_thr;
            }

            // first execute thread fork (locally on the same core)
            new_thr = internal_thread_fork(this_thr);

            if (new_thr == NULL) {
                // internal_thread_fork() error (out of memory)
                this_thr->result = ENOMEM;
                return this_thr;
            }

            // thread fork success, set result of new_thr
            new_thr->result = 0;

            int rv;

            /* If it is not the idle thread that invoking fork(), should 
             * send message to the manager core and ask another core to do the 
             * rest of fork() */
            if (this_thr != idle_thr[smp_get_cpu()]) {
                // construct message
                this_thr->my_msg->req_thr = this_thr;
                this_thr->my_msg->req_cpu = smp_get_cpu();
                this_thr->my_msg->type = FORK;
                this_thr->my_msg->data.fork_data.new_thr = new_thr;
                this_thr->my_msg->data.fork_data.retry_times = 0;
                this_thr->my_msg->data.fork_data.new_tid = new_thr->tid;
                this_thr->my_msg->data.fork_data.ppid = this_thr->pcb->pid;

                // construct part of message of FORK_RESPONSE, 
                new_thr->my_msg->type = FORK_RESPONSE;
                new_thr->my_msg->data.fork_response_data.req_msg = 
                                                               this_thr->my_msg;

                context_switch(OP_SEND_MSG, 0);

                // copy result from message
                rv = this_thr->my_msg->data.fork_response_data.result;
            } else {
                /* if it is the idle thread that invoking fork(), just do the
                 * rest of fork() locally. Because idle thread is the very first
                 * thraed in the core. It can not send message to the manager
                 * core during initialzation (idle fork init) because it can not
                 * be blocked. */ 
                new_thr->my_msg->type = NONE;
                rv = fork_create_process(new_thr, this_thr);
            }


            if (rv < 0) {
                // create process error
                tcb_free_thread(new_thr);
                this_thr->result = ENOMEM;
                return this_thr;
            } 

            // fork success
            this_thr->result = new_thr->tid;

            if (this_thr == idle_thr[smp_get_cpu()]) {
                // will unlock in asm_context_switch() --> after context switch
                // to the next thread successfully
                spinlock_lock(spinlocks[smp_get_cpu()], 1);
                // decide to enqueue this thread, should not be interrupted 
                // until context switch to the next thread successfully
                scheduler_make_runnable(this_thr);
                *cur_running_thr[smp_get_cpu()] = new_thr;
                return new_thr;
            } else
                return this_thr;
            

        case OP_THREAD_FORK:    // thread_fork and context switch to new thread
            new_thr = internal_thread_fork(this_thr);

            if (new_thr != NULL) {
                // thread fork success
                this_thr->result = new_thr->tid;
                new_thr->result = 0;
                atomic_add(&this_thr->pcb->cur_thr_num, 1);
            } else {
                // internal_thread_fork() error (out of memory)
                this_thr->result = ENOMEM;
                return this_thr;
            }

            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(spinlocks[smp_get_cpu()], 1);
            // decide to enqueue this thread, should not be interrupted until 
            // context switch to the next thread successfully
            scheduler_make_runnable(this_thr);

            *cur_running_thr[smp_get_cpu()] = new_thr;
            return new_thr;

        case OP_BLOCK: // block itself
            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(spinlocks[smp_get_cpu()], 1);
            if (this_thr->state == WAKEUP || this_thr->state == MADE_RUNNABLE) {
                // already be wakened up or made runnable, should not block
                this_thr->state = NORMAL;
                return this_thr;
            } else {
                // decide to block itself, can not be wakened up (resume) or 
                // made runnable by other thread unitl context switch to the 
                // next thread successfully
                if (this_thr->state == NORMAL) {
                    this_thr->state = BLOCKED;
                } else  {
                    panic("strange state in context_switch(OP_BLOCK,0): %d", 
                                                            this_thr->state);
                }

                // let sheduler to choose the next thread to run
                new_thr = scheduler_block();
                if (new_thr == NULL) {
                    if (this_thr == idle_thr[smp_get_cpu()])
                        panic("idle thread try to block itself, something \
                                                                goes wrong!");
                    else if (idle_thr[smp_get_cpu()] == NULL)
                        panic("no other process is running, %d can not \
                                                    be blocked", this_thr->tid);
                    else {
                        *cur_running_thr[smp_get_cpu()] = 
                                                        idle_thr[smp_get_cpu()];
                        return idle_thr[smp_get_cpu()];
                    }
                } else {
                    *cur_running_thr[smp_get_cpu()] = new_thr; 
                    return new_thr;
                }
            }    

        case OP_MAKE_RUNNABLE: // make_runnable a thread
            new_thr = (tcb_t*)arg;
            if (new_thr == NULL)
                return this_thr;

            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(spinlocks[smp_get_cpu()], 1);
            if (new_thr->state == BLOCKED) {
                // the thread has already blocked, put it to the queue of 
                // scheduler
                new_thr->state = NORMAL;
                scheduler_make_runnable(new_thr);
            } else if (new_thr->state == NORMAL) {
                // the thread hasn't blocked, set state to tell it do not block
                new_thr->state = MADE_RUNNABLE;
            } else {
                panic("strange state in context_switch(OP_MAKE_RUNNABLE, 0)");
            }

            return this_thr;

        case OP_RESUME: // resume a thread
            new_thr = (tcb_t*)arg;
            
            // will unlock in asm_context_switch() --> after context switch to 
            // the next thread successfully
            spinlock_lock(spinlocks[smp_get_cpu()], 1);
            scheduler_make_runnable(this_thr);

            if (new_thr->state == BLOCKED)
                // the thread has already blocked, context switch to it directly
                new_thr->state = NORMAL;
            else if (new_thr->state == NORMAL)
                 // the thread hasn't blocked, set state to tell it do not block
                new_thr->state = WAKEUP;
            else {
                panic("strange state in context_switch(OP_RESUME,thr)");
            }

            *cur_running_thr[smp_get_cpu()] = new_thr;
            return new_thr;

        case OP_SEND_MSG: // send message to manager core
            spinlock_lock(spinlocks[smp_get_cpu()], 1);
            worker_send_msg(this_thr->my_msg);

            // let sheduler to choose the next thread to run
            new_thr = scheduler_block();
            if (new_thr == NULL) {

                if (this_thr == idle_thr[smp_get_cpu()])
                    panic("idle thread try to block itself, something \
                                                            goes wrong!");
                else if (idle_thr[smp_get_cpu()] == NULL)
                    panic("no other process is running, %d can not \
                                                be blocked", this_thr->tid);
                else {
                    *cur_running_thr[smp_get_cpu()] = idle_thr[smp_get_cpu()];
                    return idle_thr[smp_get_cpu()];
                }
            } else { 
                *cur_running_thr[smp_get_cpu()] = new_thr;
                return new_thr;
            }

        default:
            return this_thr;
    }
}

/** @brief Implement the kernel part of thread_fork()
 *  
 *  Create a new thread and copy the entire kernel stack of this thread
 *  (from very top to this_thr->k_stack_esp) to the kernel stack of the 
 *  newly created thread. Beacuse all kernel stack is cloned for the new 
 *  thread, when it returns from context_switch_get_next() it should behave
 *  exactly the same as the original thread. 
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
    uint32_t diff = 
              (uint32_t)new_thr->k_stack_esp - (uint32_t)this_thr->k_stack_esp;
    void* ebp = (void*)((uint32_t)new_thr->k_stack_esp + 56);
    *((uint32_t*) ebp) = *((uint32_t*) ebp) + diff;
    ebp = get_last_ebp(ebp);
    *((uint32_t*) ebp) = *((uint32_t*) ebp) + diff;

    return new_thr;
}


/** @brief Get old %ebp value based on current %ebp
 *
 *  @param ebp Value of current %ebp
 *
 *  @return Value of old %ebp
 */
void* get_last_ebp(void* ebp) {
    uint32_t last_ebp = *((uint32_t*) ebp);
    return (void*) last_ebp;
}

/** @brief Initialize data structure for context switcher
 *
 *  @return On success return 0, on error return -1
 */
int context_switcher_init() {
    int cur_cpu = smp_get_cpu();

    cur_running_thr[cur_cpu] = malloc(sizeof(tcb_t*));
    if (cur_running_thr[cur_cpu] == NULL)
        return -1;

    spinlocks[cur_cpu] = malloc(sizeof(spinlock_t));
    if (spinlocks[cur_cpu] == NULL)
        return -1;

    if (spinlock_init(spinlocks[cur_cpu]) < 0)
        return -1;

    return 0;
}

/** @brief Unlock spinlock of context switcher */
void context_switch_unlock() {
    spinlock_unlock(spinlocks[smp_get_cpu()], 1);
}

/** @brief Lock spinlock of context switcher */
void context_switch_lock() {
    spinlock_lock(spinlocks[smp_get_cpu()], 1);
}

/** @brief Get the current running thread */
tcb_t* get_current_running_thr() {
    return *cur_running_thr[smp_get_cpu()];
}
