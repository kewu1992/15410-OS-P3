/** @file kernel.c
 *  @brief An initial kernel.c
 *
 *  You should initialize things in kernel_main(),
 *  and then run stuff.
 *
 *  @author Harry Q. Bovik (hqbovik)
 *  @author Fred Hacker (fhacker)
 *  @bug No known bugs.
 */

#include <common_kern.h>

/* libc includes. */
#include <stdio.h>
#include <simics.h>                 /* lprintf() */

/* multiboot header file */
#include <multiboot.h>              /* boot_info */

/* x86 specific includes */
#include <x86/asm.h>                /* enable_interrupts() */


#include <init_IDT.h>

#include <cr.h>
#include <loader.h>
#include <eflags.h>
#include <assert.h>

#include <stdint.h>// for uint32_t

#include <vm.h> // For vm
#include <scheduler.h>
#include <control_block.h>

#include <console.h>
#include <syscall_inter.h>

static void kernel_init();

/** @brief Kernel entrypoint.
 *  
 *  This is the entrypoint for the kernel.
 *
 * @return Does not return
 */
int kernel_main(mbinfo_t *mbinfo, int argc, char **argv, char **envp)
{
    /*
     * When kernel_main() begins, interrupts are DISABLED.
     * You should delete this comment, and enable them --
     * when you are ready.
     */

    lprintf( "Hello from a brand new kernel!" );
    
    lprintf("Initializing kernel");
    kernel_init();
    lprintf("Finish initialization");

    lprintf( "Ready to load first task" );
    loadFirstTask("coolness");

    // should never reach here
    return 0;
}


void kernel_init() {
    if (tcb_init() < 0)
        panic("Initialize tcb failed!");

    if (scheduler_init() < 0)
        panic("Initialize scheduler failed!");

    if (init_IDT(NULL) < 0)
        panic("Initialize IDT failed!");

    // Initialize vm, all kernel 16 MB will be directly mapped and
    // paging will be enabled after this call
    init_vm();

    if (syscall_print_init() < 0)
        panic("Initialize syscall print() failed!");

    enable_interrupts();

    clear_console();
}
