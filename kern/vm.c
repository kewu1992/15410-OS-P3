/** @file vm.c
 *  @brief Implements virtual memory.
 *
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <vm.h>

// Currently don't support more than one process, thus one page-directory
// static uint32_t initial_pd;

// Dummy frame counter
static int frame_counter;

/*
uint32_t get_pd() {
    return initial_pd;
}
*/

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

// 0 for pde, 1 for pte
uint32_t get_pg_ctrl_bits(int type) {

    uint32_t ctrl_bits = 0;
    /* Common bits for pde and pte */
    // presented in physical memory when set
    SET_BIT(ctrl_bits, PG_P);
    // r/w permission when set
    SET_BIT(ctrl_bits, PG_RW);
    // Supervisor mode only when cleared
    CLR_BIT(ctrl_bits, PG_US);
    // Write-back caching when cleared
    CLR_BIT(ctrl_bits, PG_PWT);
    // Page or page table can be cached when cleared
    CLR_BIT(ctrl_bits, PG_PCD);
    // Indicates page table hasn't been accessed when cleared
    CLR_BIT(ctrl_bits, PG_A);

    /* Different bits for pde and pte */
    if(type == 0) {
        // pde bits
        // Page size: 0 indicates 4KB
        CLR_BIT(ctrl_bits, PG_PS);
    } else {
        // pte bits
        // Indicates page is dirty when set
        SET_BIT(ctrl_bits, PG_D);
        // Page table attribute index, used with PCD, clear
        CLR_BIT(ctrl_bits, PG_PAT);
        // Global page, clear to not preserve page in TLB
        CLR_BIT(ctrl_bits, PG_G);
    }

    return ctrl_bits;

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
    // Set kernel pages as global pages, so that TLB wouldn't
    // clear them when %cr3 is reset
    // SET_BIT(pte_ctrl_bits, PG_G);

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

// Enable global page so that kernel pages in TLB wouldn't
// be cleared when %cr3 is reset
void enable_pge_flag() {
    uint32_t cr4 = get_cr4();
    cr4 |= CR4_PGE;
    set_cr4(cr4);
}

// Open virtual memory
int init_vm() {

    // Get page direcotry base for a new task
    uint32_t pdb = create_pd();
    set_cr3(pdb);

    // Set page directory base register
    // Put current task's page directory base in %cr3 register

    // Set PCD and PWT as both 0 in %cr3 if not touching 0 bits
    // uint32_t cr3 = pdb;
    // set_cr3(cr3);

    // Enable paging
    enable_paging();

    // Enable global page so that kernel pages in TLB wouldn't
    // be cleared when %cr3 is reset
    enable_pge_flag();

    lprintf("Paging is enabled!");

    return 0;
}

/** @brief Enable mapping for a region in user space (0x1000000 upwards)
 *
 *  The privilege level would be set as User level.
 *
 *  @param va The virtual address the region starts with
 *  @param size_bytes The size of the region
 *  @param rw_perm The rw permission of the region, 1 as rw, 0 as ro
 *
 *  @return 0 on success; -1 on error
 */
int new_region(uint32_t va, int size_bytes, int rw_perm) {
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
    pd_t *pd = (pd_t *)get_cr3();

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
            // Set rw permission
            rw_perm ? SET_BIT(pte_ctrl_bits, PG_RW) :
                CLR_BIT(pte_ctrl_bits, PG_RW);

            // Change privilege level to user
            // Allow user mode access when set
            SET_BIT(pte_ctrl_bits, PG_US);

            *pte = ((uint32_t)new_f | pte_ctrl_bits);
            // Clear page
            memset((void *)page, 0, PAGE_SIZE);
        }

        page += PAGE_SIZE;
    }

    return 0;
}


// Set region permission as read-only
// Return 0 on success, -1 on error
/*
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
*/


