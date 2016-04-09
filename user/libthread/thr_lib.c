/** @file thr_lib.c
 *  @brief This file contains implementation of thread management library 
 *
 *  This file contains thread management library including thr_init(), 
 *  thr_create(), thr_join(), thr_exit(), thr_getid(), thr_getktid(), 
 *  thr_yield(). Some functions will lock the entire arraytcb data strcuture to
 *  avoid race condition although we have figured out that probably it is better
 *  to allocate a mutex lock for each tcb structure to support more concurrency.
 *
 *  @bug No known bug
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <thread.h>
#include <mutex.h>
#include <cond.h>
#include <thr_lib_helper.h>
#include <thr_internals.h>
#include <arraytcb.h>
#include <hashtable.h>

/** @brief The initial size of arraytcb */
#define INIT_THR_NUM 32

/** @brief The size of hash table to stroe exit status */
#define EXIT_HASH_SIZE 1021

/** @brief The size of page_remove_info array */
#define PAGE_REMOVE_INFO_SIZE 6 

/** @brief It is an array for any threads to store which pages to remove 
 *         when they invoke thr_exit(). More info please refer to 
 *         thr_lib_helper.c get_pages_to_remove(). */
static int page_remove_info[PAGE_REMOVE_INFO_SIZE];

/** @brief The amount of stack space available for each thread */
static unsigned int stack_size;

/** @brief Number of threads created */
static unsigned int thread_count;

/** @brief Mutex to guard when calculating new stack positions */
static mutex_t mutex_thread_count;

/** @brief Mutex to protect arraytcb */
static mutex_t mutex_arraytcb;

/** @brief Hash table to store exit status of exiting thread */
static hashtable_t hash_exit;

/** @brief Initialize the thread library
 *
 *  @param size The amount of stack space which will be available for each 
 *              thread using the thread library
 *  @return On success return zero, on error return a negative number
 */
int thr_init(unsigned int size) {
    // From single thread program transfrom to multi-thread program, 
    // change the return address or main().
    set_rootthr_retaddr();

    stack_size = size;

    // At first, already has a master thread
    thread_count = 1;

    int is_error = 0;

    is_error |= mutex_init(&mutex_thread_count);

    is_error |= mutex_init(&mutex_arraytcb);

    is_error |= arraytcb_init(INIT_THR_NUM);

    is_error |= thr_lib_helper_init(stack_size);

    is_error |= thr_hashtableexit_init();

    // insert master thread to arraytcb
    is_error |= arraytcb_insert_thread(0, &mutex_arraytcb);
    // set ktid for master thread
    is_error |= arraytcb_set_ktid(0, gettid());

    return is_error ? -1 : 0;
}

/** @brief Creates a new thread to run func(args)
 *  
 *  This function will create a thread (a register set and a stack) to
 *  run func(args). 
 *
 *  @param func The address of function for new thread to run
 *  @param args The argument that passed to the function 
 *              for new thread to run  
 *  @return On success the thread ID of the new thread is returned, on error
 *          a negative number is returned
 */
int thr_create(void *(*func)(void *), void *args) {
    // calculate thread id
    mutex_lock(&mutex_thread_count);
    int tid = thread_count++;
    mutex_unlock(&mutex_thread_count);

    uint32_t stack_addr = 0;
    
    int index = arraytcb_insert_thread(tid, &mutex_arraytcb);
    if(index == -1) return -1;

    // allocate a stack with stack_size for new thread
    if ((stack_addr = (uint32_t)get_new_stack_top(index)) 
            % ALIGNMENT != 0){
        // return value can not be divided by ALIGNMENT, it is an error 
        return -1;
    }

    // "push" argument to new stack  
    memcpy((void*)(stack_addr-4), &args, 4);

    // "push" ktid to new stack --> will do in thr_create_kernel()

    // "push" stack index to new stack  
    memcpy((void*)(stack_addr-12), &index, 4);

    // create a new thread, tell it where it should start running (eip), and
    // its stack address (esp)
    int child_ktid;
    if ((child_ktid = thr_create_kernel(func, (void*)(stack_addr-12))) < 0) {
        // thread_fork error
        return -1;
    }

    /* Set ktid for newly created thread. arraytcb_set_ktid() will be called 
     * twice, one in thr_create() which is here and one in thr_create_kernel() 
     * to make sure ktid is set for the new thread before any thread need the 
     * info. When set ktid here, it must check if the stack belongs to the new 
     * thread when the code is executing because the newly created thread may 
     * already died and another thread is using the stack.
     */
    mutex_lock(&mutex_arraytcb);
    tcb_t *thr = arraytcb_get_thread(index);
    if (thr && thr->tid == tid)
        arraytcb_set_ktid(index, child_ktid);
    mutex_unlock(&mutex_arraytcb);

    return tid;
}

/** @brief Join and clean up a thread
 *  
 *  This function joins a thread, if the thread is running, block
 *  and wait for it; else, look up its return status in the exit 
 *  hashtable and clean up the hashtable entry.
 * 
 *  @param tid The thread id (assigned by our thread lib) to join on
 *  @param statusp The place to store return status of the thread to join 
 *
 *  @return 0 on success; -1 on error
 *
 */
int thr_join(int tid, void **statusp) {
    // check if tid has been created 
    mutex_lock(&mutex_thread_count);
    if (tid >= thread_count){
        mutex_unlock(&mutex_thread_count);
        return -1;
    }
    mutex_unlock(&mutex_thread_count);

    // try to find the tcb
    mutex_lock(&mutex_arraytcb);
    tcb_t* thr = arraytcb_find_thread(tid);
    if (thr) {
        // tid can be found in arraytcb, it is still running
        switch(thr->state){
        case JOINED:
            // tid has been joined by other thread
            mutex_unlock(&mutex_arraytcb);
            return -1;
        case RUNNING:
            // tid is still running, block and waiting for it
            thr->state = JOINED;
            cond_wait(&thr->cond_var, &mutex_arraytcb);
            break;
        default:
            // tcb state error
            return -1;;
        } 
    }

    // thread of tid has exitted
    mutex_unlock(&mutex_arraytcb);

    // try to find exit status of tid in hash table
    int is_find;
    if (statusp)
        *statusp = hashtable_remove(&hash_exit, (void*)tid, &is_find);
    else
        hashtable_remove(&hash_exit, (void*)tid, &is_find);

    if (is_find) {
        return 0;
    } else {
        // Can not find exit status of tid in hash table, might already been 
        // reaped by other thread
        return -1;
    }
   
}

/** @brief Exits the thread with exit status
 *  
 *  Report exit status in exit hashtable, delete the its tcb, 
 *  release its stack space and call vanish().
 * 
 *  @param status The return status
 *
 *  @return void
 *
 */
void thr_exit(void *status) {
    // Get stack position index of the current thread
    int index = get_stack_position_index();

    // get my tcb
    tcb_t *thr = arraytcb_get_thread(index);
    if(thr == NULL) {
        // Something's wrong
        panic("thr_exit() failed, can not find tcb, something's wrong");
    }
    
    // put exit status to hash table for future reaping
    hashtable_put(&hash_exit, (void*)(thr->tid), status);

    mutex_lock(&mutex_arraytcb);

    // check if some threads has called join on it
    if(thr->state == JOINED) {
        // Signal the thread who called join
        cond_signal(&thr->cond_var);
    } 

    // release resource
    if(arraytcb_delete_thread(index) < 0) {
        panic("thr_exit() failed, can not delete tcb of %d", thr->tid);
    }


    /* The following code is executing 
     *      mutex_unlock(&mutex_arraytcb);
     *      remove_page();
     *      vanish();
     * However, instead of calling these fucntions directly, 
     * the program will call them "manually" with assembly to avoid
     * using stack after mutex is unlocked. Because after mutex is 
     * unlocked, other threads may try to use the stack of the exitting thread
     * immediately. Also remove_page() will be called before vanish() so
     * stack will become unavailable when calling vanish().
     */

    // call mutex_unlock(mutex_arraytcb) "manually" to avoid "unlocking"
    SPINLOCK_LOCK(&mutex_arraytcb.inner_lock);

    node_t *tmpnode = dequeue(&mutex_arraytcb.deque);

    if (!tmpnode) {
        mutex_arraytcb.lock_available = 1;
    } else {
        tmpnode->reject = 1;
    }

    get_pages_to_remove(index, page_remove_info);

    // will call SPINLOCK_UNLOCK(&mutex_arraytcb->inner_lock), remove_page() abd
    //  vanish() in asm_thr_exit() to avoid using stack
    asm_thr_exit(&mutex_arraytcb.inner_lock, page_remove_info);

    panic("reach a place in thr_exit() that should never be reached");
    return;

}

/** @brief Get calling thread's thread id assigned by the thread lib
 *  
 *  Look up tid in arraytcb
 *
 *  @return tid on success; -1 on error
 *
 */
int thr_getid() {
    // Get stack position index of the current thread
    int index = get_stack_position_index();

    // get my tcb
    tcb_t *thr = arraytcb_get_thread(index);
    if(thr == NULL) {
        // can not find my tcb, something wrong...
        return -1;
    }

    return thr->tid;
}

/** @brief Get calling thread's thread id assigned by the kernel
 *  
 *  Look up ktid in arraytcb
 *
 *  @return ktid on success; -1 on error
 *
 */
int thr_getktid() {
    // Get stack position index of the current thread
    int index = get_stack_position_index();

    // get my tcb
    tcb_t *thr = arraytcb_get_thread(index);
    if(thr == NULL) {
        // can not find my tcb, something wrong...
        return -1;
    }

    return thr->ktid;
}

/** @brief Defers execution of the invoking thread
 *
 *  Yield in favor of tid. If tid is -1, then yield to some unspecified thread
 *
 *  @param tid Thread id (assigned by thread lib) of the thread to yield to.
 *  @return 0 on success; -1 on error
 *
 */
int thr_yield(int tid) {
    if (tid == -1)
        return yield(-1);

    mutex_lock(&mutex_arraytcb);
    tcb_t *thr = arraytcb_find_thread(tid);
    
    if (!thr){
        // tid doesn't exist
        mutex_unlock(&mutex_arraytcb);
        return -1;
    }

    int ktid = thr->ktid;
    mutex_unlock(&mutex_arraytcb);

    return yield(ktid);

}

/** @brief Initialize exit_status hash table
 *  
 *  First set hash table size and hash function.
 *  Then invoke hashtable_init() API to do initialization.
 *
 *  @return On success return 0, on error return a negative number
 */
int thr_hashtableexit_init() {
    hash_exit.size = EXIT_HASH_SIZE;
    hash_exit.func = thr_exitstatus_hashfunc;
    return hashtable_init(&hash_exit);
}

/** @brief The hash function for exit_status hash table
 *  
 *  Exit_status hash table takes tid as key and exit status as value. So this 
 *  hash function simply uses modular to do hashing. Note that this hash 
 *  function is specific for exit_status hash table.
 *  
 *  @param key The key to calculate index of exit_status hash table
 * 
 *  @return The index of exit_status hash table 
 */
int thr_exitstatus_hashfunc(void *key) {
    int tid = (int)key;
    return tid % EXIT_HASH_SIZE;
}

