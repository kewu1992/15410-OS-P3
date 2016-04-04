#include <list.h>
#include <malloc.h>
#include <stddef.h>
#include <simics.h>

/** @brief Init list
 * 
 * @param list The list to operate on
 *
 * @return 0 on success; A negative integer on error
 *
 */
int list_init(list_t *list) {

    if(mutex_init(&(list->mutex))) {
        lprintf("mutex_init failed");
        return -1;
    }

    list->head = malloc(sizeof(list_node_t)); 
    if(list->head == NULL) {
        lprintf("malloc failed");
        return ERROR_MALLOC_LIB;
    }
    list->head->data = (void *)0;
    list->head->next = list->head;
    list->head->prev = list->head;

    return 0;
}

/** @brief Append from the back
 * 
 * @param list The list to operate on
 * @param data The data value to set for the new element
 *
 * @return 0 on success; A negative integer on error
 *
 */
int list_append(list_t *list, void *data) {

    list_node_t *new_node = malloc(sizeof(list_node_t));
    if(new_node == NULL) {
        lprintf("malloc failed");
        return ERROR_MALLOC_LIB;
    }

    mutex_lock(&(list->mutex));
    list->head->prev->next = new_node;
    new_node->next = list->head;
    new_node->prev = list->head->prev;
    new_node->data = data;
    list->head->prev = new_node;
    mutex_unlock(&(list->mutex));

    return 0;
}

/** @brief Get a private copy of the list
 * 
 *  @param list The list to copy
 *
 *  @return Pointer to a new copy of list on success; NULL on error
 *
 */
list_t *list_get_copy(list_t *list) {

    list_t *copy_list = malloc(sizeof(list_t));
    if(copy_list == NULL) {
        lprintf("malloc failed");
        return NULL;
    }
    if(list_init(copy_list) == -1) {
        lprintf("list_init failed");
        return NULL;
    }

    mutex_lock(&(list->mutex));
    list_node_t *node = list->head->next;
    while(node != list->head) {
        if(list_append(copy_list, node->data) < 0) {
            lprintf("list append failed");
            mutex_unlock(&(list->mutex));
            return NULL;
        }
    }
    mutex_unlock(&(list->mutex));

    return copy_list;

}

/** @brief Remove first element of the list
 * 
 * @param list The list to operate on
 * @param datap The place to store data of the removed element
 *
 * @return 0 on success; -1 if list is empty
 *
 */
int list_remove_first(list_t *list, void **datap) {

    int ret = 0;
    mutex_lock(&list->mutex);
    if(list->head->next != list->head) {   
        *datap = list->head->next->data;
        free(list->head->next);
        list->head->next = list->head->next->next;
        if(list->head->next == list->head) {
            list->head->prev = list->head;
        }
        ret = 0;
    } else {
        ret = -1;
    }
    mutex_unlock(&list->mutex);

    return ret;
}

/** @brief Delete a specific element from list
 * 
 * @param list The list to operate on
 * @param data The data value of the element to delete
 *
 * @return 0 on success; -1 if not found
 *
 */
int list_delete(list_t *list, void *data) {

    int ret = -1;
    mutex_lock(&list->mutex);
    list_node_t *node = list->head;
    while(node->next != list->head) {
        if(node->next->data == data) {
            list_node_t *tmp = node->next->next;
            free(node->next);
            node->next = tmp;
            node->next->prev = node;
            if(node->next == list->head) {
                node->prev = list->head;
            }
            ret = 0;
        }
        node = node->next;
    }
    mutex_unlock(&list->mutex);

    return ret;
}


/** @brief Destroy a list
 * 
 * @param list The list to operate on
 * @param need_free_data Flag indicating if data field needs to be freed
 *
 * @return Void
 *
 */
void list_destroy(list_t *list, int need_free_data) {

    mutex_destroy(&list->mutex);

    list_node_t *node = list->head->next;
    while(node != list->head) {
        list_node_t *tmp = node;
        node = node->next;
        if(need_free_data && tmp->data) {
            free(tmp->data);
        }
        free(tmp);
    }
    free(list->head);

}


