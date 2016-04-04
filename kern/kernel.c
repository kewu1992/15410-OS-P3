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
    lprintf( "Hello from a brand new kernel!" );
    
    lprintf("Initializing kernel");
    kernel_init();
    lprintf("Finish initialization");

    lprintf( "Ready to load first task" );
    MAGIC_BREAK;
    loadFirstTask("fork_exit_bomb");

    // should never reach here
    return 0;
}


void kernel_init() {

    if (malloc_init() < 0)
         panic("Initialize malloc failed!");
    
    if (init_IDT(NULL) < 0)
        panic("Initialize IDT failed!");

    if (tcb_init() < 0)
        panic("Initialize tcb failed!");

    // Initialize vm, all kernel 16 MB will be directly mapped and
    // paging will be enabled after this call
    if (init_vm() < 0)
        panic("Initialize virtual memory failed!");

    enable_interrupts();

    if (scheduler_init() < 0)
        panic("Initialize scheduler failed!");

    // Initialize system call specific data structure
    if (syscall_print_init() < 0)
        panic("Initialize syscall print() failed!");

    if (syscall_read_init() < 0)
        panic("Initialize syscall readline() failed!");

    if(syscall_vanish_init() < 0)
        panic("Initialize syscall vanish() failed!");


    clear_console();
}
