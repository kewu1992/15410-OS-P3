#ifndef _PRIORITY_QUEUE_H_
#define _PRIORITY_QUEUE_H_

typedef struct pri_node_s {
    struct pri_node_s *next;
    void *data;
} pri_node_t;

typedef struct {
    pri_node_t head;
    int (*compare) (void*, void*);
} pri_queue;

int pri_queue_init(pri_queue* queue, int (*compare) (void*, void*));

int pri_queue_enqueue(pri_queue* queue, pri_node_t* node);

pri_node_t* pri_queue_dequeue(pri_queue* queue);

pri_node_t* pri_queue_get_first(pri_queue* queue);

int pri_queue_destroy(pri_queue* queue);

#endif