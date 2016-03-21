/** @file queue.c
 *
 *  @brief This file contains the implementation of a double-ended queue
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug None known
 */

#include <stdlib.h>
#include <queue.h>

/** @brief Initialize queue
 *  
 *  @param deque The double-ended queue
 *  
 *  @return 0 on success; -1 on error
 */
int queue_init(deque_t *deque){
    deque->head = malloc(sizeof(node_t));
    deque->tail = malloc(sizeof(node_t));
    if (!deque->head || !deque->tail)
        return -1;
    deque->head->next = deque->tail;
    deque->head->prev = NULL;
    deque->tail->prev = deque->head;
    deque->tail->next = NULL;
    return 0;
}

/** @brief Enqueue a element
 *  
 *  @param deque The double-ended queue to enqueue element
 *  @param data The data of the element to enqueue
 *  
 *  @return 0 on success; -1 on error
 */
int queue_enqueue(deque_t *deque, void *data) {
    node_t* element = malloc(sizeof(node_t));
    if (!element)
        return -1;
    element->data = data;

    element->prev = deque->tail->prev;
    element->next = deque->tail;
    deque->tail->prev->next = element;
    deque->tail->prev = element;

    return 0;
}

/** @brief Dequeue an element
 *  
 *  @param deque The double-ended queue to dequeue
 *  
 *  @return The data of dequeued element on success; NULL on error
 */
void* queue_dequeue(deque_t *deque) {
    if (deque->head->next == deque->tail)
        return NULL;
    node_t* element = deque->head->next;
    deque->head->next = deque->head->next->next;
    deque->head->next->prev = deque->head;
    return element->data;
}


/** @brief Remove an element from deque based on one field
 *  
 *  @param deque The double-ended queue to remove element
 *  @param field The field to identify element to remove
 *  @param func The function to get field from element
 *  
 *  @return Return the data of the element that has been removed on 
 *          success, return NULL on error
 */
void* queue_remove(deque_t *deque, void *field, void*(*func)(void*)) {
    node_t* element = deque->head;
    while (element->next != deque->tail) {
        if (func(element->next->data) == field) {
            node_t* tmp = element->next;
            element->next = element->next->next;
            element->next->prev = element;
            return tmp->data;
        }
        element = element->next;
    }
    return NULL;
}

/** @brief Destory a queue
 *  
 *  @param deque The double-ended queue to destroy
 *  
 *  @return 0 on success; -1 on error
 */
int queue_destroy(deque_t *deque) {
    if (deque->head->next != deque->tail)
        return -1;
    free(deque->head);
    free(deque->tail);
    deque->head = NULL;
    deque->tail = NULL;
    return 0;
}

/** @brief Check if a queue is active
 *  
 *  @param deque The double-ended queue to check
 *  
 *  @return 1 if queue is active; 0 else
 */
int queue_is_active(deque_t *deque) {
    return (!deque->head || !deque->tail) ? 0 : 1;
}

int queue_is_empty(deque_t *deque) {
    return (deque->head->next == deque->tail);
}

