/** @file queue.h
 *  @brief Function prototypes of a double-ended queue and declaraion of node
 *         data structure. 
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _QUEUE_H_
#define _QUEUE_H_

/** @brief The node strcuture of double-ended queue */
typedef struct node{
    /** @brief Data field */
    void *data;
    /** @brief Pointer to next node */
    struct node *next;
    /** @brief Pointer to prev node */
    struct node *prev;
} node_t;

/** @brief The strucure of a double-ended queue */
typedef struct deque{
    /** @brief Head node */
    node_t *head;
    /** @brief Tail node */
    node_t *tail;
} deque_t;

int queue_init(deque_t *deque);

int enqueue(deque_t *deque, void* data);

void* dequeue(deque_t *deque);

int queue_destroy(deque_t *deque);

int queue_is_active(deque_t *deque);

#endif
