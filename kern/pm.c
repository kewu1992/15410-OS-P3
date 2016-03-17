/**
  * @brief Physical memory allocator
  *
  *
  */
#include <pm.h>
#include <pm.h>



// Max block size is 2^(MAX_ORDER-1)*PAGE_SIZE
// Blocks are of size 4K, 8K, ..., 4M, given PAGE_SIZE is 4K

static struct free_area_struct free_area[MAX_ORDER];
//hashtable_t free_block_ht;

// Number of free frames left
static int num_free_frames;

/****** Public API *******/

// Manage continugous frame allocation
int init_pm() {
    // Init MAX_ORDER lists
    int i;
    for(i = 0; i < MAX_ORDER; i++) {
        if(list_init(&free_area[i].list)) {
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
            list_append(&(free_area[MAX_ORDER - 1].list), (void *)base);
        if(ret != 0) return ret;

        base += block_size;
    }

    return 0;
}


// For debuging
void traverse_free_area() {
    
    lprintf("traverse_free_area starts");
    /**** DEBUG *****/
    int i;
    for(i = 0; i < MAX_ORDER; i++) {
        // Test initial list content
        lprintf("order: %d", i);
        while(!list_is_empty(&(free_area[i].list))) {
            void *data = list_remove_first(&(free_area[i].list));
            lprintf("data: %x", (unsigned)data);
        }
    }
    MAGIC_BREAK;
}

// For debuging
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
    while(!list_is_empty(&list)) {
        uint32_t *data = list_remove_first(&list);
        lprintf("size:%d, base:%x", (int)data[0], (unsigned)data[1]);
    }

    MAGIC_BREAK;

}

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



/******* raw API **********/

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
uint32_t get_frames_raw(int order) {

     // lprintf("order: %d", order);
     // Check free block lists
     int cur_order = order; 
     while(cur_order < MAX_ORDER) {

         if(list_is_empty(&free_area[cur_order].list)) {
             // Try finding free block in a larger size group
             cur_order++;
         } else {
             uint32_t free_block_base =
                 (uint32_t)list_remove_first(
                         &(free_area[cur_order].list));
             // We have found a free block
             if(cur_order > order) {
                 // But it's too large, we will split it first
                 // Put unused halves back to lists
                 uint32_t block_size = (1 << cur_order) * PAGE_SIZE;
                 uint32_t block_base = free_block_base + block_size;
                 while(cur_order > order) {
                     block_size >>= 1;
                     block_base -= block_size;
                     // Place unused half to free list of one order less
                     int ret = 
                         list_append(&(free_area[cur_order - 1].list),
                             (void *)block_base);
                     if(ret < 0) return ret;
                     cur_order--;
                 }
             } 

             return free_block_base;
         }
     }

     // Failed to find a contiguous block of the size we want
     return ERROR_NOT_ENOUGH_MEM; 
}

int free_frames_raw(uint32_t base, int order) {

    // lprintf("free_frames: base: %x, order: %d", (unsigned)base, order);
    // Iteratively merge block with its buddy to the highest order possible
    while(order < MAX_ORDER) {
        if(order == MAX_ORDER - 1) {
            // block is of the highest order, no way to merge any more
            // Put block to its free list and stop
            int ret =list_append(&(free_area[order].list), (void *)base);
            if(ret < 0) return ret;
            break;
        }

        // Get buddy's base
        uint32_t buddy_base = ((((base - USER_MEM_START)/PAGE_SIZE) ^ 
            (1 << order)) * PAGE_SIZE) + USER_MEM_START;

        if(list_delete(&(free_area[order].list), (void *)buddy_base) == -1) {

            // Buddy of the same size isn't free, can't merge with it
            // Put block to its free list and stop
            int ret = list_append(&(free_area[order].list), (void *)base);
            if(ret < 0) return ret;
            break;
        }

        // Merge with buddy
        base = base < buddy_base ? base : buddy_base;

        order++;
    }

    return 0;
}

int free_contiguous_frames(uint32_t base, int count) {

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
                    ret = free_contiguous_frames(cur_frame, cur_size/2);
                    if(ret != 0) return ret;
                    cur_frame += cur_size / 2 * PAGE_SIZE;
                    ret = free_contiguous_frames(cur_frame, cur_size/2);
                    if(ret != 0) return ret;
                    cur_frame += cur_size / 2 * PAGE_SIZE;
                }
            }
            count_left -= num * cur_size;
        }
    }

    return 0;
}




