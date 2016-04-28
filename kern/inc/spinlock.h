/** @file spinlock.h
 *
 *  @brief Constains the  implementation of spinlock
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug No known bugs
 */
#ifndef _SPINLOCK_H
#define _SPINLOCK_H



/**@ brief spinlock type */
typedef struct {
    int available;
    int waiting[2];
} spinlock_t;

int spinlock_init(spinlock_t* lock);

void spinlock_lock(spinlock_t* lock, int is_disable_interrupt);

void spinlock_unlock(spinlock_t* lock, int is_enable_interrupt);

void spinlock_destroy(spinlock_t* lock);

#endif /* _SPINLOCK_H */

