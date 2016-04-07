#include <control_block.h>
#include <asm_helper.h>
#include <simics.h>
#include <priority_queue.h>
#include <spinlock.h>
#include <timer_driver.h>
#include <context_switcher.h>

#define NULL 0

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


/*************************** yield *************************/

/** @brief Yield
 *
 *  @return 0 on success; An integer error less than 0 on failure
 */
int yield_syscall_handler(int tid) {

    context_switch(6, tid);
    return tcb_get_entry((void*)asm_get_esp())->result;
}



int deschedule_syscall_handler(int *reject) {
    // CHECK PARAMETER!!!!!!!!!!!!

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

