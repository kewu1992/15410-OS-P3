#ifndef _LIST_H_
#define _LIST_H_

#include <mutex.h>

// APIs for doubly linked list
typedef struct list_node_t {
    void *data;
    struct list_node_t *next;
    struct list_node_t *prev;
} list_node_t;

typedef struct list_t {
    list_node_t *head;
    mutex_t mutex;
} list_t;

int list_init(list_t *list);
int list_append(list_t *list, void *data);
int list_remove_first(list_t *list, void **datap);
int list_delete(list_t *list, void *data);
void list_destroy(list_t *list, int need_free_data);

#ifndef ERROR_MALLOC_LIB
#define ERROR_MALLOC_LIB (-1)
#endif

#endif


