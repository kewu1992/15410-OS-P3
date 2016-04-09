/** @file arraytcb.h
 *  @brief Interfaces for the expandable tcb management array.
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _ARRAYTCB_H_
#define _ARRAYTCB_H_

#include <cond_type.h>

/** @brief Thread state */
typedef enum {
    RUNNING,
    JOINED
} thr_state_t;

/** @brief Thread control block struct */
typedef struct {
    /** @brief Kernel assigned thread id */
    int ktid;
    /** @brief Thread lib assigned thread id */
    int tid;
    /** @brief Thread state */
    thr_state_t state;
    /** @brief Condition variable that belongs to the thread */
    cond_t cond_var;
} tcb_t;

/** @brief The node type of array->avail_list */
typedef struct availnode_s {
    /** @brief Pointer to next node */
    struct availnode_s *next;
    /** @brief Which stack 'slot' is available (not used by any thread) */
    int index;
} availnode_t;

/** @brief The data structure of arraytcb */
struct arraytcb_s {
    /** @brief The maximum capacity of arraytcb */
    int maxsize;
    /** @brief Current capacity of arraytcb 
     *  When maxsize == cursize, it means there is no room for new thread,
     *  arraytcb should be doubled.
     */
    int cursize;
    /** @brief Where the actual tcb data is stored */
    tcb_t** data;
    /** @brief A linked list that stores all available (not used by any
     * thread) stack 'slot'
     */
    availnode_t *avail_list;
};

int arraytcb_init(int size);

int arraytcb_insert_thread(int tid, mutex_t *mutex_arraytcb);

int arraytcb_delete_thread(int index);

tcb_t* arraytcb_get_thread(int index);

tcb_t* arraytcb_find_thread(int tid);

int arraytcb_set_ktid(int index, int ktid);

void arraytcb_free();

int arraytcb_is_valid(int index);

#endif

