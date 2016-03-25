#include <stddef.h>
#include <malloc.h>

#include <malloc_internal.h> // for _malloc series
#include <mutex.h>

static mutex_t lock;

int malloc_init() {
    return mutex_init(&lock);
}

/* safe versions of malloc functions */
void *malloc(size_t size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _malloc(size);
    mutex_unlock(&lock);
    return rv;
}

void *memalign(size_t alignment, size_t size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _memalign(alignment, size);
    mutex_unlock(&lock);
    return rv;
}

void *calloc(size_t nelt, size_t eltsize)
{
    void* rv;
    mutex_lock(&lock);
    rv = _calloc(nelt, eltsize);
    mutex_unlock(&lock);
    return rv;
}

void *realloc(void *buf, size_t new_size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _realloc(buf, new_size);
    mutex_unlock(&lock);
    return rv;
}

void free(void *buf)
{
    mutex_lock(&lock);
    _free(buf);
    mutex_unlock(&lock);
}

void *smalloc(size_t size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _smalloc(size);
    mutex_unlock(&lock);
    return rv;
}

void *smemalign(size_t alignment, size_t size)
{
    void* rv;
    mutex_lock(&lock);
    rv = _smemalign(alignment, size);
    mutex_unlock(&lock);
    return rv;
}

void sfree(void *buf, size_t size)
{
    mutex_lock(&lock);
    _sfree(buf, size);
    mutex_unlock(&lock);
}



