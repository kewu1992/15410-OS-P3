#ifndef _LIST_H_
#define _LIST_H_

// APIs for doubly linked list
typedef struct node_t {
    void *data;
    struct node_t *next;
    struct node_t *prev;
} node_t;

typedef struct list_t {
    node_t *head;
} list_t;

int list_init(list_t *list);
void list_append(list_t *list, void *data);
void *list_remove_first(list_t *list);
void list_delete(list_t *list, void *data);
void list_destroy(list_t *list);

#endif

