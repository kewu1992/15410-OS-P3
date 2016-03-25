#include <mutex_queue.h>

#define NULL 0

int mutex_queue_init(mutex_deque_t *deque) {
    deque->head.prev = NULL;
    deque->head.next = &(deque->tail);
    deque->tail.prev = &(deque->head);
    deque->tail.next = NULL;
    return 0;
}

int mutex_queue_enqueue(mutex_deque_t *deque, mutex_node_t* new_node) {
    new_node->next = deque->head.next;
    new_node->prev = &(deque->head);
    deque->head.next->prev = new_node;
    deque->head.next = new_node;
    return 0;
}

mutex_node_t* mutex_queue_dequeue(mutex_deque_t *deque) {
    if (deque->head.next == &(deque->tail))
        return NULL;
    mutex_node_t* rv = deque->head.next;
    deque->head.next = deque->head.next->next;
    deque->head.next->prev = &(deque->head);
    return rv;
}

int mutex_queue_destroy(mutex_deque_t *deque) {
    if (deque->head.next != &(deque->tail))
        return -1;
    else
        return 0;
}