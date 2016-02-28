/** @file vm.c
 *  @brief Implements virtual memory.
 *
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <asm.h> // For idt_base();
#include <idt.h> // For IDT_PF
#include <cr.h> // For %cr
#include <simics.h> // For lprintf
#include <page.h> // For PAGE_SIZE
#include <malloc.h> // For smemalign

#include <string.h> // For memset
#include <seg.h> // For SEGSEL_KERNEL_CS

#include <vm.h>
#include <common_kern.h>
#include <cr.h>

// Currently don't support more than one process, thus one page-directory
static uint32_t initial_pd;

// Dummy frame counter
static int frame_counter;

// frame allocator
uint32_t new_frame() {

    return USER_MEM_START + (frame_counter++) * PAGE_SIZE;

}

int pf_handler() {

    uint32_t cur_cr2 = get_cr2();

    lprintf("Page fault handler called! cr2 is: 0x%x", 
            (unsigned)cur_cr2);
    MAGIC_BREAK;
    return 0;
}


int install_pf_handler() {
    // Get idt base addr
    uint32_t idt_base_addr = (uint32_t)idt_base();

    // Get page fault interrupt idt entry address
    // Why does the P3 slide say a page fault is entry 13, which is 
    // General Protection Fault, while Page Fault is entry 14? 
    // Which one to use?
    uint32_t pf_idt_addr =  idt_base_addr +
        IDT_PF * IDT_ENTRY_SIZE;

    // idt entry
    struct idt_entry_t idt_entry;
    memset(&idt_entry, 0, sizeof(struct idt_entry_t));

    // Fill in idt entry
    // Do we need an asm wrapper, and what kind of info do we need for
    // page fault?
    idt_entry.offset_lsw = ((uint32_t)(asm_pf_handler)) 
        & 0xffff;
    idt_entry.seg_selector = SEGSEL_KERNEL_CS;
    idt_entry.byte_info |= 1 << 7; // P
    idt_entry.byte_info |= 0 << 5; // DPL
    // Use trap gate, interrupts are disabled before the handler 
    // begins execution, there's also a choice of trap gate, which
    // doesn't disable interrupts, which one to use?
    idt_entry.byte_info |= 0b01111; // D
    idt_entry.offset_msw = ((uint32_t)(asm_pf_handler)) >> 16;

    // Install the page fault handler by writing the idt entry to its
    // memory location.
    int cur_offset = 0;
    memcpy((char *)pf_idt_addr, 
            &(idt_entry.offset_lsw), 
            sizeof(idt_entry.offset_lsw));
    cur_offset += sizeof(idt_entry.offset_lsw); 
    memcpy((char *)pf_idt_addr + cur_offset, 
            &(idt_entry.seg_selector), 
            sizeof(idt_entry.seg_selector));
    cur_offset += sizeof(idt_entry.seg_selector);
    memcpy((char *)pf_idt_addr + cur_offset, 
            &(idt_entry.byte_unused), 
            sizeof(idt_entry.byte_unused));
    cur_offset += sizeof(idt_entry.byte_unused);
    memcpy((char *)pf_idt_addr + cur_offset, 
            &(idt_entry.byte_info), 
            sizeof(idt_entry.byte_info));
    cur_offset += sizeof(idt_entry.byte_info);
    memcpy((char *)pf_idt_addr + cur_offset, 
            &(idt_entry.offset_msw), 
            sizeof(idt_entry.offset_msw));

    return 0;
}

// 0 for pde, 1 for pte
uint32_t get_pg_ctrl_bits(int type) {

    if(type == 0) {
        // Page directory entry 12..0 control bits
        uint32_t pde_ctrl_bits = 0;
        // presented in physical memory when set
        SET_BIT(pde_ctrl_bits, PG_P);
        // r/w permission when set
        SET_BIT(pde_ctrl_bits, PG_RW);
        // Supervisor mode only when cleared
        CLR_BIT(pde_ctrl_bits, PG_US);
        // Write-through caching when set
        SET_BIT(pde_ctrl_bits, PG_PWT);
        // Page or page table can be cached when cleared
        CLR_BIT(pde_ctrl_bits, PG_PCD);
        // Indicates page table hasn't been accessed when cleared
        CLR_BIT(pde_ctrl_bits, PG_A);
        // Page size: 0 indicates 4KB
        CLR_BIT(pde_ctrl_bits, PG_PS);
        return pde_ctrl_bits;
    } else {
        // Page table entry 12..0 control bits
        uint32_t pte_ctrl_bits = 0;
        // presented in physical memory when set
        SET_BIT(pte_ctrl_bits, PG_P);
        // r/w permission when set
        SET_BIT(pte_ctrl_bits, PG_RW);
        // Supervisor mode only when cleared
        CLR_BIT(pte_ctrl_bits, PG_US);
        // Write-through caching when set
        SET_BIT(pte_ctrl_bits, PG_PWT);
        // Page or page table can be cached when cleared
        CLR_BIT(pte_ctrl_bits, PG_PCD);
        // Indicates page table hasn't been accessed when cleared
        CLR_BIT(pte_ctrl_bits, PG_A);
        // Indicates page is dirty when set
        SET_BIT(pte_ctrl_bits, PG_D);
        // Page table attribute index, used with PCD, clear
        CLR_BIT(pte_ctrl_bits, PG_PAT);
        // Global page, set to preserve page in TLB
        SET_BIT(pte_ctrl_bits, PG_G);
        return pte_ctrl_bits;
    }

}



// Only map kernel space for the moment
uint32_t create_pd() {

    // To cover kernel 16 MB space, need at least 1 pd, 4 pt
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
        lprintf("smemalign failed");
        panic("smemalign failed");
    }
    // Clear
    memset(pd, 0, PAGE_SIZE);

    // Get pde ctrl bits
    uint32_t pde_ctrl_bits = get_pg_ctrl_bits(0);

    int i;
    for(i = 0; i < 4; i++) {
        void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
        if(pd == NULL) {
            lprintf("smemalign failed");
            panic("smemalign failed");
        }
        // Clear
        memset(new_pt, 0, PAGE_SIZE);
        pd->pde[i] = ((uint32_t)new_pt | pde_ctrl_bits);
    }

    // Get pte ctrl bits
    uint32_t pte_ctrl_bits = get_pg_ctrl_bits(1);

    int j;
    for(i = 0; i < 4; i++) {
        pt_t *pt = (pt_t *)(pd->pde[i] & PAGE_ALIGN_MASK);

        for(j = 0; j < PAGE_SIZE/4; j++) {
            // Use direct mapping for kernel memory space
            uint32_t frame_base = (i << 22) | (j << 12);
            pt->pte[j] = (pte_t)(frame_base | pte_ctrl_bits);
        }
    }

    return (uint32_t)pd;
}


// Enable paging
void enable_paging() {
    uint32_t cr0 = get_cr0();
    cr0 |= CR0_PG;
    set_cr0(cr0);
}

// Disable caching
void disable_caching() {

    uint32_t cr0 = get_cr0();
    cr0 |= CR0_CD;
    set_cr0(cr0);

}


// Open virtual memory
int init_vm() {

    // Install page fault handler
    int ret = install_pf_handler();
    if(ret < 0) {
        lprintf("Page fault handler failed");
        return -1;
    }

    // Get page direcotry base for a new task
    uint32_t pdb = create_pd();
    initial_pd = pdb;

    // Set page directory base register
    // Put current task's page directory base in %cr3 register

    // Ignore PCD and PWT in %cr3
    uint32_t cr3 = pdb;
    set_cr3(cr3);

    // Disable caching of main memory
    // This is needed for the situation where you invalidate
    // a page table entry's content, like changing the page it
    // points to to read-only, and the user program accesses 
    // the page before the cache updates, then there's a 
    // protection problem.
    // May adjust to page level cache disable later 
    disable_caching();

    enable_paging();

    lprintf("Paging is enabled!");

    return 0;
}

/* vm's interface */
// Enable new region starting from vir_addr of size_bytes
// default to user privilege, r/w permission
int new_region(uint32_t va, int size_bytes) {
    // Find frames for this region, set pd and pt

    // Enable mapping from the page where the first byte of region
    // is in, to the one where the last byte of the region is in

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;
    uint32_t page_highest = (va + (uint32_t)size_bytes - 1) &
        PAGE_ALIGN_MASK;

    // Number of pages in the region
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;
    int i;

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)initial_pd;

    for(i = 0; i < count; i++) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(((*pde) & (1 << PG_P)) == 0) {
            // not present
            // Allocate a new page table,
            // May not check this way, will address it later
            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(pd == NULL) {
                lprintf("smemalign failed");
                panic("smemalign failed");
            }
            // Clear
            memset(new_pt, 0, PAGE_SIZE);

            uint32_t pde_ctrl_bits = get_pg_ctrl_bits(0);

            // Change privilege level to user
            // Allow user mode access when set
            SET_BIT(pde_ctrl_bits, PG_US);

            *pde = ((uint32_t)new_pt | pde_ctrl_bits);
        }

        // Check page table entry presence
        uint32_t pt_index = GET_PT_INDEX(page);
        pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
        pte_t *pte = &(pt->pte[pt_index]);

        if(((*pte) & (1 << PG_P)) == 0) {
            // Not present
            // Allocate a new page
            uint32_t new_f = new_frame();

            uint32_t pte_ctrl_bits = get_pg_ctrl_bits(1);

            // Change privilege level to user
            // Allow user mode access when set
            SET_BIT(pte_ctrl_bits, PG_US);

            *pte = ((uint32_t)new_f | pte_ctrl_bits);
            // memset((void *)new_f, 0, PAGE_SIZE);
        }

        page += PAGE_SIZE;
    }

    return 0;
}


// Set region permission as read-only
// Return 0 on success, -1 on error
int set_region_ro(uint32_t va, int size_bytes) {

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;
    uint32_t page_highest = (va + (uint32_t)size_bytes - 1) &
        PAGE_ALIGN_MASK;

    // Number of pages in the region
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;
    int i;

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)initial_pd;

    for(i = 0; i < count; i++) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(((*pde) & (1 << PG_P)) == 0) {
            // Not present
            return -1;
        }

        // Check page table entry presence
        uint32_t pt_index = GET_PT_INDEX(page);
        pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
        pte_t *pte = &(pt->pte[pt_index]);

        if(((*pte) & (1 << PG_P)) == 0) {
            // Not present
            return -1;
        }

        // Set page as read-only
        uint32_t pte_value = (uint32_t)(*pte);
        CLR_BIT(pte_value, PG_RW);
        *pte = pte_value;

        page += PAGE_SIZE;
    }

    return 0;
}


