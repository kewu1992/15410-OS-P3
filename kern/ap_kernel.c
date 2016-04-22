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

static void ap_kernel_init(int cpu_id) {

    // Init virtual memory mapping for this core
    // To allocate kernel heap memory using portion of this core, must have vm
    // mapping, then must have a page directory, which must use malloc to 
    // allocate; so, initially, AP uses BSP's kernel memory to make a page
    // directory during vm_init_raw, and after that, AP can use its portion of
    // kernel heap memory.
    if(init_vm_raw() < 0)
        panic("init_vm_raw at cpu%d failed!", cpu_id);

    lprintf("finish init vm");

    if (malloc_init(cpu_id) < 0)
        panic("Initialize malloc at cpu%d failed!", cpu_id);

    if(init_pm() < 0)
        panic("init_pm at cpu%d failed!", cpu_id);

    if (context_switcher_init() < 0)
        panic("Initialize context_switcher at cpu%d failed!", cpu_id);

    if (scheduler_init() < 0)
        panic("Initialize scheduler at cpu%d failed!", cpu_id);
}


void ap_kernel_main(int cpu_id) {

    lprintf("Initializing kernel for cpu%d", cpu_id);
    ap_kernel_init(cpu_id);
    lprintf("Finish initialization for cpu%d", cpu_id);

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

