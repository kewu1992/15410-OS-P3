/** @file smp_message.c
 *  @brief This file contains the message passing interface for multi-cores
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <smp_message.h>
#include <simple_queue.h>
#include <spinlock.h>
#include <smp.h>
#include <malloc.h>
#include <mptable.h>
#include <control_block.h>
#include <simics.h>

/** @brief The message queues */
simple_queue_t** msg_queues;

/** @brief The locks that protects message queues */
spinlock_t** msg_spinlocks;

/** @brief Number of worker cores */
int num_worker_cores;

/** Idle threads on different cores */
extern tcb_t* idle_thr[MAX_CPUS];

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

/** @brief Init the array of message queues on manager core
 *
 *  @return 0 on success; -1 on error
 *
 */
int msg_init() {
    int num_cpus = smp_num_cpus();
    
    if (num_cpus <= 1)
        panic("Numbe of cpu <= 1 !");

    num_worker_cores = num_cpus - 1;

    msg_queues = calloc(2*num_worker_cores, sizeof(simple_queue_t*));
    if (msg_queues == NULL)
        return -1;

    msg_spinlocks = calloc(2*num_worker_cores, sizeof(spinlock_t*));
    if (msg_spinlocks == NULL)
        return -1;

    return 0;
}

/** @brief Initialize AP cores' message queue
  * 
  * @return 0 on success; a negative integer on error
  *
  */
int init_ap_msg() {

    int cur_cpu = smp_get_cpu();

    // Inq and outq
    simple_queue_t *inq = malloc(sizeof(simple_queue_t));
    if(inq == NULL) return -1;

    simple_queue_t *outq = malloc(sizeof(simple_queue_t));
    if(outq == NULL) return -1;

    if(simple_queue_init(inq) < 0 || simple_queue_init(outq) < 0) return -1;

    // Locks
    spinlock_t *inq_lock = malloc(sizeof(spinlock_t));
    if(inq_lock == NULL) return -1;

    spinlock_t *outq_lock = malloc(sizeof(spinlock_t));
    if(outq_lock == NULL) return -1;

    if(spinlock_init(inq_lock) < 0 || spinlock_init(outq_lock) < 0) return -1;

    // Assign to slots
    msg_queues[(cur_cpu - 1) * 2] = outq;
    msg_queues[(cur_cpu - 1) * 2 + 1] = inq;
    msg_spinlocks[(cur_cpu - 1) * 2] = outq_lock;
    msg_spinlocks[(cur_cpu - 1) * 2 + 1] = inq_lock;

    return 0;
}

/** @brief Wait for completeness of AP cores' message queues initilization 
 *  before the manager core moves on.
 *
 *  @return void
 */
void msg_synchronize() {
    int i;
    for (i = 0; i < 2*num_worker_cores; i++) {
        while (msg_queues[i] == NULL)
            continue;
        while (msg_spinlocks[i] == NULL)
            continue;
    }
}

/** @brief Send message for a worker core
 *
 *  @msg The message to send
 *
 *  @return void
 */
void worker_send_msg(msg_t* msg) {

    int cur_cpu = smp_get_cpu();

    int id = (cur_cpu - 1) * 2;

    spinlock_lock(msg_spinlocks[id], 0);
    simple_queue_enqueue(msg_queues[id], &(msg->node));
    spinlock_unlock(msg_spinlocks[id], 0);

}

/** @brief Receive a message for a worker core
 *
 *
 *  @return The message recved
 */
msg_t* worker_recv_msg() {

    int id = (smp_get_cpu() - 1) * 2 + 1;

    spinlock_lock(msg_spinlocks[id], 0);
    simple_node_t*  msg_node = simple_queue_dequeue(msg_queues[id]);
    spinlock_unlock(msg_spinlocks[id], 0);

    if (msg_node == NULL)
        return NULL;
    else {
        return (msg_t*)(msg_node->thr);
    }
}

/** @brief Send a message for the manager core
 *
 *  @param msg The message to send
 *  @param dest_cpu The destination core
 *
 *  @return void
 */
void manager_send_msg(msg_t* msg, int dest_cpu) {

    int id = (dest_cpu - 1) * 2 + 1;
    spinlock_lock(msg_spinlocks[id], 0);
    simple_queue_enqueue(msg_queues[id], &(msg->node));
    spinlock_unlock(msg_spinlocks[id], 0);
}

/** @brief Recv a message for the manager core by polling message queues
 *  on worker cores.
 *
 *  @return The message recved
 */
msg_t* manager_recv_msg() {
    int i = 0;
    simple_node_t* msg_node = NULL;

    while (1) {
        spinlock_lock(msg_spinlocks[i], 0);
        msg_node = simple_queue_dequeue(msg_queues[i]);
        spinlock_unlock(msg_spinlocks[i], 0);

        if (msg_node != NULL)
            break;

        i = (i + 2) % (2*num_worker_cores);
    }

    return msg_node->thr;
}

/** @brief Get the thread corresponding to the message
 *
 *  This function will be invoked by schedulers of worker cores. If some 
 *  messages are sent by the manager core, this function will transform the
 *  message to the corresponding thread and let scheduler to schedule it.
 *
 *  @return The thread to schedule if message isn't NULL; NULL otherwise
 */
void* get_thr_from_msg_queue() {
    if (smp_get_cpu() == 0)
        return NULL;

    msg_t* msg = worker_recv_msg();
    if (msg != NULL) {
        tcb_t* new_thr;
        switch(msg->type) {
        case FORK:
            // for fork(), should return the newly created thread
            new_thr = (tcb_t*)(msg->data.fork_data.new_thr);
            return new_thr;
        case FORK_RESPONSE:
        case WAIT_RESPONSE:
        case RESPONSE:
            // for response message, just return the associated thread
            return (tcb_t*)msg->req_thr;
        case MAKE_RUNNABLE:
        case YIELD:
            // for make_runnable and yield, temporaryly set page table base to 
            // the same as the idle_task of the core is visiting to avoid memory
            // copying of page tables accross cores
            new_thr = (tcb_t*)msg->req_thr;
            new_thr->pcb = idle_thr[smp_get_cpu()]->pcb;
            return new_thr;
        case HALT:
            // the manager core sends HALT message, should halt...
            asm_hlt();
        default:
            return NULL;
        }
    }
    return NULL;
}
