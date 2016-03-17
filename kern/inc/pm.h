/** @file pm.h 
 *
 *  @brief Header file for pm.c
 *
 */
#ifndef _PM_H_
#define _PM_H_

#include <page.h> // For PAGE_SIZE
// #include <asm.h> // For idt_base();
// #include <idt.h> // For IDT_PF
#include <cr.h> // For %cr
#include <simics.h> // For lprintf
#include <malloc.h> // For smemalign

#include <string.h> // For memset
// #include <seg.h> // For SEGSEL_KERNEL_CS

#include <common_kern.h>
// #include <cr.h>

#include <list.h>

#define MAX_ORDER 11

#define TRUE 1
#define FALSE 0

#ifndef ERROR_MALLOC_LIB
#define ERROR_MALLOC_LIB (-1)
#endif

#ifndef ERROR_NOT_ENOUGH_MEM 
#define ERROR_NOT_ENOUGH_MEM 3
#endif



struct free_area_struct {
    /* @brief a doubly linked list of blocks */
    list_t list;
};


/***** Core physical memory allocator API ****/
uint32_t get_frames_raw(int order);


/***** Wrapper for allocator's core API *****/
int init_pm();
// The outside world should call to get new frames
int get_frames(int count, list_t *list);
int free_contiguous_frames(uint32_t base, int count);

// For debugging
void traverse_free_area();
void test_frames();


#endif



