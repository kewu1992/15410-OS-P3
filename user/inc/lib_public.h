/** @file lib_public.h
 *  @brief Declares functions that are public among libraries
 *
 */
#ifndef _LIB_PUBLIC_H
#define _LIB_PUBLIC_H

/**
 * @brief Alignment requirement of memory address for stack operation
 */
#define ALIGNMENT 4

/**
 * @brief Page size alignment mask 
 *
 * If PAGE_SIZE is 4096 (0x00001000), then PAGE_ALIGN_MASK will be 0xfffff000.
 */
#define PAGE_ALIGN_MASK ((unsigned int) ~((unsigned int) (PAGE_SIZE-1)))

/** @brief Error code defined by kernel that OS has insufficient resources */
#define ERROR_NEW_PAGES_INSUFFICIENT_RESOURCE (-1)

/** @brief Error code defined by kernel that any portion of the requested 
 *  region overlap with existing task address space 
 */
#define ERROR_NEW_PAGES_OVERLAP_EXISTING_REGION (-2)

/** @brief Error code indicating new_pages() related errors
  *
  * The value is not aligned so that it wouldn't confuse with valid page 
  * address. The intention of making this error code positive is that in some 
  * functions that return an unsigned memory address, a positive error code 
  * would be handy.
  */
#define ERROR_NEW_PAGES_GENERAL (3)

/** @brief Error code indicating exception handler registration error */
#define ERROR_SWEXN_REGIS (3)

/** @brief Error code indicating memory address misalignment */
#define ERROR_MISALIGNMENT (3)

/* Functions in libautostack for other lib to use */

/** @brief Get root thread stack low */
uint32_t get_root_thread_stack_low();
/** @brief Get root thread stack high */
uint32_t get_root_thread_stack_high();

/* Functions in libthread for other lib to use */

/** @brief Initialized malloc lib */
int malloc_init();

#endif 

