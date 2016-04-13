#include <context_switcher.h>
#include <control_block.h>
#include <asm_helper.h>
#include <string.h>
#include <stdlib.h>
#include <asm_helper.h>
#include <cr.h>
#include <loader.h>
#include <vm.h>
#include <hashtable.h>

#include <scheduler.h>
#include <syscall_inter.h>
#include <asm_atomic.h>
#include <syscall_errors.h>
#include <stdio.h>

/** @brief At most half of the kernel stack to be used as buffer of exec() */
#define MAX_EXEC_BUF (K_STACK_SIZE>>1)

#define EXEC_MAX_ARG_SIZE   128

#define EXEC_MAX_ARGC (MAX_EXEC_BUF/EXEC_MAX_ARG_SIZE-1)

/** @brief System call handler for fork()
 *
 *  This function will be invoked by fork_wrapper().
 *
 *  Creates a new task. The new task receives an exact, coherent copy of all 
 *  memory regions of the invoking task. The new task contains a single thread 
 *  which is a copy of the thread invoking fork() except for the return value of
 *  the system call. The exit status of a newly-created task is 0. If a thread
 *  in the task invoking fork() has a software exception handler registered, the 
 *  corresponding thread in the newly-created task will have exactly the same
 *  handler registered.
 *
 *  Kernel will reject calls to fork() which take place while the invoking task
 *  contains more than one thread.
 *
 *  COW is not implemented for fork().
 *
 *  @return If fork() succeeds, the invoking thread will receive the ID of the
 *          new task’s thread and the newly created thread will receive the 
 *          value zero. Errors are reported via a negative return value, 
 *          in which case no new task has been created.
 */
int fork_syscall_handler() {
    context_switch(OP_FORK, 0);
    return tcb_get_entry((void*)asm_get_esp())->result;
}


/** @brief System call handler for thread_fork
 *
 *  This function will be invoked by thread_fork_wrapper().
 *
 *  Creates a new thread in the current task (i.e., the new thread will share 
 *  all task resources). The invoking thread’s return value in %eax is the 
 *  thread ID of the newly-created thread; the new thread’s return value is 
 *  zero. All other registers in the new thread will be initialized to the same
 *  values as the corresponding registers in the old thread.
 *  A thread newly created by thread fork has no software exception handler
 *  registered.
 *
 *  @return The invoking thread’s return value in %eax is the thread ID of 
 *          the newly-created thread; the new thread’s return value is zero. 
 *          Errors are reported via a negative return value, in which case no 
 *          new thread has been created.
 */
int thread_fork_syscall_handler() {
    context_switch(OP_THREAD_FORK, 0);
    return tcb_get_entry((void*)asm_get_esp())->result;
}


/** @brief System call handler for exec()
 *
 *  Replaces the program currently running in the invoking task with the program
 *  stored in the file named execname. Before the new program begins, %EIP will
 *  be set to the “entry point” (the first instruction of the main() wrapper, 
 *  as advertised by the ELF linker). The stack pointer, %ESP, will be 
 *  initialized appropriately so that the main() wrapper receives four 
 *  parameters: argc, argv, stack_high, stack_low.
 *
 *  This system call will check whether argvec[0] is the same string as execname
 *  
 *  There are limits on the number of arguments that a user program may pass to 
 *  exec() (EXEC_MAX_ARGC), and the length of each argument (EXEC_MAX_ARG_SIZE).
 *
 *  This kernel will reject calls to exec() which take place while the invoking
 *  task contains more than one thread.
 *
 *  @param execname The program that will be replaced with
 *  @param argvec Points to a null-terminated vector of null-terminated string 
 *                arguments. The number of strings in the vector and the vector
 *                itself will be transported into the memory of the new program
 *                where they will serve as the first and second arguments of the
 *                the new program’s main(), respectively.
 *
 *  @return On success, this system call does not return to the invoking 
 *          program, since it is no longer running. If something goes wrong, 
 *          an integer error code less than zero will be returned.
 *
 */
int exec_syscall_handler(char* execname, char **argvec) {
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());

    // first check the number of threads for this task
    if (this_thr->pcb->cur_thr_num > 1) {
        //the invoking task contains more than one thread, reject exec()
        lprintf("exec() with more than one thread");
        MAGIC_BREAK;

        printf("exec() failed because more than one thread\n");
        return EMORETHR;
    }

    // Start argument check

    // Check execname validness
    // Make sure a '\0' is encountered before EXEC_MAX_ARG_SIZE is reached
    int is_check_null = 1;
    int max_len = EXEC_MAX_ARG_SIZE;
    int need_writable = 0;
    if(execname == NULL || execname[0] == '\0') {
        MAGIC_BREAK;
        return EINVAL;
    } else {
        int rv = check_mem_validness((char *)execname, max_len, is_check_null, 
                              need_writable);
        if (rv < 0) {
            if (rv == ERROR_NOT_NULL_TERM)
                rv = ENAMETOOLONG;
            else
                rv = EFAULT;
            MAGIC_BREAK;
            return rv;
        }
    }

    // Check argvec validness
    int i, argc = 0;
    while (argc < EXEC_MAX_ARGC){
        // Make sure &argvec[argc] is valid
        is_check_null = 0;
        max_len = sizeof(char *);
        if(check_mem_validness((char *)(argvec + argc), max_len, is_check_null, 
                    need_writable) < 0) {
            MAGIC_BREAK;
            return EFAULT;
        }

        if(argvec[argc] == NULL) 
            break;

        // Make sure string argvec[argc] is valid and null terminated
        is_check_null = 1;
        max_len = EXEC_MAX_ARG_SIZE;
        int rv = check_mem_validness((char *)argvec[argc], max_len, is_check_null, 
                    need_writable);
        if(rv < 0) {
            if (rv == ERROR_NOT_NULL_TERM)
                rv = E2BIG;
            else
                rv = EFAULT;

            MAGIC_BREAK;
            return rv;
        }

        argc++;
    }

    // check number of arguments
    if (argc == EXEC_MAX_ARGC || argvec[argc] != NULL) {
        MAGIC_BREAK;
        return E2BIG;
    }

    // argvec[0] should be the same string as execname
    if(argvec[0] == NULL || strncmp(execname, argvec[0], EXEC_MAX_ARG_SIZE)) {
        MAGIC_BREAK;
        return EINVAL;
    }
    // Finish argument check


    // Start copying execname and argv to kernel memory
    //char my_execname[strlen(execname) + 1];
    char my_execname[EXEC_MAX_ARG_SIZE];
    memcpy(my_execname, execname, strlen(execname) + 1);

    // copy argv to kernel memory
    char *argv[argc];

    // copy argv[] to kernel memory
    /*
    for(i = 0; i < argc; i++) {
        argv[i] = malloc(strlen(argvec[i])+1);
        if (argv[i] == NULL)
            break;
        memcpy(argv[i], argvec[i], strlen(argvec[i])+1);
    }

    if (i != argc) {
        // not enough memory
        for(i = 0; i < argc; i++) {
            if (argv[i] != NULL)
                free(argv[i]);
            else
                break;
        }
        MAGIC_BREAK;
        return -7;
    }
    */

    char tmp_argv[argc][EXEC_MAX_ARG_SIZE];
    for(i = 0; i < argc; i++) {
        argv[i] = tmp_argv[i];
        memcpy(argv[i], argvec[i], strlen(argvec[i])+1);
    }
    // Finish copying



    // Start exec()

    uint32_t old_pd = get_cr3();

    // Create new page table in case when loadTask fails, we can't
    // recover old address space
    uint32_t new_pd = create_pd();
    if(new_pd == ERROR_MALLOC_LIB) {
        lprintf("create_pd() failed in exec()");
        /*
        for(i = 0; i < argc; i++)
            free(argv[i]);
        */
        MAGIC_BREAK;
        return ENOMEM;
    }
    this_thr->pcb->page_table_base = new_pd;
    set_cr3(new_pd);

    lprintf("Thread %d about to load task, new pd:%x", this_thr->tid, (unsigned int)new_pd);

    // load task
    void *my_program, *usr_esp;
    int rv;
    if ((rv = loadTask(my_execname, argc, (const char**)argv, &usr_esp, &my_program)) < 0) {
        // load task failed
        this_thr->pcb->page_table_base = old_pd;
        set_cr3(old_pd);

        int need_unreserve_frames = 1;
        free_entire_space(new_pd, need_unreserve_frames);

        /*
        for(i = 0; i < argc; i++)
            free(argv[i]);
        */
        lprintf("loadTask() failed in exec() with %d error code\n", rv);
        MAGIC_BREAK;
        return rv;
    }

    int need_unreserve_frames = 1;
    free_entire_space(old_pd, need_unreserve_frames);

    /*
    for(i = 0; i < argc; i++)
        free(argv[i]);
    */

    // modify tcb
    this_thr->k_stack_esp = tcb_get_high_addr((void*)asm_get_esp());

    // Clear swexn handler
    if(this_thr->swexn_struct != NULL) {
        free(this_thr->swexn_struct);
        this_thr->swexn_struct = NULL;
    }

    // load kernel stack, jump to new program
    load_kernel_stack(this_thr->k_stack_esp, usr_esp, my_program, 0);

    // should never reach here
    return 0;
}



/** @brief System call handler for set_status()
 *
 *  Set the exit status of current task
 *
 *  @status The status that will be set to current task
 *
 *  @return Void
 */
void set_status_syscall_handler(int status) {

    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    
    this_thr->pcb->exit_status->status = status;

    lprintf("set status for task %d: %d", this_thr->pcb->pid, status);
}



/*************************** vanish *****************************/

/** @brief The init task */
static pcb_t *init_task;

/** @brief Record init task's pcb
 *
 * @param init_pcb The init task's pcb
 * @return Void
 */
void set_init_pcb(pcb_t *init_pcb) {
    init_task = init_pcb; 
}

/** @brief Hashtable to pid to pcb map, dead process doesn't have
 * entry in hashtable
 */
static hashtable_t ht_pid_pcb;
/** @brief Lock for ht_pid_pcb */
static mutex_t ht_pid_pcb_lock;


/** @brief The size of hash table to stroe pid to pcb map */
#define PID_PCB_HASH_SIZE 1021

/** @brief The hash function for hash table */
static int ht_pid_pcb_hashfunc(void *key) {
    int tid = (int)key;
    return tid % PID_PCB_HASH_SIZE;
}

/** @brief Get wait status of a task
 *
 *  @return The wait status on success; NULL on error
 */
void *get_parent_task(int pid) {
    int is_find = 0;
    void *value = hashtable_get(&ht_pid_pcb, (void*)pid, &is_find);
    if(is_find) {
        return value;
    } 
    return NULL;
}

/** @brief Store pid to pcb maping for a task
 *  The list stores a map from pid to pcb for an alive task
 *
 *  @return On success return zero, on error return -1
 */
int ht_put_task(int pid, pcb_t *pcb) {
    mutex_lock(&ht_pid_pcb_lock);
    int rv = hashtable_put(&ht_pid_pcb, (void *)pid, (void *)pcb); 
    mutex_unlock(&ht_pid_pcb_lock);
    return rv;
}

void ht_remove_task(int pid) {
    int is_find;
    mutex_lock(&ht_pid_pcb_lock);
    hashtable_remove(&ht_pid_pcb, (void*)pid, &is_find);
    mutex_unlock(&ht_pid_pcb_lock);
}

static simple_queue_t zombie_list;

static mutex_t zombie_list_lock;

/** @brief Get next zombie in the thread zombie list
 *
 *  @return On success, return the next zombie thread, on error return NULL
 *
 */
simple_node_t* get_next_zombie() {
    simple_node_t* node = simple_queue_dequeue(&zombie_list);
    return node;
}

mutex_t *get_zombie_list_lock() {
    return &zombie_list_lock;
}

/** @brief Put next zombie in the thread zombie list
 *
 * @param thread_zombie The place to store zombie thread
 *
 * @return 0 on success; -1 on error
 *
 */
int put_next_zombie(simple_node_t* node) {
    int rv = simple_queue_enqueue(&zombie_list, node);
    return rv;
}

/** @brief Allocate a system wide stack devoted to vanish
 *
 *  @return 0 on success; -1 on error
 *
 */
int syscall_vanish_init() {

    // Initialize the hashtable that stores wait status
    ht_pid_pcb.size = PID_PCB_HASH_SIZE;
    ht_pid_pcb.func = ht_pid_pcb_hashfunc;
    if(hashtable_init(&ht_pid_pcb) < 0) {
        lprintf("hashtable_init failed");
        return -1;
    }

    if(mutex_init(&ht_pid_pcb_lock) < 0) {
        lprintf("mutex init failed");
        return -1;
    }

    if(simple_queue_init(&zombie_list) < 0) {
        lprintf("simple_queue_init failed");
        return -1;
    }

    if (mutex_init(&zombie_list_lock) < 0) {
        lprintf("mutex init failed");
        return -1;
    }

    return 0;
}

 /** @brief System call handler for vanish()
  *
  * Terminates execution of the calling thread “immediately.” If the invoking 
  * thread is the last thread in its task, the kernel deallocates all resources
  * in use by the task and makes the exit status of the task available to the 
  * parent task (the task which created this task using fork()) via wait(). If 
  * the parent task is no longer running, the exit status of the task is made 
  * available to the kernel-launched “init” task instead. The statuses of any 
  * child tasks that have not been collected via wait() are also be made 
  * available to the kernel-launched “init” task.
  *
  * @param is_kernel_kill A flag indicating if kernel is the caller
  * 
  * @return Should never return
  *
  */
void vanish_syscall_handler(int is_kernel_kill) {

    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        lprintf("tcb is NULL");
        panic("tcb is NULL");
    }

    lprintf("vanish syscall handler called for %d", this_thr->tid);

    // Get pcb of current task
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        panic("This task's pcb is NULL");
    }

    // Set init as the thread's temporary task
    this_thr->pcb = init_task;
    // Use init task's page table until death
    set_cr3(init_task->page_table_base);
    // One less thread in current task
    int cur_thr_num = atomic_add(&this_task->cur_thr_num, -1);

    // If this task has more than one thread left, do not report exit status 
    // and free task's resources. Just free resources used by this thread
    if(cur_thr_num == 0) {
        // The thread to vanish is the last thread in the task, will report 
        // exit status to its father and free resources used by this task

        if(is_kernel_kill) {
            // The thread is killed by the kernel
            // set_status(-2) first
            this_task->exit_status->status = -2;
        }

        // Assume task init shouldn't vanish and this task isn't the task init

        // ====== Start report exit status to its parent or init task =========

        // Get hashtable's lock
        mutex_lock(&ht_pid_pcb_lock);
        // Get parent task's pcb
        pcb_t *parent_task = get_parent_task(this_task->ppid);

        if(parent_task == NULL) {
            // Parent is dead 
            lprintf("parent %d is dead", this_task->ppid);
            // Report to init task
            parent_task = init_task;
        }

        // Get parent task's task_wait lock
        task_wait_t *task_wait = &parent_task->task_wait_struct;
        mutex_lock(&task_wait->lock);
        // Release hashtable's lock
        mutex_unlock(&ht_pid_pcb_lock);

        // Put exit_status into parent's child exit status list  
        simple_queue_enqueue(&parent_task->child_exit_status_list, 
                                                   this_task->exit_status_node);

        task_wait->num_zombie++;
        task_wait->num_alive--;

        // Make runnable a parent task's thread that is blocked on wait() if any
        simple_node_t* node = simple_queue_dequeue(&task_wait->wait_queue);
        tcb_t *wait_thr;
        if(node != NULL) {
            wait_thr = (tcb_t *)node->thr;
            mutex_unlock(&task_wait->lock);
            context_switch(OP_MAKE_RUNNABLE, (uint32_t)wait_thr);
        } else {
            mutex_unlock(&task_wait->lock);
        }
        // ======= End report exit status to its parent or init task ===========

        // Free resources (page table, hash table entry and pcb) for this task
        uint32_t old_pd = this_task->page_table_base;

        // Free old address space
        int need_unreserve_frames = 1;
        free_entire_space(old_pd, need_unreserve_frames);

        // Get hashtable's lock
        mutex_lock(&ht_pid_pcb_lock);
        // Get this task's pcb's lock
        task_wait_t *this_task_wait = &this_task->task_wait_struct;
        mutex_lock(&this_task_wait->lock);
        // Delete this task's hashtable entry in hashtable
        int is_find; 
        hashtable_remove(&ht_pid_pcb, (void*)this_task->pid, &is_find);
        if(is_find == 0) {
            lprintf("delete task %d in hashtable failed", this_task->pid);
            MAGIC_BREAK;
        }
        // Release pcb's lock
        mutex_unlock(&this_task_wait->lock);
        // Release hashtable's lock
        mutex_unlock(&ht_pid_pcb_lock);

        // Now nobody can alter resources in current task's pcb except itself

        // Report unreaped children's status to task init
        task_wait_t *init_task_wait = &init_task->task_wait_struct;
        mutex_lock(&init_task_wait->lock);

        // Put children tasks' exit_status into init task's child exit status list
        int has_unreaped_child = 0;
        while((node = simple_queue_dequeue(&this_task->child_exit_status_list)) != NULL) {
            has_unreaped_child = 1;
            simple_queue_enqueue(&init_task->child_exit_status_list, node);
            init_task_wait->num_zombie++;
        }

        if (has_unreaped_child) {
            // Make runnable init task
            node = simple_queue_dequeue(&init_task_wait->wait_queue);
            if(node != NULL) {
                wait_thr = (tcb_t *)node->thr;
                mutex_unlock(&init_task_wait->lock);
                context_switch(OP_MAKE_RUNNABLE, (uint32_t)wait_thr);
            } else {
                mutex_unlock(&init_task_wait->lock);
            }
        } else {
            mutex_unlock(&init_task_wait->lock);
        }

        // Delete resources in pcb and free pcb
        tcb_free_process(this_task);
    }

    // Add self to system wide zombie list. Note that stack space of 
    // vanish_syscall_handler() is used for simple_node. Because this stack 
    // will not be destroied until this thread is freed by other threads. 
    simple_node_t node;
    node.thr = this_thr;

    mutex_lock(&zombie_list_lock);
    put_next_zombie(&node);
    mutex_unlock(&zombie_list_lock);

    context_switch(OP_BLOCK, 0);

    lprintf("Vanished thread will never reach here");
    MAGIC_BREAK;

}

 /** @brief System call handler for wait()
  *
  * Collects the exit status of a task and stores it in the integer 
  * referenced by status ptr. The wait() system call may be invoked 
  * simultaneously by any number of threads in a task; exited child tasks are
  * matched to wait()’ing threads in FIFO order. Threads which cannot collect an
  * already-exited child task when there exist child tasks which have not yet
  * exited will generally block until a child task exits and collect the status
  * of an exited child task. However, threads which will definitely not be able
  * to collect the status of an exited child task in the future must not block
  * forever; in that case, wait() will return an integer error code less than 
  * zero. The invoking thread may specify a status ptr parameter of zero (NULL)
  * to indicate that it wishes to collect the ID of an exited task but wishes to
  * ignore the exit status of that task. Otherwise, if the status ptr parameter
  * does not refer to writable memory, wait() will return an integer error code
  * less than zero instead of collecting a child task.
  *
  * @param status_ptr The integer pointer to store exit status
  * 
  * @return If no error occurs, the return value of wait() is the thread ID of 
  *         the original thread of the exiting task. On error, return an integer
  *         error code less than zero.
  *
  */
int wait_syscall_handler(int *status_ptr) {

    // Check if status_ptr is valid memory
    int is_check_null = 0;
    int max_len = sizeof(int);
    int need_writable = 1;
    if(status_ptr != NULL && 
       check_mem_validness((char *)status_ptr, max_len, is_check_null, 
                                                         need_writable) < 0) {
        return EFAULT;
    }

    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        lprintf("tcb is NULL");
        MAGIC_BREAK;
    }

    // define a node for simple queue using stack space
    simple_node_t node;
    node.thr = this_thr;

    // Get current task
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        lprintf("pcb is NULL");
        MAGIC_BREAK;
    }

    task_wait_t *wait = &(this_task->task_wait_struct);

    while (1) {
        mutex_lock(&wait->lock);
        // check if can reap
        if (wait->num_zombie == 0 && 
                (wait->num_alive == simple_queue_size(&wait->wait_queue))) {
            // impossible to reap, return error
            mutex_unlock(&wait->lock);
            return ECHILD;
        } else if (wait->num_zombie == 0) {
            // have alive task (potential zombie), need to block. Enter the
            // tail of queue to wait, note that stack memory is used for 
            // simple_node_t. Because the stack of wait_syscall_handler()
            // will not be destroied until return, so it is safe
            simple_queue_enqueue(&wait->wait_queue, &node);
            mutex_unlock(&wait->lock);

            context_switch(OP_BLOCK, 0);
            continue;
        } else {
            // have zombie task, can reap directly
            wait->num_zombie--;
            simple_node_t *node = simple_queue_dequeue(&this_task->child_exit_status_list);
            mutex_unlock(&wait->lock);

            exit_status_t *es = (exit_status_t*)node->thr;

            if(status_ptr != NULL)
                *status_ptr = es->status;
            int rv = es->pid;
            lprintf("task %d reaped task %d, with exit status %d", this_task->pid, rv, es->status);
            free(es);
            free(node);
            return rv;
        }
    }

}

