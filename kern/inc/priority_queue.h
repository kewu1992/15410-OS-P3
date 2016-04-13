/** @file priority_queue.h
 *  @brief Function prototypes of a generic priority queue and declaraion of 
 *         priority queue node and priority queue data structure. 
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _PRIORITY_QUEUE_H_
#define _PRIORITY_QUEUE_H_

/** @brief The node strcuture of priority queue */
typedef struct pri_node_s {
    /** @brief Pointer to next node */
    struct pri_node_s *next;
    /** @brief The data field, which will be used to calculate priority */
    void *data;
} pri_node_t;

/** @brief The priority queue data structure */
typedef struct {
    /** @brief The head node */
    pri_node_t head;
    /** @brief The comparision function to calculate priority relationship 
    *          between two nodes */
    int (*compare) (void*, void*);
} pri_queue;

int pri_queue_init(pri_queue* queue, int (*compare) (void*, void*));

int pri_queue_enqueue(pri_queue* queue, pri_node_t* node);

pri_node_t* pri_queue_dequeue(pri_queue* queue);

pri_node_t* pri_queue_get_first(pri_queue* queue);

int pri_queue_destroy(pri_queue* queue);

#endif