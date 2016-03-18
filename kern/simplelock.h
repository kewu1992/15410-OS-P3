/**
 *  @file simplelock.h
 *
 *  @brief This is a simple lock that context_switch is not involved
 *  
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug No known bugs
 */
#ifndef _SIMPLELOCK_H
#define _SIMPLELOCK_H

#include <asm_helper.h>
#include <context_switch.h>

/**@ brief simplelock type */
typedef int simplelock_t;

/**@ brief Initialize simple lock */
#define SIMPLELOCK_INIT(lock)     *(lock) = 1

/**@ brief Destory simple lock */
#define SIMPLELOCK_DESTROY(lock)  asm_xchg(lock, 0)

/**@ brief Try to lock simple lock */
#define SIMPLELOCK_TRYLOCK(lock)  asm_xchg(lock, 0)

/**@ brief Unlock simple lock */
#define SIMPLELOCK_UNLOCK(lock)   asm_xchg(lock, 1)

#endif /* _SIMPLELOCK_H */

