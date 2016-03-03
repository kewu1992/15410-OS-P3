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

// Page alignment mask
#define PAGE_ALIGN_MASK ((unsigned int) ~((unsigned int) (PAGE_SIZE-1)))

// Page table entry type
typedef uint32_t pte_t;
// Page directory entry type
typedef uint32_t pde_t;

// Page table type
typedef struct {
    pte_t pte[PAGE_SIZE/4];
} pt_t;

// Page directory type
typedef struct {
    pde_t pde[PAGE_SIZE/4];
} pd_t;

// Bit index into page-directory entry and page-table entry
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

#define SET_BIT(a, n) ((a) |= ((1) << (n)))
#define CLR_BIT(a, n) ((a) &= ~((1) << (n)))

#define GET_PD_INDEX(va) ((va) >> 22)
#define GET_PT_INDEX(va) (((va) << 10) >> 22)

// Init paging
// default, the entire kernel 16 MB will be mapped
int init_vm();

uint32_t get_pd();

int new_region(uint32_t va, int size_bytes, int rw_perm);

// Set region permission as read-only
// Return 0 on success, -1 on error
int set_region_ro(uint32_t va, int size_bytes);

#endif


