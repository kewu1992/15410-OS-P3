/**
 * @brief Physical memory manager
 * 
 * The physical frame manager uses the similar idea as linux's buddy system to
 * manage free frames. All frames are grouped into blocks where each block 
 * consists of contiguous frames of size that is 2's power of PAGE_SIZE. 
 * There are 11 free lists that manage contiguous frames size of 2^0 * PAGE, 
 * 2^1 * PAGE_SIZE, ..., 2^10 * PAGE_SIZE. We treat the number 0, 1, ..., 11
 * as order. Initially, all free physical frames are divided into 2^10 * 
 * PAGE_SIZE blocks and put into the list that manages block size of 2^10 * 
 * PAGE_SIZE. When certain number of frames are requested, say, 16, the 
 * manager will look into the free list that has free blocks of size 2^4 * 
 * PAGE_SIZE. If there's an available block, then remove it from the list, 
 * returning its base to the requester; else, the frame manager will try 
 * looking into the free list that is one order higher, i.e., the free list
 * that has free blocks of size 2^5 * PAGE_SIZE, and if there's a hit, 
 * divide the 2^5 * PAGE_SIZE block into halves, put back one half to the 
 * free list that has block size of 2^4 * PAGE_SIZE, and return the other
 * half to the requester. When a block is later freed, the block will be
 * put back to the free list that manages blocks whose order are the same
 * as the block to free. When a block is put back to a free list, it will
 * check its buddy's status. A buddy for a block is defined to be another 
 * block that is of the same size and is ajacent to the block, and either 
 * the buddy, or the block's base address is a multiple of the block size.
 * If a block to be freed finds out that its buddy is in the free list,
 * the two will merge, be removed from the current free list and be added
 * to the free list that has a higher order. The process will iterate until
 * buddy isn't free or reaching the free list of the highest order.
 *
 * @author Jian Wang (jianwan3)
 * @author Ke Wu (kewu)
 *
 * @bug No known bugs.
 */

#include <pm.h>


/** @brief An array of lists that tracks free frames of different size */
static free_list_t free_list[MAX_ORDER];


/** @brief Number of free frames currently available in physical memory */
static int num_free_frames;


/**
 * @brief Get contiguous frames
 *
 * @param order Size of contiguous frames. 
 * Valid choices are 0, 1, 2, 3, ..., MAX_ORDER - 1
 * 
 * Example usage:
 *      uint32_t new_frame_base = get_frames(i);
 *      will get 2^i pages of contiguous frames
 * 
 * @return Base of contiguous frames on success; negative integer on error
 */
static uint32_t get_frames_raw(int order) {

    // lprintf("order: %d", order);
    // Check free block lists
    int cur_order = order; 
    while(cur_order < MAX_ORDER) {

        uint32_t *free_block_base;
        if(list_remove_first(&(free_list[cur_order].list), 
                    (void **)(&free_block_base))) {
            // Try finding free block in a larger size group
            cur_order++;
        } else {
            // We have found a free block
            if(cur_order > order) {
                // But it's too large, we will split it first
                // Put unused halves back to lists
                uint32_t block_size = (1 << cur_order) * PAGE_SIZE;
                uint32_t block_base = (uint32_t)free_block_base + block_size;
                while(cur_order > order) {
                    block_size >>= 1;
                    block_base -= block_size;
                    // Place unused half to free list of one order less
                    int ret = 
                        list_append(&(free_list[cur_order - 1].list),
                                (void *)block_base);
                    if(ret < 0) return ret;
                    cur_order--;
                }
            } 

            return (uint32_t)free_block_base;
        }
    }

    // Failed to find a contiguous block of the size we want
    return ERROR_NOT_ENOUGH_MEM; 
}

/**
 * @brief Free contiguous frames
 *
 * @param base The base of contiguous frames to free
 * @param order Size of contiguous frames. 
 * Valid choices are 0, 1, 2, 3, ..., MAX_ORDER - 1
 * 
 * 
 * @return 0 on success; negative integer on error
 */
static int free_frames_raw(uint32_t base, int order) {

    // lprintf("free_frames: base: %x, order: %d", (unsigned)base, order);
    // Iteratively merge block with its buddy to the highest order possible
    while(order < MAX_ORDER) {
        if(order == MAX_ORDER - 1) {
            // block is of the highest order, no way to merge any more
            // Put block to its free list and stop
            int ret =list_append(&(free_list[order].list), (void *)base);
            if(ret < 0) return ret;
            break;
        }

        // Get buddy's base
        uint32_t buddy_base = ((((base - USER_MEM_START)/PAGE_SIZE) ^ 
                    (1 << order)) * PAGE_SIZE) + USER_MEM_START;

        if(list_delete(&(free_list[order].list), (void *)buddy_base) == -1) {

            // Buddy of the same size isn't free, can't merge with it
            // Put block to its free list and stop
            int ret = list_append(&(free_list[order].list), (void *)base);
            if(ret < 0) return ret;
            break;
        }

        // Merge with buddy
        base = base < buddy_base ? base : buddy_base;

        order++;
    }

    return 0;
}


/****** Public interface for the physical memory manager ********/

/**
 * @brief Initialize physical memory manager
 *  
 * @return 0 on success; negative integer on error
 */
int init_pm() {
    // Init MAX_ORDER lists
    int i;
    for(i = 0; i < MAX_ORDER; i++) {
        if(list_init(&free_list[i].list)) {
            lprintf("list_init failed");
            return -1;
        }
    }

    // Populate the list of the largest order with the
    // available frames (not counting those in kernel space), 
    // which are grouped into blocks of 2^(MAX_ORDER-1) pages
    num_free_frames = machine_phys_frames() - 
        USER_MEM_START/PAGE_SIZE;

    // User space base
    uint32_t base = USER_MEM_START;
    // Number of frames per group of the largest order
    uint32_t num_frames = 1 << (MAX_ORDER - 1);
    // Block size of the group of the largest order
    uint32_t block_size = num_frames * PAGE_SIZE;
    for(i = 0; i < num_free_frames/num_frames; i++) {
        int ret = 
            list_append(&(free_list[MAX_ORDER - 1].list), (void *)base);
        if(ret != 0) return ret;

        base += block_size;
    }

    return 0;
}


/**
 * @brief Get frames. Wrapper for get_frames_raw.
 *
 * @param count The number of frames requested. (Not necessary 2's power)
 *
 * @param list The list to store the result. 
 * The data field of each node of result list is a 2D array, the size of the 
 * frame blocks and the base of the block. For instance, if 17 frames are 
 * requested, then the result list may contain 2 nodes to represent 2 blocks, 
 * where the first node's data[0] is 4, which means the block size is 
 * 2^4 * PAGE_SIZE, data[1] is the block's base; the second node's data[1] is 
 * 0, which means the block size is 2^0 * PAGE_SIZE, data[1] is the block's 
 * base. So the 17 frames are consisted of two blocks.
 *  
 * @return 0 on success; negative integer on error
 */
int get_frames(int count, list_t *list) {

    // Compare count with current number of free frames availale
    if(count > num_free_frames) {
        return -1;
    }

    // Decrease counter now
    num_free_frames -= count;
    int count_left = count;

    if(list_init(list) == -1) {
        return -1;
    }

    // count = 19
    // 16 + 2 + 1
    int i, j;
    for(i = MAX_ORDER-1; i >= 0 && count_left > 0; i--) {
        uint32_t cur_size = 1 << i;

        int num = count_left / cur_size;
        for(j = 0; j < num; j++) {
            uint32_t new_frame = get_frames_raw(i);
            if(new_frame == ERROR_NOT_ENOUGH_MEM) {
                return -1;
            }
            //        lprintf("In get_frames: get_frames_raw ret: %x",
            //                (unsigned)new_frame);
            uint32_t *data = malloc(2*sizeof(uint32_t));
            if(data == NULL) {
                list_destroy(list, TRUE);
                return ERROR_MALLOC_LIB;
            }
            data[0] = cur_size;
            data[1] = new_frame;
            int ret = list_append(list, data);
            if(ret != 0) return ret;

            count_left -= cur_size;
        }
    }

    return 0;
}

/**
 * @brief Free frames. Wrapper for free_frames_raw.
 *
 * @param base The base of the block to free
 * @param count The number of frames to free (Not necessary 2's power)
 *  
 * @return 0 on success; negative integer on error
 */
int free_frames(uint32_t base, int count) {

    uint32_t cur_frame = base;
    int count_left = count;
    int num = 0;
    uint32_t cur_size = 0;
    int ret;

    int i, j;
    for(i = MAX_ORDER - 1; i >= 0; i--) {
        cur_size = 1 << i;

        num = count_left / cur_size;
        if(num != 0) {
            for(j = 0; j < num; j++) {
                if(cur_frame % (cur_size * PAGE_SIZE) == 0) {
                    ret = free_frames_raw(cur_frame, i);
                    if(ret != 0) return ret;
                    cur_frame += cur_size * PAGE_SIZE;
                } else {
                    ret = free_frames(cur_frame, cur_size/2);
                    if(ret != 0) return ret;
                    cur_frame += cur_size / 2 * PAGE_SIZE;
                    ret = free_frames(cur_frame, cur_size/2);
                    if(ret != 0) return ret;
                    cur_frame += cur_size / 2 * PAGE_SIZE;
                }
            }
            count_left -= num * cur_size;
        }
    }

    return 0;
}



/************ The followings are for debugging, will remove later **********/

// For debuging, will remove later
void traverse_free_area() {

    lprintf("traverse_free_area starts");
    /**** DEBUG *****/
    int i;
    for(i = 0; i < MAX_ORDER; i++) {
        // Test initial list content
        lprintf("order: %d", i);
        void *data;
        while(!list_remove_first(&(free_list[i].list), &data)) {
            lprintf("data: %x", (unsigned)data);
        }
    }
    MAGIC_BREAK;
}

// For debuging, will remove later
void test_frames() {

    // Test request frame size 19
    lprintf("Test new_frames");
    list_t list;
    int count = 19;
    if(get_frames(count, &list) == -1) {
        lprintf("get_frames failed");
        return;
    }

    // Traverse result list
    uint32_t *data;
    while(!list_remove_first(&list, (void *)(&data))) {
        lprintf("size:%d, base:%x", (int)data[0], (unsigned)data[1]);
    }

    MAGIC_BREAK;

}

