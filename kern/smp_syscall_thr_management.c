/** @file smp_syscall_thr_management.c
 *  @brief Multi-core version of system calls related to thread management 
 *  that parts on manager core side.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <smp_message.h>

extern int num_worker_cores;

/** @brief Multi-core version of make_runnable syscall handler that's on 
  * manager core side 
  *
  * @param msg The message that contains the syscall request
  *
  * @return void
  *
  */
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

void smp_yield_syscall_handler(msg_t* msg) {
    if (msg->data.yield_data.result == 0) {
        // already find the tid, send this thread back to its original core
        manager_send_msg(msg, msg->req_cpu);
    } else {
        // continue looping to visit all cores
        msg->data.yield_data.next_core = (msg->data.yield_data.next_core + 1) % num_worker_cores;
        // add one to skip core 0
        msg->data.yield_data.next_core++;
        manager_send_msg(msg, msg->data.yield_data.next_core);
    }
}

