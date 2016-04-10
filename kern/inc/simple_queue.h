/** @file simple_queue.h
 *  @brief This file defines the interface of a queue that
 *         doesn't use malloc().
 */

#ifndef _SIMPLE_QUEUE_H_
#define _SIMPLE_QUEUE_H_

/** @brief The node strcuture of double-ended queue */
typedef struct simple_node{
    /** @brief Data field */
    void* thr;
    /** @brief Pointer to next node */
    struct simple_node *next;
    /** @brief Pointer to prev node */
    struct simple_node *prev;
} simple_node_t;

/** @brief The strucure of a double-ended queue */
typedef struct simple_deque{
    /** @brief Head node */
    simple_node_t head;
    /** @brief Tail node */
    simple_node_t tail;
} simple_queue_t;

int simple_queue_init(simple_queue_t *deque);

int simple_queue_enqueue(simple_queue_t *deque, simple_node_t* new_node);

simple_node_t* simple_queue_dequeue(simple_queue_t *deque);

simple_node_t* simple_queue_remove_tid(simple_queue_t *deque, int tid);

int simple_queue_destroy(simple_queue_t *deque);

int simple_queue_is_exist(simple_queue_t *deque, int tid);

int simple_queue_size(simple_queue_t *deque);


#endif /* _SIMPLE_QUEUE_H_ */
