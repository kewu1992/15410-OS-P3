/** @file ap_kernel.c
 *  @brief Contains the APs entry-point function. 
 *
 *  Do some initialization for kernel and load the first task (idle task).
 *  In our design, APs are also worker cores.
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
#include <smp_message.h>


/** @brief Initialize AP kernel 
 *  The order of initialization may not be changed. Any data structures that
 *  are initialized here means every core has its own copy. 
 */
static void ap_kernel_init(int cpu_id) {

    adopt_init_pd(cpu_id);

    if (malloc_init(cpu_id) < 0)
        panic("Initialize malloc at cpu%d failed!", cpu_id);

    if (init_pm() < 0)
        panic("init_pm at cpu%d failed!", cpu_id);

    if (init_ap_msg() < 0)
        panic("init_msg at cpu%d failed!", cpu_id);

    if (context_switcher_init() < 0)
        panic("Initialize context_switcher at cpu%d failed!", cpu_id);

    if (scheduler_init() < 0)
        panic("Initialize scheduler at cpu%d failed!", cpu_id);

    // Initialize system call specific data structure

    if (syscall_vanish_init() < 0)
        panic("Initialize vanish at cpu%d failed!", cpu_id);

    if (syscall_deschedule_init() < 0)
        panic("Initialize deschedule at cpu%d failed!", cpu_id);

    if (syscall_sleep_init() < 0) 
        panic("syscall_sleep_init at cpu%d failed", cpu_id);
}

/** @brief APs' kernel entrypoint.
 *  
 *  This is the entrypoint for APs' kernel.
 *
 * @return Does not return
 */
void ap_kernel_main(int cpu_id) {

    lprintf("Initializing kernel for cpu%d", cpu_id);
    ap_kernel_init(cpu_id);
    lprintf("Finish initialization for cpu%d", cpu_id);

    enable_interrupts();

    lprintf("Ready to load first task for cpu%d", cpu_id);
    loadFirstTask("idle");

    // should never reach here
    panic("loadFirstTask() returned!");
}

