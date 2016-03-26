/** @file vm.c
 *  @brief Implements virtual memory.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#include <vm.h>
#include <pm.h>
#include <list.h>


/** @brief Invalidate a page table entry in TLB to force consulting 
 *  memory to fetch the newest one next time the page is accessed
 *  
 *  @param va The virtual address of the base of the page to invalidate
 *
 *  @return Void
 */
void asm_invalidate_tlb(uint32_t va);

/** @brief A system wide all-zero frame used for ZFOD */
static uint32_t all_zero_frame;

/* Implementations for functions used internally in this file */

/** @brief Get default control bits for page directory or page table entry
 *  
 *  @param type 0 for page directory entry; 1 for page table entry
 *
 *  @return Void
 */
static uint32_t get_pg_ctrl_bits(int type) {

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

/** @brief Enable paging
 *
 *  @return Void
 */
static void enable_paging() {
    uint32_t cr0 = get_cr0();
    cr0 |= CR0_PG;
    set_cr0(cr0);
}


/** @brief Enable global page
 *
 *  Set kernel page table entries as global so that they wouldn't be cleared
 *  in TLB when %cr3 is reset.
 *
 *  @return Void
 */
static void enable_pge_flag() {
    uint32_t cr4 = get_cr4();
    cr4 |= CR4_PGE;
    set_cr4(cr4);
}


/** @brief Count number of pages allocated in user space
 *
 *  @return Number of pages allocated in user space
 */
static int count_pages_user_space() {
    int num_pages_allocated = 0;
    pd_t *pd = (pd_t *)get_cr3();

    int i, j;
    // skip kernel page tables, starts from user space
    for(i = NUM_PT_KERNEL; i < PAGE_SIZE/ENTRY_SIZE; i++) {
        if((pd->pde[i] & (1 << PG_P)) == 1) {

            pt_t *pt = (pt_t *)(pd->pde[i] & PAGE_ALIGN_MASK);
            for(j = 0; j < PAGE_SIZE/ENTRY_SIZE; j++) {
                if((pt->pte[j] & (1 << PG_P)) == 1) {
                    num_pages_allocated++;
                }
            }
        }
    }

    return num_pages_allocated;
}


/** @brief Count number of pages allocated in region
 *
 *  @param va The virtual address of the start of the region
 *  @param size_bytes The size of the region
 *
 *  @return Number of pages allocated in region
 */
static int count_pages_allocated(uint32_t va, int size_bytes) {
    int num_pages_allocated = 0;

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;
    uint32_t page_highest = (va + (uint32_t)size_bytes - 1) &
        PAGE_ALIGN_MASK;
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;
    int i;

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();

    for(i = 0; i < count; i++) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(IS_SET(*pde, PG_P)) {
            // Present

            // Check page table entry presence
            uint32_t pt_index = GET_PT_INDEX(page);
            pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
            pte_t *pte = &(pt->pte[pt_index]);

            if(IS_SET(*pte, PG_P)) {
                // Present
                num_pages_allocated++;
            }
        }

        page += PAGE_SIZE;
    }


    return num_pages_allocated;
}


/** @brief Remove pages in a region
 *
 *  The region range is determined by a previous new_region call
 *
 *  @param va The virtual address of the start of the region
 *
 *  @return 0 on success; negative integer on error
 */

static int remove_region(uint32_t va) {

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();
    int is_first_page = 1;
    int is_finished = 0;

    while(!is_finished) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(!IS_SET(*pde, PG_P)) {
            // Not present
            lprintf("pde not present, page: %x", (unsigned)page);
            return ERROR_BASE_NOT_PREV;
        }

        // Check page table entry presence
        uint32_t pt_index = GET_PT_INDEX(page);
        pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
        pte_t *pte = &(pt->pte[pt_index]);

        if(!IS_SET(*pte, PG_P)) {
            // Not present
            lprintf("pte not present, page: %x", (unsigned)page);
            return ERROR_BASE_NOT_PREV;
        }

        if(is_first_page) {
            // Make sure the first page is the base of a previous 
            // new_pages call
            if(!IS_SET(*pte, PG_NEW_PAGES_START)) {
                lprintf("Page 0x%x isn't the base of a previous new_pages call",
                        (unsigned)page);
                return ERROR_BASE_NOT_PREV;
            } else {
                is_first_page = 0;
            }
        }

        if(!IS_SET(*pte, PG_ZFOD)) {
            // Free the frame if it's not the system wide all-zero frame
            uint32_t frame = *pte & PAGE_ALIGN_MASK;
            int ret = free_frames_raw(frame, 0);
            if(ret < 0) {
                lprintf("free_frames failed, page: %x", 
                        (unsigned)page);
                return ret;
            }
        }

        is_finished = IS_SET(*pte, PG_NEW_PAGES_END);

        // Remove page table entry
        (*pte) = 0;

        // Invalidate tlb for page
        asm_invalidate_tlb(page);

        page += PAGE_SIZE;
    }

    return 0;
}



/** @brief Check and fix ZFOD
 *  
 *  @param va The virtual address of the page to inspect
 *
 *  @return 1 on true; 0 on false 
 */
static int is_page_ZFOD(uint32_t va) {

    uint32_t page = va & PAGE_ALIGN_MASK;

    pd_t *pd = (pd_t *)get_cr3();
    uint32_t pd_index = GET_PD_INDEX(page);
    pde_t *pde = &(pd->pde[pd_index]);

    // Check page directory entry presence
    if(IS_SET(*pde, PG_P)) {
        // Present

        // Check page table entry presence
        uint32_t pt_index = GET_PT_INDEX(page);
        pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
        pte_t *pte = &(pt->pte[pt_index]);

        if(IS_SET(*pte, PG_P)) {
            // Present

            // The page is not marked ZFOD
            if(!IS_SET(*pte, PG_ZFOD)) {
                return 0;
            }

            // Clear ZFOD bit
            CLR_BIT(*pte, PG_ZFOD);
            // Set as read-write
            SET_BIT(*pte, PG_RW);
            // Allocate a new frame
            uint32_t new_f = get_frames_raw(0);

            // Set page table entry
            *pte = new_f | (*pte & (~PAGE_ALIGN_MASK));

            // Invalidate tlb for page
            asm_invalidate_tlb(page);

            // Clear new frame
            memset((void *)page, 0, PAGE_SIZE);

            return 1;
        }
    }

    return 0;

}



/* Functions implementations that are public to other modules */


/** @brief Page fault handler (TBD)
 *  
 *
 *  @return 0 on success
 */
void pf_handler(uint32_t error_code) {

    // Get faulting virtual address
    uint32_t fault_va = get_cr2();

    // error code bit index 3 to 0: RSVD, US, RW, P

    if(!IS_SET(error_code, PG_P)) {
        // The fault was caused by a non-present page
        lprintf("The fault was caused by a non-present page");
        lprintf("cr2 is: 0x%x", (unsigned)fault_va);

        // Kill the thread?!
        MAGIC_BREAK;

    }

    if(IS_SET(error_code, PG_RSVD)) {
        // The fault was caused by reserved bit violation
        lprintf("The fault was caused by reserved bit violation");
        lprintf("cr2 is: 0x%x", (unsigned)fault_va);


        // Kill the thread?!
        MAGIC_BREAK;
    }



    if(IS_SET(error_code, PG_US)) {
        // The access causing the fault originated when the processor
        // was executing in user mode

        if(fault_va < USER_MEM_START) {
            // User trying to access kernel memory
            lprintf("The fault was caused by user accessing kernel memory");
            lprintf("cr2 is: 0x%x", (unsigned)fault_va);

            // Kill the thread?!
            MAGIC_BREAK;
        }

        if(IS_SET(error_code, PG_RW)) {
            // The access causing the fault was a write.
            // Check the corresponding page table entry for faulting addr.
            // If it's marked as ZFOD, allocate a frame for it, and retry
            // the faulting address.
            if(is_page_ZFOD(fault_va)) {

                //lprintf("ZFOD solved");
                return;

            } else {
                lprintf("The fault was caused by a write but not ZFOD related");
                lprintf("cr2 is: 0x%x", (unsigned)fault_va);

                // Kill the thread?!
                MAGIC_BREAK;
            }
        } else {
            // The access causing the fault was a read

            // How come? Kill the thread?!
            lprintf("A read caused page fault");
            lprintf("cr2 is: 0x%x", (unsigned)fault_va);
            MAGIC_BREAK;

        }
    } else {


        // The access causing the fault originated when the processor
        // was executing in kernel mode
        lprintf("page fault happened in kernel mode");
        lprintf("cr2 is: 0x%x", (unsigned)fault_va);
        MAGIC_BREAK;
    }


}


/** @brief Init virtual memory
 *  
 *  Create the first page directory to map kernel address space, 
 *  enable paging, and initial physical memory manager.
 *
 *  @return 0 on success; negative integer on error
 */
int init_vm() {

    // Get page direcotry base for a new task
    uint32_t pdb = create_pd();
    set_cr3(pdb);

    // Enable paging
    enable_paging();

    // Enable global page so that kernel pages in TLB wouldn't
    // be cleared when %cr3 is reset
    enable_pge_flag();

    // Allocate a system-wide all-zero frame
    void *new_f = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(new_f == NULL) {
        lprintf("smemalign failed");
        return ERROR_MALLOC_LIB;
    }
    // Clear
    memset(new_f, 0, PAGE_SIZE);
    all_zero_frame = (uint32_t)new_f;

    // Init buddy system to track frames in user address space
    int ret = init_pm();

    //test_frames();
    //test_vm();

    lprintf("Paging is enabled!");

    return ret;
}


/** @brief Create a new page directory along with page tables for 16 MB 
 *  kernel memory space, i.e., 0x0 to 0xffffff
 *
 *  @return The new page directory address
 */
uint32_t create_pd() {

    // To cover kernel 16 MB space, need at least 1 pd, 4 pt
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
        lprintf("smemalign failed");
        return ERROR_MALLOC_LIB;
    }
    // Clear
    memset(pd, 0, PAGE_SIZE);

    // Get pde ctrl bits
    uint32_t pde_ctrl_bits = get_pg_ctrl_bits(0);

    int i;
    for(i = 0; i < NUM_PT_KERNEL; i++) {
        void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
        if(pd == NULL) {
            lprintf("smemalign failed");
            return ERROR_MALLOC_LIB;
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
    for(i = 0; i < NUM_PT_KERNEL; i++) {
        pt_t *pt = (pt_t *)(pd->pde[i] & PAGE_ALIGN_MASK);

        for(j = 0; j < PAGE_SIZE/ENTRY_SIZE; j++) {
            // Use direct mapping for kernel memory space
            uint32_t frame_base = (i << 22) | (j << 12);
            pt->pte[j] = (pte_t)(frame_base | pte_ctrl_bits);
        }
    }

    return (uint32_t)pd;
}


/** @brief Clone the entire current address space
 *
 *  A new page directory along with new page tables are allocate. Page
 *  table entries in user space all point to new frames that are exact copies
 *  of old frames, while page table entries in kernel space still point to
 *  old frames.
 *
 *  @return The new page directory address
 */
uint32_t clone_pd() {
    // The pd to clone
    uint32_t old_pd = get_cr3();

    // Number of pages allocated in this user space
    int num_pages_allocated = count_pages_user_space();
    // lprintf("clone_pd: num_pages_allocated:%d", num_pages_allocated);
    list_t list;
    if(get_frames(num_pages_allocated, &list) == -1) {
        return ERROR_NOT_ENOUGH_MEM;
    }
    int frames_left = 0;
    uint32_t cur_frame = 0;

    /* The following code creates a new address space */

    // A buffer to copy contents between frames
    char frame_buf[PAGE_SIZE];
    // Clone pd
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
        lprintf("smemalign failed");
        return ERROR_MALLOC_LIB;
    }
    memcpy(pd, (void *)old_pd, PAGE_SIZE);
    int i, j;
    for(i = 0; i < PAGE_SIZE/ENTRY_SIZE; i++) {
        if(IS_SET(pd->pde[i], PG_P)) {
            // Clone pt
            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(pd == NULL) {
                lprintf("smemalign failed");
                return ERROR_MALLOC_LIB;
            }
            uint32_t old_pt_addr = pd->pde[i] & PAGE_ALIGN_MASK;
            memcpy((void *)new_pt, (void *)old_pt_addr, PAGE_SIZE);
            pd->pde[i] = (uint32_t)new_pt | GET_CTRL_BITS(pd->pde[i]);


            // Use the same frames for kernel space
            if(i < NUM_PT_KERNEL) {
                continue;
            }

            // Clone frames
            pt_t *pt = (pt_t *)new_pt;
            for(j = 0; j < PAGE_SIZE/ENTRY_SIZE; j++) {
                if(IS_SET(pt->pte[j], PG_P)) {

                    uint32_t old_frame_addr = pt->pte[j] & PAGE_ALIGN_MASK;

                    //uint32_t new_f = new_frame();
                    if(frames_left == 0) {
                        uint32_t *data;
                        if(!list_remove_first(&list, (void **)(&data))) {
                            frames_left = data[0];
                            cur_frame = data[1];
                            free(data);
                        } else {
                            // Shouldn't happen
                            lprintf("list_remove_first returns negative");
                            return ERROR_UNKNOWN;
                        }
                    } 

                    frames_left--;
                    uint32_t new_f = cur_frame;
                    cur_frame += PAGE_SIZE;


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

    // Destroy result list
    if(num_pages_allocated > 0) {
        list_destroy(&list, TRUE);
    }

    return (uint32_t)pd;
}


/** @brief Free entire current user address space
 *
 *  @return 0 on success; negative integer on error
 */
/*
   int free_user_space() {

   lprintf("free_user_space is called");

   pd_t *pd = (pd_t *)get_cr3();

   uint32_t frame_start = 0;
   uint32_t cur_len = 0;

   int i, j;
// skip kernel page tables, starts from user space
for(i = NUM_PT_KERNEL; i < PAGE_SIZE/ENTRY_SIZE; i++) {
if(IS_SET(pd->pde[i], PG_P)) {

uint32_t pt_addr = pd->pde[i] & PAGE_ALIGN_MASK;
pt_t *pt = (pt_t *)pt_addr;
for(j = 0; j < PAGE_SIZE/ENTRY_SIZE; j++) {
if(IS_SET(pt->pte[j], PG_P)) {

uint32_t cur_frame = pt->pte[j] & PAGE_ALIGN_MASK;
if(cur_len == 0) {
frame_start = cur_frame;
cur_len = 1;
} else if(cur_frame != frame_start + cur_len * PAGE_SIZE) {
// Free contiguous frames described by frame_start and cur_len
int ret = free_frames(frame_start, cur_len);
if(ret < 0) return ret;

frame_start = cur_frame;
cur_len = 1;
} else {
cur_len++;
}

// Remove page table entry
pt->pte[j] = 0;
}
}

// Free page table
pd->pde[i] = 0;
sfree((void *)pt_addr, PAGE_SIZE);
}
}

// Free last contiguous frames
if(cur_len != 0) {
free_frames(frame_start, cur_len);
}

lprintf("free_user_space finished");

return 0;

}
*/



/** @brief Enable mapping for a region in user space (0x1000000 and upwards)
 *
 *  This call will be called by kernel to allocate initial program space, or
 *  called by new_pages system call to allocate space requested by user 
 *  program later.
 *
 *  @param va The virtual address the region starts with
 *  @param size_bytes The size of the region
 *  @param rw_perm The rw permission of the region, 1 as rw, 0 as ro
 *  @param is_new_pages_syscall Flag showing if the caller is the new_pages 
 *  system call
 *  @param is_ZFOD Flag showing if the region uses ZFOD
 *
 *  @return 0 on success; negative integer on error
 */
int new_region(uint32_t va, int size_bytes, int rw_perm, 
        int is_new_pages_syscall, int is_ZFOD) {
    // Find frames for this region, set pd and pt

    // Enable mapping from the page where the first byte of region
    // is in, to the one where the last byte of the region is in

    // Need to traverse new_region first to know how many new frames
    // are needed, because some part of the region may already have
    // been allocated.
    int num_pages_allocated = count_pages_allocated(va, size_bytes);

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;
    uint32_t page_highest = (va + (uint32_t)size_bytes - 1) &
        PAGE_ALIGN_MASK;

    // Number of pages in the region
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;
    int i;

    if(num_pages_allocated > 0) {
        if(is_new_pages_syscall) {
            // Some pages have already been allocated in this address space
            return ERROR_OVERLAP;
        }
    }

    // Reserve how many frames will be used in the worst case
    if(reserve_frames(count - num_pages_allocated) == -1) {
        return -1;
    }

    /*
       list_t list;
       if(get_frames(count - num_pages_allocated, &list) == -1) {
       return -1;
       }
       int frames_left = 0;
       uint32_t cur_frame = 0;
       */

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();

    for(i = 0; i < count; i++) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(!IS_SET(*pde, PG_P)) {
            // not present
            // Allocate a new page table,
            // May not check this way, will address it later
            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(pd == NULL) {
                lprintf("smemalign failed");
                return ERROR_MALLOC_LIB;
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

        if(!IS_SET(*pte, PG_P)) {
            // Not present

            // Get page table entry default bits
            uint32_t pte_ctrl_bits = get_pg_ctrl_bits(1);

            // Change privilege level to user
            // Allow user mode access when set
            SET_BIT(pte_ctrl_bits, PG_US);

            // new_pages system call related
            if(is_new_pages_syscall) {
                // The page is the base of the region new_pages() requests
                if(page == page_lowest) {
                    SET_BIT(pte_ctrl_bits, PG_NEW_PAGES_START);
                }

                // The page is the last one of the region new_pages() requests
                if(page == page_highest) {
                    SET_BIT(pte_ctrl_bits, PG_NEW_PAGES_END);
                }
            }

            uint32_t new_f;
            if(is_ZFOD) {
                // Mark as ZFOD
                SET_BIT(pte_ctrl_bits, PG_ZFOD);
                // Set as read-only
                CLR_BIT(pte_ctrl_bits, PG_RW);
                // Usr a system wide all-zero page
                new_f = all_zero_frame;
            } else {
                // Set rw permission
                rw_perm ? SET_BIT(pte_ctrl_bits, PG_RW) :
                    CLR_BIT(pte_ctrl_bits, PG_RW);
                // Allocate a new frame
                new_f = get_frames_raw(0);
            }

            // Set page table entry
            *pte = new_f | pte_ctrl_bits;

            // Invalidate tlb for page
            asm_invalidate_tlb(page);

            // Clear new frame
            if(!is_ZFOD) {
                memset((void *)page, 0, PAGE_SIZE);
            }

            //            lprintf("frame %x allocated to page %x", (unsigned)new_f,
            //                    (unsigned)page);

        }

        page += PAGE_SIZE;
    }

    /*
    // Destroy result list
    if(count - num_pages_allocated > 0) {
    list_destroy(&list, TRUE);
    }
    */

    return 0;
}




/**
 * @brief new_pages syscall
 *
 * Allocate memory for contiguous pages of len bytes.
 *
 * @param base The virtual address of the base of the contiguous pages
 * @param len Number of bytes of the contiguous pages
 *
 * @return 0 on success; negative integer on error
 */
int new_pages(void *base, int len) {

    // if base is not aligned
    if((uint32_t)base % PAGE_SIZE != 0) {
        return ERROR_BASE_NOT_ALIGNED;     
    }

    // if len is not a positive integral multiple of the system page size
    if(!(len > 0 && len % PAGE_SIZE == 0)) {
        return ERROR_LEN;
    }

    // if any portion of the region intersects a part of the address space
    // reserved by the kernel
    if(!((uint32_t)base >= USER_MEM_START)) {
        return ERROR_KERNEL_SPACE;
    }

    // Allocate pages of read-write permission
    int ret = new_region((uint32_t)base, len, 1, TRUE, TRUE);    
    return ret;

}

/**
 * @brief remove_pages syscall
 *  
 * Free memory for contiguous pages whose virtual address starts from base. 
 * Call remove_region to do the actual work.
 *
 * @param base The The virtual address of the base of the start page of
 * contiguous pages.
 *
 * @return 0 on success; negative integer on error 
 */
int remove_pages(void *base) {

    return remove_region((uint32_t)base);

}



/** @brief Check if pages in region are allocated and of specified permission
 *
 *  @param va The virtual address of the start of the region
 *  @param size_bytes The size of the region
 *  @param The permission to check: 0 for read-only, 1 for read-write
 *
 *  @return 1 if true; 0 if false 
 */
int is_region_alloc_perm(uint32_t va, int size_bytes, int rw_perm) {

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;
    uint32_t page_highest = (va + (uint32_t)size_bytes - 1) &
        PAGE_ALIGN_MASK;
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;
    int i;

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();

    for(i = 0; i < count; i++) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(IS_SET(*pde, PG_P)) {
            // Present

            // Check page table entry presence
            uint32_t pt_index = GET_PT_INDEX(page);
            pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
            pte_t *pte = &(pt->pte[pt_index]);

            if(IS_SET(*pte, PG_P)) {
                // Present
                // Check rw permission
                int pte_rw_bit = IS_SET(*pte, PG_RW);
                if(rw_perm != pte_rw_bit) {
                    return 0;
                }
            } else {
                // Page table entry not present
                // Return not valid
                return 0;
            }
        } else {
            // Page directory entry not present
            // Return not valid
            return 0;
        }

        page += PAGE_SIZE;
    }

    // Return true
    return 1;
}


/************ The followings are for debugging, will remove later **********/

// For debuging, will remove later
void test_vm() {

    void *base = (void *)0x1040000;
    int len = 3 * PAGE_SIZE;
    int ret = new_pages(base, len);
    if(ret < 0) {
        lprintf("new_pages failed");
        MAGIC_BREAK;
    }
    *((char *)base) = '9'; 

    lprintf("wrote to newly allocated space");
    ret = remove_pages(base);
    if(ret < 0) {
        lprintf("remove_pages failed");
        MAGIC_BREAK;
    }
    traverse_free_area();

}


