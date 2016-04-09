/** @file cond_type.h
 *  @brief This file defines the type for condition variables.
 */

#ifndef _COND_TYPE_H
#define _COND_TYPE_H

#include <mutex_type.h>
#include <queue.h>


/** @brief Condition variable type */
typedef struct cond {
    /** @brief A mutex to protect critical section of condition varaible code 
     */
    mutex_t mutex;
    /** @brief A double-ended queue to place the threads that are blocking on 
     *  the condition varaible.
     */
    deque_t deque;
} cond_t;

#endif /* _COND_TYPE_H */
