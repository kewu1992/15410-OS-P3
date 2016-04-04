#include <priority_queue.h>

#define NULL 0

int pri_queue_init(pri_queue* queue, int (*compare) (void*, void*)) {
    queue->compare = compare;
    return 0;
}

int pri_queue_enqueue(pri_queue* queue, pri_node_t* node) {
    pri_node_t* tmp = &(queue->head);
    while(tmp->next && queue->compare(node->data, tmp->next->data) > 0)
        tmp = tmp->next;
    node->next = tmp->next;
    tmp->next = node;
    return 0;
}

pri_node_t* pri_queue_dequeue(pri_queue* queue) {
    if (!queue->head.next)
        return NULL;
    pri_node_t * rv = queue->head.next;
    queue->head.next = rv->next;
    return rv;
}

pri_node_t* pri_queue_get_first(pri_queue* queue) {
    return queue->head.next;
}

int pri_queue_destroy(pri_queue* queue) {
    return 0;
}

