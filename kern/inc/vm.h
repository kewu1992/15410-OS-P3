/** @file vm.h 
 *
 *  @brief Header file for vm.c
 *
 *  @author Jian Wang (jianwan3)
 *  @bug No known bugs
 */
#ifndef _VM_H_
#define _VM_H_

#include <page.h> // For PAGE_SIZE
#include <asm.h> // For idt_base();
#include <idt.h> // For IDT_PF
#include <cr.h> // For %cr
#include <simics.h> // For lprintf
#include <page.h> // For PAGE_SIZE
#include <malloc.h> // For smemalign

#include <string.h> // For memset
#include <seg.h> // For SEGSEL_KERNEL_CS

#include <common_kern.h>
#include <cr.h>


/* @brief Page alignment mask */
#define PAGE_ALIGN_MASK ((unsigned int) ~((unsigned int) (PAGE_SIZE-1)))

/* @brief Page directory entry or page table entry size */
#define ENTRY_SIZE 4

/* @brief Number of page tables needed for kernel space */
#define NUM_PT_KERNEL 4

/* @brief Page table entry type */
typedef uint32_t pte_t;

/* @brief Page directory entry type */
typedef uint32_t pde_t;

/* @brief Page table type */
typedef struct {
    pte_t pte[PAGE_SIZE/ENTRY_SIZE];
} pt_t;

/* @brief Page directory type */
typedef struct {
    pde_t pde[PAGE_SIZE/ENTRY_SIZE];
} pd_t;

/******* Bit index in page directory entry or page table entry ******/
#define PG_P 0 
#define PG_RW 1
#define PG_US 2
#define PG_PWT 3
#define PG_PCD 4
#define PG_A 5
#define PG_D 6
#define PG_PS 7
#define PG_PAT 7
#define PG_G 8
// Bits availabe for programmer's use
// Use these 2 of these 3 bits to record start and end of pages
// allocated by new_pages() system call
#define PG_NEW_PAGES_START 9
#define PG_NEW_PAGES_END 10


#define GET_PD_INDEX(va) ((va) >> 22)
#define GET_PT_INDEX(va) (((va) << 10) >> 22)
#define GET_CTRL_BITS(e) (((e) << 20) >> 20)


#define SET_BIT(a, n) ((a) |= ((1) << (n)))
#define CLR_BIT(a, n) ((a) &= ~((1) << (n)))
#define IS_SET(a, n) ((((a) >> (n)) & 1) == 1)


/*********** Memory error types ***********/
#define ERROR_MALLOC_LIB (-1)
#define ERROR_NOT_ENOUGH_MEM 3
#define ERROR_INSUF_RESOURCE (-1)
#define ERROR_BASE_NOT_ALIGNED (-2)
#define ERROR_LEN (-3)
#define ERROR_OVERLAP (-4)
#define ERROR_KERNEL_SPACE (-5)
#define ERROR_BASE_NOT_PREV (-1)




int init_vm();
uint32_t create_pd();
uint32_t clone_pd();
int new_region(uint32_t va, int size_bytes, int rw_perm, 
        int is_new_pages_syscall);
int free_user_space();
int new_pages(void *base, int len);
int remove_pages(void *base);
int is_region_alloc_perm(uint32_t va, int size_bytes, int rw_perm);




// The followings are for debugging, will remove later
void test_vm();

#endif



