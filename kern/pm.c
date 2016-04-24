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
#include <smp.h>
#include <mptable.h>

/** @brief Number of cores */
static int num_cpus;

/** @brief Number of free frames per core initially */
static int num_free_frames_per_core;

/** @brief Number of free frames currently available in physical memory */
static int *num_free_frames_left[MAX_CPUS];

/** @brief The lapic base frame that shouldn't be allocated */
static uint32_t lapic_base;

/** @brief Mutex that protects frame allocation */
static mutex_t *lock[MAX_CPUS];

/**
 * @brief Get a free frame
 * 
 * @return Base of the frame on success; a negative integer on error
 * (cast to int to check, those error integers are not aligned, so they are
 * distinguishable from correct frame address.
 *
 */
uint32_t get_frames_raw() {

    int cur_cpu = smp_get_cpu();

    mutex_lock(lock[cur_cpu]);
    uint32_t index = get_next();
    mutex_unlock(lock[cur_cpu]);

    if((int)index == NAN) {
        return ERROR_NOT_ENOUGH_MEM;
    }

    uint32_t new_frame = USER_MEM_START + index * PAGE_SIZE + 
        cur_cpu * num_free_frames_per_core * PAGE_SIZE;

    if(new_frame == lapic_base) {
        return get_frames_raw();
    } 

    return new_frame;

}

/**
 * @brief Free a frame
 *
 * @param base The base of the frame to free
 * 
 * @return Void
 */
void free_frames_raw(uint32_t base) {

    int cur_cpu = smp_get_cpu();

    int index = (base - cur_cpu * num_free_frames_per_core * PAGE_SIZE - 
            USER_MEM_START) / PAGE_SIZE;

    mutex_lock(lock[cur_cpu]);
    put_back(index);
    mutex_unlock(lock[cur_cpu]);

}


/**
 * @brief Initialize physical memory manager
 *  
 * @return 0 on success; a negative integer on error
 */
int init_pm() {

    int cur_cpu = smp_get_cpu();

    lprintf("Init pm for cpu %d", cur_cpu);

    if(cur_cpu == 0) {
        // Get number of cores
        num_cpus = smp_num_cpus();

        // Max number of free frames in user space initially
        int max_num_free_frames = machine_phys_frames() - 
            USER_MEM_START/PAGE_SIZE;

        // Divide frames evenly among cores
        num_free_frames_per_core = max_num_free_frames / num_cpus;

        // A frame that shouldn't be allocated
        lapic_base = (uint32_t)smp_lapic_base();
    }

    // Malloc on each core to avoid false sharing
    num_free_frames_left[cur_cpu] = malloc(sizeof(int));
    if(num_free_frames_left[cur_cpu] == NULL) {
        return -1; 
    }

    *num_free_frames_left[cur_cpu] = num_free_frames_per_core;
    lprintf("add user memory %d frames for cpu %d succeeded",
            num_free_frames_per_core, cur_cpu);

    if(init_seg_tree(num_free_frames_per_core) < 0) {
        lprintf("init_seg_tree() failed when init_pm()");
        return -1;
    }

    lock[cur_cpu] = malloc(sizeof(mutex_t));
    if(lock[cur_cpu] == NULL) {
        return -1;
    }

    if (mutex_init(lock[cur_cpu]) < 0) {
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

    int cur_cpu = smp_get_cpu();

    *num_free_frames_left[cur_cpu] = 
        atomic_add(num_free_frames_left[cur_cpu], -count);
    if(*num_free_frames_left[cur_cpu] < 0) {
        atomic_add(num_free_frames_left[cur_cpu], count);
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

    int cur_cpu = smp_get_cpu();
    atomic_add(num_free_frames_left[cur_cpu], count); 
}

