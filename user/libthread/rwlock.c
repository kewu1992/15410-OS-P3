/** @file rwlock.c
 *  @brief Contains the implementation of rwlock
 *
 *  rwlock_t contains the following fields
 *     1. lock_state: it is used to indicate the state of rwlock. 
 *        lock_state == 0 means rwlock is available (unlocked).
 *        lock_state > 0 means some readers are holding the lock (shared), the 
 *                       value of lock_state indicates how many readers are 
 *                       holding the lock
 *        lock_state == -1 means a writer is holding the lock (exclusvie)
 *        lock_state == -2 means the rwlock is destoried
 *     2. writer_waiting_count: indicates how many writers are waiting the lock
 *     3. reader_waiting_count: indicates how many readers are waiting the lock
 *     4. mutex_inner: a mutex to protect critical section of rwlock code.
 *     5. cond_reader: conditional variable for readers to block
 *     6. cond_writer: conditional variable for writers to block
 *
 *
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */
#include <rwlock.h>
#include <mutex.h>
#include <cond.h>
#include <assert.h>
#include <simics.h>
#include <stdio.h>

/** @brief Initialize rwlock
 *
 *  @param rwlock The rwlock to initialize
 *  @return 0 on success; -1 on error
 */
int rwlock_init( rwlock_t *rwlock ) { 
    rwlock->lock_state = 0;
    rwlock->writer_waiting_count = 0;
    rwlock->reader_waiting_count = 0;

    int is_error = 0;
    is_error |= mutex_init(&rwlock->mutex_inner);
    is_error |= cond_init(&rwlock->cond_reader);
    is_error |= cond_init(&rwlock->cond_writer);
    return is_error ? -1 : 0;
}

/** @brief Lock rwlock
 *
 *  @param rwlock The rwlock to acquire lock
 *  @param type Type of lock to acquire (RWLOCK_READ or RWLOCK_WRITE)
 *
 *  @return void
 */
void rwlock_lock( rwlock_t *rwlock, int type ) {
    if (type == RWLOCK_READ) {
        mutex_lock(&rwlock->mutex_inner);

        if (rwlock->lock_state == -2) {
            panic("readers/writers lock %p has already been destroyed!", 
                    rwlock);
        }

        rwlock->reader_waiting_count++;
        // as long as rwlock is not available or some writers are waiting, 
        // reader should wait
        while (rwlock->lock_state != 0 || rwlock->writer_waiting_count > 0) {
            // only when there is no writer is waiting and rwlock is a reader
            // lock can the new reader hold the shared reader lock(favor writer)
            if (rwlock->lock_state > 0 && rwlock->writer_waiting_count == 0) 
                break;
            cond_wait(&rwlock->cond_reader, &rwlock->mutex_inner);
        }
        rwlock->reader_waiting_count--;

        // share the lock with other readers
        rwlock->lock_state++;

        mutex_unlock(&rwlock->mutex_inner);
    } else {
        mutex_lock(&rwlock->mutex_inner);

        if (rwlock->lock_state == -2) {
            panic("readers/writers lock %p has already been destroied!", 
                    rwlock);
        }

        rwlock->writer_waiting_count++;
        // as long as rwlock is not available, writer must wait
        while (rwlock->lock_state != 0) 
            cond_wait(&rwlock->cond_writer, &rwlock->mutex_inner);
        rwlock->writer_waiting_count--;

        // mark the lock as writer lock
        rwlock->lock_state = -1;

        mutex_unlock(&rwlock->mutex_inner);
    }
}

/** @brief Unlock rwlock
 *
 *  Whether the type of rwlock is a read lock or write lock, calling this
 *  function marks the end of the locked state.
 *  
 *  @param rwlock The rwlock to release lock
 *
 *  @return void
 */
void rwlock_unlock( rwlock_t *rwlock ) {
    mutex_lock(&rwlock->mutex_inner);

    if (rwlock->lock_state == -2) {
        panic("readers/writers lock %p has already been destroied!", rwlock);
    }

    while (rwlock->lock_state == 0) {
        lprintf("try to unlock an unlocked rwlock %p, "
                "will wait until it is locked", rwlock);
        printf("try to unlock an unlocked rwlock %p, "
                "will wait until it is locked\n", rwlock);
        mutex_unlock(&rwlock->mutex_inner);
        yield(-1);
        mutex_lock(&rwlock->mutex_inner);
    }

    // for writer lock (lock_state < 0), release rwlock is lock_state++
    // for reader lock (lock_state > 0). release rwlock is lock_state--
    rwlock->lock_state = (rwlock->lock_state>0) ? 
        (rwlock->lock_state-1) : 
        (rwlock->lock_state+1);

    // if lock_state == 0, it is available for other waiting threads 
    if (rwlock->lock_state == 0) {
        // if some writers are waiting, give the rwlock to writer(favor writer)
        if (rwlock->writer_waiting_count > 0) 
            cond_signal(&rwlock->cond_writer);
        else
            // no writer is waiting, all readers can share the lock
            cond_broadcast(&rwlock->cond_reader);
    }

    mutex_unlock(&rwlock->mutex_inner);
}

/** @brief Destory rwlock
 *
 *  @param rwlock The rwlock to deactivate
 *
 *  @return void
 */
void rwlock_destroy( rwlock_t *rwlock ) {
    mutex_lock(&rwlock->mutex_inner);

    if (rwlock->lock_state == -2) {
        panic("readers/writers lock %p has already been destroied!", rwlock);
    }

    // It's illegal to invoke this function while the lock is held or
    // while other threads are waiting on it
    while (rwlock->lock_state != 0 || 
            rwlock->reader_waiting_count != 0 || 
            rwlock->writer_waiting_count != 0) {
        lprintf("Destroy rwlock %p failed, rwlock is locked, "
                "will try again...", rwlock);
        printf("Destroy rwlock %p failed, rwlock is locked, "
                "will try again...\n", rwlock);
        mutex_unlock(&rwlock->mutex_inner);
        yield(-1);
        mutex_lock(&rwlock->mutex_inner);
    }

    rwlock->lock_state = -2;

    mutex_unlock(&rwlock->mutex_inner);

    mutex_destroy(&rwlock->mutex_inner);
    cond_destroy(&rwlock->cond_reader);
    cond_destroy(&rwlock->cond_writer);
}

/** @brief Downgrade rwlock
 *
 *  @param rwlock The rwlock to downgrade, must be locked in RWLOCK_WRITE mode
 *  
 *  When the function returns, no threads hold the lock in RWLOCK_WRITE mode;
 *  the invoking thread, and possibly some other threads, hold the lock in
 *  RWLOCK_READ mode; previously blocked or newly arriving writers must still 
 *  wait for the lock to be released entirely.
 *
 *  @return void
 */
void rwlock_downgrade( rwlock_t *rwlock) {
    mutex_lock(&rwlock->mutex_inner);
    if (rwlock->lock_state != -1){
        // illegal
        panic("readers/writers lock %p cannot be downgraded while not locked",
                rwlock);
    }
    // downgrade lock from writer lock to reader lock
    rwlock->lock_state = 1;
    // other readers may share the rwlock
    cond_broadcast(&rwlock->cond_reader);
    mutex_unlock(&rwlock->mutex_inner);
}

