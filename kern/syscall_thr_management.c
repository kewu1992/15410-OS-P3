/** @file syscall_thr_management.c
 *  @brief System calls related to thread management
 *
 *  This file contains implementations of system calls that are related to 
 *  thread management.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <control_block.h>
#include <asm_helper.h>
#include <simics.h>
#include <priority_queue.h>
#include <spinlock.h>
#include <timer_driver.h>
#include <context_switcher.h>
#include <malloc.h>
#include <vm.h>
#include <exception_handler.h>
#include <syscall_errors.h>

#include <smp.h>
#include <scheduler.h>

 static int sleep_queue_compare(void* this, void* that);

/** @brief For sleep() syscall.
 *         The data field for node of sleep priority queue */
typedef struct {
    /** @brief Num of ticks */
    unsigned int ticks;
    /** @brief Thread tcb */
    tcb_t *thr;
} sleep_queue_data_t;

/** @brief For sleep() syscall.
 *         The priority queue for sleep(). All threads that are blocked on
 *         sleep() will be stored in this queue. They are ordered by their
 *         time to wake up. The thread that should be wakened up in the 
 *         clostest future is at the head of the priority queue */
static pri_queue *sleep_queue[MAX_CPUS];

/** @brief For sleep() syscall.
 *         A spinlock to avoid timer interrupt when manipulating data structure
 *         of sleep() */
static spinlock_t *sleep_lock[MAX_CPUS];

/** @brief For deschedule() and make_runnable() syscalls.
 *         All threads that are blocked because of deschedule() will be stored 
 *         in this queue. make_runnable() will try to find the thread to be 
 *         made runable in this queue */
static simple_queue_t *deschedule_queues[MAX_CPUS];

/** @brief For deschedule() and make_runnable() syscalls.
 *         Mutex lock to protect deschedule_queue */
static mutex_t *deschedule_mutexs[MAX_CPUS];

/** @brief Initialize data structure for sleep() syscall 
 *
 *  @return 0 on success; -1 on error
 *
 */
int syscall_sleep_init() {

    int cur_cpu = smp_get_cpu();

    sleep_queue[cur_cpu] = malloc(sizeof(pri_queue));
    if(sleep_queue[cur_cpu] == NULL)
        return -1;

    if(pri_queue_init(sleep_queue[cur_cpu], sleep_queue_compare) < 0)
        return -1;

    sleep_lock[cur_cpu] = malloc(sizeof(spinlock_t));
    if(sleep_lock[cur_cpu] == NULL) 
        return -1;
    if(spinlock_init(sleep_lock[cur_cpu])) 
        return -1;

    return 0;

}

/** @brief Initialize data structure for deschedule() syscall 
 *
 *  @return 0 on success; -1 on error
 */
int syscall_deschedule_init() {
    int cur_cpu = smp_get_cpu();

    deschedule_queues[cur_cpu] = malloc(sizeof(simple_queue_t));
    if (deschedule_queues[cur_cpu] == NULL)
        return -1;

    deschedule_mutexs[cur_cpu] = malloc(sizeof(mutex_t));
    if (deschedule_mutexs[cur_cpu] == NULL)
        return -1;

    if (simple_queue_init(deschedule_queues[cur_cpu]) < 0)
        return -1;

    if (mutex_init(deschedule_mutexs[cur_cpu]) < 0)
        return -1;

    return 0;
}

/** @brief System call handler for gettid()
 *
 *  @return The thread ID of the invoking thread.
 */
int gettid_syscall_handler() {
    return tcb_get_entry((void*)asm_get_esp())->tid;
}

/** @brief System call handler for get_ticks()
 *
 *  This function will be invoked by get_ticks_wrapper().
 *
 *  Note that this function should be invoked with interrupt disabled otherwise
 *  the return value is imprecise.
 *
 *  @return The number of timer ticks which have occurred since system boot.
 */
unsigned int get_ticks_syscall_handler() {
    return timer_get_ticks();
}

/** @brief System call handler for sleep()
 *
 *  This function will be invoked by sleep_wrapper().
 *
 *  Deschedules the calling thread until at least ticks timer interrupts have 
 *  occurred after the call. Returns immediately if ticks is zero.
 *
 *  @param ticks The number of ticks to sleep
 *
 *  @return Returns an integer error code less than zero if ticks is negative. 
 *          Returns zero otherwise.
 */
int sleep_syscall_handler(int ticks) {
    if (ticks < 0)
        return EINVAL;
    else if (ticks == 0)
        return 0;

    int cur_cpu = smp_get_cpu();

    // lock the spinlock to avoid timer interrupt when manipulating 
    // priority queue of sleep()
    spinlock_lock(sleep_lock[cur_cpu], 1);

    sleep_queue_data_t my_data;
    // calculate its time to wake up
    my_data.ticks = (unsigned int)ticks + timer_get_ticks();
    my_data.thr = tcb_get_entry((void*)asm_get_esp());

    // here stack space is used for node of queue. Because the stack of this 
    // function will not be destroied before this thread wake up from sleep()
    // and return, it is safe
    pri_node_t my_node;
    my_node.data = &my_data;
    pri_queue_enqueue(sleep_queue[cur_cpu], &my_node);

    spinlock_unlock(sleep_lock[cur_cpu], 1);

    context_switch(OP_BLOCK, 0);

    return 0;
}

/** @brief Comparision function for priority queue of sleep()
 *
 *  This function will compare two operands based on their time to wake up
 *
 *  @param this The first operand to compare
 *  @param that The second operand to compare
 *
 *  @return The comparision result
 */
static int sleep_queue_compare(void* this, void* that) {
    unsigned int t1 = ((sleep_queue_data_t*)this)->ticks;
    unsigned int t2 = ((sleep_queue_data_t*)that)->ticks;

    if (t1 > t2)
        return 1;
    else if (t1 < t2)
        return -1;
    else
        return 0;
}

/** @brief Callback function that will be invoked by timer interrupt handler
 *
 *  This function will check if the thread at the head of the priority queue
 *  of sleep() should be wakened up. This function is invoked by timer interrupt
 *  handler, so this function call will not be interrupted. It can manipulate 
 *  priority queue of sleep() safely.
 *
 *  @param ticks The number of ticks passed to callback of timer
 *
 *  @return If the thread at the head of the priority queue should be wakened
 *          up, return the thread. Otherwise return NULL. 
 */
void* timer_callback(unsigned int ticks) {   

    int cur_cpu = smp_get_cpu();

    pri_node_t* node = pri_queue_get_first(sleep_queue[cur_cpu]);
    if (node && ((sleep_queue_data_t*)node->data)->ticks 
            <= timer_get_ticks()) {
        pri_queue_dequeue(sleep_queue[cur_cpu]);
        return (void*)((sleep_queue_data_t*)node->data)->thr; 
    } else
        return NULL;
}


/** @brief System call handler for yield()
 *
 *  This function will be invoked by yield_wrapper().
 *
 *  Defers execution of the invoking thread to a time determined by the 
 *  scheduler, in favor of the thread with ID tid. If tid is -1, the scheduler
 *  may determine which thread to run next. Ideally, the only threads whose 
 *  scheduling should be affected by yield() are the calling thread and the 
 *  thread that is yield()ed to. If the thread with ID tid does not exist, 
 *  is awaiting an external event in a system call such as readline() or wait(),
 *  or has been suspended via a system call, then an integer error code less 
 *  than zero is returned. Zero is returned on success.
 *
 *  @param tid The thread that the invoking thread will yield to. If tid is -1,
 *             the scheduler may determine which thread to run next.
 *
 *  @return 0 on success; An integer error less than 0 on failure
 */
int yield_syscall_handler(int tid) {

    context_switch(OP_YIELD, tid);
    int rv = tcb_get_entry((void*)asm_get_esp())->result;

    if (rv < 0) {
        // the thread is not on this core, try other cores
        
        tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
        pcb_t* pcb = this_thr->pcb;

        // Construct message
        msg_t* msg = this_thr->my_msg;
        msg->req_thr = this_thr;
        msg->req_cpu = smp_get_cpu();
        msg->type = YIELD;
        msg->data.yield_data.tid = tid;
        msg->data.yield_data.result = -1;
        msg->data.yield_data.next_core = smp_get_cpu();

        do {
            if (scheduler_is_exist_or_running(msg->data.yield_data.tid)) {
                msg->data.yield_data.result = 0;
            }

            // loop to visit all cores
            context_switch(OP_SEND_MSG, 0);

        } while (msg->data.yield_data.result < 0 && 
                  smp_get_cpu() != msg->req_cpu);

        // set page table base back to its own
        this_thr->pcb = pcb;
        set_cr3(this_thr->pcb->page_table_base);

        if (msg->data.yield_data.result < 0)
            return ETHREAD;
        else
            return 0;
    } else
        return 0;
}

/** @brief Check validness of values in ureg
 *
 *  @param ureg The ureg struct to check
 *
 *  @return 1 if valid, else 0
 *
 */
static int is_newureg_valid(ureg_t *ureg) {

    // Check segment registers
    if(ureg->ds != SEGSEL_USER_DS || ureg->es != SEGSEL_USER_DS 
            || ureg->fs != SEGSEL_USER_DS || ureg->gs != SEGSEL_USER_DS
            || ureg->ss != SEGSEL_USER_DS || ureg->cs != SEGSEL_USER_CS) {
        return 0;
    }

    // Check ebp, esp, and eip
    if(ureg->ebp < USER_MEM_START || ureg->esp < USER_MEM_START
            || ureg->eip < USER_MEM_START) {
        return 0;
    }

    // Check eflags
    if(EFLAGS_GET_RSV(ureg->eflags) != EFLAGS_EX_VAL_RSV || 
            EFLAGS_GET_IOPL(ureg->eflags) != EFLAGS_EX_VAL_IOPL ||
            EFLAGS_GET_TF(ureg->eflags) != EFLAGS_EX_VAL_TF ||
            EFLAGS_GET_IF(ureg->eflags) != EFLAGS_EX_VAL_IF ||
            EFLAGS_GET_NT(ureg->eflags) != EFLAGS_EX_VAL_NT ||
            EFLAGS_GET_OTHER(ureg->eflags) != EFLAGS_EX_VAL_OTHER) {
        return 0;
    }

    return 1;

}

/** @brief swexn syscall handler
 *
 *  @param esp3 The exception stack address (one word higher than the first
 *  address that the kernel should use to push values onto exception stack.)
 *  @param eip The first instruction of the handler function
 *  @param arg The argument to pass to the swexn handler
 *  @param user_newureg The ureg struct that the user wants kernel to adopt
 *
 *  @return 0 on success; a negative integer on error
 *
 */
int swexn_syscall_handler(void *esp3, swexn_handler_t eip, void *arg, 
        ureg_t *user_newureg) {

    ureg_t newureg;
    // Check newureg validness
    if(user_newureg != NULL) {
        // Check user memory validness
        if(check_mem_validness((char *)user_newureg, sizeof(ureg_t), 0, 0) < 0){
            // user_newureg is invalid
            return EINVAL;
        }
        // Copy new_ureg to kernel space
        memcpy(&newureg, user_newureg, sizeof(ureg_t));
        // Check newureg validness
        if(!is_newureg_valid(&newureg)) {
            return EINVAL;
        }
    }

    // Register or deregister swexn handler
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        panic("tcb is NULL");
    }
    if(esp3 == NULL || eip == NULL) {
        // Deregister swexn handler
        if(this_thr->swexn_struct != NULL) {
            free(this_thr->swexn_struct);
            this_thr->swexn_struct = NULL;
        }
    } else {
        // Register new swexn handler

        // Check swexn handler parameter
        // Check esp3 validness
        // Specify 1 byte so check_mem_validness will check at least one page.
        int max_bytes = 1; 
        int is_check_null = 0;
        int need_writable = 1;
        if(check_mem_validness(esp3, max_bytes, is_check_null, need_writable)
                < 0) {
            return EINVAL;
        }

        // Check eip validness
        max_bytes = 1; 
        is_check_null = 0;
        need_writable = 0;
        if(check_mem_validness(esp3, max_bytes, is_check_null, need_writable) 
                < 0) {
            return EINVAL;
        }

        // Record swexn handler parameter
        if(this_thr->swexn_struct == NULL) {
            // swexn register isn't registered now
            this_thr->swexn_struct = malloc(sizeof(swexn_t));
            if(this_thr->swexn_struct == NULL) {
                return ENOMEM;
            }
        }
        // Reregister swexn handler
        this_thr->swexn_struct->esp3 = esp3;
        this_thr->swexn_struct->eip = eip;
        this_thr->swexn_struct->arg = arg;
    }

    if(user_newureg != NULL) {
        // newureg is the same as user_newureg except that it's a copy in
        // the kernel space.
        // Adopt newureg specified by the user
        asm_ret_newureg(&newureg);
        panic("swexn: adopted newureg but back from user to kernel "
                "exception handler?!");
    }

    return 0;

}



/** @brief System call handler for deschedule()
 *
 *  This function will be invoked by deschedule_wrapper().
 *
 *  Atomically checks the integer pointed to by reject and acts on it. If the 
 *  integer is non-zero, the call returns immediately with return value zero. 
 *  If the integer pointed to by reject is zero, then the calling thread will 
 *  not be run by the scheduler until a make runnable() call is made specifying
 *  the deschedule()’d thread, at which point deschedule() will return zero.
 *
 *  This system call is atomic with respect to make runnable(): the process of 
 *  examining reject and suspending the thread will not be interleaved with any
 *  execution of make runnable() specifying the thread calling deschedule().
 *
 *  @param reject An integer pointer to indicate whether to block or not.
 *
 *  @return 0 on success; An integer error code less than zero is returned if 
 *          reject is not a valid pointer
 */
int deschedule_syscall_handler(int *reject) {

    // Check parameter
    int is_check_null = 0;
    int max_len = sizeof(int);
    int need_writable = 1;
    if(check_mem_validness((char*)reject, max_len, is_check_null,
                need_writable) < 0) {
        return EFAULT;
    }
    // Finish parameter check

    // using mutex to protect deschedule_queue, it also make sure examine the 
    // value of *reject and block the thread (at here it is done by put the 
    // thread in the deschedule_queue) is atomic with respect to make runnable()
    mutex_lock(deschedule_mutexs[smp_get_cpu()]);
    if (*reject) {
        mutex_unlock(deschedule_mutexs[smp_get_cpu()]);
        return 0;
    }
    simple_node_t node;
    node.thr = tcb_get_entry((void*)asm_get_esp());

    // enter the tail of deschedule_queue to wait, note that stack
    // memory is used for node of queue. Because the stack of 
    // deschedule_syscall_handler() will not be destroied until this 
    // function return, so it is safe
    simple_queue_enqueue(deschedule_queues[smp_get_cpu()], &node);
    mutex_unlock(deschedule_mutexs[smp_get_cpu()]);

    context_switch(OP_BLOCK, 0);
    return 0;

}

/** @brief System call handler for make_runnable()
 *
 *  This function will be invoked by make_runnable_wrapper().
 *
 *  Makes the deschedule()d thread with ID tid runnable by the scheduler. 
 *  
 *  @param tid The tid of the thread that will be made runnable
 *
 *  @return On success, zero is returned. An integer error code less than zero 
 *          will be returned unless tid is the ID of a thread which exists 
 *          but is currently non-runnable due to a call to deschedule().
 */
int make_runnable_syscall_handler(int tid) {

    // Hand over to manager core
    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    pcb_t* pcb = this_thr->pcb;

    // Construct message
    msg_t* msg = this_thr->my_msg;
    msg->req_thr = this_thr;
    msg->req_cpu = smp_get_cpu();
    msg->type = MAKE_RUNNABLE;
    msg->data.make_runnable_data.tid = tid;
    msg->data.make_runnable_data.result = -1;
    msg->data.make_runnable_data.next_core = smp_get_cpu();

    do {
        mutex_lock(deschedule_mutexs[smp_get_cpu()]);
        simple_node_t* node = simple_queue_remove_tid(deschedule_queues[smp_get_cpu()], tid);
        mutex_unlock(deschedule_mutexs[smp_get_cpu()]);

        if (node != NULL) {
            // find the descheduled thread, make it runnable
            context_switch(OP_MAKE_RUNNABLE, (uint32_t)node->thr);
            msg->data.make_runnable_data.result = 0;
        }

        // loop to visit deschedule queues of all cores
        context_switch(OP_SEND_MSG, 0);

    } while (msg->data.make_runnable_data.result < 0 &&
             smp_get_cpu() != msg->req_cpu);

    
    // set page table base back to its own
    this_thr->pcb = pcb;
    set_cr3(this_thr->pcb->page_table_base);

    if (msg->data.make_runnable_data.result < 0)
        return ETHREAD;
    else
        return 0;

}

