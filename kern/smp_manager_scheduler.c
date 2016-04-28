/** @file smp_manager_scheduler.c
 *  @brief This file contains the main function for manager core
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <smp.h>
#include <smp_message.h>
#include <mptable.h>
#include <smp_syscall.h>
#include <simics.h>
#include <stdlib.h>
#include <timer_driver.h>


/** @brief The kernel_main function for worker cores */
extern void ap_kernel_main(int cpu_id);

/** @brief Boot manager core and run
  * 
  * @return void
  *
  */
void smp_manager_boot() {
    
    if (msg_init() < 0)
        panic("msg_init() in smp_manager_boot() failed");

    if (smp_syscall_vanish_init() < 0)
        panic("smp_syscall_vanish_init() failed");

    if (smp_syscall_print_init() < 0)
        panic("smp_syscall_print_init failed!");

    if (smp_syscall_read_init() < 0)
        panic("smp_syscall_read_init failed!");

    // Init lapic timer
    init_lapic_timer_driver();

    // Boot AP kernels after initilization is done
    smp_boot(ap_kernel_main);

    // barrier to wait for all AP cores ready
    msg_synchronize();

    lprintf("all cores synchronized");
    

    while(1) {
        msg_t* msg = manager_recv_msg();

        switch(msg->type) {
        case FORK:
            smp_syscall_fork(msg);
            break;
        case FORK_RESPONSE:
            smp_fork_response(msg);
            break;
        case WAIT:
            smp_syscall_wait(msg);
            break;
        case VANISH:
            smp_syscall_vanish(msg);
            break;
        case VANISH_BACK:
            msg->type = RESPONSE;
            manager_send_msg(msg, msg->data.vanish_back_data.ori_cpu);
            break;
        case SET_CURSOR_POS:
            smp_syscall_set_cursor_pos(msg);
            break;
        case SET_TERM_COLOR:
            smp_syscall_set_term_color(msg);
            break;
        case GET_CURSOR_POS:
            smp_syscall_get_cursor_pos(msg);
            break;
        case READLINE:
            smp_syscall_readline(msg);
            break;
        case PRINT:
            smp_syscall_print(msg);
            break;
        case SET_INIT_PCB:
            smp_set_init_pcb(msg);
            break;
        case MAKE_RUNNABLE:
            smp_make_runnable_syscall_handler(msg);
            break;
        case YIELD:
            smp_yield_syscall_handler(msg);
            break;
        case HALT:
            smp_syscall_halt(msg);
            break;
        default:
            break;
        }
    } 
}

