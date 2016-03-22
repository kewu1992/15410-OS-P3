#include <spinlock.h>
#include <asm.h>
#include <asm_atomic.h>

int spinlock_init(spinlock_t* lock) {
    *lock = 1;
    return 0;
}

void spinlock_lock(spinlock_t* lock) {
    disable_interrupts();

    while (!asm_xchg(lock, 0)) 
       continue;
}

void spinlock_unlock(spinlock_t* lock) {
    asm_xchg(lock, 1);
    enable_interrupts();
}

void spinlock_destroy(spinlock_t* lock) {
    asm_xchg(lock, 0);
}