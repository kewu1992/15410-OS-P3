/** @file mutex_queue.h
 *  @brief This file defines the interface of a queue that is for 
 *         mutex especially. This queue doesn't use malloc().
 */

#ifndef _MUTEX_QUEUE_H_
#define _MUTEX_QUEUE_H_

/** @brief The node strcuture of double-ended queue */
typedef struct mutex_node{
    /** @brief Data field */
    int tid;
    /** @brief Pointer to next node */
    struct mutex_node *next;
    /** @brief Pointer to prev node */
    struct mutex_node *prev;
} mutex_node_t;

/** @brief The strucure of a double-ended queue */
typedef struct mutex_deque{
    /** @brief Head node */
    mutex_node_t head;
    /** @brief Tail node */
    mutex_node_t tail;
} mutex_deque_t;

int mutex_queue_init(mutex_deque_t *deque);

int mutex_queue_enqueue(mutex_deque_t *deque, mutex_node_t* new_node);

mutex_node_t* mutex_queue_dequeue(mutex_deque_t *deque);

int mutex_queue_destroy(mutex_deque_t *deque);


#endif /* _MUTEX_QUEUE_H_ */
