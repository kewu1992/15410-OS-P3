/** @file pm.h 
 *
 *  @brief Contains public interface for physical memory manager.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs
 */
 
#ifndef _PM_H_
#define _PM_H_

#include <page.h> // For PAGE_SIZE
#include <cr.h> // For %cr
#include <simics.h> // For lprintf
#include <malloc.h> // For smemalign
#include <string.h> // For memset
#include <common_kern.h>
#include <mem_errors.h>

int init_pm();
int reserve_frames(int count);
void unreserve_frames(int count);
uint32_t get_frames_raw();
void free_frames_raw(uint32_t base);

#endif



