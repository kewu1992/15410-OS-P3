/** @file spinlock.c
 *
 *  @brief Constains the implementation of spinlock
 *  
 *  Using disable_interrupts() and xchg instruction to achieve a simple 
 *  spinlock. 
 *
 *  This implementation of spinlock can be used in either uni-processor or 
 *  multi-processor environment. disable_interrupts() guarantees that no 
 *  interrupt from the same processor will happen when spinlock is locked. 
 *  Besides, xchg instruction and while loop are used to prevent threads 
 *  of other processors from accessing the critical section. 
 *
 *  Because interrupts are disabled and while loop is used, spinlock should not
 *  be locked in a very long time. Normally, spinlock is supposed
 *  to protect a few lines of code (e.g. the implementation of mutex) or very
 *  tricky data structure (e.g. queue of scheduler during context switch). 
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug No known bugs
 */

#include <spinlock.h>
#include <asm.h>
#include <asm_atomic.h>
#include <smp.h>

/** @brief Init spin lock
 *  
 *  @param lock The lock to init
 *  @return 0
 */
int spinlock_init(spinlock_t* lock) {
    lock->available = 1;
    lock->waiting[0] = 0;
    lock->waiting[1] = 0;
    return 0;
}

/** @brief Lock a spinlock
 *  
 *  @param lock The lock to lock
 *  @return void
 */
void spinlock_lock(spinlock_t* lock) {
    disable_interrupts();

    int cpu_id = (smp_get_cpu() == 0) ? 0 : 1;

    lock->waiting[cpu_id] = 1;

    while (lock->waiting[cpu_id] && !asm_xchg(&lock->available, 0)) 
       continue;

    lock->waiting[cpu_id] = 0;
}

/** @brief Unlock a spinlock
 *  
 *  @param lock The lock to unlock
 *  @return void
 */
void spinlock_unlock(spinlock_t* lock) {
    int cpu_id = (smp_get_cpu() == 0) ? 0 : 1;

    if (lock->waiting[1-cpu_id])
        lock->waiting[1-cpu_id] = 0;
    else
        asm_xchg(&lock->available, 1);

    enable_interrupts();
}

/** @brief Destroy a spinlock
 *  
 *  @param lock The lock to destroy
 *  @return void
 */
void spinlock_destroy(spinlock_t* lock) {
    asm_xchg(&lock->available, 0);
}