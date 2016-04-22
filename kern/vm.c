/** @file vm.c
 *  @brief Implements virtual memory.
 *
 *  This file implements virtual memory. 16 MB kernel space (0x0 to 0xffffff)
 *  are directily mapped to the same physical memory, e.g., virtual memory 
 *  0x1000 maps to 0x1000, for all tasks. User space, 0x1000000 and above
 *  are mapped differently for different tasks. Except the page directory,
 *  tasks share the same kernel page tables, which are marked as global.
 *  ZFOD is used for pages requested by new_pages() syscall and initial
 *  thread stack requested by kernel.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#include <vm.h>
#include <pm.h>
#include <control_block.h>
#include <asm_helper.h>
#include <apic.h>
#include <mptable.h>

#include <lmm/lmm.h>
#include <lmm/lmm_types.h>
#include <malloc/malloc_internal.h>

/** Number of kernel heap memory initially allocated for CPU0 */
#define LMM_0_INIT_MEM  (256 * 1024)

/** @brief Invalidate a page table entry in TLB to force consulting actual 
 *  memory to fetch the page table entry next time the page is accessed.
 *  
 *  This operation is needed when the content of a page table entry is modified
 *  (e.g., when changing the frame the page table entry points to from a system
 *  wide all-zero page to a new allocated frame for writing; or when the 
 *  control bits of the page table entry are modified.
 *
 *  @param va The virtual address of the base of the page to invalidate in
 *  TLB.
 *
 *  @return Void
 */
extern void asm_invalidate_tlb(uint32_t va);

/** @brief Initial page directory for all cores */
static uint32_t init_page_dir[MAX_CPUS];

/** @brief A system wide all-zero frame used for ZFOD */
static uint32_t all_zero_frame;

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
 *  This function is called by clone_pd so that it can know how many new frames
 *  are needed in the future.
 *
 *  @return Number of pages allocated in user space
 */
static int count_pages_user_space() {
    int num_pages_allocated = 0;
    pd_t *pd = (pd_t *)get_cr3();

    int i, j;
    // Skip kernel page tables, start from user space
    for(i = NUM_PT_KERNEL; i < PAGE_SIZE/ENTRY_SIZE; i++) {
        if(IS_SET(pd->pde[i], PG_P)) {

            pt_t *pt = (pt_t *)(pd->pde[i] & PAGE_ALIGN_MASK);
            for(j = 0; j < PAGE_SIZE/ENTRY_SIZE; j++) {
                if(IS_SET(pt->pte[j], PG_P)) {
                    num_pages_allocated++;
                }
            }
        }
    }

    return num_pages_allocated;
}


/** @brief Clear page directory entry specified and free corresponding
 * page tables.
 *
 * This function is needed when kernel memory becomes insufficient during 
 * the process of allocating page tables and changes need to be reverted.
 *
 * @param bitmap A bitmap where 1s in it are the relative index of the page 
 * directory entry to clear.
 * @param bitmap_size The size of the bitmap
 * @param pd_index_start The offset that the index in the bitmap should
 * consider to get the absolute index in the page directory entry
 *
 * @return void
 *
 */
static void clear_pd_entry(char *bitmap, int bitmap_size, int pd_index_start) {

    pd_t *pd = (pd_t *)get_cr3();
    int i, j;
    for(i = 0; i < bitmap_size; i++) {
        char byte = bitmap[i];
        for(j = 0; j < sizeof(char); j++) {
            if(IS_SET(byte, j)) {
                // Get page table address
                int pd_index = i * sizeof(char) + j + pd_index_start;
                uint32_t pt_addr = 
                    (uint32_t)(pd->pde[pd_index]) & PAGE_ALIGN_MASK;
                sfree((void *)pt_addr, PAGE_SIZE);
                pd->pde[pd_index] = 0;
            }
        }
    }


}


/** @brief Count number of pages allocated in region and allocate page
 *  directories along the way.
 *
 *  This function is called by new_region to know how many pages have already 
 *  been allocated to decide how many new frames are needed. In addition, 
 *  page tables are allocated for this region if previously not allocated,
 *  But this is done in a all or none fashion, meaning all newly needed pages 
 *  tables can be allocated or none are created by reverting changes back.
 *
 *  @param va The virtual address of the start of the region
 *  @param size_bytes The size of the region
 *  @param is_new_pages_syscall Flag indicating if the purpose
 *  of calling this function is to serve new_pages syscall.
 *
 *  @return Number of pages allocated in region on success,
 *  a negative integer on error.
 *  
 */
static int count_pages_allocated(uint32_t va, int size_bytes, 
        int is_new_pages_syscall) {

    int num_pages_allocated = 0;

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;
    uint32_t page_highest = (va + (uint32_t)size_bytes - 1) &
        PAGE_ALIGN_MASK;
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;
    int i;

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();

    int page_highest_pd_index = GET_PD_INDEX(page_highest);
    int page_lowest_pd_index = GET_PD_INDEX(page_lowest);
    int num_page_tables = page_highest_pd_index - page_lowest_pd_index + 1;

    // Track the newly allcoated page tables in case there's not enough
    // kernel memory as we proceed, we can revert changes
    int bitmap_size = (num_page_tables - 1)/sizeof(char) + 1;
    char bitmap[bitmap_size];
    memset(bitmap, 0, bitmap_size); 

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
                if(is_new_pages_syscall) {
                    // Revert changes
                    clear_pd_entry(bitmap, bitmap_size, page_lowest_pd_index);
                    return num_pages_allocated;
                }
            }
        } else {
            // Page table is not present

            // Allocate a new page table
            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(new_pt == NULL) {
                // Revert changes 
                clear_pd_entry(bitmap, bitmap_size, page_lowest_pd_index);
                return ERROR_MALLOC_LIB;
            }

            // Clear
            memset(new_pt, 0, PAGE_SIZE);

            uint32_t pde_ctrl_bits = ctrl_bits_pde;

            // Change privilege level to user
            // Allow user mode access when set
            SET_BIT(pde_ctrl_bits, PG_US);

            *pde = ((uint32_t)new_pt | pde_ctrl_bits);

            int byte_index = (pd_index - page_lowest_pd_index)/8;
            int bit_index = (pd_index - page_lowest_pd_index)%8;
            SET_BIT(bitmap[byte_index], bit_index); 
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

    // Get page table lock
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
        // Acquire lock for the page table that
        // current page is in. Release previous
        // lock as we move move forward.
        uint32_t pd_index = GET_PD_INDEX(page);

        int cur_pt_lock_index = pd_index/NUM_PT_PER_LOCK;
        // If the current page is in a page table that's not covered
        // by the previous lock, get the lock that covers the current
        // page table.
        if(cur_pt_lock_index != pt_lock_index) {
            if(pt_lock_index != -1) {
                // Not the first time, release previous lock
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
            mutex_unlock(&pt_locks[pt_lock_index]);
            return ERROR_BASE_NOT_PREV;
        }

        // Check page table entry presence
        uint32_t pt_index = GET_PT_INDEX(page);
        pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
        pte_t *pte = &(pt->pte[pt_index]);

        if(!IS_SET(*pte, PG_P)) {
            // Not present
            mutex_unlock(&pt_locks[pt_lock_index]);
            return ERROR_BASE_NOT_PREV;
        }

        if(is_first_page) {
            // Make sure the first page is the base of a previous 
            // new_pages call.
            if(!IS_SET(*pte, PG_NEW_PAGES_START)) {
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
        }
        // Whether ZFOD, the frame was reserved at the time of allocation,
        // still need to update physical frame.
        unreserve_frames(1);

        is_finished = IS_SET(*pte, PG_NEW_PAGES_END);

        // Remove page table entry
        (*pte) = 0;

        // Invalidate tlb for page as we have changed page table entry
        asm_invalidate_tlb(page);

        page += PAGE_SIZE;
    }

    // Unlock lock for the page tables that the last page of the region is in
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
 *  and there's literally no page fault at all when it calls this function
 *  function, while if this function is called during page fault, there's
 *  error code to inspect).
 *
 *  @return 1 on true; 0 on false 
 */
int is_page_ZFOD(uint32_t va, uint32_t error_code, int need_check_error_code) 
{

    // To be eligible for ZFOD check, the fault must be 
    // a write from user space to a page that is present.
    if(need_check_error_code) {
        if(!(IS_SET(error_code, PG_P) && IS_SET(error_code, PG_US) && 
                    IS_SET(error_code, PG_RW))) {
            return 0;
        }
    }

    // Pages in kernel space are not marked as ZFOD, so there wouldn't
    // be confusion for permission.

    // Check the corresponding page table entry of the faulting address.
    // If it's marked as ZFOD, allocate a frame for it.
    uint32_t page = va & PAGE_ALIGN_MASK;

    pd_t *pd = (pd_t *)get_cr3();
    uint32_t pd_index = GET_PD_INDEX(page);

    // Get lock for the page table where the page is in
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
            // Since we have reserved allocation before, should have enough 
            // memory; else, something's wrong, should panic.
            uint32_t new_f = get_frames_raw();
            if(new_f == ERROR_NOT_ENOUGH_MEM) {
                panic("get_frames_raw failed in is_page_ZFOD");
                mutex_unlock(&pt_locks[pt_lock_index]);
            }

            // Set page table entry
            *pte = new_f | (*pte & (~PAGE_ALIGN_MASK));

            // Invalidate tlb for page as we have updated page table entry
            asm_invalidate_tlb(page);

            // Clear new frame
            memset((void *)page, 0, PAGE_SIZE);

            mutex_unlock(&pt_locks[pt_lock_index]);
            return 1;
        }
    }

    // Release page table lock
    mutex_unlock(&pt_locks[pt_lock_index]);
    return 0;

}



/** @brief Create an initial page directory along with page tables 
 *  for 16 MB kernel memory space, i.e., 0x0 to 0xffffff. 
 *  
 *  This function will be invoked only once when init_vm()
 *
 *  @return The new page directory base address
 */
static uint32_t init_pd() {

    // Cover kernel 16 MB space
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
        lprintf("smemalign() failed when init_pd()");
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
            lprintf("smemalign() failed when init_pd()");
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
            // Get page virtual address base the page table entry points to.
            uint32_t va_base = GET_VA_BASE(i, j);
            pt->pte[j] = (pte_t)(va_base | pte_ctrl_bits);
        }
    }

    return (uint32_t)pd;
}

/** @brief Set local apic mapping
 *   
 *  @param pd_base The page diretory base to set mapping
 *  @return void
 */
static void set_local_apic_translation(uint32_t pd_base) {

    uint32_t page = LAPIC_VIRT_BASE;

    uint32_t pd_index = GET_PD_INDEX(page);

    pd_t *pd = (pd_t *)pd_base;
    pde_t *pde = &(pd->pde[pd_index]);

    // Check page directory entry presence
    if(!IS_SET(*pde, PG_P)) {
        // not present
        // Impossible since we have allocated it.
        panic("Allocated page table not present?!");
    }

    // Set page table entry
    uint32_t pt_index = GET_PT_INDEX(page);
    pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
    pte_t *pte = &(pt->pte[pt_index]);

    // Get page table entry default bits
    uint32_t pte_ctrl_bits = ctrl_bits_pte;

    // Change privilege level to user
    // Allow user mode access when set
    SET_BIT(pte_ctrl_bits, PG_US);

    // Disable cache
    SET_BIT(pte_ctrl_bits, PG_PCD);

    uint32_t local_apic_addr = (uint32_t)smp_lapic_base();

    // Set page table entry
    *pte = local_apic_addr | pte_ctrl_bits;

}

/** @brief Distribute kernel heap memory
 *  
 *  Evenly distribute kernel heap memory among available cores
 *
 *  @return void
 */
void dist_kernel_mem() {

    // Get number of cores
    int num_cpus = smp_num_cpus();
    lprintf("Current number of cpus: %d", num_cpus);

    // Distribute kernel memory evenly among cores

    // Get amount of memory available in the pool
    vm_size_t kmem_avail = lmm_avail(&malloc_lmm, 0);
    void *smidge = NULL;
    while(1) {
        smidge = lmm_alloc(&malloc_lmm, kmem_avail, 0);
        if(smidge != NULL) break;
        kmem_avail -= sizeof(uint32_t);
    }

    vm_size_t kmem_per_core = kmem_avail / num_cpus;
    lprintf("kernel heap memory per core: %x", (unsigned)kmem_per_core);

    int i;
    for(i = 0; i < num_cpus; i++) {
        lmm_add_free(&core_malloc_lmm[i], smidge + i * kmem_per_core, 
                kmem_per_core);
        lprintf("add kernel memory %x bytes for cpu %d succeeded", 
                (unsigned)kmem_per_core, i);
    }
}

/** @brief Initilize virtual memory
 *
 *  Create a page directory that directly maps kernel space, set it as
 *  %cr3, and enable paging.
 *
 *  @return 0 on success; -1 on error
 */
static int init_vm_raw() {

    int num_cpus = smp_num_cpus();

    int i;
    // Create page directories for all cores on CPU0
    for(i = 0; i < num_cpus; i++) {
        uint32_t pd = init_pd();
        if(pd == ERROR_MALLOC_LIB) {
            return -1;
        }

        // Establish a translation from virtual address LAPIC_VIRT_BASE to the
        // local APIC's physical address
        set_local_apic_translation(pd);

        init_page_dir[i] = pd;
    }


    return 0;

}

/** @brief Adopt initial page directory and enable paging
  *
  * @param cur_cpu Current cpu index
  *
  * @return void
  *
  */
void adopt_init_pd(int cur_cpu) {

    set_cr3(init_page_dir[cur_cpu]);

    // Enable paging
    enable_paging();

    // Enable global page so that kernel pages in TLB wouldn't
    // be cleared when %cr3 is reset
    enable_pge_flag();
}

/** @brief Init virtual memory
 *  
 *  Create the first page directory to map kernel address space, 
 *  enable paging, and initialize physical memory manager.
 *
 *  @return 0 on success; negative integer on error
 */
int init_vm() {

    // Configure default page table entry and page diretory entry control bits
    init_pg_ctrl_bits();

    if(init_vm_raw() < 0) {
        lprintf("init_vm_raw failed");
        return -1;
    }

    adopt_init_pd(0);

    // Allocate a system-wide all-zero frame to do ZFOD later
    void *new_f = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(new_f == NULL) {
        lprintf("smemalign() failed when init_vm()");
        return ERROR_MALLOC_LIB;
    }
    // Clear
    memset(new_f, 0, PAGE_SIZE);
    all_zero_frame = (uint32_t)new_f;

    // Init user space physical memory manager
    int ret = init_pm();

    return ret;
}

/** @brief Create a new address space that only covers kernel space
 *
 *  @return New page directory base
 */
uint32_t create_pd() {

    // Only create a new page diretory
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
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
 *  of the frames of the address space to clone, while page table entries in 
 *  kernel space still point to the same frames of the address space to clone.
 *
 *  This function is called by fork(), and as fork is not permitted when 
 *  a task is multithreading, so there's no need to acquire page table
 *  locks when performing this operation. On failure, resources allocated
 *  for creating the new address space will be all freed.
 *
 *  @return The new page directory base address on success, a negative integer
 *  on error.
 */
uint32_t clone_pd() {
    // The pd to clone
    pd_t *old_pd = (pd_t *)get_cr3();

    // Number of pages allocated in the user space of the task
    int num_pages_allocated = count_pages_user_space();

    // Reserve all frames that will be needed
    if(reserve_frames(num_pages_allocated) < 0) {
        return ERROR_NOT_ENOUGH_MEM;
    }

    /* The following code creates a new address space */

    // A buffer to copy contents between frames
    char *frame_buf = malloc(PAGE_SIZE);
    if(frame_buf == NULL) {
        return ERROR_MALLOC_LIB;
    }

    // Clone pd
    pd_t *pd = smemalign(PAGE_SIZE, PAGE_SIZE);
    if(pd == NULL) {
        free(frame_buf);
        return ERROR_MALLOC_LIB;
    }
    memset(pd, 0, PAGE_SIZE);

    int i, j;
    for(i = 0; i < PAGE_SIZE/ENTRY_SIZE; i++) {
        if(IS_SET(old_pd->pde[i], PG_P)) {
            // Use the same page tables and frames for kernel space
            if(i < NUM_PT_KERNEL) {
                // Clone page table entry
                memcpy(&pd->pde[i], (void *)&old_pd->pde[i], ENTRY_SIZE);
                continue;
            }

            // Clone page table
            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(new_pt == NULL) {
                // Revert changes and free resources
                free(frame_buf);
                int need_unreserve_frames = 0;
                free_entire_space((uint32_t)pd, need_unreserve_frames);

                // Need to unreserve frames here since only here do we 
                // know how many frames we have reserved, and when 
                // free_entire_space traverses page directory, it doesn't
                // know the exact number of frames to unreserve because
                // we get stuck when cloning and the page directory isn't
                // complete.
                unreserve_frames(num_pages_allocated);
                return ERROR_MALLOC_LIB;
            } 
            uint32_t old_pt_addr = old_pd->pde[i] & PAGE_ALIGN_MASK;
            memcpy((void *)new_pt, (void *)old_pt_addr, PAGE_SIZE);
            pd->pde[i] = (uint32_t)new_pt | GET_CTRL_BITS(old_pd->pde[i]);

            // Clone frames
            pt_t *pt = (pt_t *)new_pt;
            for(j = 0; j < PAGE_SIZE/ENTRY_SIZE; j++) {
                if(IS_SET(pt->pte[j], PG_P)) {

                    uint32_t old_frame_addr = pt->pte[j] & PAGE_ALIGN_MASK;

                    // Allocate a new frame
                    uint32_t new_f = get_frames_raw();

                    // Find out the corresponding virtual address that the
                    // current page table entry points to.
                    uint32_t va = GET_VA_BASE(i, j);
                    // Copy the content in the frame to a buffer
                    memcpy(frame_buf, (void *)va, PAGE_SIZE);

                    // Temporarily change the frame where the old page table 
                    // entry points to to the new frame so that we can copy 
                    // contents to the new frame by enabling mapping of the
                    // new frame.
                    ((pt_t *)old_pt_addr)->pte[j] = 
                        new_f | GET_CTRL_BITS(pt->pte[j]);
                    // Invalidate page in tlb as we update page table entry
                    asm_invalidate_tlb(va);
                    memcpy((void *)va, frame_buf, PAGE_SIZE);
                    // Change back old page table entry to point to the old
                    // frame.
                    ((pt_t *)old_pt_addr)->pte[j] = 
                        old_frame_addr | GET_CTRL_BITS(pt->pte[j]);
                    // Invalidate page in tlb as we update page table entry
                    asm_invalidate_tlb(va);

                    // Let new address space's page table entry point to the 
                    // newly allocated frame.
                    pt->pte[j] = new_f | GET_CTRL_BITS(pt->pte[j]);
                }
            }
        }
    }

    // Free allocated buffer
    free(frame_buf);
    return (uint32_t)pd;
}


/** @brief Free entire address space (kernel and user)
 *
 *  @param pd_base The address space's page directory base
 *  @param need_unreserve_frames Flag indicating if there's need to
 *  unreserve frames while freeing. (Since there are situations 
 *  where frames are unreserved in a different place, like clone_pd());
 *
 *  @return Void
 */
void free_entire_space(uint32_t pd_base, int need_unreserve_frames) {
    // Free user space
    free_space(pd_base, 0, need_unreserve_frames);

    // Free kernel space
    free_space(pd_base, 1, need_unreserve_frames);

    // Free page directory
    sfree((void *)pd_base, PAGE_SIZE);

}


/** @brief Free address space
 *
 *  @param pd_base The address space's page directory base
 *
 *  @param is_kernel_space 1 for kernel space; 0 for user space
 *
 *  @param need_unreserve_frames Flag indicating if there's need to
 *  unreserve frames while freeing. (This is needed since there are situations 
 *  where frames are unreserved in a different place, like clone_pd());
 *
 *  @return 0 on success; a negative integer on error
 */
void free_space(uint32_t pd_base, int is_kernel_space, 
        int need_unreserve_frames) {

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
                        }

                        if(need_unreserve_frames) {
                            // Whether ZFOD, the frame was reserved at the 
                            // time of allocation, still need to unreserve.
                            unreserve_frames(1);
                        }

                    }
                }
            }

            // Free page table only for user space, since kernel page tables
            // are shared across tasks and are never freed.
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
 *  program later. In case any failure happens like not enough kernel memory
 *  for page tables during the allocating process, all resources already
 *  allocated in the life span of this call will be freed and changes will be
 *  reverted back.
 *
 *  @param va The virtual address of the base of the region to allocate
 *  @param size_bytes The size of the region
 *  @param rw_perm The read-write permission of the region to set, 1 as read 
 *  write, 0 as read-only
 *  @param is_new_pages_syscall Flag showing if the caller is the new_pages 
 *  system call
 *  @param is_ZFOD Flag showing if the region should use ZFOD
 *
 *  @return 0 on success; negative integer on error
 */
int new_region(uint32_t va, int size_bytes, int rw_perm, 
        int is_new_pages_syscall, int is_ZFOD) {

    // Enable mapping from the page where the first byte of region
    // is in, to the one where the last byte of the region is in.

    // Need to traverse new region first to know how many new frames
    // are needed, because some part of the region may already have
    // been allocated.

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;
    uint32_t page_highest = (va + (uint32_t)size_bytes - 1) &
        PAGE_ALIGN_MASK;

    // Acquire page table lock only for new pages syscall, since there's
    // no concurrency problem when kernel calls new_region to load tasks
    // (the initial thread in the new task owns the address space solely)

    uint32_t page_lowest_pd_index = GET_PD_INDEX(page_lowest);
    uint32_t page_highest_pd_index = GET_PD_INDEX(page_highest);

    // The lock index for the lowest page table that region is in.
    // For example, if NUM_PT_PER_LOCK is 8, then page table 0 - 7
    // has lock index 0, page table 8 - 15 has lock index 1
    int lowest_pt_lock_index = page_lowest_pd_index/NUM_PT_PER_LOCK;
    int highest_pt_lock_index = page_highest_pd_index/NUM_PT_PER_LOCK;
    int num_pt_lock = highest_pt_lock_index - lowest_pt_lock_index + 1;

    mutex_t *pt_locks;
    if(is_new_pages_syscall) {
        // Only need lock when serving new_pages_syscall, since in the other 
        // case where kernel calls this function to allocate initial space 
        // for a task, the address space is owned solely by the task at the
        // time.

        // Get page table locks that cover the region
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


    // Count the number of pages allocated in the region and allocate page 
    // tables to cover the region if necessary. Upon completion of the 
    // count_pages_allocated() call, all page tables needed for the region
    // will be availbale on success.
    int num_pages_allocated = count_pages_allocated(va, size_bytes, 
            is_new_pages_syscall);
    if(num_pages_allocated < 0) {
        // Not enough kernel memory to allocate page tables
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

    if(is_new_pages_syscall && num_pages_allocated > 0) {
        // Some pages have already been allocated in this address space,
        // which is an error for new_pages syscall, and should not proceed,
        // while if kernel calls this function to allocate initial space
        // for tasks, it's OK to have regions overlap (e.g., rodata and text
        // segments may overlap with each other.)
        int i;
        int pt_lock_index = highest_pt_lock_index;
        for(i = 0; i < num_pt_lock; i++) {
            mutex_unlock(&pt_locks[pt_lock_index]);
            pt_lock_index--;
        }

        return ERROR_OVERLAP;
    }

    // Number of pages in the region
    int count = 1 + (page_highest - page_lowest) / PAGE_SIZE;

    // Reserve number of frames that will be used in the worst case in the 
    // future, since pages marked as ZFOD now is likely to be requested to
    // be writable in the future, and it's strange to tell the requester that
    // no physical memory is availale at that time, so that we should count 
    // the frames as allocated now.
    if(reserve_frames(count - num_pages_allocated) == -1) {
        if(is_new_pages_syscall) {
            int i;
            int pt_lock_index = highest_pt_lock_index;
            for(i = 0; i < num_pt_lock; i++) {
                mutex_unlock(&pt_locks[pt_lock_index]);
                pt_lock_index--;
            }
        }
        return ERROR_NOT_ENOUGH_MEM;
    }

    // Traverse region to enable mappings.
    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();

    int i;
    for(i = 0; i < count; i++) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(!IS_SET(*pde, PG_P)) {
            // not present
            // Impossible since we have allocated it in 
            // count_pages_allocated().
            panic("Allocated page table not present?!");
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
                // The page is the base of the region that new_pages() requests
                if(page == page_lowest) {
                    SET_BIT(pte_ctrl_bits, PG_NEW_PAGES_START);
                }
                // The page is the last one in the region new_pages() requests
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
                // Set rw permission as specified
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

    // Unlock locks
    if(is_new_pages_syscall) {
        int i;
        int pt_lock_index = highest_pt_lock_index;
        // Unlock locks of the page tables that cover the region
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
 * @return 0 on success; a negative integer on error
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
 * Will call remove_region to do the actual work.
 *
 * @param base The The virtual address of the base of the start page of
 * contiguous pages.
 *
 * @return 0 on success; a negative integer on error 
 */
int remove_pages(void *base) {

    return remove_region((uint32_t)base);

}


/** @brief Check user space memory validness
 *
 *  Can check if region are allocated, NULL terminated, writable
 *  and ZFOD as specified by parameters.
 *
 *  @param va The virtual address of the start of the region
 *  @param max_bytes Max bytes to check
 *  @param is_check_null If 1, then check if a '\0' is encountered before 
 *  max_bytes are encountered
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

    // Get page table lock
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

    // Release page table locks covered by region
    pt_lock_index = highest_pt_lock_index;
    for(i = 0; i < num_pt_lock; i++) {
        mutex_unlock(&pt_locks[pt_lock_index]);
        pt_lock_index--;
    }
    return ret;
}

