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


typedef struct {
    unsigned int ticks;
    tcb_t *thr;
} sleep_queue_data_t;


static pri_queue sleep_queue;

static spinlock_t sleep_lock;

static simple_queue_t deschedule_queue;

static mutex_t deschedule_mutex;

static int sleep_queue_compare(void* this, void* that);

int syscall_sleep_init() {
    int error = pri_queue_init(&sleep_queue, sleep_queue_compare);
    error |= spinlock_init(&sleep_lock);
    return error ? -1 : 0;
}

int syscall_deschedule_init() {
    int error = simple_queue_init(&deschedule_queue);
    error |= mutex_init(&deschedule_mutex);
    return error ? -1 : 0;
}

/** @brief System call handler for gettid()
 *
 *  @return The thread ID of the invoking thread.
 */
int gettid_syscall_handler() {
    return tcb_get_entry((void*)asm_get_esp())->tid;
}


int sleep_syscall_handler(int ticks) {
    if (ticks < 0)
        return -1;
    else if (ticks == 0)
        return 0;

    spinlock_lock(&sleep_lock);

    sleep_queue_data_t my_data;
    my_data.ticks = (unsigned int)ticks + timer_get_ticks();
    my_data.thr = tcb_get_entry((void*)asm_get_esp());

    pri_node_t my_node;
    my_node.data = &my_data;
    pri_queue_enqueue(&sleep_queue, &my_node);

    spinlock_unlock(&sleep_lock);

    context_switch(3, 0);

    return 0;
}


int sleep_queue_compare(void* this, void* that) {
    unsigned int t1 = ((sleep_queue_data_t*)this)->ticks;
    unsigned int t2 = ((sleep_queue_data_t*)that)->ticks;

    if (t1 > t2)
        return 1;
    else if (t1 < t2)
        return -1;
    else
        return 0;
}

// shoule be called by timer interrupt
void* timer_callback(unsigned int ticks) {   
    pri_node_t* node = pri_queue_get_first(&sleep_queue);
    if (node && ((sleep_queue_data_t*)node->data)->ticks <= timer_get_ticks()) {
        pri_queue_dequeue(&sleep_queue);
        // resume to sleeping thread
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

    context_switch(6, tid);
    return tcb_get_entry((void*)asm_get_esp())->result;
}


/*************************** swexn *************************/

/** @brief Check values in ureg validness
 *
 * @param ureg The ureg struct to check
 *
 * @return 1 if valid; 0 if invalid
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
    // Check reserved bits
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

/** @brief swexn syscall
 *
 * @param esp3 The exception stack address (one word higher than the first
 * address that the kernel should use to push values onto exception stack.
 * @param eip The first instruction of the handler function
 *
 * @return 0 on success; -1 on error
 *
 */
int swexn_syscall_handler(void *esp3, swexn_handler_t eip, void *arg, 
        ureg_t *user_newureg) {

    lprintf("swexn syscall handler called");

    ureg_t newureg;
    // Check newureg validness
    if(user_newureg != NULL) {
        // Check user memory validness
        if(!is_mem_valid((char *)user_newureg, sizeof(ureg_t), 0, 0)) {
            // user_newureg is invalid
            return -1;
        }
        // Copy new_ureg to kernel space
        memcpy(&newureg, user_newureg, sizeof(ureg_t));
        // Check newureg validness
        if(!is_newureg_valid(&newureg)) {
            lprintf("Values in newureg isn't valid");
            return -1;
        }
    }

    // Register or deregister swexn handler
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        lprintf("tcb is NULL");
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
        // Don't know how many bytes the user has allocated for exception,
        // stack, specify 1 byte so is_mem_valid will check at least one page
        int max_bytes = 1; 
        int is_check_null = 0;
        int need_writable = 1;
        if(!is_mem_valid(esp3, max_bytes, is_check_null, need_writable)) {
            return -1;
        }

        // Check eip validness
        max_bytes = 1; 
        is_check_null = 0;
        need_writable = 0;
        if(!is_mem_valid(esp3, max_bytes, is_check_null, need_writable)) {
            return -1;
        }

        // Record swexn handler parameter
        if(this_thr->swexn_struct == NULL) {
            // swexn register isn't registered now
            this_thr->swexn_struct = malloc(sizeof(swexn_t));
            if(this_thr->swexn_struct == NULL) {
                lprintf("malloc failed");
                return -1; // Should separate error types
            }
        }
        // Whether registered or not, modify saved value
        this_thr->swexn_struct->esp3 = esp3;
        this_thr->swexn_struct->eip = eip;
        this_thr->swexn_struct->arg = arg;
    }

    // Adopt newureg 
    if(user_newureg != NULL) {
        lprintf("swexn: newureg isn't NULL, will adopt it");
        // newureg is the same as user_newureg except that it's a copy in
        // in the kernel space
        asm_ret_newureg(&newureg);

        panic("swexn: adopted newureg but back from user space to kernel "
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
    if(!is_mem_valid((char*)reject, max_len, is_check_null,
                need_writable)) {
        return EFAULT;
    }

    mutex_lock(&deschedule_mutex);
    if (*reject) {
        mutex_unlock(&deschedule_mutex);
        return 0;
    }
    simple_node_t node;
    node.thr = tcb_get_entry((void*)asm_get_esp());

    // enter the tail of deschedule_queue to wait, note that stack
    // memory is used for node of queue. Because the stack of 
    // deschedule_syscall_handler() will not be destroied until this 
    // function return, so it is safe
    simple_queue_enqueue(&deschedule_queue, &node);
    mutex_unlock(&deschedule_mutex);

    context_switch(3, 0);
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
    mutex_lock(&deschedule_mutex);
    simple_node_t* node = simple_queue_remove_tid(&deschedule_queue, tid);
    mutex_unlock(&deschedule_mutex);

    if (node != NULL) {
        context_switch(4, (uint32_t)node->thr);
        return 0;
    } else
        return ETHREAD;
}
