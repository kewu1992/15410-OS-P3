/** @file mutex.h
 *  @brief This file defines the interface for mutexes.
 */

#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <spinlock.h>
#include <mutex_queue.h>

/** @brief Mutex type */
typedef struct mutex {
    /** @brief Indicating the holder of the the mutex lock, -1 means lock is 
     *         available, -2 means lock is destroied */
    int lock_holder;
    /** @brief A spinlock to protect critical section of mutex code */
    spinlock_t inner_lock;
    /** @brief A double-ended queue to store the threads that are blocking on
     *         the mutex */
    mutex_deque_t deque;
} mutex_t;


int mutex_init( mutex_t *mp );
void mutex_destroy( mutex_t *mp );
void mutex_lock( mutex_t *mp );
void mutex_unlock( mutex_t *mp );

#endif /* _MUTEX_H_ */
