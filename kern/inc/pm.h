/** @file pm.h 
 *
 *  @brief Header file for pm.c
 *
 */
#ifndef _PM_H_
#define _PM_H_

#include <page.h> // For PAGE_SIZE
#include <cr.h> // For %cr
#include <simics.h> // For lprintf
#include <malloc.h> // For smemalign
#include <string.h> // For memset
#include <common_kern.h>



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


//typedef struct {
    /* @brief a circular doubly linked list of free blocks */
//    list_t list;
//} free_list_t;

int init_pm();
//int get_frames(int count, list_t *list);
//int free_frames(uint32_t base, int count);
int reserve_frames(int count);
void unreserve_frames(int count);
uint32_t get_frames_raw();
void free_frames_raw(uint32_t base);



// The followings are for debugging, will remove later
//void traverse_free_area();
//void test_frames();


#endif



