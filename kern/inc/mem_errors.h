/** @file mem_errors.h 
 *
 *  @brief Contains error type definition for memory management.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs
 */
#ifndef _MEM_ERRORS_H_
#define _MEM_ERRORS_H_

/* Error types used by virtual memory and physical memory */
/** @brief Kernel malloc library fails */
#define ERROR_MALLOC_LIB (-1)
/** @brief Not enough free physical frames */
#define ERROR_NOT_ENOUGH_MEM (-3)
/** @brief Memory address base isn't aligned */
#define ERROR_BASE_NOT_ALIGNED (-7)
/** @brief Memory region length is not valid */
#define ERROR_LEN (-15)
/** @brief Memory region to allocate has some parts overlapped with already 
  * allocated region
  */
#define ERROR_OVERLAP (-31)
/** @brief Memory region references kernel space */
#define ERROR_KERNEL_SPACE (-63)
/** @brief Memory region is read only */
#define ERROR_READ_ONLY (-127)
/** @brief Memory region is not NULL terminated */
#define ERROR_NOT_NULL_TERM (-255)
/** @brief Page table or page itself isn't allocated */
#define ERROR_PAGE_NOT_ALLOC (-511)
/** @brief The base parameter of the remove_pages syscall is not from a 
  * previous call of new_pages syscall.
  */
#define ERROR_BASE_NOT_PREV (-1023)

#endif

