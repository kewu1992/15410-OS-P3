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
#include <console.h>
#include <cr.h>
#include <loader.h>
#include <eflags.h>
#include <assert.h>

#include <stdint.h>// for uint32_t
extern uint32_t asm_get_ebp();
extern uint32_t asm_get_esp();


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

    clear_console();
    printf("Hello, world");

    lprintf("ebp:%x", (unsigned int)asm_get_ebp());
    lprintf("esp:%x", (unsigned int)asm_get_esp());

    /*      0xffffffff  <-- logical max address (4GB)
     *      
     *      0x10000000  <-- physical max address (256MB)  
     *
     *      0x00ffffff  <-- max memory address for kernel (V=P)
     *      
     *      0x0011b03c  <-- %ebp of kernel_main(): can be various
     *      0x0011b020  <-- %esp of kernel_main(): can be various
     *
     *      0x00000000
     */
 
    lprintf( "Ready to load first task" );
    loadFirstTask("small_program");


    // should never reach here
    return 0;
}
