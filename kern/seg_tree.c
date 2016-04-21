/** @file seg_tree.c
 *  @brief This file contains implementation of a segment tree
 *
 *  This data structure is used as physical frames allocator to track free 
 *  frames. The segment tree is a perfect binary tree. Each node is 32-bits.
 *  The leaf nodes of the segment tree represent physical frames.  Here, bitmap 
 *  is used to save space. So each leaf node can represent allocation states of
 *  32 physical frames. Each internal node of the segment tree stores a single 
 *  value which is the smallest index of free frame in all of its child 
 *  leaf nodes. 
 *
 *  When VM system ask for a physical frame, PM system will give it the physical 
 *  frame that has the smallest index in all free frames. Note that this is the 
 *  value stored in the root of segment tree. Then segment tree will mark the 
 *  corresponding bit in the leaf node as allocated. After that. O(logn) time is
 *  needed to update the segment tree for maintenance. Similarly, when VM system
 *  frees a frame, O(logn) time is needed to update the segment tree.
 *
 *  Compared with a simple bitmap implementation of physical frames allocator. 
 *  Segment tree implementation costs double memory space. However, the simple
 *  bitmap needs O(n) time to allocate/free a frame while segment tree needs 
 *  O(logn) time.
 *
 *  @author Ke Wu (kewu)
 *  @bug No known bug
 */

#include <stdint.h>
#include <malloc.h>
#include <asm_helper.h>
#include <seg_tree.h>
#include <smp.h>
#include <mptable.h>

/** @brief Check if a given node index is a leaf node */
#define IS_LEAF(x)  (x>=size)

/** @brief Check if a given node index is a valid node */
#define IS_VALID(x) (x<2*size)

/** @brief The number of physical frames. Note that the number of physical 
 *         frame is not necessary the number of leaf nodes in segment tree. 
 *         Because 1). Bitmap is used so each leaf node represent 32 frames
 *                 2). the number of physical frames may not be a power of 2,
 *                     so some padding leaf nodes might be necessary to make 
 *                     sure that the segment tree is a perfect tree */
static int max_num;

/** @brief The array of segment tree. Because it is a perfect tree, so the 
 *         relationship between parent node and child node can be easily 
 *         calculated by index */
static uint32_t *seg_tree[MAX_CPUS];

/** @brief The number of leaf nodes */
static uint32_t size;

/** @brief Get the next number that is a power of 2
 *   
 *  @param value The next number that is a power of 2 should >= this value
 *
 *  @return The next number that is a power of 2
 */
static uint32_t get_next_pow2(uint32_t value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

/** @brief Helper function for segment tree initialization
 *   
 *  This function will initialize a node of segment tree recursively. It will 
 *  first initialize its left child node and right child node, and then set its
 *  value based on its children. 
 *
 *  @param index The index of the node that will be initialized
 *
 *  @return The value for this node, which will be used for its parent node
 */
static uint32_t init_recursive(uint32_t index) {

    int cur_cpu = smp_get_cpu();

    if (!IS_VALID(index))
        return NAN;

    if (!IS_LEAF(index)) {
        // for internal node, first initialize its child nodes. 
        // Because at first all physical frames are free, the smallest index of
        // free frame must be the value of its left child node
        seg_tree[cur_cpu][index] = init_recursive(index * 2);
        init_recursive(index * 2 + 1);
        return seg_tree[cur_cpu][index];
    } else {
        // for leaf node, mark its value as 0xFFFFFFFF, which means all 32 
        // frames are free at begining
        if ((index-size+1) << 5 <= max_num){
            seg_tree[cur_cpu][index] = 0xFFFFFFFF;
        } else {
            // for leaf node that not all 32 bits are valid physical frames,
            // only mark part of its bits as 1 (free)
            seg_tree[cur_cpu][index] = 0;
            uint32_t mask = 1;
            int i = 0;
            while (((index-size) << 5)+i < max_num) {
                seg_tree[cur_cpu][index] |= mask;
                i++;
                mask <<= 1;
            }
        }
        // If seg_tree[index] equals to zero, it means no free physical frame,
        // so just return NAN, other wise return the free frame with the
        // smallest idnex 
        return (seg_tree[cur_cpu][index] == 0) ? NAN : (index-size)<<5;
    }
}

/** @brief Initialize segment tree data structure
 *   
 *  @param num The num of physical frames.
 *
 *  @return On success return 0, on error return -1
 */
int init_seg_tree(int num) {

    int cur_cpu = smp_get_cpu();

    if(cur_cpu == 0) {
        max_num = num;
        size = get_next_pow2(max_num) >> 5;
    }

    seg_tree[cur_cpu] = calloc(2*size, sizeof(uint32_t));
    if (seg_tree[cur_cpu] == NULL)
        return -1;
    
    init_recursive(1);

    return 0;

}

/** @brief Update segment tree
 * 
 *  This will go from an internal node to the root of segment 
 *  tree and update the values of all nodes on the path.
 *   
 *  @param index The internal node to start updating.
 *
 *  @return Void
 */
static void update_tree(uint32_t index) {

    int cur_cpu = smp_get_cpu();

    // updating tree unitl the root
    while (index != 0) {
        uint32_t left, right;
        int left_index = index*2;
        // get value from its child nodes
        if (IS_LEAF(left_index)) {
            left = (seg_tree[cur_cpu][left_index] == 0) ? NAN : 
                    ((left_index-size) << 5) + 
                    asm_bsf(seg_tree[cur_cpu][left_index]);
        } else
            left = seg_tree[cur_cpu][left_index];
        int right_index = left_index+1;
        if (IS_LEAF(right_index)) {
            right = (seg_tree[cur_cpu][right_index] == 0) ? NAN : 
                    ((right_index-size) << 5) + 
                    asm_bsf(seg_tree[cur_cpu][right_index]);
        } else
            right = seg_tree[cur_cpu][right_index];

        // update value of this node
        seg_tree[cur_cpu][index] = (left != NAN) ? left : right;
        
        // continue update its parent node
        index /= 2;
    }
}

/** @brief Get the free physical frame with the smallest index
 *   
 *  @return On success return the free physical frame with the smallest index
 *          On error, return NAN which means there is no free frame.
 */
uint32_t get_next() {

    int cur_cpu = smp_get_cpu();

    // the free physical frame with the smallest index is the value of root
    uint32_t rv = seg_tree[cur_cpu][1];
    if (rv == NAN)
        return NAN;

    // mark the corresponding bit as allcoated
    uint32_t index = (rv >> 5) + size;
    int pos = rv % 32;
    seg_tree[cur_cpu][index] &= ~(1<<pos);

    // update segment tree
    update_tree(index/2);

    return rv;
}

/** @brief Free a physical frame
 *
 *  @param frame_index The index of the physical frame that will be freed
 *   
 *  @return Void
 */
void put_back(uint32_t frame_index) {

    int cur_cpu = smp_get_cpu();

    // mark the corresponding bit as freed
    uint32_t index = (frame_index >> 5) + size;
    int pos = frame_index % 32;
    seg_tree[cur_cpu][index] |= (1<<pos);

    // update segment tree
    update_tree(index/2);
}


