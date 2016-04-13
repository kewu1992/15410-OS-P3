#include <simple_queue.h>
#include <control_block.h>
#include <simics.h>

static simple_queue_t queue;

int scheduler_init() {
    if (simple_queue_init(&queue) < 0)
        return -1;
    return 0;
}

// thread-unsafe
tcb_t* scheduler_get_next(int mode) {
    simple_node_t* node;

    if (mode == -1)
        node = simple_queue_dequeue(&queue);
    else {
        // yield to a specific thread
        node = simple_queue_remove_tid(&queue, mode);
    }

    if (node == NULL)
        return NULL;
    else 
        return node->thr;
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

