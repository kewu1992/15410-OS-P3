/** @file smp_syscall_thr_management.c
 *  @brief Multi-core version of system calls related to thread management 
 *  that parts on manager core side.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <control_block.h>
#include <asm_helper.h>
#include <simics.h>
#include <priority_queue.h>
#include <spinlock.h>
#include <timer_driver.h>
#include <context_switcher.h>
#include <malloc.h>
#include <vm.h>
#include <exception_handler.h>
#include <syscall_errors.h>

extern int num_worker_cores;

void smp_make_runnable_syscall_handler(msg_t *msg) {

    if (msg->data.make_runnable_data.result == 0) {
        // already find the tid, send this thread back to its original core
        manager_send_msg(msg, msg->req_cpu);
    } else {
        // continue looping to visit all cores
        msg->data.make_runnable_data.next_core = (msg->data.make_runnable_data.next_core + 1) % num_worker_cores;
        // add one to skip core 0
        msg->data.make_runnable_data.next_core++;
        manager_send_msg(msg, msg->data.make_runnable_data.next_core);
    }
}

