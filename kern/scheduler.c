#include <queue.h>
#include <control_block.h>

static deque_t queue;

int scheduler_init() {
    if (queue_init(&queue) < 0)
        return -1;
    return 0;
}

int scheduler_enqueue_tail(tcb_t *thread) {
    int rv = enqueue(&queue, (void*)thread);

    return rv;
}

tcb_t* scheduler_get_next() {
    return dequeue(&queue);
}