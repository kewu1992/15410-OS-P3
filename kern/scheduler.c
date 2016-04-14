/** @file scheduler.c
 *  @brief Contains the implementation of a thread-unsafe version scheduler
 *
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug None known
 */

#include <simple_queue.h>
#include <control_block.h>
#include <simics.h>

/** @brief The scheduler queue */
static simple_queue_t queue;

/** @brief Init scheduler
 *
 *  @return 0 on success; -1 on error
 */
int scheduler_init() {
    if (simple_queue_init(&queue) < 0)
        return -1;
    return 0;
}

/** @brief Get next thread to run
 *
 *  @param mode Tid of the thread to yield to if not -1; else, pick the next
 *  thread in the queue
 *
 *  @return The tcb of next thread to run on success; NULL if schedueler's
 *  queue is empty.
 */
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


/** @brief Block and get next thread to run
 *
 *  @return The tcb of next thread to run on success; NULL if schedueler's
 *  queue is empty.
 */
tcb_t* scheduler_block() {
    simple_node_t* node = simple_queue_dequeue(&queue);

    if (node == NULL)
        return NULL;
    else 
        return node->thr;
}


/** @brief Make runnable a thread
 *
 *  Put the thread to make runnable in the scheduler's queue
 *
 *  @param thread The thread to make runnable
 *
 *  @return void
 */
void scheduler_make_runnable(tcb_t *thread) {
    // using the kernel stack space of thread to store its queue node
    // it is safe because the stack memory will not be reclaimed until 
    // the next time context switch 
    simple_node_t* node = (simple_node_t*)thread->k_stack_esp;
    
    node->thr = thread;
    simple_queue_enqueue(&queue, node);
}

