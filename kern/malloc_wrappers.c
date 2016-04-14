/** @file malloc_wrapper.c
 *  @brief This file contains the wrapper for malloc library to provide a
 *  thread-safe version of malloc library.
 *
 *  @author Ke Wu (kewu)
 *  @bug No known bug
 */

#include <stddef.h>
#include <malloc.h>

#include <malloc_internal.h> // for _malloc series
#include <mutex.h>
#include <simics.h>

/** @brief Lock that protects mutex library */
static mutex_t lock;

/** @brief Init malloc library 
 *
 *  @return 0 on success; a negative integer on error
 *
 */
int malloc_init() {
    return mutex_init(&lock);
}


/** @brief Thread-safe version of _malloc
 *
 *  @param size The first arg of _malloc
 *  @return _malloc's return value
 *
 */
void *malloc(size_t size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _malloc(size);
    mutex_unlock(&lock);
    return rv;
}

/** @brief Thread-safe version of _memalign
 *
 *  @param alignment The first arg of _memalign
 *  @param size The second arg of _memalign
 *  @return _memalign's return value
 *
 */
void *memalign(size_t alignment, size_t size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _memalign(alignment, size);
    mutex_unlock(&lock);
    return rv;
}

/** @brief Thread-safe version of _calloc
 *
 *  @param nelt The first arg of _calloc
 *  @param eltsize The second arg of _calloc
 *  @return _calloc's return value
 *
 */
void *calloc(size_t nelt, size_t eltsize)
{
    void* rv;
    mutex_lock(&lock);
    rv = _calloc(nelt, eltsize);
    mutex_unlock(&lock);
    return rv;
}

/** @brief Thread-safe version of _realloc
 *
 *  @param buf The first arg of _realloc
 *  @param new_size The second arg of _realloc
 *  @return _realloc's return value
 *
 */
void *realloc(void *buf, size_t new_size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _realloc(buf, new_size);
    mutex_unlock(&lock);
    return rv;
}

/** @brief thread-safe version of _free
 *
 *  @param buf the first arg of _free
 *  @return void
 *
 */
void free(void *buf)
{
    mutex_lock(&lock);
    _free(buf);
    mutex_unlock(&lock);
}

/** @brief Thread-safe version of _smalloc
 *
 *  @param size The first arg of _smalloc
 *  @return _smalloc's return value
 *
 */
void *smalloc(size_t size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _smalloc(size);
    mutex_unlock(&lock);
    return rv;
}

/** @brief Thread-safe version of _smemalign
 *
 *  @param alignment The first arg of _smemalign
 *  @param size The second arg of _smemalign
 *  @return _smemalign's return value
 *
 */
void *smemalign(size_t alignment, size_t size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _smemalign(alignment, size);
    mutex_unlock(&lock);
    return rv;
}

/** @brief Thread-safe version of _sfree
 *
 *  @param buf The first arg of _sfree
 *  @param size The second arg of _sfree
 *  @return _sfree's return value
 *
 */
void sfree(void *buf, size_t size)
{
    mutex_lock(&lock);
    _sfree(buf, size);
    mutex_unlock(&lock);
}

/** @brief Get malloc library's lock
 *
 *  @return Malloc library's lock
 *
 */
mutex_t *get_malloc_lib_lock() {
    return &lock;
}

