/** @file kernel.c
 *  @brief An initial kernel.c
 *
 *  Do some initialization for kernel and load the first task (idle task)
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

#include <mptable.h>
#include <smp.h>

static void kernel_init();

extern void ap_kernel_main(int cpu_id);

/** @brief Kernel entrypoint.
 *  
 *  This is the entrypoint for the kernel.
 *
 * @return Does not return
 */
int kernel_main(mbinfo_t *mbinfo, int argc, char **argv, char **envp)
{

    if(smp_init(mbinfo) < 0) {
        panic("smp_init failed");
    }

    // Distribute kernel heap memory among cores
    dist_kernel_mem();

    lprintf("Initializing kernel");
    kernel_init();
    lprintf("Finish initialization");

    // Boot AP kernels after initilization is done
    smp_boot(ap_kernel_main);

    lprintf( "Ready to load first task" );
    loadFirstTask("idle");

    // should never reach here
    return 0;
}

/** @brief Initialize kernel 
 *  The order of initialization may not be changed
 */
void kernel_init() {

    if (malloc_init() < 0)
        panic("Initialize malloc failed!");

    if (init_IDT(timer_callback) < 0)
        panic("Initialize IDT failed!");

    if (tcb_init() < 0)
        panic("Initialize tcb failed!");

    // Initialize vm, all kernel 16 MB will be directly mapped and
    // paging will be enabled after this call
    if (init_vm() < 0)
        panic("Initialize virtual memory failed!");

    enable_interrupts();

    if (context_switcher_init() < 0) {
        panic("Initialize context_switcher failed!");
    }

    if (scheduler_init() < 0)
        panic("Initialize scheduler failed!");

    // Initialize system call specific data structure

    if (syscall_print_init() < 0)
        panic("Initialize syscall print() failed!");

    if (syscall_read_init() < 0)
        panic("Initialize syscall readline() failed!");

    if (syscall_sleep_init() < 0)
        panic("Initialize syscall sleep() failed!");

    if(syscall_vanish_init() < 0)
        panic("Initialize syscall vanish() failed!");

    if (syscall_deschedule_init() < 0)
        panic("Initialize syscall deschedule() failed!");

    if (syscall_readfile_init() < 0)
        panic("Initialize syscall readfile() failed!");

    clear_console();
}
