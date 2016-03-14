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

#include <console.h>

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

    if (init_IDT(NULL) < 0)
        panic("Initialize IDT failed!");

    enable_interrupts();

    clear_console();
    printf("Hello, world");

    // Initialize vm, all kernel 16 MB will be directly mapped and
    // paging will be enabled after this call
    init_vm();
 
    lprintf( "Ready to load first task" );
    loadFirstTask("small_program");
    //loadFirstTask("ck1");


    // should never reach here
    return 0;
}
