/** @file asm_helper.h
 *
 *  @brief This file contains interfaces for some assembly helper functions 
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _ASM_HELPER_H_
#define _ASM_HELPER_H_

#include <stdint.h>

/** @brief Get the current value of %esp
 *  @return The value of %esp
 */
uint32_t asm_get_esp();

/** @brief Get the current value of %ebp
 *  @return The value of %ebp
 */
uint32_t asm_get_ebp();

/** @brief Get the current value of %cs
 *  @return The value of %cs
 */
uint32_t asm_get_cs();

/** @brief Pop all generic registers except %esp and %eax from current stack
 *  
 *  Note that although only 6 values are poped from the stack, it actually pops
 *  7*4 bytes of memory space. To obey stack discipline, an extra return address 
 *  will also be poped. This function should be used with asm_push_generic() 
 *  correspondingly.
 */
void asm_pop_generic();

/** @brief Push all generic registers except %esp and %eax to current stack
 *  
 *  Note that although only 6 values are pushed to the stack, it actually costs
 *  7*4 bytes of memory space. To obey stack discipline, an extra return address 
 *  will also be pushed. This function should be used with asm_pop_generic() 
 *  correspondingly.
 */
void asm_push_generic();

/** @brief Pop all data segment selectors from current stack
 *  
 *  Note that although only 4 values are poped from the stack, it actually pops
 *  5*4 bytes of memory space. To obey stack discipline, an extra return address 
 *  will also be poped. This function should be used with asm_push_ss() 
 *  correspondingly.
 */
void asm_pop_ss();

/** @brief Push all data segment selectors to current stack
 *  
 *  Note that although only 4 values are pushed to the stack, it actually costs
 *  5*4 bytes of memory space. To obey stack discipline, an extra return address 
 *  will also be pushed. This function should be used with asm_pop_ss() 
 *  correspondingly.
 */
void asm_push_ss();

/** @brief Set all data segment selectors to SEGSEL_KERNEL_DS */
void asm_set_ss();

/** @brief Using bsf instruction to search the parameter for the least 
 *         significant set bit (1 bit).
 * 
 *  @param value The value that will be searched for the least significant set 
 *               bit 
 *
 *  @return The bit index of the least significant set bit. If the parameter
 *          equals to zero, the return value is undefined.
 */
int asm_bsf(uint32_t value);

#endif
