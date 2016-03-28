#ifndef _ASM_ATOMIC_H_
#define _ASM_ATOMIC_H_

/** @brief Atomically execute num++
 *  
 *  This function using instruction cmpxchg (CAS) to implement atomic counter
 *
 *  @param num Pointer points to the integer to execute num++ 
 *
 *  @return The value of num after addition
 */
int atomic_add(int* num);


/** @brief Atomically exchange values
 *  
 *  This function using instruction xchg to exchange two values atomically
 *
 *  @param lock_available Pointer points to the integer to be exchanged
 *  @param val The value that will be exchanged with lock_available
 *
 *  @return The original value of lock_available (before exchange)
 */
int asm_xchg(int *lock_available, int val);

#endif