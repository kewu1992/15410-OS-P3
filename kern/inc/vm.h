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


// Size of each idt entry in bytes
#define IDT_ENTRY_SIZE 8

/** @brief A struct representing an 8-byte idt entry */
struct idt_entry_t {
    /** offset 15..0 */
    uint16_t offset_lsw;    
    /** segment selector */
    uint16_t seg_selector;  
    /** unused byte */
    uint8_t byte_unused;    
    /** gate info */
    uint8_t byte_info;      
    /** offset 16..31 */
    uint16_t offset_msw;    
};

// Asm wrapper of page fault handler
void asm_pf_handler();

// Init paging
// default, the entire kernel 16 MB will be mapped
int init_vm();

// Enable mapping for a region in user space, 0x1000000 upwards
// default to user privilege, r/w permission
int new_region(uint32_t va, int size_bytes);

// Set region as read-only
// Return 0 on success, -1 on error
int set_region_ro(uint32_t va, int size_bytes);

#endif


