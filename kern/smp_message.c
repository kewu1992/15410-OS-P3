#include <smp_message.h>
#include <simple_queue.h>
#include <spinlock.h>
#include <smp.h>
#include <malloc.h>
#include <mptable.h>
#include <control_block.h>
#include <simics.h>

simple_queue_t** msg_queues;
spinlock_t** msg_spinlocks;

int num_worker_cores;

extern tcb_t* idle_thr[MAX_CPUS];

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

void msg_synchronize() {
    int i;
    for (i = 0; i < 2*num_worker_cores; i++) {
        while (msg_queues[i] == NULL)
            continue;
        while (msg_spinlocks[i] == NULL)
            continue;
    }
}

void worker_send_msg(msg_t* msg) {
    //lprintf("thr %d at cpu%d send a msg, type:%d", ((tcb_t*)(msg->req_thr))->tid, msg->req_cpu, msg->type);

    int cur_cpu = smp_get_cpu();

    int id = (cur_cpu - 1) * 2;

    spinlock_lock(msg_spinlocks[id], 0);
    simple_queue_enqueue(msg_queues[id], &(msg->node));
    spinlock_unlock(msg_spinlocks[id], 0);

}

msg_t* worker_recv_msg() {

    int id = (smp_get_cpu() - 1) * 2 + 1;

    spinlock_lock(msg_spinlocks[id], 0);
    simple_node_t*  msg_node = simple_queue_dequeue(msg_queues[id]);
    spinlock_unlock(msg_spinlocks[id], 0);

    if (msg_node == NULL)
        return NULL;
    else {
        //lprintf("cpu%d recv a msg (req thr:%d), type:%d", ((msg_t*)(msg_node->thr))->req_cpu, ((tcb_t*)(((msg_t*)(msg_node->thr))->req_thr))->tid, ((msg_t*)(msg_node->thr))->type);
        return (msg_t*)(msg_node->thr);
    }
}


void manager_send_msg(msg_t* msg, int dest_cpu) {
    //lprintf("manager send a msg to cpu%d, type:%d", dest_cpu, msg->type);

    int id = (dest_cpu - 1) * 2 + 1;
    spinlock_lock(msg_spinlocks[id], 0);
    simple_queue_enqueue(msg_queues[id], &(msg->node));
    spinlock_unlock(msg_spinlocks[id], 0);
}

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

    //lprintf("manager recv a msg at cpu%d, req thr:%d, type:%d", i/2+1, ((tcb_t*)(((msg_t*)(msg_node->thr))->req_thr))->tid, ((msg_t*)(msg_node->thr))->type);
    return msg_node->thr;
}

void* get_thr_from_msg_queue() {
    if (smp_get_cpu() == 0)
        return NULL;

    msg_t* msg = worker_recv_msg();
    if (msg != NULL) {
        tcb_t* new_thr;
        switch(msg->type) {
        case FORK:
            new_thr = (tcb_t*)(msg->data.fork_data.new_thr);
            return new_thr;
        case FORK_RESPONSE:
        case WAIT_RESPONSE:
        case RESPONSE:
            return (tcb_t*)msg->req_thr;
        default:
            return NULL;
        }
    }
    return NULL;
}
