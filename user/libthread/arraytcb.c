/** @file arraytcb.c
 *  @brief This file contains implementation of arraytcb
 *
 *  Arraytcb is a data structure to maintain metadata of user-level threads.
 *  Each element in the array of arraytcb represents a thread, it contains 
 *  information such as tid, ktid, thread state, etc. The index of the array
 *  indicates the stack number that the thread is running on. For example,
 *  array[3]->data->tid == 2 means thread #2 is running on stack #3. At 
 *  beginning, master thread (tid == 0) is running on stack 0. 
 *  array[i] == NULL means stack i is not used by any thread. Besides 
 *  array->data[] there is another data structure array->avail_list which is a
 *  linked list that stores all avilable (no be used by any thread) stack 
 *  'slot' so that arraytcb_insert_thread() can be done in O(1) time. 
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug no known bug
 */

#include <stdlib.h>
#include <stdio.h>
#include <simics.h>

#include <cond.h>
#include <mutex.h>
#include <arraytcb.h>

/** @brief An array to manage tcbs */
static struct arraytcb_s *array;

/** @brief Initialize arraytcb data structure
 *  
 *  @param size The initial size of arraytcb
 *
 *  @return On success return 0, on error return -1
 *
 */
int arraytcb_init(int size) {
    if (size <= 0)
        return -1;
    array = malloc(sizeof(struct arraytcb_s));
    if (!array)
        return -1;
    array->maxsize = size;
    array->cursize = 0;
    array->data = calloc(size, sizeof(tcb_t*));

    array->avail_list = malloc(sizeof(availnode_t));
    if (!array->avail_list)
        return -1;
    array->avail_list->next = NULL;
    return 0;
}

/** @brief Double the size of arrarytcb
 *
 *  When array->cursize == array->maxsize, this function will be invoked to 
 *  double the size of arraytcb.
 *
 *  @return On success return 0, on error return -1
 */
static int double_array() {
    int newsize = array->maxsize * 2;
    tcb_t** newdata = calloc(newsize, sizeof(tcb_t*));
    if (!newdata)
        return -1;
    
    int i;
    for (i = 0; i < array->cursize; i++)
        newdata[i] = array->data[i];

    array->maxsize = newsize;
    free(array->data);
    array->data = newdata;

    return 0;
}

/** @brief Insert a thread (indicated by tid) to arraytcb
 *  
 *  It will instantiate a tcb structure for the new thread and try to insert it
 *  to arraytcb. Then the program will check if there is any existing stack 
 *  'slot' that is available by looking at array->avail_list, if not it will 
 *  allocate a new stack 'slot' for the thread. double_array() will be invoked 
 *  if there is no more space in arraytcb for new thread. 
 *
 *  Note that although arraytcb will be locked when insert a new thread, only 
 *  the minimum amount of work is in critical section. Some expensive 
 *  operations such as malloc() and free() are not in critical section.
 *  
 *  @param tid The tid of the new thread that need to be inserted
 *  @param mutex_arraytcb The mutex to protect arraytcb data structure so that 
 *                        only one thread can access arraytcb at the same time.
 *
 *  @return On success return a non-negative number which is the index of the 
 *          stack 'slot' that is used for the new thread. On error -1 is 
 *          returned.
 *          
 */
int arraytcb_insert_thread(int tid, mutex_t *mutex_arraytcb) {
    // instantiate a tcb structure for the new thread
    tcb_t* new_thread = malloc(sizeof(tcb_t));
    if (!new_thread)
        return -1;
    new_thread->tid = tid;
    new_thread->state = RUNNING;
    cond_init(&new_thread->cond_var);

    mutex_lock(mutex_arraytcb);

    // check if there is any existing stack 'slot' that is available
    if (array->avail_list->next) {
        // using an existing available stack 'slot' from array->avail_list
        availnode_t *tmp = array->avail_list->next;
        array->avail_list->next = array->avail_list->next->next;
        int index = tmp->index;
        array->data[index] = new_thread;
        
        mutex_unlock(mutex_arraytcb);

        free(tmp);
        return index;
    } else {
        // no available exisiting stack 'slot', allocate a new stack 'slot'
        if (array->cursize == array->maxsize){
            if (double_array(array) < 0) {
                free(new_thread);
                return -1;
            }
        }

        int index = array->cursize++;
        array->data[index] = new_thread;
        
        mutex_unlock(mutex_arraytcb);

        return index;
    }
}

/** @brief Delete a thread from arraytcb
 *  
 *  After deletion, the stack 'slot' that belonged to the deleted thread becomes 
 *  available for other threads to use. The program will insert the index of 
 *  the stack to array->avail_list so that the next time 
 *  arraytcb_insert_thread() can find this available stack is O(1) time.
 *
 *  This function should be invoked() when arraytcb is locked.
 *  
 *  @param index The stack index for the thread that need to be deleted
 *
 *  @return On success return 0, on error return -1
 *
 */
int arraytcb_delete_thread(int index) {
    if (array->data[index]){
        tcb_t *thr = array->data[index];
        array->data[index] = NULL;

        cond_destroy(&thr->cond_var);
        free(thr);

        availnode_t *tmp = malloc(sizeof(availnode_t));
        while (!tmp) {
            lprintf("malloc failed, will try again...");
            printf("malloc failed, will try again...\n");
            yield(-1);
            tmp = malloc(sizeof(availnode_t));
        }
        tmp->index = index;

        tmp->next = array->avail_list->next;
        array->avail_list->next = tmp;

        return 0;
    } else{
        return -1;
    }
}

/** @brief Get the tcb structure given an array index
 *
 *  arraytcb is unnecessary locked when this function is invoked.
 *  
 *  @param index Index of array to get tcb structure
 *
 *  @return Pointer points to the tcb structure with index
 *
 */
tcb_t* arraytcb_get_thread(int index) {
    if (index < 0 || index >= array->cursize)
        return NULL;
    else
        return array->data[index];
}

/** @brief Find the tcb structure of a given thraed
 *
 *  This function should be invoked() when arraytcb is locked.
 *  
 *  @param tid The tid of the thread that need to find its index
 *
 *  @return On success return the pointer points to the tcb structure of the 
 *          thread, on error return NULL (can not find the thread in arraytcb)
 *
 */
tcb_t* arraytcb_find_thread(int tid) {
    int i;
    for (i = 0; i < array->cursize; i++)
       if (array->data[i] && array->data[i]->tid == tid)
            return array->data[i];
    return NULL; 
}

/** @brief Set ktid to the tcb structure specified by index
 *
 *  arraytcb is unnecessary locked when this function is invoked.
 *  
 *  @param index Specify which tcb structure will set ktid
 *  @param ktid The value of ktid (kernel tid) to set to tcb strcuture 
 *
 *  @return On success return zero, on error return -1 (can not find the tcb
 *          structure specified by index)
 *
 */
int arraytcb_set_ktid(int index, int ktid) {
    if (index >= array->cursize || !array->data[index])
        return -1;

    array->data[index]->ktid = ktid;
    return 0;
}

/** @brief Free arraytcb data structure, release resource
 *  
 *  Probably this function should never be called...
 *
 */
void arraytcb_free() {
     int i;
    for (i = 0; i < array->cursize; i++)
        if (array->data[i]) {
            cond_destroy(&array->data[i]->cond_var);
            free(array->data[i]);
        }
    free(array->data);
    free(array);
}

/** @brief Check if index is within the boundry of the arraytcb
 *  
 *  @param index The index to check
 *
 *  @return Return 0 if index is not within the boundray; return 1 if it is in
 */
int arraytcb_is_valid(int index) {
    if(index < 0 || index >= array->cursize) {
        return 0;
    } else {
        return 1;
    }
}

