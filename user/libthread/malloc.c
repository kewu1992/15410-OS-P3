/** @file malloc.c
 *  @brief Wrapper for malloc lib
 *
 *  @author Ke Wu (kewu)
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <stdlib.h>
#include <types.h>
#include <stddef.h>

#include <spinlock.h>

/** @brief Mutex to guard malloc library */
spinlock_t mutex_malloc;

/** @brief Initialize malloc lib
 *  
 *  @return 0 on success
 */
int malloc_init() {
    SPINLOCK_INIT(&mutex_malloc);
    return 0;
}

/** @brief Wrapper for malloc syscall
 *  
 *  @param __size Parameter 1 of malloc syscall
 *
 *  @return Return value of malloc syscall
 */
void *malloc(size_t __size)
{
    SPINLOCK_LOCK(&mutex_malloc);
    void *ret = _malloc(__size);
    SPINLOCK_UNLOCK(&mutex_malloc);

    return ret;
}

/** @brief Wrapper for calloc syscall
 *  
 *  @param __nelt Parameter 1 of calloc syscall
 *  @param __eltsize Parameter 2 of calloc syscall
 *
 *  @return Return value of calloc syscall
 */
void *calloc(size_t __nelt, size_t __eltsize)
{
    SPINLOCK_LOCK(&mutex_malloc);
    void *ret = _calloc(__nelt, __eltsize);
    SPINLOCK_UNLOCK(&mutex_malloc);

    return ret;
}

/** @brief Wrapper for realloc syscall
 *  
 *  @param __buf Parameter 1 of realloc syscall
 *  @param __new_size Parameter 2 of realloc syscall
 *
 *  @return Return value of realloc syscall
 */
void *realloc(void *__buf, size_t __new_size)
{
    SPINLOCK_LOCK(&mutex_malloc);
    void *ret = _realloc(__buf, __new_size);
    SPINLOCK_UNLOCK(&mutex_malloc);

    return ret;
}

/** @brief Wrapper for free syscall
 *  
 *  @param __buf Parameter 1 of free syscall
 *
 *  @return void
 */
void free(void *__buf)
{
    SPINLOCK_LOCK(&mutex_malloc);
    _free(__buf);
    SPINLOCK_UNLOCK(&mutex_malloc);
}

