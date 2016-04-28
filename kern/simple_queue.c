/** @file simple_queue.c
 *
 *  @brief This file contains the implementation of a simple double-ended
 *         FIFO queue without using malloc(). To avoid using malloc(), 
 *         it is caller's responsibility to provide space for node of simple 
 *         queue. The simple queue is NOT thread safe.
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug None known
 */

#include <simple_queue.h>
#include <control_block.h>

/** @brief Initialize simple queue data structure
 *   
 *  @param deque The simple queue to be initialized
 *
 *  @return On success return 0, on error return -1
 */
int simple_queue_init(simple_queue_t *deque) {
    deque->head.prev = NULL;
    deque->head.next = &(deque->tail);
    deque->tail.prev = &(deque->head);
    deque->tail.next = NULL;
    return 0;
}

/** @brief Enqueue a node to simple queue
 *
 *  @param deque The simple queue to enqueue
 *  @param new_node The node to be enqueued. The node will be at the tail of the
 *                  simple queue
 *
 *  @return On success return 0, on error return -1
 */
int simple_queue_enqueue(simple_queue_t *deque, simple_node_t* new_node) {
    new_node->next = &(deque->tail);
    new_node->prev = deque->tail.prev;
    deque->tail.prev->next = new_node;
    deque->tail.prev = new_node;
    return 0;
}

/** @brief Dequeue the head node from simple queue
 *
 *  @param deque The simple queue to dequeue
 *
 *  @return If the simple queue is not empty, return the head node of queue.
 *          If the simple queue is empty, return NULL. 
 */
simple_node_t* simple_queue_dequeue(simple_queue_t *deque) {
    if (deque->head.next == &(deque->tail))
        return NULL;
    simple_node_t* rv = deque->head.next;
    deque->head.next = deque->head.next->next;
    deque->head.next->prev = &(deque->head);
    return rv;
}

/** @brief Remove a specific node from simple queue
 *
 *  This is an application-specific function. Because in most of time, simple 
 *  queue is used to store tcb_t, it is convenient for us if simple queue can
 *  remove node based on tid. 
 *
 *  @param deque The simple queue to remove node
 *  @param tid Treat each node in simple queue as tcb_t, and remove the node
 *             whose tid equals to this parameter. 
 *
 *  @return If successfully remove a node, return that node
 *          If can not find such node in simple queue, return NULL
 */
simple_node_t* simple_queue_remove_tid(simple_queue_t *deque, int tid) {
    simple_node_t* node = &(deque->head);

    while(node->next != &(deque->tail)) {
        if (((tcb_t *)node->next->thr)->tid == tid) {
            simple_node_t* tmp = node->next;
            node->next = node->next->next;
            node->next->prev = node;
            return tmp;
        }
        node = node->next;
    }

    return NULL;
}

int simple_queue_is_exist_tid(simple_queue_t *deque, int tid) {
    simple_node_t* node = &(deque->head);

    while(node->next != &(deque->tail)) {
        if (((tcb_t *)node->next->thr)->tid == tid) {
            return 1;
        }
        node = node->next;
    }

    return 0;
}


/** @brief Multi-core version of simple_queue_remove_tid
  *
  * The data field of simple queue node is a message struct.
  *
  * @param deque The queue to search from
  * @param tid The key to search for
  *
  * @return The node that we find in the queue
  *
  */
simple_node_t* smp_simple_queue_remove_tid(simple_queue_t *deque, int tid) {
    simple_node_t* node = &(deque->head);

    while(node->next != &(deque->tail)) {
        msg_t *msg = (msg_t *)(node->next->thr);
        tcb_t *tcb = (tcb_t *)msg->req_thr;

        if(tcb->tid == tid) {
            simple_node_t* tmp = node->next;
            node->next = node->next->next;
            node->next->prev = node;
            return tmp;
        }
        node = node->next;
    }

    return NULL;
}

/** @brief Destroy a simple queue
 *   
 *  @param deque The simple queue to be destroied
 *
 *  @return On success return 0, on error return -1
 */
int simple_queue_destroy(simple_queue_t *deque) {
    if (deque->head.next != &(deque->tail))
        return -1;
    else
        return 0;
}

/** @brief Get the size a simple queue
 *   
 *  @param deque The simple queue to calculate its size
 *
 *  @return The size (# of nodes) of the simple queue
 */
int simple_queue_size(simple_queue_t *deque) {
    int count = 0;
    simple_node_t* node = &(deque->head);
    while(node->next != &(deque->tail)) {
        count++;
        node = node->next;
    }
    return count;
}
