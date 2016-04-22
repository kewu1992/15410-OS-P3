/** @file vm.h 
 *
 *  @brief Header file for vm.c
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
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
#include <mem_errors.h>


/** @brief Page alignment mask */
#define PAGE_ALIGN_MASK ((unsigned int) ~((unsigned int) (PAGE_SIZE-1)))

/** @brief Page directory entry or page table entry size */
#define ENTRY_SIZE 4

/** @brief Number of page tables needed for kernel space */
#define NUM_PT_KERNEL 4

/* Bit index in page directory entry or page table entry */
/** @brief The index of present flag */
#define PG_P 0 
/** @brief The index of the bit in charge of read write permission flag */
#define PG_RW 1
/** @brief The index of the privilege level permission flag */
#define PG_US 2
/** @brief The index of the page-level write through flag */
#define PG_PWT 3
/** @brief The index of the page-level disable caching flag */
#define PG_PCD 4
/** @brief The index of the accessed flag */
#define PG_A 5
/** @brief The index of the dirty flag */
#define PG_D 6
/** @brief The index of the page size flag (page directory entry) */
#define PG_PS 7
/** @brief The index of the page table attribute (page table entry) */
#define PG_PAT 7
/** @brief The index of global flag */
#define PG_G 8

/* 3 bits in page table entry "availabe for programmer's use" */
/** @brief One of the bits for "programmer's use" used to mark the start of
  * a region allocated by a new_pages syscall.
  */
#define PG_NEW_PAGES_START 9
/** @brief One of the bits for "programmer's use" used to mark the end of
  * a region allocated by a new_pages syscall.
  */
#define PG_NEW_PAGES_END 10
/** @brief One of the bits for "programmer's use" used to mark the the page
  * as ZFOD.
  */
#define PG_ZFOD 11

/* Bit index in error code when a page fault happens */
/** @brief Reserved bit */
#define PG_RSVD 3


/* Macro to manipulate page directory and page table entry */
/** @brief Get page directory index given a virtual address */
#define GET_PD_INDEX(va) ((va) >> 22)
/** @brief Get page table index given a virtual address */
#define GET_PT_INDEX(va) (((va) << 10) >> 22)
/** @brief Get control bits given a page directory entry or page table entry */
#define GET_CTRL_BITS(e) (((e) << 20) >> 20)

/** @brief Get page virtual address the page table entry points to.
  * i The page diretory entry index
  * j The page table entry index 
  */
#define GET_VA_BASE(i, j) (((i) << 22) | ((j) << 12))

/* Utility macro */
/** @brief Set a given bit */
#define SET_BIT(a, n) ((a) |= ((1) << (n)))
/** @brief Clear a given bit */
#define CLR_BIT(a, n) ((a) &= ~((1) << (n)))
/** @brief Check if a given bit is set */
#define IS_SET(a, n) ((((a) >> (n)) & 1) == 1)


/** @brief Num of page tables per lock, if it's 8, then every 8 consecutive 
  * page tables share a lock 
  */
#define NUM_PT_PER_LOCK 16
/** @brief Num of locks per page directory, if NUM_PT_PER_LOCK is 8, and since
  * there are 1024 entries in a page table, then there're 128 locks for
  * a page directory.
  */
#define NUM_PT_LOCKS_PER_PD (PAGE_SIZE/ENTRY_SIZE/NUM_PT_PER_LOCK)

/** @brief Page table entry type */
typedef uint32_t pte_t;

/** @brief Page directory entry type */
typedef uint32_t pde_t;

/** @brief Page table type */
typedef struct {
    /** @brief Page table entry */
    pte_t pte[PAGE_SIZE/ENTRY_SIZE];
} pt_t;

/** @brief Page directory type */
typedef struct {
    /** @brief Page directory entry */
    pde_t pde[PAGE_SIZE/ENTRY_SIZE];
} pd_t;

int init_vm();
void adopt_init_pd(int cur_cpu);
uint32_t create_pd();
uint32_t clone_pd();
int new_region(uint32_t va, int size_bytes, int rw_perm, 
        int is_new_pages_syscall, int is_ZFOD);
void free_space(uint32_t pd_base, int is_kernel_space, 
        int need_unreserve_frames);
void free_entire_space(uint32_t pd_base, int need_unreserve_frames);
int new_pages(void *base, int len);
int remove_pages(void *base);
int check_mem_validness(char *va, int max_bytes, int is_check_null, 
        int need_writable);
int is_page_ZFOD(uint32_t va, uint32_t error_code, int need_check_error_code);
void dist_kernel_mem();

#endif

