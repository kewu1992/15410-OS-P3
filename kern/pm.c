/**
 * @brief Physical memory manager
 *
 * @author Jian Wang (jianwan3)
 * @author Ke Wu (kewu)
 *
 * @bug No known bugs.
 */

#include <pm.h>
#include <seg_tree.h>
#include <asm_atomic.h>
#include <mutex.h>

/** @brief Number of free frames currently available in physical memory */
static int num_free_frames;

/** @brief Mutex that protects frame allocation */
static mutex_t lock;

/**
 * @brief Get a free frame
 * 
 * @return Base of the frame on success; a negative integer on error
 * (cast to int to check, those error integers are not aligned, so they are
 * distinguishable from correct frame address.
 *
 */
uint32_t get_frames_raw() {
    mutex_lock(&lock);
    uint32_t index = get_next();
    mutex_unlock(&lock);

    if((int)index == NAN) {
        return ERROR_NOT_ENOUGH_MEM;
    }

    return USER_MEM_START + index * PAGE_SIZE;

}

/**
 * @brief Free a frame
 *
 * @param base The base of the frame to free
 * 
 * @return Void
 */
void free_frames_raw(uint32_t base) {

    int index = (base - USER_MEM_START) / PAGE_SIZE;

    mutex_lock(&lock);
    put_back(index);
    mutex_unlock(&lock);

}


/**
 * @brief Initialize physical memory manager
 *  
 * @return 0 on success; a negative integer on error
 */
int init_pm() {

    // Max number of free frames in user space initially
    num_free_frames = machine_phys_frames() - 
        USER_MEM_START/PAGE_SIZE;

    if(init_seg_tree(num_free_frames) < 0) {
        lprintf("init_seg_tree() failed when init_pm()");
        return -1;
    }

    if (mutex_init(&lock) < 0) {
        lprintf("mutex_init() failed when init_pm()");
        return -1;
    }

    return 0;

}

/**
 * @brief Declare how many frames are needed and check if there's enough of 
 * them available
 *
 * Predeclare futural usage so that frames will be enough when actually needed.
 *
 * @param count The number of frames requested.
 *  
 * @return 0 on success; A negative integer on error
 */
int reserve_frames(int count) {

    int num_free_frames_left = atomic_add(&num_free_frames, -count);
    if(num_free_frames_left < 0) {
        atomic_add(&num_free_frames, count);
        return -1;
    }

    return 0;

}

/**
 * @brief Increase number of free frames as frames have been freed.
 *
 * @param count The number of free frames newly available.
 *  
 * @return Void
 */
void unreserve_frames(int count) {
    atomic_add(&num_free_frames, count); 
}

