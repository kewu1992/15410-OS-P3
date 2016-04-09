/** @file sem.c
 *
 *  @brief This file contains implementation of semaphore built
 *  on top of mutex and condition variables
 *  
 *  The semaphore contains three fields: a mutux to guard access to changes to
 *  its other fields, a condition variable to wait and signal threads according
 *  to available resources, and a counter indicating the number of resources 
 *  availbale.
 *  
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug None known
 */

#include <sem.h>
#include <assert.h>

/** @brief Initialize semaphore
 *  
 *  @param sem The semaphore to initialize
 *  @param count The value to initialize sem to
 *
 *  @return 0 on success; -1 on error
 */
int sem_init(sem_t *sem, int count) {
    if((mutex_init(&sem->mutex) < 0) ||
            (cond_init(&sem->cond) < 0)) {
        return -1;
    }

    if(count < 0) {
        return -1;
    }

    sem->count = count; 

    return 0;
}

/** @brief Decrement a semaphore value
 *  
 *  May block indefinitely until it is legal to perfom the decrement
 *
 *  @param sem The semaphore to decrement value
 *
 *  @return void
 */
void sem_wait(sem_t *sem) {
    mutex_lock(&sem->mutex);

    if(sem->count > 0) {
        sem->count--;
    } else {
        if (sem->count < 0) {
            panic("semaphore %p has already been destroied!", sem);
        }

        // sem->count is 0, should block
        while(sem->count == 0) {
            cond_wait(&sem->cond, &sem->mutex);
            // Waked up, but others calling sem_wait may have made 
            // sem->count 0 again, may need to block again
        }
        sem->count--;
    }

    mutex_unlock(&sem->mutex);
}

/** @brief Increment a semaphore value
 *  
 *  Wake up a thread waiting on the semaphore if there exists one
 *
 *  @param sem The semaphore to increment value
 *
 *  @return void
 */
void sem_signal(sem_t *sem) {
    mutex_lock(&sem->mutex);
    
    if (sem->count < 0) {
        panic("semaphore %p has already been destroied!", sem);
    }

    sem->count++;

    mutex_unlock(&sem->mutex);

    // Wake up one
    cond_signal(&sem->cond);

}

/** @brief Deactivate a semaphore
 *  
 *  @param sem The semaphore to deactivate
 *
 *  @return void
 */
void sem_destroy(sem_t *sem) {

    mutex_lock(&sem->mutex);

    if (sem->count < 0) {
        panic("semaphore %p has already been destroied!", sem);
    }

    sem->count = -1;
    
    mutex_unlock(&sem->mutex);

    mutex_destroy(&sem->mutex);
    cond_destroy(&sem->cond);
}


