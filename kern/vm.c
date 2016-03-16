/** @file vm.c
 *  @brief Implements virtual memory.
 *
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <vm.h>
//#include <list.h>

void asm_invalidate_tlb(uint32_t va);

// Currently don't support more than one process, thus one page-directory
// static uint32_t initial_pd;

// Dummy frame counter
static int frame_counter;

/*
uint32_t get_pd() {
    return initial_pd;
}
*/

struct free_area_struct {
    /* @brief a doubly linked circular list of blocks */
    //list_t list;
    /* @brief Keep track of the blocks it allocates in this group */
    // int bitmap;
};

#define MAX_ORDER 10

struct free_area_struct free_area[MAX_ORDER];


/*
// Manage continugous page allocation
void init_buddy_system() {
    // init MAX_ORDER lists
    int i;
    for(i = 0; i < MAX_ORDER; i++) {
        if(list_init(&free_area[i].list)) {
            lprintf("list_init failed");
            panic("list_init failed");
        }
    }

    // Populate the list of the largest order with the
    // available frames (not counting kernel space), 
    // which are divided into blocks of size 2^(MAX_ORDER-1)
    // pages
    int avail_frame_count = machine_phys_frames() - 
        USER_MEM_START/PAGE_SIZE;

    uint32_t base = USER_MEM_START;
    uint32_t block_size = (2 << (MAX_ORDER - 1)) * PAGE_SIZE;
    for(i = 0; i < avail_frame_count; i++) {
        list_append(&(free_area[MAX_ORDER - 1].list), (void *)base);
        base += block_size;
    }

}
*/
    /*
    for each frame, need to track:
    usage count: 0 free, >0 used
    flags for dirty, locked, referenced, etc?


    The buddy system should expose 2 APIs:
    get_free_frames()
    free_frames()

    All frames are grouped into 10 lists of blocks that contain groups of
    1, 2, 4, 8, 16, ..., 512 contiguous framse, repectively
    meaning 1*PAGE_SIZE, 2*PAGE_SIZE, ..., 512*PAGE_SIZE per block size 
    in each group, 4k, 8k, 4m

    The address of the 1st frame of a block is a multiple of the group size
    */


/**
  * @brief Get contiguous frames
  *
  * @param order Represent size of contiguous frames. 
  * Valid choices are 0, 1, 2, 3, ..., MAX_ORDER - 1
  * 
  * @return Base of contiguous frames on success; 3 on failure
  */
// uint32_t get_frames(int order) {

    /* Example usage:
       uint32_t new_frame_base = get_frames(i);
       return 2^i pages of contiguous frames
    */

/*
    // Check free area
    int cur_order = order; 
    while(order < MAX_ORDER) {
        void *block = list_remove_first(&(free_area[cur_order].list));

        if(block == NULL) {
            // Try find free block in larger size group
            cur_order++;
        } else {
            free_block_base = (uint32_t)block;
            // We have found a free block
            if(cur_order > order) {
                // But it's too large, we will split it first
                // Put unused halves back to lists
                uint32_t block_size = (2 << cur_order) * PAGE_SIZE;
                uint32_t block_base = free_block_base + block_size;
                while(cur_order >= order) {
                    block_size >>= 1;
                    block_base -= block_size;
                    // Place unused half to list of one order less
                    list_append(&(free_area[cur_order - 1].list),
                            block_base);
                }
            } 
           
            return free_block_base;
        }
    }
    
    // Failed to find a contiguous block of the size we want
    return 3;
}

void free_frames() {

    // while(buddy is free) {
    //     combine(this and buddy);
    // }

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

    // Number of pages available
    int frame_count = machine_phys_frames();
    lprintf("frame_count: %d", frame_count);

    // Get page direcotry base for a new task
    uint32_t pdb = create_pd();
    set_cr3(pdb);

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


uint32_t clone_pd() {
    // The pd to clone
    uint32_t old_pd = get_cr3();

    /* 
       Do this when using buddy system
    // Traverse page directory to get the total number of pages used
    // Should consider remember this number
    int count = count_num_pages(pd);

    // Allocate count new frames
    int i;
    for(i = 0; i < count; i++) {
        uint32_t new_f = new_frame();
    }
    */

    /* Create a new address space */
    // Used to copy contents between frames
    char frame_buf[PAGE_SIZE];
    // Clone pd
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
        lprintf("smemalign failed");
        panic("smemalign failed");
    }
    memcpy(pd, (void *)old_pd, PAGE_SIZE);
    int i, j;
    for(i = 0; i < PAGE_SIZE/4; i++) {
        if((pd->pde[i] & (1 << PG_P)) == 1) {
            // Page table is present
            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(pd == NULL) {
                lprintf("smemalign failed");
                panic("smemalign failed");
            }
            uint32_t old_pt_addr = pd->pde[i] & PAGE_ALIGN_MASK;
            memcpy((void *)new_pt, (void *)old_pt_addr, PAGE_SIZE);
            pd->pde[i] = (uint32_t)new_pt | GET_CTRL_BITS(pd->pde[i]);

            // Clone pt
            pt_t *pt = (pt_t *)new_pt;
            for(j = 0; j < PAGE_SIZE/4; j++) {
                if((pt->pte[j] & (1 << PG_P)) == 1) {

                    // Used the same frame for kernel space
                    if(i < 4) {
                        continue;
                    }

                    uint32_t old_frame_addr = pt->pte[j] & PAGE_ALIGN_MASK;
                    uint32_t new_f = new_frame();

                    // Find out the corresponding va of current page
                    uint32_t va = (i << 22) | (j << 12);
                    memcpy(frame_buf, (void *)va, PAGE_SIZE);
                    
                    // Temporarily change the frame that 
                    // old page table points to
                    ((pt_t *)old_pt_addr)->pte[j] = 
                        new_f | GET_CTRL_BITS(pt->pte[j]);
                    // Invalidate page in tlb as we update page table entry
                    asm_invalidate_tlb(va);
                    memcpy((void *)va, frame_buf, PAGE_SIZE);
                    ((pt_t *)old_pt_addr)->pte[j] = 
                        old_frame_addr | GET_CTRL_BITS(pt->pte[j]);
                    // Invalidate page in tlb as we update page table entry
                    asm_invalidate_tlb(va);

                    // Set new pt points to new frame
                    pt->pte[j] = new_f | GET_CTRL_BITS(pt->pte[j]);
                }
            }
        }
    }

    return (uint32_t)pd;
}

