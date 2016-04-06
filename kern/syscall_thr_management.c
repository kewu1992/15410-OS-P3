#include <control_block.h>
#include <asm_helper.h>
#include <simics.h>
#include <priority_queue.h>
#include <spinlock.h>
#include <timer_driver.h>
#include <context_switcher.h>


typedef struct {
    unsigned int ticks;
    tcb_t *thr;
} sleep_queue_data_t;


static pri_queue sleep_queue;

static spinlock_t sleep_lock;

static int sleep_queue_compare(void* this, void* that);

int syscall_sleep_init() {
    int error = pri_queue_init(&sleep_queue, sleep_queue_compare);
    error |= spinlock_init(&sleep_lock);
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
void timer_callback(unsigned int ticks) {   
    // what about thread with same ticks???
    pri_node_t* node = pri_queue_get_first(&sleep_queue);
    if (node && ((sleep_queue_data_t*)node->data)->ticks == timer_get_ticks()) {
        pri_queue_dequeue(&sleep_queue);
        // make sleeping thread runnable
        context_switch(4, (uint32_t)((sleep_queue_data_t*)node->data)->thr); 
    }

}


/*************************** yield *************************/

/** @brief Yield
 *
 *  @return 0 on success; An integer error less than 0 on failure
 */
int yield_syscall_handler(int tid) {

    context_switch(0, tid);
    int ret = tcb_get_entry((void*)asm_get_esp())->result;
    lprintf("yield ret: %d", ret);
    return ret;
}



