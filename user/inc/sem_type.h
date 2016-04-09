/** @file sem_type.h
 *  @brief This file defines the type for semaphores.
 */

#ifndef _SEM_TYPE_H
#define _SEM_TYPE_H

#include <mutex.h>
#include <cond.h>
#include <stddef.h>

/** @brief semaphore type */ 
typedef struct sem {
    /** @brief A mutux to guard access to changes to cond and count */
    mutex_t mutex;
    /** @brief A condition variable to wait and signal threads according 
     * to the available count. 
     */
    cond_t cond;
    /** @brief A counter indicating the number of resources availbale */
    int count;
} sem_t;

#endif /* _SEM_TYPE_H */
