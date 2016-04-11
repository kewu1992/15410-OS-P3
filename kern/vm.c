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
#include <control_block.h>
#include <asm_helper.h>


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

/** @brief Default page directory entry control bits */
static uint32_t ctrl_bits_pde;
/** @brief Default page table entry control bits */
static uint32_t ctrl_bits_pte;

/** @brief Initialize default control bits for page directory and page table 
 * entry
 *
 * @return void
 */
static void init_pg_ctrl_bits() {

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

    ctrl_bits_pde = ctrl_bits;
    ctrl_bits_pte = ctrl_bits;

    /* Different bits for pde and pte */
    // pde bits
    // Page size: 0 indicates 4KB
    CLR_BIT(ctrl_bits_pde, PG_PS);

    // pte bits
    // Indicates page is dirty when set
    SET_BIT(ctrl_bits_pte, PG_D);
    // Page table attribute index, used with PCD, clear
    CLR_BIT(ctrl_bits_pte, PG_PAT);
    // Global page, clear to not preserve page in TLB
    CLR_BIT(ctrl_bits_pte, PG_G);

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
    // skip kernel page tables, start from user space
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

    int pt_lock_index = -1;;

    // Get page table lock of current task
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        panic("tcb is NULL");
    }
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        panic("This task's pcb is NULL");
    }
    mutex_t *pt_locks = this_task->pt_locks;

    while(!is_finished) {
        uint32_t pd_index = GET_PD_INDEX(page);

        int cur_pt_lock_index = pd_index/NUM_PT_PER_LOCK;
        if(cur_pt_lock_index != pt_lock_index) {
            if(pt_lock_index != -1) {
                // Not the first time
                mutex_unlock(&pt_locks[pt_lock_index]);
            }
            pt_lock_index = cur_pt_lock_index;
            // Get next lock
            mutex_lock(&pt_locks[pt_lock_index]);
        }

        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(!IS_SET(*pde, PG_P)) {
            // Not present
            lprintf("pde not present, page: %x", (unsigned)page);
            mutex_unlock(&pt_locks[pt_lock_index]);
            return ERROR_BASE_NOT_PREV;
        }

        // Check page table entry presence
        uint32_t pt_index = GET_PT_INDEX(page);
        pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
        pte_t *pte = &(pt->pte[pt_index]);

        if(!IS_SET(*pte, PG_P)) {
            // Not present
            lprintf("pte not present, page: %x", (unsigned)page);
            mutex_unlock(&pt_locks[pt_lock_index]);
            return ERROR_BASE_NOT_PREV;
        }

        if(is_first_page) {
            // Make sure the first page is the base of a previous 
            // new_pages call
            if(!IS_SET(*pte, PG_NEW_PAGES_START)) {
                lprintf("Page 0x%x isn't the base of a previous new_pages call",
                        (unsigned)page);
                mutex_unlock(&pt_locks[pt_lock_index]);
                return ERROR_BASE_NOT_PREV;
            } else {
                is_first_page = 0;
            }
        }

        if(!IS_SET(*pte, PG_ZFOD)) {
            // Free the frame if it's not the system wide all-zero frame
            uint32_t frame = *pte & PAGE_ALIGN_MASK;
            free_frames_raw(frame);
            unreserve_frames(1);
        }

        is_finished = IS_SET(*pte, PG_NEW_PAGES_END);

        // Remove page table entry
        (*pte) = 0;

        // Invalidate tlb for page
        asm_invalidate_tlb(page);

        page += PAGE_SIZE;
    }

    mutex_unlock(&pt_locks[pt_lock_index]);
    return 0;
}



/** @brief Check and fix ZFOD
 *  
 *  @param va The virtual address of the page to inspect
 *  @param error_code The error code for page fault
 *  @param need_check_error_code If there's need to check error code
 *  before proceeding to check page itself (Kernel can eliminate
 *  the step of error code checking if it wants to inspect user memory
 *  and there's literally no page fault at all)
 *
 *  @return 1 on true; 0 on false 
 */
int is_page_ZFOD(uint32_t va, uint32_t error_code, int need_check_error_code) {

    // To be eligible for ZFOD check, the faulting must be 
    // a write from user space to a page that is present.
    if(need_check_error_code) {
        if(!(IS_SET(error_code, PG_P) && IS_SET(error_code, PG_US) && 
            IS_SET(error_code, PG_RW))) {
            return 0;
        }
    }

    // Pages in kernel space are not marked as ZFOD, so there wouldn't
    // be confusion for permission.

    // Check the corresponding page table entry for faulting addr.
    // If it's marked as ZFOD, allocate a frame for it.
    uint32_t page = va & PAGE_ALIGN_MASK;

    pd_t *pd = (pd_t *)get_cr3();
    uint32_t pd_index = GET_PD_INDEX(page);

    // Get page table lock of current task
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        panic("tcb is NULL");
    }
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        panic("This task's pcb is NULL");
    }
    mutex_t *pt_locks = this_task->pt_locks;

    // Acquire lock first
    int pt_lock_index = pd_index/NUM_PT_PER_LOCK;
    mutex_lock(&pt_locks[pt_lock_index]);

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
                mutex_unlock(&pt_locks[pt_lock_index]);
                return 0;
            }

            // Clear ZFOD bit
            CLR_BIT(*pte, PG_ZFOD);
            // Set as read-write
            SET_BIT(*pte, PG_RW);
            // Allocate a new frame
            // Reserved allocate before, should have enough memory
            uint32_t new_f = get_frames_raw();
            if(new_f == ERROR_NOT_ENOUGH_MEM) {
                lprintf("get_frames_raw failed in is_page_ZFOD");
                mutex_unlock(&pt_locks[pt_lock_index]);
                MAGIC_BREAK;
            }

            // Set page table entry
            *pte = new_f | (*pte & (~PAGE_ALIGN_MASK));

            // Invalidate tlb for page
            asm_invalidate_tlb(page);

            // Clear new frame
            memset((void *)page, 0, PAGE_SIZE);

            mutex_unlock(&pt_locks[pt_lock_index]);
            return 1;
        }
    }

    mutex_unlock(&pt_locks[pt_lock_index]);
    return 0;

}



/** @brief Create a new page directory along with page tables for 16 MB 
 *  kernel memory space, i.e., 0x0 to 0xffffff
 *
 *  @return The new page directory address
 */
static uint32_t init_pd() {

    // To cover kernel 16 MB space, need at least 1 pd, 4 pt
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
        lprintf("smemalign failed");
        return ERROR_MALLOC_LIB;
    }
    // Clear
    memset(pd, 0, PAGE_SIZE);

    // Get pde ctrl bits
    uint32_t pde_ctrl_bits = ctrl_bits_pde;

    int i;
    for(i = 0; i < NUM_PT_KERNEL; i++) {
        void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
        if(new_pt == NULL) {
            lprintf("smemalign failed");
            return ERROR_MALLOC_LIB;
        }
        // Clear
        memset(new_pt, 0, PAGE_SIZE);
        pd->pde[i] = ((uint32_t)new_pt | pde_ctrl_bits);
    }

    // Get pte ctrl bits
    uint32_t pte_ctrl_bits = ctrl_bits_pte;
    // Set kernel pages as global pages, so that TLB wouldn't
    // clear them when %cr3 is reset
    SET_BIT(pte_ctrl_bits, PG_G);

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

/** @brief Init virtual memory
 *  
 *  Create the first page directory to map kernel address space, 
 *  enable paging, and initial physical memory manager.
 *
 *  @return 0 on success; negative integer on error
 */
int init_vm() {

    // Configure default page table entry and page diretory entry control bits
    init_pg_ctrl_bits();

    // Get page direcotry base for a new task
    uint32_t pdb = init_pd();
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

    lprintf("Paging is enabled!");

    return ret;
}

/** @brief Create a new address space with kernel space memory allocated
 *
 *  @return New page directory base
 */
uint32_t create_pd() {

    // Only create one new page as page diretory
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
        lprintf("smemalign failed");
        return ERROR_MALLOC_LIB;
    }
    memset((void *)pd, 0, PAGE_SIZE);
    
    // Reuse kernel space page tables
    uint32_t old_pd = get_cr3();
    memcpy(pd, (void *)old_pd, NUM_PT_KERNEL * sizeof(uint32_t));

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

    // Reserve all frames that will be needed in this round of clone_pd
    if(reserve_frames(num_pages_allocated) < 0) {
        lprintf("not enough memory when doing clone_pd");
        return ERROR_NOT_ENOUGH_MEM;
    }

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

            // Use the same pts and frames for kernel space
            if(i < NUM_PT_KERNEL) {
                continue;
            }

            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(new_pt == NULL) {
                lprintf("smemalign failed");
                return ERROR_MALLOC_LIB;
            } 

            uint32_t old_pt_addr = pd->pde[i] & PAGE_ALIGN_MASK;
            memcpy((void *)new_pt, (void *)old_pt_addr, PAGE_SIZE);
            pd->pde[i] = (uint32_t)new_pt | GET_CTRL_BITS(pd->pde[i]);

            // Clone frames
            pt_t *pt = (pt_t *)new_pt;
            for(j = 0; j < PAGE_SIZE/ENTRY_SIZE; j++) {
                if(IS_SET(pt->pte[j], PG_P)) {

                    uint32_t old_frame_addr = pt->pte[j] & PAGE_ALIGN_MASK;

                    // Allocate a new frame
                    uint32_t new_f = get_frames_raw();

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


/** @brief Free entire address space (kernel and user)
 *
 *  @param pd_base The address space's page directory base
 *
 *  @return 0 on success; negative integer on error
 */
void free_entire_space(uint32_t pd_base) {
    // Free user space
    free_space(pd_base, 0);

    // Free kernel space
    free_space(pd_base, 1);

    // Free page directory
    sfree((void *)pd_base, PAGE_SIZE);

}


/** @brief Free current user address space
 *
 *  @param pd_base The address space's page directory base
 *
 *  @param is_kernel_space 1 for kernel space; 0 for user space
 *
 *  @return 0 on success; negative integer on error
 */
void free_space(uint32_t pd_base, int is_kernel_space) {

    pd_t *pd = (pd_t *)pd_base;

    // Start and end index of page directory entry
    int pde_start = is_kernel_space ? 0 : NUM_PT_KERNEL;
    int pde_end = is_kernel_space ? NUM_PT_KERNEL : PAGE_SIZE/ENTRY_SIZE;

    int i, j;
    for(i = pde_start; i < pde_end; i++) {
        if(IS_SET(pd->pde[i], PG_P)) {
            // Page table is present
            uint32_t pt_addr = pd->pde[i] & PAGE_ALIGN_MASK;
            pt_t *pt = (pt_t *)pt_addr;

            if(!is_kernel_space) {
                // Remove frames only for user space
                for(j = 0; j < PAGE_SIZE/ENTRY_SIZE; j++) {
                    if(IS_SET(pt->pte[j], PG_P)) {
                        // Page is present, free the frame if it's not the 
                        // system wide all-zero frame.

                        if(!IS_SET(pt->pte[j], PG_ZFOD)) {
                            uint32_t frame = pt->pte[j] & PAGE_ALIGN_MASK;
                            free_frames_raw(frame);
                            unreserve_frames(1);
                        }

                    }
                }
            }

            // Free page table only for user space
            if(!is_kernel_space) {
                sfree((void *)pt_addr, PAGE_SIZE);
            }
        }
    }

}




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

    // Need to traverse new region first to know how many new frames
    // are needed, because some part of the region may already have
    // been allocated.

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;
    uint32_t page_highest = (va + (uint32_t)size_bytes - 1) &
            PAGE_ALIGN_MASK;

    // Acquire page table lock only for new pages syscall, since there's
    // on concurrency problem when kernel calls new_region to load tasks
    // Acquire locks for page tables covered by region first
    // Get page table index range in page diretory
    uint32_t page_lowest_pd_index = GET_PD_INDEX(page_lowest);
    uint32_t page_highest_pd_index = GET_PD_INDEX(page_highest);

    // The lock index for the lowest page table that region is in
    // For example, if NUM_PT_PER_LOCK is 8, then page table 0 - 7
    // has lock index 0, page table 8 - 15 has lock index 1
    int lowest_pt_lock_index = page_lowest_pd_index/NUM_PT_PER_LOCK;
    int highest_pt_lock_index = page_highest_pd_index/NUM_PT_PER_LOCK;
    int num_pt_lock = highest_pt_lock_index - lowest_pt_lock_index + 1;

    mutex_t *pt_locks;
    if(is_new_pages_syscall) {
        // Get page table lock of current task
        tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
        if(this_thr == NULL) {
            panic("tcb is NULL");
        }
        pcb_t *this_task = this_thr->pcb;
        if(this_task == NULL) {
            panic("This task's pcb is NULL");
        }

        pt_locks = this_task->pt_locks;
        int pt_lock_index = lowest_pt_lock_index;
        int i;
        for(i = 0; i < num_pt_lock; i++) {
            mutex_lock(&pt_locks[pt_lock_index]);
            pt_lock_index++;
        }
    }

    int num_pages_allocated = count_pages_allocated(va, size_bytes);

    // Number of pages in the region
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;

    if(num_pages_allocated > 0) {
        if(is_new_pages_syscall) {
            // Some pages have already been allocated in this address space
            int i;
            int pt_lock_index = highest_pt_lock_index;
            for(i = 0; i < num_pt_lock; i++) {
                mutex_unlock(&pt_locks[pt_lock_index]);
                pt_lock_index--;
            }

            return ERROR_OVERLAP;
        }
    }

    // Reserve how many frames will be used in the worst case in the future 
    // including the case where ZFOD pages are actually requested in the future
    if(reserve_frames(count - num_pages_allocated) == -1) {
        if(is_new_pages_syscall) {
            int i;
            int pt_lock_index = highest_pt_lock_index;
            for(i = 0; i < num_pt_lock; i++) {
                mutex_unlock(&pt_locks[pt_lock_index]);
                pt_lock_index--;
            }
        }
        return -1;
    }

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();

    int i;
    for(i = 0; i < count; i++) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(!IS_SET(*pde, PG_P)) {
            // not present
            // Allocate a new page table
            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(new_pt == NULL) {
                lprintf("smemalign failed");
                if(is_new_pages_syscall) {
                    int i;
                    int pt_lock_index = highest_pt_lock_index;
                    for(i = 0; i < num_pt_lock; i++) {
                        mutex_unlock(&pt_locks[pt_lock_index]);
                        pt_lock_index--;
                    }
                }
                return ERROR_MALLOC_LIB;
            }
            // Clear
            memset(new_pt, 0, PAGE_SIZE);

            uint32_t pde_ctrl_bits = ctrl_bits_pde;

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
            uint32_t pte_ctrl_bits = ctrl_bits_pte;

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
                new_f = get_frames_raw();
            }

            // Set page table entry
            *pte = new_f | pte_ctrl_bits;

            // Invalidate tlb for page
            asm_invalidate_tlb(page);

            // Clear new frame
            if(!is_ZFOD) {
                memset((void *)page, 0, PAGE_SIZE);
            }

        }

        page += PAGE_SIZE;
    }

    if(is_new_pages_syscall) {
        int i;
        int pt_lock_index = highest_pt_lock_index;
        for(i = 0; i < num_pt_lock; i++) {
            mutex_unlock(&pt_locks[pt_lock_index]);
            pt_lock_index--;
        }
    }

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

    // Region will overflow address space
    if(UINT32_MAX - (uint32_t)base + 1 < len) {
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


/** @brief Check user space memory validness
 *
 *  Can check if region are allocated, NULL terminated, writable
 *  and solve ZFOD.
 *
 *  @param va The virtual address of the start of the region
 *  @param max_bytes Max bytes to check
 *  @param is_check_null If 1, then check if a '\0' is encountered
 *  before max_bytes are checked
 *  @param need_writable If 1, check if the region is writable
 *
 *  @return 0 if valid; a negative integer if invalid:
 *  ERROR_KERNEL_SPACE, ERROR_LEN, ERROR_NOT_NULL_TERM, ERROR_READ_ONLY,
 *  ERROR_PAGE_NOT_ALLOC
 *
 */
int check_mem_validness(char *va, int max_bytes, int is_check_null, 
        int need_writable) {

    // Reference kernel memory
    if((uint32_t)va < USER_MEM_START) {
        return ERROR_KERNEL_SPACE;
    }

    if(max_bytes < 0) {
        return ERROR_LEN;
    }

    uint32_t last_byte = (uint32_t)va + max_bytes - 1;
    if(!is_check_null) {
        // check overflow
        if(last_byte < (uint32_t)va) {
            // Len not valid
            return ERROR_LEN;
        }
    } else {
        last_byte = (last_byte < (uint32_t)va) ?
            UINT32_MAX : last_byte;
    }

    uint32_t page_lowest = (uint32_t)va & PAGE_ALIGN_MASK;
    uint32_t page_highest = last_byte & PAGE_ALIGN_MASK;
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;
    int i;

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();

    uint32_t current_byte = (uint32_t)va;

    // Get page table lock of current task
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        panic("tcb is NULL");
    }
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        panic("This task's pcb is NULL");
    }
    mutex_t *pt_locks = this_task->pt_locks;

    // Acquire locks for page tables covered by region first
    // Get page table index range in page diretory
    uint32_t page_lowest_pd_index = GET_PD_INDEX(page_lowest);
    uint32_t page_highest_pd_index = GET_PD_INDEX(page_highest);

    // The lock index for the lowest page table that region is in
    // For example, if NUM_PT_PER_LOCK is 8, then page table 0 - 7
    // has lock index 0, page table 8 - 15 has lock index 1
    int lowest_pt_lock_index = page_lowest_pd_index/NUM_PT_PER_LOCK;
    int highest_pt_lock_index = page_highest_pd_index/NUM_PT_PER_LOCK;
    int num_pt_lock = highest_pt_lock_index - lowest_pt_lock_index + 1;
    int pt_lock_index = lowest_pt_lock_index;
    for(i = 0; i < num_pt_lock; i++) {
        mutex_lock(&pt_locks[pt_lock_index]);
        pt_lock_index++;
    }

    int ret = 0;
    int is_finished = 0;
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
                // Page table is present
                if(is_check_null) {
                    while((current_byte & PAGE_ALIGN_MASK) == page) {
                        if(*((char *)current_byte) == '\0') {
                            is_finished = 1;
                            break;
                        }

                        if(current_byte == last_byte) {
                            ret = ERROR_NOT_NULL_TERM;
                            is_finished = 1;
                            break;
                        }

                        current_byte++;
                    }
                    if(is_finished) break;
                } else if(need_writable) {
                    if(!IS_SET(*pte, PG_RW)) {
                        // Page is read-only
                        // If it's marked ZFOD, then it's valid
                        int need_check_error_code = 0;
                        uint32_t error_code = 0;
                        if(!is_page_ZFOD(page, error_code, 
                                    need_check_error_code)) {
                            ret = ERROR_READ_ONLY;
                            break;
                        }
                    }
                }
            } else {
                // Page table entry not present
                ret = ERROR_PAGE_NOT_ALLOC;
                break;
            }
        } else {
            // Page directory entry not present
            ret = ERROR_PAGE_NOT_ALLOC;
            break;
        }

        page += PAGE_SIZE;
    }

    pt_lock_index = highest_pt_lock_index;
    for(i = 0; i < num_pt_lock; i++) {
        mutex_unlock(&pt_locks[pt_lock_index]);
        pt_lock_index--;
    }
    return ret;
}


/************ The followings are for debugging, will remove later **********/

/*
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

*/


