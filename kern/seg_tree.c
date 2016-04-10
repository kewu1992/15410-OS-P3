#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// #define MAX_NUM 7477

#include <seg_tree.h>

#define IS_LEAF(x)  (x>=size)
#define IS_VALID(x) (x<2*size)

static int max_num;

extern int asm_bsf(uint32_t value);

uint32_t get_next_pow2(uint32_t value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

static uint32_t *seg_tree;

static uint32_t size;

uint32_t init_recursive(uint32_t index) {
    if (!IS_VALID(index))
        return NAN;

    if (!IS_LEAF(index)) {
        seg_tree[index] = init_recursive(index * 2);
        init_recursive(index * 2 + 1);
        return seg_tree[index];
    } else {
        if ((index-size+1) << 5 <= max_num){
            seg_tree[index] = NAN;
        } else {
            seg_tree[index] = 0;
            uint32_t mask = 1;
            int i = 0;
            while (((index-size) << 5)+i < max_num) {
                seg_tree[index] |= mask;
                i++;
                mask <<= 1;
            }
        }
        return (seg_tree[index] == 0) ? NAN : (index-size)<<5;
    }
}

int init_seg_tree(int num) {

    max_num = num;

    size = get_next_pow2(max_num) >> 5;

    seg_tree = calloc(2*size, sizeof(uint32_t));
    if (seg_tree == NULL)
        return -1;
    
    init_recursive(1);

    return 0;

}

void update_tree(uint32_t index) {
    while (index != 0) {
        uint32_t left, right;
        int left_index = index*2;
        if (IS_LEAF(left_index)) {
            left = (seg_tree[left_index] == 0) ? NAN : 
                    ((left_index-size) << 5) + asm_bsf(seg_tree[left_index]);
        } else
            left = seg_tree[left_index];
        int right_index = left_index+1;
        if (IS_LEAF(right_index)) {
            right = (seg_tree[right_index] == 0) ? NAN : 
                    ((right_index-size) << 5) + asm_bsf(seg_tree[right_index]);
        } else
            right = seg_tree[right_index];

        seg_tree[index] = (left != NAN) ? left : right;
        index /= 2;
    }
}

uint32_t get_next() {
    uint32_t rv = seg_tree[1];
    if (rv == NAN)
        return NAN;

    uint32_t index = (rv >> 5) + size;
    int pos = rv % 32;
    seg_tree[index] &= ~(1<<pos);
    update_tree(index/2);

    return rv;
}

void put_back(uint32_t bits) {
    uint32_t index = (bits >> 5) + size;
    int pos = bits % 32;
    seg_tree[index] |= (1<<pos);
    update_tree(index/2);
}


/*


static int *naive;
void init_naive(){
    naive = calloc(max_num, sizeof(int));
}
uint32_t get_next_naive(){
    int i;
    for (i = 0; i < max_num; i++)
        if (naive[i] == 0) {
            naive[i] = 1;
            return i;
        }
    return -1;
}
void put_back_naive(uint32_t index){
    naive[index] = 0;
}



int main() {
    init();
    init_naive();
    
    int array[max_num+1];
    int count = 0;
    while (1) {
        if (count == 0) {
            uint32_t mine = get_next();
            uint32_t naives = get_next_naive();
            if (mine != naives){
                printf("error!\n");
                break;
            }
            printf("get %d\n", (int)mine);
            if (mine != NAN)
                array[count++] = mine;
        } else {
            int r = rand() % 7;
            if (r < 3) {
                int index = rand() % count;
                put_back(array[index]);
                put_back_naive(array[index]);
                printf("put back %d\n", array[index]);
                int i;
                for (i = index; i < count; i++)
                    array[i] = array[i+1];
                count--;
            } else {
                uint32_t mine = get_next();
                uint32_t naives = get_next_naive();
                if (mine != naives) {
                    printf("error!\n");
                    break;
                }
                printf("get %d\n", (int)mine);
                if (mine != NAN)
                    array[count++] = mine;
            }
        }
    }

    return 0;
}
*/


