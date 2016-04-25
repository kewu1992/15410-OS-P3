#include <malloc.h>
#include <smp_message.h>
#include <simics.h>

extern int num_worker_cores;

/** @brief Halt by calling simics command 
  *
  * @return No return
  */
extern void sim_halt(void);

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
        msgs[i].type = HALT;
        manager_send_msg(&msgs[i], i+1);
    }

    while(1);
    
    // halt manager core
    sim_halt();
    // if kernel is run on real hardware....
    asm_hlt();

}