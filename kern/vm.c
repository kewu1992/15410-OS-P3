/** @file vm.c
 *  @brief Implements virtual memory.
 *
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <vm.h>
#include <pm.h>
#include <list.h>

void asm_invalidate_tlb(uint32_t va);

/*
   All frames are grouped into 11 lists of blocks that contain groups of
   1, 2, 4, 8, 16, ..., 512, 1024 contiguous framse, repectively
   meaning 1*PAGE_SIZE, 2*PAGE_SIZE, ..., 1024*PAGE_SIZE per block size 
   in each group, 4k, 8k, ..., 4m

   The address of the 1st frame of a block is a multiple of the group size
*/

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
        return ERROR_MALLOC_LIB;
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

    // Enable paging
    enable_paging();

    // Enable global page so that kernel pages in TLB wouldn't
    // be cleared when %cr3 is reset
    enable_pge_flag();

    // Init buddy system to track frames in user address space
    init_pm();

    //test_frames();
    test_vm();

    lprintf("Paging is enabled!");

    return 0;
}

/** @brief Count number of pages allocated in user space
 *
 *
 */
int count_pages_user_space() {
    int num_pages_allocated = 0;
    pd_t *pd = (pd_t *)get_cr3();

    int i, j;
    // i = 4, starts from user space
    for(i = 4; i < PAGE_SIZE/4; i++) {
        if((pd->pde[i] & (1 << PG_P)) == 1) {

            pt_t *pt = (pt_t *)(pd->pde[i] & PAGE_ALIGN_MASK);
            for(j = 0; j < PAGE_SIZE/4; j++) {
                if((pt->pte[j] & (1 << PG_P)) == 1) {
                    num_pages_allocated++;
                }
            }
        }
    }

    return num_pages_allocated;
}

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


/** @brief Free pages allocated in this user space
 *
 *
 */
int free_user_space() {

    lprintf("free_user_space is called");

    pd_t *pd = (pd_t *)get_cr3();

    uint32_t frame_start = 0;
    uint32_t cur_len = 0;

    int i, j;
    // i = 4, starts from user space
    for(i = 4; i < PAGE_SIZE/4; i++) {
        if((pd->pde[i] & (1 << PG_P)) == 1) {

            uint32_t pt_addr = pd->pde[i] & PAGE_ALIGN_MASK;
            pt_t *pt = (pt_t *)pt_addr;
            for(j = 0; j < PAGE_SIZE/4; j++) {
                if((pt->pte[j] & (1 << PG_P)) == 1) {

                    uint32_t cur_frame = pt->pte[j] & PAGE_ALIGN_MASK;
                    if(cur_len == 0) {
                        frame_start = cur_frame;
                        cur_len = 1;
                    } else if(cur_frame != frame_start + cur_len * PAGE_SIZE) {
                        // Free contiguous frames described by frame_start and cur_len
                        int ret = free_contiguous_frames(frame_start, cur_len);
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
        free_contiguous_frames(frame_start, cur_len);
    }

    lprintf("free_user_space finished");

    return 0;

}

/** @brief Count number of pages allocated in region
 *
 *
 */
int count_pages_allocated(uint32_t va, int size_bytes) {
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
        if(((*pde) & (1 << PG_P)) == 1) {
            // Present

            // Check page table entry presence
            uint32_t pt_index = GET_PT_INDEX(page);
            pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
            pte_t *pte = &(pt->pte[pt_index]);

            if(((*pte) & (1 << PG_P)) == 1) {
                // Present
                num_pages_allocated++;
            }
        }

        page += PAGE_SIZE;
    }


    return num_pages_allocated;
}


/** @brief Enable mapping for a region in user space (0x1000000 upwards)
 *
 *  The privilege level would be set as User level.
 *
 *  @param va The virtual address the region starts with
 *  @param size_bytes The size of the region
 *  @param rw_perm The rw permission of the region, 1 as rw, 0 as ro
 *  @param is_new_pages_syscall If the caller is the new_pages system call
 *
 *  @return 0 on success; -1 on error
 */
int new_region(uint32_t va, int size_bytes, int rw_perm, 
        int is_new_pages_syscall) {
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

        count -= num_pages_allocated;
    }
    
    list_t list;
    if(get_frames(count, &list) == -1) {
        return -1;
    }
    int frames_left = 0;
    uint32_t cur_frame = 0;

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

        if(((*pte) & (1 << PG_P)) == 0) {
            // Not present
            // Allocate a new page
            if(frames_left == 0) {
                uint32_t *data = list_remove_first(&list);
                frames_left = data[0];
                cur_frame = data[1];
            } 

            frames_left--;
            uint32_t new_f = cur_frame;
            cur_frame += PAGE_SIZE;

            uint32_t pte_ctrl_bits = get_pg_ctrl_bits(1);
            // Set rw permission
            rw_perm ? SET_BIT(pte_ctrl_bits, PG_RW) :
                CLR_BIT(pte_ctrl_bits, PG_RW);

            // Change privilege level to user
            // Allow user mode access when set
            SET_BIT(pte_ctrl_bits, PG_US);

            // new_pages system call related
            if(is_new_pages_syscall) {
                if(page == page_lowest) {
                    SET_BIT(pte_ctrl_bits, PG_NEW_PAGES_START);
                }

                if(page == page_highest) {
                    SET_BIT(pte_ctrl_bits, PG_NEW_PAGES_END);
                }
            }

            *pte = ((uint32_t)new_f | pte_ctrl_bits);
            // Clear page
            memset((void *)page, 0, PAGE_SIZE);
        }

        page += PAGE_SIZE;
    }

    list_destroy(&list, TRUE);

    return 0;
}


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
    for(i = 0; i < PAGE_SIZE/4; i++) {
        if((pd->pde[i] & (1 << PG_P)) == 1) {
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
            if(i < 4) {
                continue;
            }

            // Clone frames
            pt_t *pt = (pt_t *)new_pt;
            for(j = 0; j < PAGE_SIZE/4; j++) {
                if((pt->pte[j] & (1 << PG_P)) == 1) {

                    uint32_t old_frame_addr = pt->pte[j] & PAGE_ALIGN_MASK;
                    
                    //uint32_t new_f = new_frame();
                    if(frames_left == 0) {
                        uint32_t *data = list_remove_first(&list);
                        frames_left = data[0];
                        cur_frame = data[1];
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

    return (uint32_t)pd;
}

// Called by remove_pages system call
int remove_region(uint32_t va) {

    uint32_t page_lowest = va & PAGE_ALIGN_MASK;

    uint32_t frame_start = 0;
    uint32_t cur_len = 0;

    uint32_t page = page_lowest;
    pd_t *pd = (pd_t *)get_cr3();

    while(1) {
        uint32_t pd_index = GET_PD_INDEX(page);
        pde_t *pde = &(pd->pde[pd_index]);

        // Check page directory entry presence
        if(((*pde) & (1 << PG_P)) == 0) {
            // Not present
            lprintf("pde not present, page: %x", (unsigned)page);
            return ERROR_BASE_NOT_PREV;
        }

        // Check page table entry presence
        uint32_t pt_index = GET_PT_INDEX(page);
        pt_t *pt = (pt_t *)((*pde) & PAGE_ALIGN_MASK);
        pte_t *pte = &(pt->pte[pt_index]);

        if(((*pte) & (1 << PG_P)) == 0) {
            // Not present
            lprintf("pte not present, page: %x", (unsigned)page);
            return ERROR_BASE_NOT_PREV;
        }

        uint32_t cur_frame = (*pte) & PAGE_ALIGN_MASK;
        if(cur_len == 0) {
            if(!IS_SET(*pte, PG_NEW_PAGES_START)) {
                lprintf("PG_NEW_PAGES_START not set, first page: %x", 
                        (unsigned)page);
                return ERROR_BASE_NOT_PREV;
            }
            frame_start = cur_frame;
            cur_len = 1;
        } else if(cur_frame != frame_start + cur_len * PAGE_SIZE) {
            // Free contiguous frames described by frame_start and cur_len
            int ret = free_contiguous_frames(frame_start, cur_len);
            if(ret < 0) {
                lprintf("free_contiguous_frames failed, page: %x", 
                        (unsigned)page);
                return ret;
            }

            frame_start = cur_frame;
            cur_len = 1;
        } else {
            cur_len++;
        }


        if(IS_SET(*pte, PG_NEW_PAGES_END)) {
            //lprintf("PG_NEW_PAGES_END found, page: %x", 
            //        (unsigned)page);
            // Free last page in this region
            int ret = free_contiguous_frames(frame_start, cur_len);
            if(ret < 0) {
                lprintf("free_contiguous_frames failed, page: %x", 
                        (unsigned)page);
            }
            // Remove page table entry
            (*pte) = 0;
            return ret;
        }

        // Remove page table entry
        (*pte) = 0;

        page += PAGE_SIZE;
    }

    return 0;
}


/**
  * @brief new_pages syscall
  *
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
    int ret = new_region((uint32_t)base, len, 1, TRUE);    
    return ret;

}

/**
  * @brief remove_pages syscall
  *
  */
int remove_pages(void *base) {

    return remove_region((uint32_t)base);

}

