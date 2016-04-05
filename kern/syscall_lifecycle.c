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

#define EXEC_MAX_ARGC   32
#define EXEC_MAX_ARG_SIZE   128

int fork_syscall_handler() {
    context_switch(1, 0);
    return tcb_get_entry((void*)asm_get_esp())->result;
}

int exec_syscall_handler(char* execname, char **argvec) {

    // Start argument check

    // Check execname validness
    // Make sure a '\0' is encountered before EXEC_MAX_ARG_SIZE is reached
    int is_check_null = 1;
    int max_len = EXEC_MAX_ARG_SIZE;
    int need_writable = 0;
    if(execname == NULL || 
            !is_mem_valid((char *)execname, max_len, is_check_null, 
                need_writable) || execname[0] == '\0') {
        return -1;
    }

    // Check argvec validness
    int i, argc = 0;
    while (argc < EXEC_MAX_ARGC){
        // Make sure &argvec[argc] is valid
        is_check_null = 0;
        max_len = sizeof(char *);
        if(!is_mem_valid((char *)(argvec + argc), max_len, is_check_null, 
                    need_writable)) {
            return -1;
        }

        if(argvec[argc] == NULL) break;

        // Make sure string argvec[argc] is null terminated
        is_check_null = 1;
        max_len = EXEC_MAX_ARG_SIZE;
        if(!is_mem_valid((char *)argvec[argc], max_len, is_check_null, 
                    need_writable)) {
            return -1;
        }

        argc++;
    }
    // check arguments number
    if (argc == EXEC_MAX_ARGC)
        return -1;

    // Make sure argvec is null terminated
    if(argvec[argc] != NULL) 
        return -1;

    // argvec[0] should be the same string as execname
    if(argvec[0] == NULL || strncmp(execname, argvec[0], EXEC_MAX_ARG_SIZE)) {
        return -1;
    }
    // Finish argument check


    // need to copy execname to kernel memory
    char my_execname[strlen(execname) + 1];
    memcpy(my_execname, execname, strlen(execname) + 1);

    // need to copy argv and argv[] to kernel memory
    char *argv[argc];
    memset(argv, 0, argc);

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
        return -1;
    }

    // check arguments finished, start exec()

    uint32_t old_pd = get_cr3();

    // create new page table
    set_cr3(create_pd());
    // load task

    void *my_program, *usr_esp;
    if ((my_program = loadTask(my_execname, argc, (const char**)argv, &usr_esp)) == NULL) {
        // load task failed

        // free_pd(get_cr3());

        set_cr3(old_pd);

        return -1;
    }

    // free_pd(old_pd);

    for(i = 0; i < argc; i++)
        free(argv[i]);

    // modify tcb
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    this_thr->k_stack_esp = tcb_get_high_addr((void*)asm_get_esp());

    // load kernel stack, jump to new program
    load_kernel_stack(this_thr->k_stack_esp, usr_esp, my_program);

    // should never reach here
    return 0;
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

/** @brief The size of hash table to stroe pid to pcb map */
#define PID_PCB_HASH_SIZE 1021

/** @brief The hash function for hash table */
static int ht_pid_pcb_hashfunc(void *key) {
    int tid = (int)key;
    return tid % PID_PCB_HASH_SIZE;
}

//void asm_vanish(void *safe_stack_high, tcb_t *this_thr, 
//        int is_only_thread_in_task);
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


void vanish_syscall_handler() {

    lprintf("vanish syscall handler called, cr3: %x", (unsigned)get_cr3());

    // For the moment, assume there's only one thread for each task

    // Who is my parent?
    // Get tcb of current thread
    
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        lprintf("tcb is NULL");
        panic("tcb is NULL");
    }

    lprintf("tid: %d", this_thr->tid);

    // Get pcb of current task
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        panic("This task's pcb is NULL");
    }
    

    //*******************Need to lock this operation
    // One less thread in current task
    spinlock_lock(&this_task->lock_cur_thr_num);
    this_task->cur_thr_num--;
    spinlock_unlock(&this_task->lock_cur_thr_num);
    int cur_thr_num = this_task->cur_thr_num;

    // If this task has more than one thread left, do not report
    // exit status, proceed directly to remove resources used by this thread
    // int is_only_thread_in_task = 0;
    if(cur_thr_num == 0) {
        // The thread to vanish is the last thread in the task, will remove
        // resources used by this task

        // Assume task init, pid 0, shouldn't vanish

        // *****************Assume now this task isn't the task init

        // ****************Assume task's parent is alive
        // Report exit status to it

        // Construct exit_status
        exit_status_t *exit_status = malloc(sizeof(exit_status_t));
        if(this_task->pid == 0) {
            lprintf("Exit task init? this_task pid 0?!");
            MAGIC_BREAK;
        }
        exit_status->pid = this_task->pid;
        exit_status->status = this_task->exit_status;

        // Get parent task's pcb
        pcb_t *parent_task = get_parent_task(this_task->ppid);
        if(parent_task == NULL) {
            // Parent is dead 
            lprintf("parent %d is dead", this_task->ppid);
            MAGIC_BREAK;
        }

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

        // Make runnable thread block on wait if there's any
        task_wait_t *task_wait = &this_task->task_wait_struct;
        mutex_lock(&task_wait->lock);
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

    } else {
        lprintf("cur_thr_num != 0?! %d", cur_thr_num);
        MAGIC_BREAK;
    }


    lprintf("task %d vanish syscall handler finished, cr3: %x", this_task->pid,
            (unsigned)get_cr3());

    
    lprintf("Some useless words");
    lprintf("Some more useless words");

    // Free resources that this task itself can free
    if(this_task->cur_thr_num == 0) {
        lprintf("vanish_wipe_thread called for tid: %d, only thread in task", 
                this_thr->tid);
        // Last thread in the task
        uint32_t old_pd = this_task->page_table_base;

        // Use init task's page table until death
        set_cr3(init_task->page_table_base);

        // Free old address space
        int ret = free_entire_space(old_pd);
        if(ret < 0) {
            lprintf("free_entire_space failed");    
            MAGIC_BREAK;
        }

        // Free pcb, destroy stuff in the pcb
        // TBD*****************************
    } else {
        lprintf("vanish_wipe_thread called for tid: %d,"
                "cur_thr_num != 0?! %d", this_thr->tid, this_task->cur_thr_num);
        MAGIC_BREAK;
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

    /*
    int ret;
    pcb_t *task = thread->pcb; 
    if(task->cur_thr_num == 0) {
        lprintf("vanish_wipe_thread called for tid: %d, only thread in task", 
                thread->tid);
        // Last thread in the task
        uint32_t old_pd = task->page_table_base;

        // Free old address space
        ret = free_entire_space(old_pd);
        if(ret < 0) return ret;

        // Free pcb, destroy stuff in the pcb
        // TBD*****************************
    } else {
        lprintf("vanish_wipe_thread called for tid: %d,"
                "cur_thr_num != 0?! %d", thread->tid, task->cur_thr_num);
        MAGIC_BREAK;
    }
    */
    


    // Free thread resource: tcb, kernel stack
    tcb_free_thread(thread);

    return 0;

}

/*************************** Wait *************************/


/** @brief Store pid to pcb maping for a task
 *  The list stores a map from pid to pcb for an alive task
 *
 *  @return 0 on success; -1 on error
 */
void ht_put_task(int pid, pcb_t *pcb) {

    hashtable_put(&ht_pid_pcb, (void *)pid, (void *)pcb); 

}



#define ERR_PARAM -2
#define ERR_NO_CHILD -1
int wait_syscall_handler(int *status_ptr) {
    return 0;
    /*

    lprintf("wait syscall handler called");

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

    // Get current task
    pcb_t *this_task = this_thr->pcb;
    if(this_task == NULL) {
        lprintf("pcb is NULL");
        MAGIC_BREAK;
    }

    // Check number of alive children tasks 
    spinlock_lock(&this_task->lock_cur_child_num);
    int cur_child_num = this_task->cur_child_num;
    spinlock_unlock(&this_task->lock_cur_child_num);
    if(cur_child_num == 0) {
        lprintf("number of alive children tasks is 0");
        return ERR_NO_CHILD;
    }

    // Get current task's children exit status list and collect child's exit 
    // status.
    list_t *child_exit_status_list = &this_task->child_exit_status_list;
    if(child_exit_status_list == NULL) {
        lprintf("child_exit_status_list is NULL, but task is alive?!");
        MAGIC_BREAK;
    }

    // Get current task's wait list and put itself in it
    list_t *wait_list = &(this_task->wait_list);
    if(wait_list == NULL) {
        lprintf("wait_list is NULL, but task is alive?!");
        MAGIC_BREAK;
    }
    if(list_append(wait_list, (void *)this_thr) < 0) {
        lprintf("list_append failed");
        MAGIC_BREAK;
    }

    exit_status_t *es;
    while(list_remove_first(&zombie_list, (void **)&es) < 0) {
        // There's no child in zombie list

        // There's a race condition that other thread collects the zombie
        // task, but isn't fast enough to update cur_child_num, so this
        // thread will block again, but later the thread that collects
        // the zombie will make runnable its peers so this this thread
        // will wake up anyway.

        // Check number of alive children tasks, probably a child has been
        // reaped by someone else.
        spinlock_lock(&this_task->lock_cur_child_num);
        cur_child_num = this_task->cur_child_num;
        spinlock_unlock(&this_task->lock_cur_child_num);
        if(cur_child_num == 0) {
            lprintf("number of alive children tasks is 0");
            // Delete itself from wait list
            if(list_delete(wait_list, (void *)this_thr) < 0) {
                lprintf("thread %d is not in list", this_thr->tid);
                MAGIC_BREAK;
            }
            return ERR_NO_CHILD;
        }

        // There's no children in zombie list yet, add current thread to
        // wait list and block.
        context_switch(3, 0);
    }

    // Delete itself from wait list
    if(list_delete(wait_list, (void *)this_thr) < 0) {
        lprintf("thread %d is not in list", this_thr->tid);
        MAGIC_BREAK;
    }

    // One less child task of current task
    spinlock_lock(&this_task->lock_cur_child_num);
    this_task->cur_child_num--;
    spinlock_unlock(&this_task->lock_cur_child_num);

    // Store result
    if(status_ptr != NULL) {
        *status_ptr = es->status;
    }
    if(es->pid == 0) {
        lprintf("in wait, es->pid is 0 ?!");
        MAGIC_BREAK;
    }

    int ret = es->pid;
    free(es);
    //lprintf("task %d wait found one! pid: %d, status: %d", this_task->pid,
    //        es->pid, es->status);
    // lprintf("wait ret: %d", es->pid);

    if(cur_child_num == 0) {
        // Make runnable peers in wait list in case they haven't noticed 
        // that there's no children to reap anymore
        list_t *wait_list_copy = list_get_copy(wait_list);
        if(wait_list_copy == NULL) {
            lprintf("list_get_copy failed");
            return -1;
        }
        tcb_t *wait_thr;
        while(list_remove_first(wait_list_copy, (void **)&wait_thr) == 0) {
            lprintf("wake up thread %d", wait_thr->tid);
            context_switch(4, (uint32_t)wait_thr);
        }
        list_destroy(wait_list_copy, 0);
        free(wait_list_copy);

    }

    return ret;
    */
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
