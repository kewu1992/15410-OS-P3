#include <simics.h>
#include <stdlib.h>

extern void sim_halt(void);

void halt_syscall_handler() {
    sim_halt();

    // if kernel is run on real hardware....
    panic("kernel is halt!");
}