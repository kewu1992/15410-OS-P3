#include <smp.h>
#include <malloc.h>
#include <context_switcher.h>
#include <control_block.h>
#include <spinlock.h>
#include <smp_message.h>
#include <mptable.h>
#include <asm_helper.h>
#include <mptable.h>

#define NUM_WORKER_THR  16

//static tcb_t* mailbox_thr;

static simple_queue_t worker_queue;

static mutex_t queue_mutex;

static simple_queue_t** msg_queues;
static spinlock_t** spinlocks;

static int num_worker_cores;

static void worker_run();
static void work_finish();

extern void ap_kernel_main(int cpu_id);

void smp_manager_boot() {
    int num_cpus = smp_num_cpus();
    
    if (num_cpus <= 1)
        panic("Numbe of cpu <= 1 !");

    num_worker_cores = num_cpus - 1;

    msg_queues = calloc(2*num_worker_cores, sizeof(simple_queue_t*));
    if (msg_queues == NULL)
        panic("smp_manager_boot() failed");

    spinlocks = calloc(2*num_worker_cores, sizeof(spinlock_t*));
    if (spinlocks == NULL)
        panic("smp_manager_boot() failed");

    /*
    if (mutex_init(&queue_mutex) < 0)
        panic("smp_manager_boot() failed");


    // thread fork all worker threads
    int i;
    for (i = 0; i < NUM_WORKER_THR; i++) {
        context_switch(OP_THREAD_FORK, 0);
        if (tcb_get_entry((void*)asm_get_esp())->result == 0) {
            worker_run();
        }
    }
    */

    // Boot AP kernels after initilization is done
    smp_boot(ap_kernel_main);

    while(1);

    worker_run();

}


void worker_run() {
    while (1) {
        lprintf("thr %d at cpu%d goes here", tcb_get_entry((void*)asm_get_esp())->tid, smp_get_cpu());
        work_finish();
    }
}

void work_finish() {
    simple_node_t node;
    node.thr = tcb_get_entry((void*)asm_get_esp());

    mutex_lock(&queue_mutex);
    simple_queue_enqueue(&worker_queue, &node);
    mutex_unlock(&queue_mutex);

    context_switch(OP_BLOCK, 0);
}

/*
void assign_work() {

    while(1) {
        mutex_lock(&queue_mutex);
        simple_node_t* thr_node = simple_queue_dequeue(&worker_queue);
        mutex_unlock(&queue_mutex);
        if (thr_node != NULL) {
            int i = 0;
            simple_node_t* msg_node = NULL;
            do {
                spinlock_lock(spinlocks[i]);
                msg_node = simple_queue_dequeue(msg_queues[i]);
                spinlock_unlock(spinlocks[i]);
                i = (i + 2) % num_worker_cores;
            } while (msg_node == NULL);

            tcb_t* worker_thr = thr_node->thr;
            msg_t* msg = msg_node->thr;
            worker_thr->my_msg = msg;
            context_switch(OP_MAKE_RUNNABLE
                , (uint32_t)worker_thr); 
        }
    } 
}
*/
