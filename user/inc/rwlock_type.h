/** @file rwlock_type.h
 *  @brief This file defines the type for reader/writer locks.
 */

#ifndef _RWLOCK_TYPE_H
#define _RWLOCK_TYPE_H

#include <mutex_type.h>
#include <cond_type.h>

/** @brief rwlock type */
typedef struct rwlock {
    /** @brief State of rwlock */
    int lock_state;
    /** @brief A count of how many writers are waiting on the lock */
    int writer_waiting_count;
    /** @brief A count of how many readers are waiting on the lock */
    int reader_waiting_count;
    /** @brief A mutex to protect critical section of rwlock code */
    mutex_t mutex_inner;
    /** @brief Conditional variable for readers to block and signal */
    cond_t  cond_reader;
    /** @brief Conditional variable for writers to block and signal */
    cond_t  cond_writer;
} rwlock_t;

#endif /* _RWLOCK_TYPE_H */
