/** NEED TO CHANGE COMMENT!
 *  @file mutex.c
 *  @brief Implementation of mutex
 *
 *  mutex_t contains the following fields
 *     1. lock_available: it is an integer to indicate if the mutex lock is
 *        available. lock_available == 1 means available (unlocked), 
 *        lock_available == 0 means unavailable (locked).
 *        lock_available == -1 means the mutex is destroied
 *     2. inner_lock: a spinlock to protect critical section of mutex code.
 *     3. deque: a double-ended queue to store the threads that are blocking on
 *        the mutex. The queue is FIFO so first blocked thread will get the 
 *        mutex first.
 *
 *  To achieve bounded waiting, a spinlock and a queue are used. Although 
 *  spinlock itself doesn't satisfy bounded waitting, the critical section 
 *  (which is the code of mutex) that protected by spinlock are guaranteed to 
 *  be short. Becuase only when one thread is in the mutex code need to obtain
 *  the spinlock. It is unlikely that there are always some threads that hold 
 *  the spinlock of mutex. So it is better than we use spinlock (xchg) as 
 *  implementation of mutex directly because we can not predict what the 
 *  critical section mutex is trying to protect. So spinlock help mutex somewhat
 *  approximate bounded waiting.
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <mutex.h>
#include <stdlib.h>
#include <simics.h>
#include <stdio.h>
#include <asm_helper.h>
#include <control_block.h>
#include <context_switcher.h>

/** @brief Initialize mutex
 *  
 *  @param mp The mutex to initiate
 *
 *  @return 0 on success; -1 on error
 */
int mutex_init(mutex_t *mp) {
    mp->lock_holder = -1; 
    int is_error = spinlock_init(&mp->inner_lock);
    is_error |= queue_init(&mp->deque);
    return is_error ? -1 : 0;
}

/** @brief Destroy mutex
 *  
 *  @param mp The mutex to destory
 *
 *  @return void
 */
void mutex_destroy(mutex_t *mp) {
    spinlock_lock(&mp->inner_lock);

    if (mp->lock_holder == -2) {
        // try to destroy a destroied mutex
        panic("mutex %p has already been destroied!", mp);
    }

    
    while (mp->lock_holder != -1) {
        // illegal, mutex is locked
        printf("Destroy mutex %p failed, mutex is locked, "
                "will try again...\n", mp);
        spinlock_unlock(&mp->inner_lock);
        context_switch(-1);
        spinlock_lock(&mp->inner_lock);
    }

    while (queue_destroy(&mp->deque) < 0){
        // illegal, some threads are blocked waiting on it
        printf("Destroy mutex %p failed, some threads are blocking on it, "
                "will try again later...\n", mp);
        spinlock_unlock(&mp->inner_lock);
        context_switch(-1);
        spinlock_lock(&mp->inner_lock);
    }

    mp->lock_holder = -2;

    spinlock_unlock(&mp->inner_lock);
}

/** @brief Lock mutex
 *  
 *  A thread will gain exclusive access to the region
 *  after this call if it successfully acquires the lock
 *  until it calles mutex_unlock; or, it will block until
 *  it gets the lock if other thread is holding the lock
 *
 *  @param mp The mutex to lock
 *
 *  @return void
 */
void mutex_lock(mutex_t *mp) {
    int tid = tcb_get_entry((void*)asm_get_esp())->tid;

    spinlock_lock(&mp->inner_lock);
    if (mp->lock_holder == -2) {
        // try to lock a destroied mutex
        panic("mutex %p has already been destroied!", mp);
    }

    if (mp->lock_holder == -1){
        // mutex is unlocked, get the mutex lock directly and set it to locked
        mp->lock_holder = tid;
        spinlock_unlock(&mp->inner_lock);
    } else {
        // mutex is locked, enter the tail of queue to wait
        while (queue_enqueue(&mp->deque, (void*)tid) < 0) {
            printf("Out of memory when try to lock mutex %p, will try again \
                    later...\n", mp);
            spinlock_unlock(&mp->inner_lock);
            context_switch(-1);
            spinlock_lock(&mp->inner_lock);
        }

        spinlock_unlock(&mp->inner_lock);

        // while this thread doesn't get the mutex, let the thread that grab
        // the mutex to run 
        while(mp->lock_holder != tid) 
            context_switch(mp->lock_holder);
    }
}

/** @brief Unlock mutex
 *  
 *  A thread's exclusive access to the region before this call
 *  till mutex_lock will be lost after this call and other threads
 *  awaiting the lock will have a chance to gain the lock.
 *
 *  @param mp The mutex to unlock
 *
 *  @return void
 */
void mutex_unlock(mutex_t *mp) {
    spinlock_lock(&mp->inner_lock);

    if (mp->lock_holder == -2) {
        // try to unlock a destroied mutex
        panic("mutex %p has already been destroied!", mp);
    }

    
    while (mp->lock_holder == -1) {
        printf("try to unlock an unlocked mutex %p, "
                "will wait until it is locked\n", mp);
        spinlock_unlock(&mp->inner_lock);
        context_switch(-1);
        spinlock_lock(&mp->inner_lock);
    }
    

    if (queue_is_empty(&mp->deque)) {
        // no thread is waiting on the mutex, set mutex as available 
        mp->lock_holder = -1;
    } else {
        // some threads are waiting the mutex, hand over the lock to the thread
        // in the head of queue
        int tid = (int)queue_dequeue(&mp->deque);
        mp->lock_holder = tid;
    }

    spinlock_unlock(&mp->inner_lock);
}
