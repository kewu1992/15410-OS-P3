#include <queue.h>
#include <control_block.h>
#include <spinlock.h>

#define NULL 0

static deque_t queue;
static spinlock_t spinlock;

static void* queue_remove_gettid(void* element);

int scheduler_init() {
    if (queue_init(&queue) < 0)
        return -1;
    if (spinlock_init(&spinlock) < 0)
        return -1;
    return 0;
}

/*
 *  @return 0 on success; -1 on error
 */
int scheduler_enqueue_tail(tcb_t *thread) {
    int rv;

    spinlock_lock(&spinlock);
    rv = queue_enqueue(&queue, (void*)thread);
    spinlock_unlock(&spinlock);

    return rv;
}

tcb_t* scheduler_get_next(int mode) {
    tcb_t* rv;

    spinlock_lock(&spinlock);
    if (mode == -1)
        rv = queue_dequeue(&queue);
    else {
        // yield to a specific thread
        rv = queue_remove(&queue, (void*)mode, queue_remove_gettid);
        if (rv == NULL){

        }
    }
    spinlock_unlock(&spinlock);

    return rv;
}

void* queue_remove_gettid(void* thread) {
    return (void*)(((tcb_t*)thread)->tid);
}