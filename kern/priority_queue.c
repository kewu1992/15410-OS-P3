/** @file priority_queue.c
 *  @brief This file contains implementation of a priority queue
 *
 *  The priority queue is implemented using linked list. To avoid using
 *  malloc(), it is caller's responsibility to provide space for node of 
 *  priority queue. The priority queue is NOT thread safe.  
 *
 *  @author Ke Wu (kewu)
 *  @bug No known bug
 */

#include <priority_queue.h>

#define NULL 0

/** @brief Initialize priority queue data structure
 *   
 *  @param queue The priority queue to be initialized
 *  @param compare The comparision function for priority queue to compare 
 *         priority of two nodes
 *
 *  @return On success return 0, on error return -1
 */
int pri_queue_init(pri_queue* queue, int (*compare) (void*, void*)) {
    queue->compare = compare;
    queue-head.next = NULL;
    return 0;
}

/** @brief Enqueue a node to priority queue
 *
 *  The node with the smallest value of priority will be the head of queue
 *   
 *  @param queue The priority queue to enqueue
 *  @param node The node to be enqueued. The value of priority is calculated
 *              using data filed of the node based on comparision function
 *
 *  @return On success return 0, on error return -1
 */
int pri_queue_enqueue(pri_queue* queue, pri_node_t* node) {
    pri_node_t* tmp = &(queue->head);
    while(tmp->next && queue->compare(node->data, tmp->next->data) > 0)
        tmp = tmp->next;
    node->next = tmp->next;
    tmp->next = node;
    return 0;
}

/** @brief Dequeue the head node from priority queue
 *
 *  @param queue The priority queue to dequeue
 *
 *  @return If the priority queue is not empty, return the head node of queue.
 *          If the priority queue is empty, return NULL. 
 */
pri_node_t* pri_queue_dequeue(pri_queue* queue) {
    if (!queue->head.next)
        return NULL;
    pri_node_t * rv = queue->head.next;
    queue->head.next = rv->next;
    return rv;
}

/** @brief Get the head node from priority queue (without removing it)
 *
 *  @param queue The priority queue to get the head node
 *
 *  @return If the priority queue is not empty, return the head node of queue.
 *          If the priority queue is empty, return NULL. 
 */
pri_node_t* pri_queue_get_first(pri_queue* queue) {
    return queue->head.next;
}

/** @brief Destroy a priority queue
 *   
 *  @param queue The priority queue to be destroied
 *
 *  @return On success return 0, on error return -1
 */
int pri_queue_destroy(pri_queue* queue) {
    return 0;
}

