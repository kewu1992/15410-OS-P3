/** @file spinlock.h
 *
 *  @brief Constains the macro implementation of spinlock
 *  
 *  Using xchg insturction achieve a simple spinlock. 
 *  It simply tries to acqure a lock for a 
 *  limited times until it defers trying through yielding. The design 
 *  choice related to it is whether to yield immediately if a lock is not 
 *  available. In a single core machine, if a lock is not available, then it's 
 *  likely other thread not running is holding the lock, so the current thread 
 *  should yield immediately; in a multi-core machine, the thread that is 
 *  holding the lock may be running as well and is likely to release the lock 
 *  in a short time, so it makes sense for the current thread to try acquring 
 *  for a few times instead of yielding immediately. To adapt to work well in a
 *  multi-threaded environment, our spinlock tries a few times before it yields.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug No known bugs
 */
#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include <thr_internals.h>
#include <syscall.h>

/**@ brief spinlock type */
typedef int spinlock_t;

/**@ brief Maximal number of spinning times trying to acquire a lock */
#define MAX_SPIN_NUM  1000

/**@ brief Initialize spin lock */
#define SPINLOCK_INIT(lock)     *(lock) = 1

/**@ brief Destory spin lock */
#define SPINLOCK_DESTROY(lock)  asm_xchg(lock, 0)

/**@ brief Lock spin lock */
#define SPINLOCK_LOCK(lock)     do { \
    while (1) { \
        int i = 0; \
        while (i<MAX_SPIN_NUM &&   \
                !asm_xchg(lock, 0)) \
        i++; \
        if (i==MAX_SPIN_NUM) \
        yield(-1); \
        else \
        break; \
    } \
} while(0)

/**@ brief Unlock spin lock */
#define SPINLOCK_UNLOCK(lock)   asm_xchg(lock, 1)

#endif /* _SPINLOCK_H */

