#include <simics.h>
#include <vm.h>

/** @brief System call handler for new_pages()
 *
 *  This function will be invoked by new_pages_wrapper().
 *
 *  Allocates new memory to the invoking task, starting at base and extending 
 *  for len bytes.
 *
 *  @param base The starting address for new pages, it should be page-aligned
 *  @param len The length of bytes to be allocated, it should be multiple of
 *             the system page size.
 *
 *  @return On success, return zero.
 *          On error, returning a negative integer error code.
 *          1) If base is not page-aligned or if len is not a positive integral 
 *             multiple of the system page size, EINVAL will be returned.
 *          2) If any portion of the region represents memory already in the 
 *             task’s address space, EALLOCATED will be returned.
 *          3) If any portion of the region intersects a part of the address 
 *             space reserved by the kernel, EFAULT will be returned.
 *          4) If the operating system has insufficient resources to satisfy the
 *             request, ENOMEM will be returned. 
 */
int new_pages_syscall_handler(void *base, int len) {
    
    //lprintf("new_pages base: %x", (unsigned)base);
    int ret = new_pages(base, len);
    if (ret == ERROR_BASE_NOT_ALIGNED || ERROR_LEN)
        ret = EINVAL;
    else if (ret == ERROR_KERNEL_SPACE)
        ret = EFAULT;
    else if (ret == ERROR_OVERLAP)
        ret = EALLOCATED;
    else if (ret == ERROR_MALLOC_LIB || ERROR_NOT_ENOUGH_MEM)
        ret = ENOMEM;
    return ret;
}

/** @brief System call handler for remove_pages()
 *
 *  This function will be invoked by remove_pages_wrapper().
 *
 *  Deallocates the specified memory region, which must presently be allocated 
 *  as the result of a previous call to new pages() which specified the same 
 *  value of base.
 *
 *  @param base The starting address for remove pages
 *
 *  @return Returns zero if successful or returns a negative integer failure 
 *          code.
 */
int remove_pages_syscall_handler(void *base) {
    
    //lprintf("remove_pages base: %x", (unsigned)base);
    int ret = remove_pages(base);
    if (ret < 0)
        ret = EINVAL;
    return ret;
}

