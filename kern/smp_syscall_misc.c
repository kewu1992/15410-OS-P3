#include <malloc.h>
#include <smp_message.h>
#include <simics.h>

extern int num_worker_cores;

/** @brief Halt 
  *
  * Disable interrupt and execute halt instruction
  *
  * @return No return
  */
extern void asm_hlt(void);

void smp_syscall_halt(msg_t *msg) {
    
    msg_t msgs[num_worker_cores];

    // broadcast HALT to all cores
    int i = 0;
    for (i = 0; i < num_worker_cores; i++) {
        msgs[i].req_thr = msg->req_thr;
        msgs[i].req_cpu = msg->req_cpu;
        msgs[i].type = HALT;
        msgs[i].node.thr = &msgs[i];
        manager_send_msg(&msgs[i], i+1);
    }

    // halt manager core
    asm_hlt();

}