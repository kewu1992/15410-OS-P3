/** @file mutex_type.h
 *  @brief This file defines the type for mutexes.
 */

#ifndef _MUTEX_TYPE_H
#define _MUTEX_TYPE_H

#include <queue.h>
#include <spinlock.h>

/** @brief Mutex type */
typedef struct mutex {
    /** @brief A flag indicating if the mutex lock is available */
    int lock_available;
    /** @brief A spinlock to protect critical section of mutex code */
    spinlock_t inner_lock;
    /** @brief A double-ended queue to store the threads that are blocking on
      * the mutex
      */
    deque_t deque;
} mutex_t;

#endif /* _MUTEX_TYPE_H */
