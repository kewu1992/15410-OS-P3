/** @file thr_lib_helper.c
 *
 *  @brief A helper for thread library that contains functions 
 *  that do stack space management.
 *  
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug No known bugs
 */
#include <stdint.h>
#include <thr_lib_helper.h>
#include <arraytcb.h>
#include <string.h>
#include <thr_internals.h>

/**
 * @brief Root thread stack low
 */
static uint32_t root_thread_stack_low;

/**
 * @brief Root thread stack high
 */
static uint32_t root_thread_stack_high;

/** @brief The max amount of stack space available for each thread */
static unsigned int stack_size;

/** @brief The %ebp value of the _main() function stack frame, this value is
 *         set by install_autostack() of autostack.c before program is running*/
extern void *ebp__main;


/** @brief Initialize thr_lib_helper.
 *
 *  Set stack size limit for each thread except for root thread (which may
 *  have a chance to grow because of the autostack function) and get root 
 *  thread stack high (which is deterministic at the moment kernel handed
 *  control to the thread library). Root thread stack low is not determined
 *  at this point so that we will wait until we create a first thread.
 *
 */
int thr_lib_helper_init(unsigned int size) {

    stack_size = size;

    root_thread_stack_high = get_root_thread_stack_high();

    return 0;
}

/** @brief Get stack top for a new thread
 *  
 *  Compute the stack region for a new thread, allocate new pages for its
 *  stack, and return a stack top that meets alignment requirement. To achieve
 *  maximum concurrency, multiple threads can call thr_create at the 
 *  same time, so that there's no guarantee that ajacent stack spaces are
 *  allocated in any order, each thread will not assume stack space upper
 *  or lower than its stack are allocated or not.
 *
 *  @param index The index of thread stacks (0 based)
 *
 *  @return Stack top for a new thread on success; -1 on error
 *
 */
uint32_t get_new_stack_top(int index) {

    // When the first new thread is to be created, fixate the root thread 
    // stack low
    if(index == 1) {
        if(root_thread_stack_low == 0) {
            root_thread_stack_low = get_root_thread_stack_low();
            if(root_thread_stack_low % ALIGNMENT) {
                return ERROR_MISALIGNMENT;
            }
        }
    }

    // Allocate stack space for new threads

    // Stack space allocated for root thread will be preserved until task 
    // vanishes, since it's allocated by kernel and it may be the result
    // of one or more new_pages() call, so that without this information
    // it would be hard to call remove_pages() correctly. Future thread
    // that is put on the highest stack already has space allocated.
    if(index == 0) {
        return root_thread_stack_low;
    }

    uint32_t new_thread_stack_low = root_thread_stack_low - 
        index * stack_size;
    uint32_t new_thread_stack_high = new_thread_stack_low + 
        stack_size - 1;

    // The page address where new thread stack low is in
    uint32_t new_thread_stack_low_page = new_thread_stack_low &
        PAGE_ALIGN_MASK;

    // The page address where new thread stack high is in
    uint32_t new_thread_stack_high_page = new_thread_stack_high &
        PAGE_ALIGN_MASK;

    // # of pages between where stack high is in and where stack low is in.
    // Note this doesn't include the highest page of the new thread stack
    int num_pages = (new_thread_stack_high_page - 
            new_thread_stack_low_page)/PAGE_SIZE;

    // When trying to allocate stack space, consider the new thread stack 
    // space in 3 parts, the highest page that may overlap with upper stack, 
    // the middle pages that are private to itself, and the lowest page that 
    // may overlap with lower stack.

    // Allocate highest page of this stack region, fail is normal since this 
    // page may have been already been allocated 
    int ret = new_pages((void *)(new_thread_stack_high & PAGE_ALIGN_MASK), 
            PAGE_SIZE);
    if(ret && ret != ERROR_NEW_PAGES_OVERLAP_EXISTING_REGION) {
        return ret;
    } 

    // Allocate middle pages of this stack region, shouldn't fail since 
    // middle pages don't overlap with other threads' stack regions
    if(num_pages > 1) {
        ret = new_pages((void *)((new_thread_stack_low & PAGE_ALIGN_MASK)
                    + PAGE_SIZE),  (num_pages - 1) * PAGE_SIZE);
        if(ret) {
            return ret;
        }    
    }

    // Allocate lowest page, fail is normal since the page may have already 
    // been allocated.
    if((new_thread_stack_low & PAGE_ALIGN_MASK) != 
            (new_thread_stack_high & PAGE_ALIGN_MASK)) {
        ret = new_pages((void *)(new_thread_stack_low & PAGE_ALIGN_MASK),
                PAGE_SIZE);
        if(ret && ret != ERROR_NEW_PAGES_OVERLAP_EXISTING_REGION) {
            return ret;
        } 
    }

    // The 1st available new stack position is last thread's stack low - 1
    // Keep decrementing until it aligns with 4
    uint32_t new_stack_top = new_thread_stack_high; 
    while(new_stack_top % ALIGNMENT != 0) {
        new_stack_top--;
    }

    return new_stack_top;
}

/** @brief Get the information about pages to remove in a thread's stack space
 *  
 *  For each stack space, at the time we allocate it, we divide the stack
 *  space into 3 parts, so that when we need to remove it, we also need to 
 *  consider 3 parts separately. The general idea is not to remove a page
 *  if any other thread is use it.
 *
 *  @param index The index of thread stacks (0 based)
 *
 *  @param page_remove_info An array that information about which pages to 
 *  remove.
 *
 *  @return 0 on success; -1 on error.
 *
 */
uint32_t get_pages_to_remove(int index, int *page_remove_info) {

    if(index == 0) {
        page_remove_info[HIGHEST_PAGE_CAN_REMOVE] = 0;
        page_remove_info[MIDDLE_PAGES_CAN_REMOVE] = 0;
        page_remove_info[LOWEST_PAGE_CAN_REMOVE] = 0;
        return 0;
    }

    uint32_t new_thread_stack_low = root_thread_stack_low - 
        index * stack_size;
    uint32_t new_thread_stack_high = new_thread_stack_low + 
        stack_size - 1;

    tcb_t *thr;

    // The page address where new_thread_stack_high is in
    uint32_t new_thread_stack_high_page = new_thread_stack_high 
        & PAGE_ALIGN_MASK;
    uint32_t new_thread_stack_low_page = new_thread_stack_low & 
        PAGE_ALIGN_MASK;
    uint32_t upper_stack_low = new_thread_stack_high + 1; 
    int can_remove = 1;
    int i = 1;
    while(((upper_stack_low & PAGE_ALIGN_MASK) == new_thread_stack_high_page) 
            && (index -1 != 0)) {
        if(!arraytcb_is_valid(index - i)) {
            break;
        }

        thr = arraytcb_get_thread(index - i);
        if(thr) {
            // There's thread alive
            can_remove = 0;
            break;
        } 

        upper_stack_low += stack_size; 
        i++;
    } 

    if(can_remove) {
        page_remove_info[HIGHEST_PAGE_BASE] = new_thread_stack_high_page;
        page_remove_info[HIGHEST_PAGE_CAN_REMOVE] = 1;
    } else {
        page_remove_info[HIGHEST_PAGE_CAN_REMOVE] = 0;
    }

    // # of pages between where stack high is in and where stack low is in.
    // Note this doesn't include the highest page of the new thread stack
    int num_pages = (new_thread_stack_high_page - 
            new_thread_stack_low_page)/PAGE_SIZE;

    // Middle pages
    if(num_pages > 1) {
        page_remove_info[MIDDLE_PAGES_BASE] = 
            new_thread_stack_low_page + PAGE_SIZE;
        page_remove_info[MIDDLE_PAGES_CAN_REMOVE] = 1;
    } else {
        page_remove_info[MIDDLE_PAGES_CAN_REMOVE] = 0;
    }

    // If lowest page overlaps with lower thread's stack, remove page
    // if there's no thread alive in lower stack

    // If stack size is smaller than a page, must ensure all threads inside 
    // the page are not alive
    uint32_t lower_stack_high = new_thread_stack_low - 1;
    can_remove = 1;
    i = 1;
    while((lower_stack_high & PAGE_ALIGN_MASK) == new_thread_stack_low_page) {
        if(!arraytcb_is_valid(index + i)) {
            break;
        }

        thr = arraytcb_get_thread(index + i);
        if(thr) {
            // There's thread alive
            can_remove = 0;
            break;
        } 

        lower_stack_high -= stack_size; 
        i++;
    } 

    // Check if highest page overlaps with the lowest page
    if(new_thread_stack_low_page != new_thread_stack_high_page) {
        if(can_remove) {
            page_remove_info[LOWEST_PAGE_BASE] = new_thread_stack_low_page;
            page_remove_info[LOWEST_PAGE_CAN_REMOVE] = 1;
        } else {
            page_remove_info[LOWEST_PAGE_CAN_REMOVE] = 0;
        }
    } else {
        // Highest page and lowest page are the same page!
        // then must ensure all threads within the page are not alive, 
        // thus here we combine the information of highest page computed 
        // above with the lowest page.
        page_remove_info[LOWEST_PAGE_CAN_REMOVE] = 0;

        page_remove_info[HIGHEST_PAGE_CAN_REMOVE] &= can_remove;
    } 

    return 0;
}


/** @brief Get stack position index of the current thread 
 *  
 *
 *  @return Stack position index of the current thread
 */
int get_stack_position_index() {

    uint32_t esp = asm_get_esp();

    if(esp <= root_thread_stack_high && esp >= root_thread_stack_low) {
        return 0;
    } else {
        // (root_thread_stack_low - esp - 1) so that all esp within a stack 
        // region maps to the same number
        return 1 + (root_thread_stack_low - esp - 1) / stack_size;
    }

}

/** @brief get old %ebp value based on current %ebp
 *
 *  @param ebp Value of current %ebp
 *
 *  @return Value of old %ebp
 */
void* get_last_ebp(void* ebp) {
    unsigned long last_ebp = *((unsigned long*) ebp);
    return (void*) last_ebp;
}

/** @brief Modify the return address of master thread 
 *  
 *  Modify return address of master thread (return address of main()) 
 *  when the program become a multi-thread program. So that when main() return 
 *  it will go to thr_ret2exit() in thr_create_kernel.S and call thr_exit().
 *
 *  This function will use %ebp to trace back until find %ebp of _main(). And 
 *  then modify return address of main().
 *
 */
void set_rootthr_retaddr() {
    // trace back, until find main() and __main()
    void* ebp = (void*)asm_get_ebp();
    void* last_ebp = get_last_ebp(ebp);
    while (last_ebp != ebp__main) {
        ebp = last_ebp;
        last_ebp = get_last_ebp(ebp);
    }
    // last_ebp is for _main() then ebp is for main(), change return address
    void *thr_ret2exit_addr = thr_ret2exit;
    memcpy((void*)((uint32_t)ebp + 4), &thr_ret2exit_addr, 4);
}

