#ifndef _ASM_ATOMIC_H_
#define _ASM_ATOMIC_H_

int atomic_add(int* num);

int asm_xchg(int *lock_available, int val);

#endif