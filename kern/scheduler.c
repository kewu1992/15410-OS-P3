#include <simple_queue.h>
#include <control_block.h>
#include <spinlock.h>
#include <cr.h>
#include <simics.h>

#define NULL 0



static simple_queue_t queue;
static spinlock_t spinlock;

int scheduler_init() {
    if (simple_queue_init(&queue) < 0)
        return -1;
    if (spinlock_init(&spinlock) < 0)
        return -1;
    return 0;
}

// thread-safe
tcb_t* scheduler_get_next(int mode) {
    simple_node_t* node;

    spinlock_lock(&spinlock);
    if (mode == -1)
        node = simple_queue_dequeue(&queue);
    else {
        // yield to a specific thread
        node = simple_queue_remove_tid(&queue, mode);
    }
    spinlock_unlock(&spinlock);

    if (node == NULL)
        return NULL;
    else 
        return node->thr;
}

/** @brief Thread safe version, check if an element is in scheduler's queue
  *
  * @param tid The tid of thread to search in the queue
  *
  * @return 1 on success; 0 on failure
  *
  */
int scheduler_is_exist(int tid) {

    spinlock_lock(&spinlock);
    int ret = simple_queue_is_exist(&queue, tid);
    spinlock_unlock(&spinlock);

    return ret;

}

// thread-unsafe
tcb_t* scheduler_block() {
    simple_node_t* node = simple_queue_dequeue(&queue);

    if (node == NULL)
        return NULL;
    else 
        return node->thr;
}

// thread-unsafe
void scheduler_make_runnable(tcb_t *thread) {
    // using the kernel stack space of thread to store its queue node
    // it is safe because the stack memory will not be reclaimed until 
    // the next time context switch 
    simple_node_t* node = (simple_node_t*)thread->k_stack_esp;
    
    node->thr = thread;
    simple_queue_enqueue(&queue, node);
}

