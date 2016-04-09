/** @file cond_var.c
 *  @brief This file contains the implementation of condition varaible
 *
 *  cond_t contains the following fields
 *     1. mutex: a mutex to protect critical section of condition varaible code.
 *     2. deque: a double-ended queue to store the threads that are blocking on
 *        the condition varaible. The queue is FIFO so first blocked thread will
 *        get be signaled first.
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */
#include <mutex.h>
#include <thread.h>
#include <syscall.h>
#include <cond_type.h>
#include <stdlib.h>
#include <queue.h>
#include <stdio.h>
#include <thr_internals.h>
#include <simics.h>

/** @brief Initialize condition variable
 *  
 *  @param cv Condition variable to initialize
 *
 *  @return 0 on success; -1 on error
 */
int cond_init(cond_t *cv) {
    int is_error = 0;
    is_error |= mutex_init(&cv->mutex);
    is_error |= queue_init(&cv->deque);

    return is_error ? -1 : 0;
}

/** @brief Destory condition variable
 *  
 *  @param cv Condition variable to destroy
 *
 *  @return void
 */
void cond_destroy(cond_t *cv) {
    mutex_lock(&cv->mutex);

    if (!queue_is_active(&cv->deque)) {
        // try to destory a destroied cond_var
        panic("condition variable %p has already been destroied!", cv);
    }

    while (queue_destroy(&cv->deque) < 0) {
        // illegal, some threads are blocked waiting on it
        lprintf("Destroy condition variable %p failed, "
                "some threads are blocking on it, will try again...", cv);
        printf("Destroy condition variable %p failed, "
                "some threads are blocking on it, will try again...\n", cv);
        mutex_unlock(&cv->mutex);
        yield(-1);
        mutex_lock(&cv->mutex);
    }

    mutex_unlock(&cv->mutex);

    mutex_destroy(&cv->mutex);

}

/** @brief Allows a thread to wait for a condition
 *  
 *  @param cv Condition variable to wait on
 *  @param mp The mutex needed to check condition
 *  
 *  @return void
 */
void cond_wait(cond_t *cv, mutex_t *mp) {
    // first allocate node for queue
    node_t *tmp = malloc(sizeof(node_t));
    while (!tmp) {
        lprintf("malloc failed, will try again...");
        printf("malloc failed, will try again...\n");
        yield(-1);
        tmp = malloc(sizeof(node_t));
    }
    tmp->ktid = thr_getktid();
    tmp->reject = 0;

    mutex_lock(&cv->mutex);

    if (!queue_is_active(&cv->deque)) {
        // try to wait on a destroied cond_var
        panic("condition variable %p has already been destroied!", cv);
    }

    enqueue(&cv->deque, tmp);

    mutex_unlock(mp);

    mutex_unlock(&cv->mutex);
    // The while loop is used to guard against inproper "wake ups", reject is 
    // used to indicate if the thread has been dequeued by others
    while(!tmp->reject) {
        if (deschedule(&tmp->reject) < 0) {
            panic("deschedule error of condition variable %p", cv);
        }
    }

    free(tmp);

    mutex_lock(mp);
}

/** @brief Wake up a thread waiting on the condition variable, if one exists
 *  
 *  @param cv Condition variable that a thread may wait on
 *  
 *  @return void
 */
void cond_signal(cond_t *cv) {
    mutex_lock(&cv->mutex);

    if (!queue_is_active(&cv->deque)) {
        // try to singal a destroied cond_var
        panic("condition variable %p has already been destroied!", cv);
    }

    node_t *tmp = dequeue(&cv->deque);
    if (tmp) {
        // if some threads are waiting on the condition varaible, awaken the 
        // thread in the head of the queue
        int tmp_ktid = tmp->ktid;
        tmp->reject = 1;
        make_runnable(tmp_ktid);
    }
    mutex_unlock(&cv->mutex);
}

/** @brief Wake up all threads waiting on the condition variable
 *  
 *  This function will not awaken threads that may invoke cond_wait(cv)
 *  after this call has begun execution, because cond_broadcast(cv) and
 *  cond_broadcast(cv) are guarded by the same mutex cv->mutex on entry.
 *
 *  @param cv Condition variable that threads may wait on
 *  
 *  @return void
 */
void cond_broadcast(cond_t *cv) {
    mutex_lock(&cv->mutex);

    if (!queue_is_active(&cv->deque)) {
        // try to singal a destroied cond_var
        panic("condition variable %p has already been destroied!", cv);
    }

    node_t *tmp = dequeue(&cv->deque);
    while (tmp) {
        int tmp_ktid = tmp->ktid;
        tmp->reject = 1;
        make_runnable(tmp_ktid);
        tmp = dequeue(&cv->deque);
    }
    mutex_unlock(&cv->mutex);
}

