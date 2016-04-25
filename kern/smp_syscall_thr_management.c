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


/** @brief For deschedule() and make_runnable() syscalls.
 *         All threads that are blocked because of deschedule() will be stored 
 *         in this queue. make_runnable() will try to find the thread to be 
 *         made runable in this queue */
static simple_queue_t deschedule_queue;

/** @brief For deschedule() and make_runnable() syscalls.
 *         Mutex lock to protect deschedule_queue */
static mutex_t deschedule_mutex;


/** @brief Initialize data structure for deschedule() syscall */
int smp_syscall_deschedule_init() {
    int error = simple_queue_init(&deschedule_queue);
    error |= mutex_init(&deschedule_mutex);
    return error ? -1 : 0;
}


void smp_deschedule_syscall_handler(msg_t *msg) {
    // using mutex to protect deschedule_queue, it also make sure examine the 
    // value of *reject and block the thread (at here it is done by put the 
    // thread in the deschedule_queue) is atomic with respect to make runnable()
    mutex_lock(&deschedule_mutex);

    int reject = msg->data.deschedule_data.reject;

    if (reject) {
        mutex_unlock(&deschedule_mutex);
        msg->type = RESPONSE;
        msg->data.response_data.result = 0;
        manager_send_msg(msg, msg->req_cpu);
    }

    // enter the tail of deschedule_queue to wait
    simple_queue_enqueue(&deschedule_queue, &msg->node);
    mutex_unlock(&deschedule_mutex);

}

void smp_make_runnable_syscall_handler(msg_t *msg) {

    int tid = msg->data.make_runnable_data.tid;

    mutex_lock(&deschedule_mutex);
    simple_node_t* node = smp_simple_queue_remove_tid(&deschedule_queue, tid);
    mutex_unlock(&deschedule_mutex);

    if (node != NULL) {
        // Found the descheduled thread, make it runnable
        msg_t *deschedule_msg = (msg_t *)node->thr;

        // Send a messsage back to the core that previously called this 
        // deschedule()
        deschedule_msg->type = RESPONSE;
        deschedule_msg->data.response_data.result = 0;
        manager_send_msg(deschedule_msg, deschedule_msg->req_cpu);

        // Send a message back to the core that called this make_runnable()
        msg->type = RESPONSE;
        msg->data.response_data.result = 0;
        manager_send_msg(msg, msg->req_cpu);
    } else {
        // Send the message back to the core that called this make_runnable()
        msg->type = RESPONSE;
        msg->data.response_data.result = ETHREAD;
        manager_send_msg(msg, msg->req_cpu);
    }
}

