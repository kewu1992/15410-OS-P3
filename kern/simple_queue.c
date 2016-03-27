#include <simple_queue.h>
#include <control_block.h>
#define NULL 0

int simple_queue_init(simple_queue_t *deque) {
    deque->head.prev = NULL;
    deque->head.next = &(deque->tail);
    deque->tail.prev = &(deque->head);
    deque->tail.next = NULL;
    return 0;
}

int simple_queue_enqueue(simple_queue_t *deque, simple_node_t* new_node) {
    new_node->next = &(deque->tail);
    new_node->prev = deque->tail.prev;
    deque->tail.prev->next = new_node;
    deque->tail.prev = new_node;
    return 0;
}

simple_node_t* simple_queue_dequeue(simple_queue_t *deque) {
    if (deque->head.next == &(deque->tail))
        return NULL;
    simple_node_t* rv = deque->head.next;
    deque->head.next = deque->head.next->next;
    deque->head.next->prev = &(deque->head);
    return rv;
}

simple_node_t* simple_queue_remove_tid(simple_queue_t *deque, int tid) {
    simple_node_t* node = &(deque->head);

    while(node->next != &(deque->tail)) {
        if (node->next->thr->tid == tid) {
            simple_node_t* tmp = node->next;
            node->next = node->next->next;
            node->next->prev = node;
            return tmp;
        }
        node = node->next;
    }

    return NULL;
}

int simple_queue_destroy(simple_queue_t *deque) {
    if (deque->head.next != &(deque->tail))
        return -1;
    else
        return 0;
}