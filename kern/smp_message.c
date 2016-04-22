#include <smp_message.h>

simple_queue_t** msg_queues;
spinlock_t** msg_spinlocks;

static int num_worker_cores;

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
    msg_queues[cur_cpu * 2] = inq;
    msg_queues[cur_cpu * 2 + 1] = outq;
    msg_spinlocks[cur_cpu * 2] = inq_lock;
    msg_spinlocks[cur_cpu * 2 + 1] = outq_lock;

    return 0;
}

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

}

msg_t* worker_recv_msg() {

}


void manager_send_msg(msg_t* msg) {
    int id = msg->req_cpu * 2 + 1;
    spinlock_lock(spinlocks[id]);
    simple_queue_enqueue(msg_queues[id], &(msg->node));
    spinlock_unlock(spinlocks[id]);
}

msg_t* manager_recv_msg() {
    int i = 0;
    simple_node_t* msg_node = NULL;

    do {
        spinlock_lock(spinlocks[i]);
        msg_node = simple_queue_dequeue(msg_queues[i]);
        spinlock_unlock(spinlocks[i]);
        i = (i + 2) % num_worker_cores;
    } while (msg_node == NULL);

    return msg_node->thr;
}

