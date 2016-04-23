#include <smp_message.h>
#include <asm_helper.h>
#include <malloc.h>
#include <context_switcher.h>
#include <control_block.h>
#include <spinlock.h>

static int fork_next_core = 0;

extern int num_worker_cores;

void smp_syscall_fork(msg_t* msg) {
    manager_send_msg(msg, fork_next_core+1); // add one to skip core 0
    fork_next_core = (fork_next_core + 1) % num_worker_cores;
}

void smp_fork_response(msg_t* msg) {
    msg_t* ori_msg;
    if (msg->data.fork_response_data.result == 0) { // fork success 
        // change the type of the original message to FORK_RESPONSE
        ori_msg = msg->data.fork_response_data.req_msg;
        ori_msg->type = FORK_RESPONSE;
        ori_msg->data.fork_response_data.result = msg->data.fork_response_data.result;

        // send the response back to the thread calling fork()
        manager_send_msg(ori_msg, ori_msg->req_cpu);

        // also send the request message (which belongs to the new thread) back 
        manager_send_msg(msg, msg->req_cpu);
    } else {
        // fork failed
        ori_msg = msg->data.fork_response_data.req_msg;
        if (ori_msg->data.fork_data.retry_times == num_worker_cores) {
            // reach the maximum retry times, just return failed
            ori_msg->type = FORK_RESPONSE;
            ori_msg->data.fork_response_data.result = msg->data.fork_response_data.result;
            // send response back to the thread calling fork()
            manager_send_msg(ori_msg, ori_msg->req_cpu);
        } else {
            // try on another core
            ori_msg->data.fork_data.retry_times++;
            int next_core = (msg->req_cpu + 1) % num_worker_cores;
            manager_send_msg(ori_msg, next_core+1); // add one to skip core 0
        }
    }
}
