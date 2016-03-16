/** @file vm.c
 *  @brief Implements virtual memory.
 *
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */

#include <vm.h>
#include <list.h>
#include <hashtable.h>

void asm_invalidate_tlb(uint32_t va);

// Dummy frame counter
static int frame_counter;

struct free_area_struct {
    /* @brief a doubly linked list of blocks */
    list_t list;
};

// Max block size is 2^(MAX_ORDER-1)*PAGE_SIZE
// Blocks are of size 4K, 8K, ..., 4M, given PAGE_SIZE is 4K
#define MAX_ORDER 11

struct free_area_struct free_area[MAX_ORDER];
//hashtable_t free_block_ht;

/*
// A map that stores the information of whether a block is free,
// and how large it is.
// Free block map: base-> {order}
// API: 
// uint8_t order = free_block_map_get(base);
// void free_block_map_put(uint32_t base, uint8_t order);
// void free_block_map_delete(uint32_t base);

int free_block_map_get(uint32_t base);
void free_block_map_put(uint32_t base, int order);
void free_block_map_delete(uint32_t base);
*/

/*
#define FREE_BLOCK_HT_SIZE 97
int free_block_ht_hashfunc(void *key) {
    return (uint32_t)key % FREE_BLOCK_HT_SIZE;
}

int free_block_ht_init() {
    free_block_ht.size = FREE_BLOCK_HT_SIZE; 
    free_block_ht.func = free_block_ht_hashfunc; 
    return hashtable_init(&free_block_ht);
}
*/

// Manage continugous frame allocation
void init_buddy_system() {
    // Init MAX_ORDER lists
    int i;
    for(i = 0; i < MAX_ORDER; i++) {
        if(list_init(&free_area[i].list)) {
            lprintf("list_init failed");
            panic("list_init failed");
        }
    }

    /*
    // Init free block hashtable
    if(free_block_ht_init() == -1) {
        lprintf("free_block_ht_init failed");
        panic("free_block_ht_init failed");
    }
    */

    // Populate the list of the largest order with the
    // available frames (not counting those in kernel space), 
    // which are grouped into blocks of 2^(MAX_ORDER-1) pages
    int avail_frame_count = machine_phys_frames() - 
        USER_MEM_START/PAGE_SIZE;

    // User space base
    uint32_t base = USER_MEM_START;
    // Number of frames per group of the largest order
    uint32_t num_frames = 1 << (MAX_ORDER - 1);
    // Block size of the group of the largest order
    uint32_t block_size = num_frames * PAGE_SIZE;
    for(i = 0; i < avail_frame_count/num_frames; i++) {
        list_append(&(free_area[MAX_ORDER - 1].list), (void *)base);
        base += block_size;

    // hashtable_put(&free_block_ht, (void *)base, (void *)(MAX_ORDER - 1));
    }

}

/**
 * @brief Get contiguous frames
 *
 * @param order Size of contiguous frames. 
 * Valid choices are 0, 1, 2, 3, ..., MAX_ORDER - 1
 * 
 * Example usage:
 *      uint32_t new_frame_base = get_frames(i);
 *      get 2^i pages of contiguous frames
 * 
 * @return Base of contiguous frames on success; 3 on failure
 */
uint32_t get_frames(int order) {


    lprintf("order: %d", order);
    // Check free block lists
    int cur_order = order; 
    while(cur_order < MAX_ORDER) {
        void *block = list_remove_first(&(free_area[cur_order].list));

        //if(block == NULL) {
        if(block == (void *)0xDEADBEEF) {
            // Try finding free block in a larger size group
            cur_order++;
        } else {
            uint32_t free_block_base = (uint32_t)block;
            // We have found a free block
            if(cur_order > order) {
                // But it's too large, we will split it first
                // Put unused halves back to lists
                uint32_t block_size = (1 << cur_order) * PAGE_SIZE;
                uint32_t block_base = free_block_base + block_size;
                while(cur_order > order) {
                    block_size >>= 1;
                    block_base -= block_size;
                    // Place unused half to the free list of one order less
                    list_append(&(free_area[cur_order - 1].list), 
                            (void *)block_base);
                //    hashtable_put(&free_block_ht, (void *)block_base,
                //            (void *)(cur_order - 1));
                    cur_order--;
                }
            } 

            /*
            int is_find;
            hashtable_remove(&free_block_ht, (void *)free_block_base,
                    &is_find);
            if(!is_find) {
                lprintf("get_frames: hashtable_remove failed, element doesn't exist");
                MAGIC_BREAK;
                return 3;
            }
            */
            return free_block_base;
        }
    }

    // Failed to find a contiguous block of the size we want
    return 3;
}

/*
void free_frames(uint32_t base, int order) {

    // Iteratively merge block with its buddy to the highest order possible
    while(order < MAX_ORDER) {
        if(order == MAX_ORDER - 1) {
            // block is of the highest order, no way to merge any more
            // Put block to its free list and stop
            list_append(&(free_area[order].list), (void *)base);
            hashtable_put(&free_block_ht, (void *)base, (void *)order);
            return;
        }

        // Get buddy's base
        uint32_t buddy_base = (base/PAGE_SIZE) ^ (1 << order) * PAGE_SIZE;

        // Check if buddy is free and of the same order
        int is_find;
        int buddy_order = (int)hashtable_get(&free_block_ht, 
                (void *)buddy_base, &is_find);
        if(is_find == 0 || buddy_order != order) {
            // Buddy isn't free, or the free part of it isn't of 
            // the same size, so can't merge with it
            // Put block to its free list and stop
            list_append(&(free_area[order].list), (void *)base);
            hashtable_put(&free_block_ht, (void *)base, (void *)order);
            return;
        }
*/
        /* Merge with buddy */
        // Delete buddy from free block map and free list
/*       hashtable_remove(&free_block_ht, (void *)buddy_base, &is_find);
        if(!is_find) {
            lprintf("hashtable_remove failed, element doesn't exist");
            return;
        }
        list_delete(&(free_area[order].list), (void *)buddy_base);
        base = base < buddy_base ? base : buddy_base;
        order++;
    }

}
*/

/*
   for each frame, need to track:

   The buddy system should expose 2 APIs:
   get_free_frames()
   free_frames()

   All frames are grouped into 11 lists of blocks that contain groups of
   1, 2, 4, 8, 16, ..., 512, 1024 contiguous framse, repectively
   meaning 1*PAGE_SIZE, 2*PAGE_SIZE, ..., 1024*PAGE_SIZE per block size 
   in each group, 4k, 8k, ..., 4m

   The address of the 1st frame of a block is a multiple of the group size
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

    // Enable paging
    enable_paging();

    // Enable global page so that kernel pages in TLB wouldn't
    // be cleared when %cr3 is reset
    enable_pge_flag();

    // Init buddy system to track frames in user address space
    init_buddy_system();

    lprintf("Paging is enabled!");

    return 0;
}

/** @brief Get next frame from a new_frames_t
  *
  * @param
  * @return Address of the next frame in the frames list
  *
  */
uint32_t get_next_frame(new_frames_t *new_frames_struct) {

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

    lprintf("va: 0x%x, count:%d", (unsigned)va, count);
    // count = 2^i + 2^j + ...
    // frames = get_frames(order);
    // Apply for new frames
    new_frames_t *new_frames_struct = get_frames_list(count); 
    if(new_frames_struct == NULL) {
        // Insufficient memory
        return -1;
    }

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
            // uint32_t new_f = new_frame();
            // uint32_t new_f = get_frames(0);

            // Get next frame from the frames list
            uint32_t new_f = get_next_frame(new_frames_struct);

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

    /* The following code creates a new address space */

    // A buffer to copy contents between frames
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
            // Clone pt
            void *new_pt = smemalign(PAGE_SIZE, PAGE_SIZE);
            if(pd == NULL) {
                lprintf("smemalign failed");
                panic("smemalign failed");
            }
            uint32_t old_pt_addr = pd->pde[i] & PAGE_ALIGN_MASK;
            memcpy((void *)new_pt, (void *)old_pt_addr, PAGE_SIZE);
            pd->pde[i] = (uint32_t)new_pt | GET_CTRL_BITS(pd->pde[i]);

            // Clone frames
            pt_t *pt = (pt_t *)new_pt;
            for(j = 0; j < PAGE_SIZE/4; j++) {
                if((pt->pte[j] & (1 << PG_P)) == 1) {

                    // Use the same frames for kernel space
                    if(i < 4) {
                        continue;
                    }

                    uint32_t old_frame_addr = pt->pte[j] & PAGE_ALIGN_MASK;
                    //uint32_t new_f = new_frame();
                    uint32_t new_f = get_frames(0);


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

