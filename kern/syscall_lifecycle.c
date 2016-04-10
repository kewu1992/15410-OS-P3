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
#include <syscall_lifecycle.h>
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
    lprintf("fork called");
    context_switch(1, 0);
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
    lprintf("thread fork called");
    context_switch(2, 0);
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
        int rv = is_mem_valid((char *)execname, max_len, is_check_null, 
                              need_writable);
        if (rv < 0) {
            if (rv == E2BIG)
                rv = ENAMETOOLONG;
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
        if(is_mem_valid((char *)(argvec + argc), max_len, is_check_null, 
                    need_writable) != 1) {
            MAGIC_BREAK;
            return EFAULT;
        }

        if(argvec[argc] == NULL) 
            break;

        // Make sure string argvec[argc] is valid and null terminated
        is_check_null = 1;
        max_len = EXEC_MAX_ARG_SIZE;
        int rv = is_mem_valid((char *)argvec[argc], max_len, is_check_null, 
                    need_writable);
        if(rv < 0) {
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

        free_entire_space(new_pd);

        /*
        for(i = 0; i < argc; i++)
            free(argv[i]);
        */
        lprintf("loadTask() failed in exec() with %d error code\n", rv);
        MAGIC_BREAK;
        return rv;
    }

    free_entire_space(old_pd);

    /*
    for(i = 0; i < argc; i++)
        free(argv[i]);
    */

    // modify tcb
    this_thr->k_stack_esp = tcb_get_high_addr((void*)asm_get_esp());

    // load kernel stack, jump to new program
    load_kernel_stack(this_thr->k_stack_esp, usr_esp, my_program, 0);

    // should never reach here
    return 0;
}



/*************************** set_status *************************/

/** @brief Set the exit status of current task
 *
 *  @return Void
 */
void set_status_syscall_handler(int status) {

    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        lprintf("tcb is NULL");
        MAGIC_BREAK;
    }

    // Get current task
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        lprintf("pcb is NULL");
        MAGIC_BREAK;
    }

    this_task->exit_status = status;

    lprintf("set status for task %d: %d", this_task->pid, status);

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

static list_t zombie_list;

/** @brief Get next zombie in the thread zombie list
 *
 * @param thread_zombie The place to store zombie thread
 *
 * @return 0 on success; -1 on error
 *
 */
int get_next_zombie(tcb_t **thread_zombie) {
    return list_remove_first(&zombie_list, (void **)thread_zombie);

}

mutex_t *get_zombie_list_lock() {
    return &zombie_list.mutex;
}

/** @brief Put next zombie in the thread zombie list
 *
 * @param thread_zombie The place to store zombie thread
 *
 * @return 0 on success; -1 on error
 *
 */
int put_next_zombie(tcb_t *thread_zombie) {

    return list_append(&zombie_list, (void *)thread_zombie);

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

    if(list_init(&zombie_list) < 0) {
        lprintf("list_init failed");
        return -1;
    }

    return 0;
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

/** @brief Free pcb and all resources that are associated with it */
static void vanish_free_pcb(pcb_t *task) {

    // The list should be empty now
    void *data;
    if (list_remove_first(&task->child_exit_status_list, (void **)&data) == 0) {
        // something's wrong
        lprintf("child_exit_status_list isn't empty when task is freeing itself");
        MAGIC_BREAK;
    }
    int need_free_data = 1;
    list_destroy(&task->child_exit_status_list, need_free_data);

    task_wait_t *task_wait_struct = &task->task_wait_struct;
    mutex_destroy(&task_wait_struct->lock);
    // No need to destroy task->wait_queue because it's a simple (use stack as
    // node place holder).

    free(task);

}

/** @brief Vanish syscall handler
  *
  * @param is_kernel_kill A flag indicating if kernel is the caller
  * 
  * @return void 
  *
  */
void vanish_syscall_handler(int is_kernel_kill) {

    lprintf("vanish syscall handler called");

    // For the moment, assume there's only one thread for each task

    // Who is my parent?
    // Get tcb of current thread

    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        lprintf("tcb is NULL");
        panic("tcb is NULL");
    }

    // Get pcb of current task
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        panic("This task's pcb is NULL");
    }


    //*******************Need to lock this operation
    // One less thread in current task
    int cur_thr_num = atomic_add(&this_task->cur_thr_num, -1);

    // If this task has more than one thread left, do not report
    // exit status, proceed directly to remove resources used by this thread
    // int is_only_thread_in_task = 0;
    if(cur_thr_num == 0) {
        // The thread to vanish is the last thread in the task, will remove
        // resources used by this task

        if(is_kernel_kill) {
            // The thread is killed by the kernel
            // set_status(-2) first
            set_status_syscall_handler(-2);
        }

        // Assume task idle(pid 0), shouldn't vanish
        // The thread to vanish is the last thread in the task, will report 
        // exit status to its father and remove resources used by this task

        // *****************Assume now this task isn't the task init

        // ======== Report exit status to its father or init task ===========

        // Construct exit_status
        exit_status_t *exit_status = malloc(sizeof(exit_status_t));
        if(exit_status == NULL) {
            lprintf("malloc failed");
            MAGIC_BREAK;
        }
        if(this_task->pid == 0) {
            lprintf("Exit task idle? this_task pid 0?!");
            MAGIC_BREAK;
        }
        exit_status->pid = this_task->pid;
        exit_status->status = this_task->exit_status;

        // Get hashtable's lock
        mutex_lock(&ht_pid_pcb_lock);
        // Get parent task's pcb
        pcb_t *parent_task = get_parent_task(this_task->ppid);

        if(parent_task == NULL) {
            // Parent is dead 
            lprintf("parent %d is dead", this_task->ppid);
            // Report to init task
            // Assume init task is not dead**************
            parent_task = init_task;
        }

        // Get pcb's lock
        task_wait_t *task_wait = &parent_task->task_wait_struct;
        mutex_lock(&task_wait->lock);
        // Release hashtable's lock
        mutex_unlock(&ht_pid_pcb_lock);

        // Put exit_status into parent's child exit status list  
        list_t *child_exit_status_list = &parent_task->child_exit_status_list;
        if(child_exit_status_list == NULL) {
            lprintf("child_exit_status_list is NULL, but parent is alive?!");
            MAGIC_BREAK;
        }
        if(list_append(child_exit_status_list, (void *)exit_status) < 0) {
            lprintf("list_append failed");
            MAGIC_BREAK;
        }

        // Make runnable a thread that's block on wait if there's any
        //task_wait_t *task_wait = &parent_task->task_wait_struct;
        //mutex_lock(&task_wait->lock);
        task_wait->num_zombie++;
        task_wait->num_alive--;

        simple_node_t* node = simple_queue_dequeue(&task_wait->wait_queue);
        tcb_t *wait_thr;
        if(node != NULL) {
            wait_thr = (tcb_t *)node->thr;
            mutex_unlock(&task_wait->lock);
            context_switch(4, (uint32_t)wait_thr);
        } else {
            mutex_unlock(&task_wait->lock);
        }




        // Free resources (page table, hash table entry and pcb) for this task itself
        uint32_t old_pd = this_task->page_table_base;

        // Use init task's page table until death
        this_task->page_table_base = init_task->page_table_base;
        set_cr3(init_task->page_table_base);

        // Free old address space
        int ret = free_entire_space(old_pd);
        if(ret < 0) {
            lprintf("free_entire_space failed");    
            MAGIC_BREAK;
        }

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
        // Assume init task's alive ***************
        task_wait_t *init_task_wait = &init_task->task_wait_struct;
        mutex_lock(&init_task_wait->lock);

        // Put children tasks' exit_status into init task's child exit status list
        list_t *init_task_child_exit_status_list = 
            &init_task->child_exit_status_list;
        if(init_task_child_exit_status_list == NULL) {
            lprintf("init_child_exit_status_list is NULL, but task is alive?!");
            MAGIC_BREAK;
        }

        int has_unreaped_child = 0;
        exit_status_t *es;
        while(list_remove_first(&this_task->child_exit_status_list, (void **)&es) == 0) {
            has_unreaped_child = 1;
            if(list_append(init_task_child_exit_status_list, (void *)es) < 0) {
                lprintf("list_append failed");
                MAGIC_BREAK;
            }
            init_task_wait->num_zombie++;
        }

        if (has_unreaped_child) {
            // Make runnable init task
            node = simple_queue_dequeue(&init_task_wait->wait_queue);
            if(node != NULL) {
                wait_thr = (tcb_t *)node->thr;
                mutex_unlock(&init_task_wait->lock);
                context_switch(4, (uint32_t)wait_thr);
            } else {
                mutex_unlock(&init_task_wait->lock);
            }
        } else {
            mutex_unlock(&init_task_wait->lock);
        }

        // Delete resources in pcb and free pcb
        // Set init as the thread's temporary task
        this_thr->pcb = init_task;
        // Free resources in pcb
        vanish_free_pcb(this_task);
    }

    // Add self to system wide zombie list, let next thread in scheduler's 
    // queue run.
    if(put_next_zombie(this_thr) < 0) {
        lprintf("put_next_zombie failed");
        MAGIC_BREAK;
    }

    context_switch(3, 0);

    lprintf("Vanished thread will never reach here");
    MAGIC_BREAK;

}

/** @brief Release resources used by a thread and get the next thread to run
 *
 * @param thread The thread to release resources
 *
 * @return 0 on success; A negative integer on error
 *
 */
int vanish_wipe_thread(tcb_t *thread) {

    // Free thread resource: tcb, kernel stack
    tcb_free_thread(thread);

    return 0;

}

/*************************** Wait *************************/


/** @brief Store pid to pcb maping for a task
 *  The list stores a map from pid to pcb for an alive task
 *
 *  @return Void
 */
void ht_put_task(int pid, pcb_t *pcb) {
    mutex_lock(&ht_pid_pcb_lock);
    hashtable_put(&ht_pid_pcb, (void *)pid, (void *)pcb); 
    mutex_unlock(&ht_pid_pcb_lock);
}



#define ERR_PARAM -2
#define ERR_NO_CHILD -1
int wait_syscall_handler(int *status_ptr) {

    // Check if status_ptr is valid memory
    int is_check_null = 0;
    int max_len = sizeof(int);
    int need_writable = 1;
    if(status_ptr != NULL && !is_mem_valid((char *)status_ptr, max_len, 
                is_check_null, need_writable)) {
        return ERR_PARAM;
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
                (wait->num_alive == simple_queue_size(&wait->wait_queue))){
            // impossible to reap, return error
            mutex_unlock(&wait->lock);
            return ERR_NO_CHILD;
        } else if (wait->num_zombie == 0) {
            // have alive task (potential zombie), need to block. Enter the
            // tail of queue to wait, note that stack memory is used for 
            // simple_node_t. Because the stack of wait_syscall_handler()
            // will not be destroied until return, so it is safe
            simple_queue_enqueue(&wait->wait_queue, &node);
            mutex_unlock(&wait->lock);

            context_switch(3, 0);
            continue;
        } else {
            // have zombie task, can reap directly
            wait->num_zombie--;
            mutex_unlock(&wait->lock);

            exit_status_t *es;
            if (list_remove_first(&this_task->child_exit_status_list, (void **)&es) < 0) {
                // something wrong
                lprintf("wait_syscall_handler() --> list_remove_first() failed!");
                MAGIC_BREAK;
            }

            if(status_ptr != NULL)
                *status_ptr = es->status;
            int rv = es->pid;
            lprintf("task %d reaped task %d, with exit status %d", this_task->pid, rv, es->status);
            free(es);
            return rv;
        }
    }

}

