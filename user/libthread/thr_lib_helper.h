/** @file thr_lib_helper.h
 *  @brief Declares types and functions used by thr_lib_helper.c which do 
 *  stack space management.
 *  
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug None known.
 */

#ifndef _THR_LIB_HELPER_H_
#define _THR_LIB_HELPER_H_

#include <syscall.h>  
#include <simics.h>
#include <stdint.h>

#include <lib_public.h>

/** @brief Index information of the array specifying page remove
 * information for a thread's stack space.
 */
enum page_remove_info_index_enum {
    HIGHEST_PAGE_BASE,
    HIGHEST_PAGE_CAN_REMOVE,
    MIDDLE_PAGES_BASE,
    MIDDLE_PAGES_CAN_REMOVE,
    LOWEST_PAGE_BASE,
    LOWEST_PAGE_CAN_REMOVE
};

/** @brief Get current %esp value */
uint32_t asm_get_esp();
/** @brief Get current %ebp value */

uint32_t asm_get_ebp();
int thr_lib_helper_init(unsigned int size);
uint32_t get_pages_to_remove(int index, int *page_remove_info);
uint32_t get_new_stack_top(int count);
int get_stack_position_index();
void* get_last_ebp(void* ebp);
void set_rootthr_retaddr();

#endif

