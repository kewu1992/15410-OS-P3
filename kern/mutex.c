/** @file mutex.c
 *  @brief Implementation of mutex
 *
 *  mutex_t contains the following fields
 *     1. lock_holder: it is an integer to indicate the holder of the mutex 
 *        lock, -1 means mutex is available, -2 means mutex is destroied. A
 *        non-negative number means mutex is locked by someone, the number is 
 *        the tid of the thread that holding the mutex.
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
 *  Note that to avoid using malloc() (which will be protected by mutex after 
 *  mutex is implemented...), the stack space of blocked threads is used to 
 *  construct node for queue. Because the stack of the procedure call will not 
 *  be destroied until the blocked thread get the mutex, so it is safe.
 *
 *  A thread that can not get the mutex immediately when calling mutex_lock() 
 *  will be blocked on the mutex and will not be scheduled by scheduler until 
 *  the mutex holder give the lock to the blocked thread and make it runnable. 
 *  
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
    is_error |= simple_queue_init(&mp->deque);
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

    
    if  (mp->lock_holder != -1 || 
        simple_queue_destroy(&mp->deque) < 0) {
        // illegal, mutex is locked or some threads are waiting for the mutex
        // this is impossible in our implementation of kernel
        panic("Destroy mutex %p failed", mp);
    }

    mp->lock_holder = -2;

    spinlock_unlock(&mp->inner_lock);
}

/** @brief Lock mutex
 *  
 *  A thread will gain exclusive access to the region
 *  after this call if it successfully acquires the lock
 *  until it calles mutex_unlock(); or, it will block until
 *  it gets the lock if other thread is holding the lock
 *
 *  @param mp The mutex to lock
 *
 *  @return void
 */
void mutex_lock(mutex_t *mp) {
    tcb_t* thr = tcb_get_entry((void*)asm_get_esp());

    spinlock_lock(&mp->inner_lock);
    if (mp->lock_holder == -2) {
        // try to lock a destroied mutex
        panic("mutex %p has already been destroied!", mp);
    }

    if (mp->lock_holder == -1) {
        // mutex is unlocked, get the mutex lock directly and set it to locked
        mp->lock_holder = thr->tid;
        spinlock_unlock(&mp->inner_lock);
    } else {
        simple_node_t node;
        node.thr = thr;

        // mutex is locked, enter the tail of queue to wait, note that stack
        // memory is used for queue_node. Because the stack of mutex_lock()
        // will not be destroied until this thread get the mutex, so it is safe
        simple_queue_enqueue(&mp->deque, &node);

        spinlock_unlock(&mp->inner_lock);

        // while this thread doesn't get the mutex, block itself
        // in our implementation, this while loop should only loop once
        while(mp->lock_holder != thr->tid) {
            context_switch(OP_BLOCK, 0);
        }
    }
}

/** @brief Try to lock mutex
 *  
 *  Try to lock a mutex. If the mutex is unlocked, then the invoking thread will
 *  get the mutex lock immediately. However, unlike mutex_lock(), if the mutex
 *  is locked by other thread, the thread that invoking mutex_try_lock() will
 *  not be blocked, instead it will return with a failure value. 
 *
 *  @param mp The mutex to try to lock
 *
 *  @return Return zero if the thread get the mutex successfully, return -1 if
 *          the mutex is unavailable and the thread didn't get the mutex.
 */
int mutex_try_lock(mutex_t *mp) {
    tcb_t* thr = tcb_get_entry((void*)asm_get_esp());

    int rv;

    spinlock_lock(&mp->inner_lock);
    if (mp->lock_holder == -2) {
        // try to lock a destroied mutex
        panic("mutex %p has already been destroied!", mp);
    }

    if (mp->lock_holder == -1) {
        // mutex is unlocked, get the mutex lock directly and set it to locked
        mp->lock_holder = thr->tid;
        rv = 0;
    } else {
        // mutex is locked, just return failed
        rv = -1;
    }
    spinlock_unlock(&mp->inner_lock);

    return rv;
}

/** @brief Unlock mutex
 *  
 *  A thread's exclusive access to the critical region will be lost after this 
 *  call and other threads awaiting the lock will have a chance to gain the 
 *  lock.
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
        panic("try to unlock an unlocked mutex %p", mp);
    }

    simple_node_t* node = simple_queue_dequeue(&mp->deque);

    if (node == NULL) {
        // no thread is waiting on the mutex, set mutex as available 
        mp->lock_holder = -1;
    } else {
        // some threads are waiting the mutex, hand over the lock to the thread
        // in the head of queue
        mp->lock_holder = ((tcb_t *)(node->thr))->tid;
    }

    spinlock_unlock(&mp->inner_lock);

    if (node != NULL) {
        // Make runnable the blocked thread in the head of queue (and now it is
        // the holder of the mutex, so it can make progress when it is running)
        context_switch(OP_MAKE_RUNNABLE, (uint32_t)node->thr);
    }
}
