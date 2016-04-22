/** @file ap_kernel.c
 *  @brief Contains the AP entry-point function
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug No known bugs.
 */

#include <common_kern.h>
#include <simics.h>                 /* lprintf() */
/* multiboot header file */
#include <multiboot.h>              /* boot_info */
/* x86 specific includes */
#include <x86/asm.h>                /* enable_interrupts() */
#include <init_IDT.h>
#include <loader.h>
#include <vm.h>
#include <scheduler.h>
#include <control_block.h>
#include <console.h>
#include <syscall_inter.h>
#include <context_switcher.h>
#include <pm.h>

#include <mptable.h>
#include <smp.h>


#include <apic.h> // TO BE REMOVED
#include <timer_driver.h> // TO BE REMOVED

static int ap_kernel_init() {

    // Init virtual memory mapping for this core
    // To allocate kernel heap memory using portion of this core, must have vm
    // mapping, then must have a page directory, which must use malloc to 
    // allocate; so, initially, AP uses BSP's kernel memory to make a page
    // directory during vm_init_raw, and after that, AP can use its portion of
    // kernel heap memory.
    if(init_vm_raw() < 0) {
        lprintf("init_vm_raw failed");
        return -1;
    }

    if(init_pm() < 0) {
        lprintf("init pm failed");
        return -1;
    }

    return 0;
}


void ap_kernel_main(int cpu_id) {

    lprintf("ap kernel %d runs!", cpu_id);

    if(ap_kernel_init() < 0) {
        panic("ap_kernel_init failed");
    }

    uint32_t cr3 = (uint32_t)get_cr3();
    lprintf("cpu %d's cr3:%x", smp_get_cpu(), (unsigned)cr3);
/*
    // Try heap memory
    void *p = malloc(4);
    if(p == NULL) {
        lprintf("malloc failed");
        MAGIC_BREAK;
    }

    *((int *)p) = 177;
    lprintf("cpu %d malloc succeeded: 0x%x", 
            smp_get_cpu(), (unsigned)p);
*/

    init_lapic_timer_driver();

    enable_interrupts();

    while(1) ;


}

