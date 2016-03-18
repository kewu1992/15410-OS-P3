#include <list.h>
#include <malloc.h>
#include <stddef.h>
#include <simics.h>

// Init list
int list_init(list_t *list) {
    list->head = malloc(sizeof(node_t)); 
    if(list->head == NULL) {
        lprintf("malloc failed");
        return ERROR_MALLOC_LIB;
    }
    list->head->data = (void *)0;
    list->head->next = list->head;
    list->head->prev = list->head;

    return 0;
}

// Append from the end
int list_append(list_t *list, void *data) {

    node_t *new_node = malloc(sizeof(node_t));
    if(new_node == NULL) {
        lprintf("malloc failed");
        return ERROR_MALLOC_LIB;
    }
    list->head->prev->next = new_node;
    new_node->next = list->head;
    new_node->prev = list->head->prev;
    new_node->data = data;
    list->head->prev = new_node;
    return 0;
}

int list_is_empty(list_t *list) {
    if(list->head->next != list->head) {
        return 0;
    } else {
        return 1;
    }
}

// Should call list_is_empty first before calling
// this function
// Remove the first element
void *list_remove_first(list_t *list) {

    void *data = list->head->next->data;
    free(list->head->next);
    list->head->next = list->head->next->next;
    if(list->head->next == list->head) {
        list->head->prev = list->head;
    }
    return data;
}

// Delete a specific element from list
int list_delete(list_t *list, void *data) {

    node_t *node = list->head;
    while(node->next != list->head) {
        if(node->next->data == data) {
            node_t *tmp = node->next->next;
            free(node->next);
            node->next = tmp;
            node->next->prev = node;
            if(node->next == list->head) {
                node->prev = list->head;
            }
            return 0;
        }
        node = node->next;
    }

    return -1;
}


// Destroy a list
void list_destroy(list_t *list, int need_free_data) {

    node_t *node = list->head->next;
    while(node != list->head) {
        node_t *tmp = node;
        node = node->next;
        if(need_free_data && tmp->data) {
            free(tmp->data);
        }
        free(tmp);
    }
    free(list->head);

}



