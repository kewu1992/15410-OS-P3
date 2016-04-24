#include <smp_message.h>
#include <asm_helper.h>
#include <malloc.h>
#include <context_switcher.h>
#include <control_block.h>
#include <spinlock.h>
#include <mutex.h>
#include <hashtable.h>
#include <syscall_errors.h>

/** @brief The struct to store exit status for a task */
typedef struct {
    /** @brief The vanished task's pid */
    int pid;
    /** @brief The vanished task's exit status */
    int status;
} exit_status_t;

/** @brief Data structure for wait() syscall. Each task (pcb) has one if 
 *        this struct */
typedef struct {
    /** @brief The number of alive child tasks */
    int num_alive;
    /** @brief The number of zombie child tasks */
    int num_zombie;
    /** @brief A queue for threads that invoke wait() to block on */
    simple_queue_t wait_queue;
    /** @brief Pcb level Lock */
    mutex_t lock;
} task_wait_t;

typedef struct {
    /** @brief Child tasks exit status list. When a child task dies, it will
     *  put its exit_status_node to the parent's child_exit_status_list */
    simple_queue_t child_exit_status_list;

    /** @brief Exit status of the task */
    exit_status_t *exit_status;

    /** @brief Exit status node that will be inserted to parent task's
     *         child_exit_status_list when the task dies */
    simple_node_t *exit_status_node;

    /** @brief Data structure for wait() syscall */
    task_wait_t task_wait_struct;
} pcb_vanish_wait_t;

static int ht_put_task(int pid, pcb_vanish_wait_t *pcb);

static void ht_remove_task(int pid);

static void *get_task(int pid);

static pcb_vanish_wait_t* init_task;


static int fork_next_core = 0;

extern int num_worker_cores;

static int create_pcb_vanish_wait_struct(int pid);

static void free_pcb_vanish_wait_struct(pcb_vanish_wait_t* pcb);


/** @brief The initial size of hash table to stroe pid to pcb map,
 *         pick a prime number */
#define PID_PCB_HASH_SIZE 1021


/** @brief Hashtable to map pid to pcb, dead process doesn't have entry in 
 *         hashtable. It is useful to detect if partent task dead */
static hashtable_t ht_pid_pcb;

/** @brief Lock for ht_pid_pcb */
static mutex_t ht_pid_pcb_lock;




void smp_syscall_fork(msg_t* msg) {
     if (create_pcb_vanish_wait_struct(msg->data.fork_data.new_tid) < 0) {
        // create_pcb_vanish_wait_struct failed, return error
        // change message type to response
        msg->type = FORK_RESPONSE;
        msg->data.fork_response_data.result = -1;
        // send response back to the thread calling fork()
        manager_send_msg(msg, msg->req_cpu);
     }

    // send fork message to another call to continue executing fork()
    manager_send_msg(msg, fork_next_core+1); // add one to skip core 0
    fork_next_core = (fork_next_core + 1) % num_worker_cores;
}



void smp_fork_response(msg_t* msg) {
    msg_t* ori_msg;
    if (msg->data.fork_response_data.result == 0) { // fork success 
        ori_msg = msg->data.fork_response_data.req_msg;

        // add num_alive of child process for parent process
        mutex_lock(&ht_pid_pcb_lock);
        pcb_vanish_wait_t* parent_task = (pcb_vanish_wait_t*)get_task(msg->data.fork_data.ppid);
        mutex_unlock(&ht_pid_pcb_lock);

        mutex_lock(&parent_task->task_wait_struct.lock);
        parent_task->task_wait_struct.num_alive++;
        mutex_unlock(&parent_task->task_wait_struct.lock);

        // change the type of the original message to FORK_RESPONSE
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

            // remove pid from map
            ht_remove_task(msg->data.fork_data.new_tid);

            // free pcb_vanish_wait_t 
            mutex_lock(&ht_pid_pcb_lock);
            pcb_vanish_wait_t* this_task = (pcb_vanish_wait_t*)get_task(msg->data.fork_data.new_tid);
            mutex_unlock(&ht_pid_pcb_lock);
            free_pcb_vanish_wait_struct(this_task);

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

/************ vanish ***********************/




/** @brief The hash function for hash table 
  *
  * @param key The hash function key
  *
  * @return The hashed value
  *
  */
static int ht_pid_pcb_hashfunc(void *key) {
    int tid = (int)key;
    return tid % PID_PCB_HASH_SIZE;
}

/** @brief Get pcb_vanish_wait_t for pid
 *
 *  @param pid The pid of the task to look for
 *
 *  @return Task's pcb_vanish_wait_t if it's alive; NULL if not
 */
static void *get_task(int pid) {
    int is_find = 0;
    void *value = hashtable_get(&ht_pid_pcb, (void*)pid, &is_find);
    if(is_find) {
        return value;
    } 
    return NULL;
}

/** @brief Put pid to pcb_vanish_wait_t mapping for a task
 *
 *  @param pid Key of the hashtable
 *  @param pcb Value of the hashtable
 *  @return On success return zero, a negative integer on error
 */
static int ht_put_task(int pid, pcb_vanish_wait_t *pcb) {
    mutex_lock(&ht_pid_pcb_lock);
    int rv = hashtable_put(&ht_pid_pcb, (void *)pid, (void *)pcb); 
    mutex_unlock(&ht_pid_pcb_lock);
    return rv;
}

/** @brief Remove a task's pid to pcb mapping entry from the hashtable
 *
 *  @param pid Key of the hashtable
 *  @return void
 */
static void ht_remove_task(int pid) {
    int is_find;
    mutex_lock(&ht_pid_pcb_lock);
    hashtable_remove(&ht_pid_pcb, (void*)pid, &is_find);
    mutex_unlock(&ht_pid_pcb_lock);
}


int smp_syscall_vanish_init() {
    // Initialize the hashtable that stores wait status
    ht_pid_pcb.size = PID_PCB_HASH_SIZE;
    ht_pid_pcb.func = ht_pid_pcb_hashfunc;
    if(hashtable_init(&ht_pid_pcb) < 0) {
        return -1;
    }

    if(mutex_init(&ht_pid_pcb_lock) < 0) {
        return -1;
    }

    return 0;
}

void smp_set_init_pcb(msg_t* msg) {  
    int rv = create_pcb_vanish_wait_struct(msg->data.set_init_pcb_data.pid);
    if (rv == 0) {
        mutex_lock(&ht_pid_pcb_lock);
        init_task = (pcb_vanish_wait_t*)get_task(msg->data.set_init_pcb_data.pid);
        mutex_unlock(&ht_pid_pcb_lock);
    }
    
    msg->type = RESPONSE;
    msg->data.response_data.result = rv;
    manager_send_msg(msg, msg->req_cpu);
}


void smp_syscall_vanish(msg_t* msg) {
    mutex_lock(&ht_pid_pcb_lock);
    pcb_vanish_wait_t* this_task = (pcb_vanish_wait_t*)get_task(msg->data.vanish_data.pid);
    mutex_unlock(&ht_pid_pcb_lock);
    if (this_task == NULL) {
        panic("Can not find pcb_vanish_wait_t in smp_syscall_vanish()");
    }

    // Get hashtable's lock
    mutex_lock(&ht_pid_pcb_lock);
    // Get parent task's pcb
    pcb_vanish_wait_t *parent_task = get_task(msg->data.vanish_data.ppid);

    if(parent_task == NULL) {
        // Parent is dead, report to init task
        parent_task = init_task;
    }

    // Get parent task's task_wait lock
    task_wait_t *task_wait = &parent_task->task_wait_struct;
    mutex_lock(&task_wait->lock);
    // Release hashtable's lock
    mutex_unlock(&ht_pid_pcb_lock);

    // Put exit_status into parent's child exit status list 
    this_task->exit_status->status = msg->data.vanish_data.status;
    simple_queue_enqueue(&parent_task->child_exit_status_list, 
                                               this_task->exit_status_node);

    task_wait->num_zombie++;
    task_wait->num_alive--;

    // Make runnable a parent task's thread that is blocked on wait() if any
    simple_node_t* wait_node = simple_queue_dequeue(&task_wait->wait_queue);
    if(wait_node != NULL) {
        task_wait->num_zombie--;
        simple_node_t *exit_status_node = 
            simple_queue_dequeue(&parent_task->child_exit_status_list);
        mutex_unlock(&task_wait->lock);

        exit_status_t *es = (exit_status_t*)(exit_status_node->thr);

        msg_t* wait_msg = (msg_t *)(wait_node->thr);
        wait_msg->type = WAIT_RESPONSE;
        wait_msg->data.wait_response_data.pid = es->pid;
        wait_msg->data.wait_response_data.status = es->status;
        free(es);
        free(exit_status_node);

        manager_send_msg(wait_msg, wait_msg->req_cpu);

    } else {
        mutex_unlock(&task_wait->lock);
    }
    // ======= End report exit status to its parent or init task ==========

    // Get hashtable's lock
    mutex_lock(&ht_pid_pcb_lock);
    // Get this task's pcb's lock
    task_wait_t *this_task_wait = &this_task->task_wait_struct;
    mutex_lock(&this_task_wait->lock);
    // Delete this task's hashtable entry in hashtable
    int is_find; 
    hashtable_remove(&ht_pid_pcb, (void*)(msg->data.vanish_data.pid), &is_find);
    if(is_find == 0) {
        panic("delete task %d in hashtable failed", msg->data.vanish_data.pid);
    }
    // Release pcb's lock
    mutex_unlock(&this_task_wait->lock);
    // Release hashtable's lock
    mutex_unlock(&ht_pid_pcb_lock);

    // Now nobody can alter resources in current task's pcb except itself

    // Report unreaped children's status to task init
    task_wait_t *init_task_wait = &init_task->task_wait_struct;
    mutex_lock(&init_task_wait->lock);

    // Put children tasks' exit_status into init task's child exit status 
    // list
    simple_node_t* node;
    int has_unreaped_child = 0;
    while((node = simple_queue_dequeue(
                    &this_task->child_exit_status_list)) != NULL) {
        has_unreaped_child = 1;
        simple_queue_enqueue(&init_task->child_exit_status_list, node);
        init_task_wait->num_zombie++;
    }

    if (has_unreaped_child) {
        // Make runnable init task
        wait_node = simple_queue_dequeue(&init_task_wait->wait_queue);
        if(wait_node != NULL) {
            init_task_wait->num_zombie--;
            simple_node_t *exit_status_node = 
                simple_queue_dequeue(&init_task->child_exit_status_list);
            mutex_unlock(&init_task_wait->lock);

            exit_status_t *es = (exit_status_t*)(exit_status_node->thr);

            msg_t* wait_msg = (msg_t *)(wait_node->thr);
            wait_msg->type = WAIT_RESPONSE;
            wait_msg->data.wait_response_data.pid = es->pid;
            wait_msg->data.wait_response_data.status = es->status;
            free(es);
            free(exit_status_node);

            manager_send_msg(wait_msg, wait_msg->req_cpu);
        } else
            mutex_unlock(&init_task_wait->lock);
    } else
        mutex_unlock(&init_task_wait->lock);

    free_pcb_vanish_wait_struct(this_task);

    msg->type = RESPONSE;
    manager_send_msg(msg, msg->req_cpu);
}

void smp_syscall_wait(msg_t* msg) {

    mutex_lock(&ht_pid_pcb_lock);
    pcb_vanish_wait_t* pcb = (pcb_vanish_wait_t*)get_task(msg->data.wait_data.pid);
    mutex_unlock(&ht_pid_pcb_lock);

    if (pcb == NULL)
        panic("Can not find pcb_vanish_wait_t in smp_syscall_wait()");

    task_wait_t *wait = &(pcb->task_wait_struct);


    mutex_lock(&wait->lock);
    // check if can reap
    if (wait->num_zombie == 0 && 
            (wait->num_alive == simple_queue_size(&wait->wait_queue))) {
        // impossible to reap, return error
        mutex_unlock(&wait->lock);

        msg->type = WAIT_RESPONSE;
        msg->data.wait_response_data.pid = ECHILD;
        manager_send_msg(msg, msg->req_cpu);
    } else if (wait->num_zombie == 0) {
        // have alive task (potential zombie), need to block. Enqueue the
        // message to the tail of queue to wait
        simple_queue_enqueue(&wait->wait_queue, &msg->node);
        mutex_unlock(&wait->lock);
    } else {
        // have zombie task, can reap directly
        wait->num_zombie--;
        simple_node_t *exit_status_node = 
            simple_queue_dequeue(&pcb->child_exit_status_list);
        mutex_unlock(&wait->lock);

        exit_status_t *es = (exit_status_t*)(exit_status_node->thr);

        msg->type = WAIT_RESPONSE;
        msg->data.wait_response_data.pid = es->pid;
        msg->data.wait_response_data.status = es->status;
        free(es);
        free(exit_status_node);

        manager_send_msg(msg, msg->req_cpu);
    }
    
}


static int create_pcb_vanish_wait_struct(int pid) {
    pcb_vanish_wait_t* process = malloc(sizeof(pcb_vanish_wait_t));
    if (process == NULL)
        return -1;

    // Put pid to pcb mapping in hashtable
    if (ht_put_task(pid, process) < 0) {
        // out of memory
        free(process);
        return -1;
    }

    process->exit_status = malloc(sizeof(exit_status_t));
    if (process->exit_status == NULL) {
        // out of memory
        ht_remove_task(pid);
        free(process);
        return -1;
    }
    process->exit_status->pid = pid;
    // Initially exit status is 0
    process->exit_status->status = 0;

    process->exit_status_node = malloc(sizeof(simple_node_t));
    if (process->exit_status_node == NULL) {
        // out of memory
        ht_remove_task(pid);
        free(process->exit_status);
        free(process);
        return -1;
    }
    process->exit_status_node->thr = (void*)process->exit_status;

    if(simple_queue_init(&process->child_exit_status_list) < 0) {
        free(process->exit_status_node);
        ht_remove_task(pid);
        free(process->exit_status);
        free(process);
        return -1;
    }

    // Initialize task wait struct
    task_wait_t *task_wait = &process->task_wait_struct;
    if(simple_queue_init(&task_wait->wait_queue) < 0) {
        simple_queue_destroy(&process->child_exit_status_list);
        free(process->exit_status_node);
        ht_remove_task(pid);
        free(process->exit_status);
        free(process);
        return -1;
    }
    if(mutex_init(&task_wait->lock) < 0) {
        simple_queue_destroy(&task_wait->wait_queue);
        simple_queue_destroy(&process->child_exit_status_list);
        free(process->exit_status_node);
        ht_remove_task(pid);
        free(process->exit_status);
        free(process);
        return -1;
    }
    // Initially 0 alive child task
    task_wait->num_alive = 0;
    // Initially 0 zombie child task
    task_wait->num_zombie = 0;

    return 0;
}

static void free_pcb_vanish_wait_struct(pcb_vanish_wait_t* pcb) {
    mutex_destroy(&pcb->task_wait_struct.lock);
    simple_queue_destroy(&pcb->task_wait_struct.wait_queue);
    simple_queue_destroy(&pcb->child_exit_status_list);
    free(pcb);
}