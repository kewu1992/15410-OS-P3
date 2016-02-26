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


#include <console_driver.h>
#include <console.h>
#include <cr.h>



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

    init_console_driver();

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

    /* The following code works find, because now there is no VM */
    int *p = (int*)(0x01000000);
    *p = 123;

    lprintf("successfully write data to 0x01000000");

    /*  The following code cause fault (double fault....) due to exceed 
     *  physical memory limit (and there is no VM)
     *
     *  int *q = (int*)(0x10000000);
     *  *q = 123;
     */

    // open VM
    int mask = 1 << 30;
    set_cr0(get_cr0()|mask);

    // set PDBR
    set_cr3(0x01000000);

    //..... then what?

    while (1) {
        continue;
    }

    return 0;
}
