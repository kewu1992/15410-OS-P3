/** @file thr_internals.h
 *
 *  @brief This file is used to define some internal functions for
 *         the thread library.
 *
 *  @author Jian Wang <jianwan3andrew.cmu.edu>
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 */

#ifndef THR_INTERNALS_H
#define THR_INTERNALS_H

/** @brief C wrapper for xchg(lock_available, val)
 *  
 *  In the inside, it will atomically exchange *lock_available with val
 *
 *  @param lock_available The address of variable to be exchanged
 *  @param val The value to replace *lock_available
 * 
 *  @return The old value of (*lock_availble)
 */
int asm_xchg(int *lock_available, int val);

/** @brief Creates a new thread to run func(args) on a given stack
 *  
 *  This function is writtrn in assembly. It will create a thread 
 *  (a register set) to run func(args) on a given stack.
 *
 *  To be more specific, this function will first save its two parameters to
 *  registers. Then it will invoke thread_fork which is a trap. After that, two
 *  threads will run the same code. The original thread will just return. The 
 *  new thread will first get its ktid, and call arraytcb_set_ktid() to save its
 *  ktid in arraytcb. Then it will set its esp to new_stack, and call func(). 
 *  After return from func(), it will push the return value to stack, and call
 *  thr_exit() if the thread doesn't call itself.
 *
 *  Note that the two paramters (func, new_stack) must be saved in registers
 *  before calling thread_fork. Because at first they are saved at the original
 *  thread's stack. After thread_fork, the new thread can't rely on the original 
 *  threads's stack to fetch these two paramters. Because at that time, the 
 *  stack maybe destroied by the original thread. 
 *  
 *
 *  @param func The address of function for new thread to run
 *  @param new_stack The address of new stack (esp) for new thread to run on.
 *                   Note that the argument for new thread to run func(args) has 
 *                   been pushed to stack conforming to the calling conventions.
 * 
 *  @return On success the thread ID of the new thread is returned, on error a 
 *          negative number is returned
 */
int thr_create_kernel(void *(*func)(void *), void *new_stack);

int thr_getktid();

int thr_hashtableexit_init();

int thr_exitstatus_hashfunc(void *key);

/** @brief Delete a thread from arraytcb and vanish
 *  
 *  This function is called by thr_exit() to delete a thread from arraytcb,
 *  deallocate its stack memory and call vanish(). The code is written in 
 *  assembly, but it equals to exceute the following code:
 *      if (page_remove_info[1])
 *          remove_pages(page_remove_info[0]);
 *      if (page_remove_info[3])
 *          remove_pages(page_remove_info[2]);
 *      if (page_remove_info[5])
 *          remove_pages(page_remove_info[4]);
 *      SPINLOCK_UNLOCK(&mutex_arraytcb->inner_lock);
 *      vanish();
 *  However, it must be written in assembly because when stack region is
 *  deallocated, the program can not rely on stack to call new functions 
 *  (i.e. the following remove_pages(), SPINLOCK_UNLOCK() and vanish()). So 
 *  asm_thr_exit() will just use some registers to execute all code above.    
 *
 *  @param inner_lock It is the addres of mutex_arraytcb->inner_lock. it is 
 *                 used to execute SPINLOCK_UNLOCK(&mutex_arraytcb->inner_lock);
 *  @param page_remove_info Address of the array page_remove_info. It is used to
 *                          indicate which pages should be deallcated. More info
 *                          please refer to thr_lib_helper.c 
 *                          get_pages_to_remove().
 * 
 *  @return Should never return
 */
void asm_thr_exit(void *inner_lock, int* page_remove_info);

/** @brief Indicate a symbol in thr_create_kernel()
 *  
 *  This is used by set_rootthr_retaddr() in thr_lib_helper.c to modify return
 *  address of master thread (return address of main()) when the program become
 *  a multi-thread program. So that when main() return it will go to 
 *  thr_ret2exit() and call thr_exit().
 *
 */
void thr_ret2exit();

#endif /* THR_INTERNALS_H */
