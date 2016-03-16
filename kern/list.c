#include <list.h>
#include <malloc.h>
#include <stddef.h>

// Init list
int list_init(list_t *list) {
    list->head = malloc(sizeof(node_t)); 
    if(list->head == NULL) {
        return -1;
    }
    list->head->data = (void *)0;
    list->head->next = list->head;
    list->head->prev = list->head;

    return 0;
}

// Append from the end
void list_append(list_t *list, void *data) {
    
    node_t *new_node = malloc(sizeof(node_t));
    list->head->prev->next = new_node;
    new_node->next = list->head;
    new_node->prev = list->head->prev;
    new_node->data = data;
    list->head->prev = new_node;

}

// Remove the first element
void *list_remove_first(list_t *list) {
    
    if(list->head->next != list->head) {
        void *data = list->head->next->data;
        free(list->head->next);
        list->head->next = list->head->next->next;
        if(list->head->next == list->head) {
            list->head->prev = list->head;
        }
        return data;
    } else {
        return (void *)0xDEADBEEF;
    }
}

// Delete a specific element from list
void list_delete(list_t *list, void *data) {

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
            return;
        }
        node = node->next;
    }

}


// Destroy a list
void list_destroy(list_t *list) {
    
    node_t *node = list->head->next;
    while(node != list->head) {
        node_t *tmp = node;
        node = node->next;
        free(tmp);
    }
    free(list->head);

}


