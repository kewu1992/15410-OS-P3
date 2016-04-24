/** @file syscall_lifecycle.c
 *  @brief System calls related to life cycle
 *
 *  This file contains implementations of system calls that are related to 
 *  life cycle.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

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
#include <smp.h>

/** @brief At most half of the kernel stack to be used as buffer of exec() */
#define MAX_EXEC_BUF (K_STACK_SIZE>>1)

/** @brief At most 128 bytes per argument of exec() */
#define EXEC_MAX_ARG_SIZE   128

/** @brief The maxinum number of arguments of exec() */
#define EXEC_MAX_ARGC (MAX_EXEC_BUF/EXEC_MAX_ARG_SIZE-1)



/** @brief A list that stores the zombie thread to be freed  */
static simple_queue_t* zombie_lists[MAX_CPUS];

/** @brief The lock for the zombie list */
static mutex_t* zombie_list_locks[MAX_CPUS];


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

int fork_create_process(tcb_t* new_thr, tcb_t* old_thr) {
    // allocate resources for new process 
    // clone page table
    uint32_t new_page_table_base = clone_pd();
    if (new_page_table_base == ERROR_MALLOC_LIB ||
        new_page_table_base == ERROR_NOT_ENOUGH_MEM) {

        // clone_pd() error (out of memory, either physical frames or
        // kernel memory)
        return -1;
    }

    // clone swexn handler
    if(old_thr->swexn_struct != NULL) {
        new_thr->swexn_struct = malloc(sizeof(swexn_t));
        if(new_thr->swexn_struct == NULL) {
            int need_unreserve_frames = 1;
            free_entire_space(new_page_table_base, 
                    need_unreserve_frames);
            return -1;
        }
        memcpy(new_thr->swexn_struct, old_thr->swexn_struct, 
                sizeof(swexn_t));
    }
    
    // create new process
    if (tcb_create_process_only(new_thr, old_thr, 
                                        new_page_table_base) == NULL) {
        // create new process failed, out of memory
        free(new_thr->swexn_struct);
        int need_unreserve_frames = 1;
        free_entire_space(new_page_table_base, need_unreserve_frames);
        return -1;
    }

    return 0;
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
        return EMORETHR;
    }

    // Start argument check

    // Check execname validness
    // Make sure a '\0' is encountered before EXEC_MAX_ARG_SIZE is reached
    int is_check_null = 1;
    int max_len = EXEC_MAX_ARG_SIZE;
    int need_writable = 0;
    if(execname == NULL || execname[0] == '\0') {
        return EINVAL;
    } else {
        int rv = check_mem_validness((char *)execname, max_len, is_check_null, 
                              need_writable);
        if (rv < 0) {
            if (rv == ERROR_NOT_NULL_TERM)
                rv = ENAMETOOLONG;
            else
                rv = EFAULT;
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
            return EFAULT;
        }

        if(argvec[argc] == NULL) 
            break;

        // Make sure string argvec[argc] is valid and null terminated
        is_check_null = 1;
        max_len = EXEC_MAX_ARG_SIZE;
        int rv = check_mem_validness((char *)argvec[argc], max_len, 
                is_check_null, need_writable);
        if(rv < 0) {
            if (rv == ERROR_NOT_NULL_TERM)
                rv = E2BIG;
            else
                rv = EFAULT;

            return rv;
        }

        argc++;
    }

    // check number of arguments
    if (argc == EXEC_MAX_ARGC || argvec[argc] != NULL) {
        return E2BIG;
    }

    // argvec[0] should be the same string as execname
    if(argvec[0] == NULL || strncmp(execname, argvec[0], EXEC_MAX_ARG_SIZE)) {
        return EINVAL;
    }
    // Finish argument check


    // Start copying execname and argv to kernel memory
    char my_execname[EXEC_MAX_ARG_SIZE];
    memcpy(my_execname, execname, strlen(execname) + 1);

    // copy argv to kernel memory
    char *argv[argc];

    char tmp_argv[argc][EXEC_MAX_ARG_SIZE];
    for(i = 0; i < argc; i++) {
        argv[i] = tmp_argv[i];
        memcpy(argv[i], argvec[i], strlen(argvec[i])+1);
    }
    // Finish copying


    // Start exec()

    uint32_t old_pd = get_cr3();

    // Create new page table to load new binary file. If loadTask() fails, 
    // we can recover to old address space
    uint32_t new_pd = create_pd();
    if(new_pd == ERROR_MALLOC_LIB) {
        return ENOMEM;
    }
    this_thr->pcb->page_table_base = new_pd;
    set_cr3(new_pd);

    // load task
    void *my_program, *usr_esp;
    int rv;
    if ((rv = loadTask(my_execname, argc, (const char**)argv, &usr_esp, 
                    &my_program)) < 0) {

        // load task failed, reset to old page table, free new page table
        this_thr->pcb->page_table_base = old_pd;
        set_cr3(old_pd);

        int need_unreserve_frames = 1;
        free_entire_space(new_pd, need_unreserve_frames);

        return rv;
    }

    // load task succeed, free old page table
    int need_unreserve_frames = 1;
    free_entire_space(old_pd, need_unreserve_frames);

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
    
    this_thr->pcb->status = status;
}

/** @brief The init task, this will be used as parent task to store exit status
 *         when an orphan task vanish() */
static pcb_t* init_task;

/** @brief Record init task's pcb
 *
 * @param init_pcb The init task's pcb
 * @return On success return 0, on error return -1
 */
int set_init_pcb(pcb_t *init_pcb) {
    init_task = init_pcb; 
    // construct message
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    this_thr->my_msg->req_thr = this_thr;
    this_thr->my_msg->req_cpu = smp_get_cpu();
    this_thr->my_msg->type = SET_INIT_PCB;
    this_thr->my_msg->data.set_init_pcb_data.pid = init_pcb->pid;
    context_switch(OP_SEND_MSG, 0);

    return this_thr->my_msg->data.response_data.result;
}




/** @brief Get next zombie in the thread zombie list
 *
 *  @return On success, return the next zombie thread, on error return NULL
 */
simple_node_t* get_next_zombie() {
    simple_node_t* node = simple_queue_dequeue(zombie_lists[smp_get_cpu()]);
    return node;
}

/** @brief Get the lock for the zombie list
 *
 *  @return Lock for the zombie list
 */
mutex_t *get_zombie_list_lock() {
    return zombie_list_locks[smp_get_cpu()];
}

/** @brief Put next zombie in the thread zombie list
 *
 * @param node The node describing the zombie thread
 *
 * @return 0 on success; -1 on error
 *
 */
int put_next_zombie(simple_node_t* node) {
    int rv = simple_queue_enqueue(zombie_lists[smp_get_cpu()], node);
    return rv;
}

/** @brief Initialize vanish syscall
 *
 *  @return 0 on success; -1 on error
 *
 */
int syscall_vanish_init() {
    zombie_lists[smp_get_cpu()] = malloc(sizeof(simple_queue_t));
    if (zombie_lists[smp_get_cpu()] == NULL)
        return -1;

    zombie_list_locks[smp_get_cpu()] = malloc(sizeof(mutex_t));
    if (zombie_list_locks[smp_get_cpu()] == NULL)
        return -1;

    if(simple_queue_init(zombie_lists[smp_get_cpu()]) < 0)
        return -1;


    if (mutex_init(zombie_list_locks[smp_get_cpu()]) < 0)
        return -1;


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
  * @param is_kernel_kill A flag indicating if kernel is the caller of the 
  * function.
  * 
  * @return Should never return
  *
  */
void vanish_syscall_handler(int is_kernel_kill) {

    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        panic("tcb is NULL");
    }

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
            // The thread is killed by the kernel set_status(-2) first
            this_task->status = -2;
        }

        // construct message
        this_thr->my_msg->req_thr = this_thr;
        this_thr->my_msg->req_cpu = smp_get_cpu();
        this_thr->my_msg->type = VANISH;
        this_thr->my_msg->data.vanish_data.pid = this_task->pid;
        this_thr->my_msg->data.vanish_data.ppid = this_task->ppid;
        this_thr->my_msg->data.vanish_data.status = this_task->status;

        context_switch(OP_SEND_MSG, 0);

        // Free resources (page table, hash table entry and pcb) for this task
        uint32_t old_pd = this_task->page_table_base;

        // Free old address space
        int need_unreserve_frames = 1;
        free_entire_space(old_pd, need_unreserve_frames);

        // Delete resources in pcb and free pcb
        tcb_free_process(this_task);
    }

    // send this thread back to the cpu who malloc() it
    this_thr->my_msg->req_thr = this_thr;
    this_thr->my_msg->req_cpu = smp_get_cpu();
    this_thr->my_msg->type = VANISH_BACK;
    this_thr->my_msg->data.vanish_back_data.ori_cpu = this_thr->ori_cpu;
    context_switch(OP_SEND_MSG, 0);

    // now this thread is running on the cpu who malloc() it

    // Add self to system wide zombie list. Note that stack space of 
    // vanish_syscall_handler() is used for simple_node. Because this stack 
    // will not be destroied until this thread is freed by other threads. 
    simple_node_t node;
    node.thr = this_thr;

    mutex_lock(zombie_list_locks[smp_get_cpu()]);
    put_next_zombie(&node);
    mutex_unlock(zombie_list_locks[smp_get_cpu()]);

    context_switch(OP_BLOCK, 0);

    panic("Vanished thread will never reach here");

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

    // construct message
    this_thr->my_msg->req_thr = this_thr;
    this_thr->my_msg->req_cpu = smp_get_cpu();
    this_thr->my_msg->type = WAIT;
    this_thr->my_msg->data.wait_data.pid = this_thr->pcb->pid;

    context_switch(OP_SEND_MSG, 0);

    if(status_ptr != NULL && this_thr->my_msg->data.wait_response_data.pid > 0)
        *status_ptr = this_thr->my_msg->data.wait_response_data.status;

    return this_thr->my_msg->data.wait_response_data.pid;
}

