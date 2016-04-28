/** @file loader.c
 *  @brief This file contains functions for loading a new program.
 *
 *  Load user programs from binary files into memory and prepare user stack and
 *  kernel stack for the new process. 
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug No known bugs.
 */

/* --- Includes --- */
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <exec2obj.h>
#include <loader.h>
#include <elf_410.h>

#include <seg.h>
#include <control_block.h>
#include <simics.h>
#include <eflags.h>
#include <cr.h>
#include <common_kern.h>
#include <string.h>

#include <vm.h> // For vm

#include <asm_helper.h>

#include <syscall_inter.h>
#include <context_switcher.h>
#include <syscall_errors.h>

#include <smp.h>
#include <timer_driver.h>

/** @brief The maximum address space supported by the kernel */
#define MAX_ADDR 0xFFFFFFFF

/** @brief The size of arguments for the entry point _main() of user program.
 *         There are four arguments in _main() so it needs 20 bytes */
#define SIZE_USER_STACK_ARG  20

/** @brief Alignment that %esp must align */
#define ALIGNMENT 4

/** @brief Jump to the prepared kernel stack and iret to run user program
 *
 *  This function will set %esp to a new value (a new stack), set data segment 
 *  selectors to SEGSEL_USER_DS and execute iret. The data for iret should be 
 *  preparted by load_kernel_stack(). 
 *
 *  @param esp The new value that %esp will set to.
 *
 *  @return Should never return
 */
extern void asm_new_process_iret(void *esp);

/** @brief Jump to the prepared kernel stack and iret to run idle task
 *
 *  This function is very similar with asm_new_process_iret() except that it
 *  will call idle_process_init() before iret to do some initialization for 
 *  idle task.
 *
 *  @param esp The new value that %esp will set to.
 *
 *  @return Should never return
 */
extern void asm_idle_process_iret(void *esp);


/**********************
 *  P4 new functions  *
 **********************/

/** @brief Jump to the prepared kernel stack and call smp_manager_boot()
 *
 *  This function will be invoked by ASP in loadMailboxTask(). It is important
 *  that the mailbox task jump to the created kernel stack as soon as possible 
 *  because it needs its stack to find its tcb during context switch. 
 *
 *  @param esp The new value that %esp will set to.
 *
 *  @return Should never return
 */
extern void asm_mailbox_process_load(void *esp);


/** @brief Jump to the prepared kernel stack and call load_idle_process()
 *
 *  This function will be invoked by APs in loadFirstTask(). It is important
 *  that the idle task jump to the created kernel stack as soon as possible 
 *  because it needs its stack to find its tcb during context switch. 
 *
 *  @param esp The new value that %esp will set to.
 *
 *  @return Should never return
 */
extern void asm_idle_process_load(void* esp, const char *filename);

/** @brief The initial value of EFLAGS that will be set to every new process */
static uint32_t init_eflags;

/** @brief tcb for idle task, it is a global variable that will be used
 *         in context_switcher.c as well */
tcb_t* idle_thr[MAX_CPUS];

/** @brief Return the initial value of EFLAGS */
uint32_t get_init_eflags() {
    return init_eflags;
}


/* --- Local function prototypes --- */ 

static void* push_to_stack(void *esp, uint32_t value);


/** @brief Check if a file exists in 'file system' 
 *  @param filename The filename of the file to check
 *  @return Return 1 if the file exists, otherwise return 0
 */
static int is_file_exist(const char *filename) {
    int i;
    for (i = 0; i < exec2obj_userapp_count; i++)
        if (strcmp(filename, exec2obj_userapp_TOC[i].execname) == 0) {
            return 1;
        }
    return 0;
}


/**
 * @brief Copies data from a file into a buffer.
 *
 * @param filename   the name of the file to copy data from
 * @param offset     the location in the file to begin copying from
 * @param size       the number of bytes to be copied
 * @param buf        the buffer to copy the data into
 *
 * @return Returns the number of bytes copied on success; -1 on failure
 */
int getbytes( const char *filename, int offset, int size, char *buf )
{
    int i;
    for (i = 0; i < exec2obj_userapp_count; i++)
        if (strcmp(filename, exec2obj_userapp_TOC[i].execname) == 0) {
            if (offset + size > exec2obj_userapp_TOC[i].execlen)
                return -1;
            memcpy(buf, exec2obj_userapp_TOC[i].execbytes+offset, size);
            return size;
        }
    return -1;
}

/** @brief Load the first task
 *
 *  This function will be invoked by kernel_main(). Normally the first task
 *  to load is idle. 
 *
 *  @param filename The executable file of the first task to be loaded  
 *  @return Should never return
 */
void loadFirstTask(const char *filename) {  
    // create the idle process
    tcb_t *thread = tcb_create_idle_process(NORMAL, get_cr3());
    if (thread == NULL)
        panic("Load first task failed for cpu%d", smp_get_cpu());

    asm_idle_process_load(thread->k_stack_esp, filename);
}

/** @brief Load the idle task for APs
 *
 *  This function will be invoked by asm_idle_process_load() to load idle task.
 *
 *  @param filename The executable file of idle task to be loaded  
 *  @return Should never return
 */
void load_idle_process(const char *filename) {
    tcb_t* thread = tcb_get_entry((void*)asm_get_esp());

    // Init lapic timer
    init_lapic_timer_driver();

    lprintf("Lapic timer inited for cpu%d", smp_get_cpu());


    void *my_program, *usr_esp;
    int rv;

    const char *argv[1] = {filename};
    if ((rv = loadTask(filename, 1, argv, &usr_esp, &my_program)) < 0)
        panic("Load first task failed for cpu%d", smp_get_cpu());

    // set idle thread
    idle_thr[smp_get_cpu()] = thread;

    load_kernel_stack(thread->k_stack_esp, usr_esp, my_program, 
                                                strcmp(filename, "idle") == 0);

    // should never reach here
}


/** @brief Load a task from binary file into memory
 *
 *  This function will load user programs from binary files into memory and 
 *  prepare for user stack (arguments of _main()). 
 *
 *  @param filename The executable file of the task to be loaded  
 *  @param argc The number of arguments in argv[] (arguments for main())
 *  @param argv The arguments vector (arguments for main())
 *  @param usr_esp This is also a return value, it will be used to store the 
 *         %esp value when iret to user program at the first time
 *  @param my_program This is also a return value, it will be used to store the
 *         %eip value when iret to user program at the first time (entry point
 *         of user program)
 *
 *  @return On success return zero, on error a negative number is retuned
 */
int loadTask(const char *filename, int argc, const char **argv, 
                                            void** usr_esp, void** my_program) {

    if (!is_file_exist(filename))
        return ENOENT;

    if (elf_check_header(filename) == ELF_NOTELF)
        return ENOEXEC;

    simple_elf_t simple_elf;
    if (elf_load_helper(&simple_elf, filename) == ELF_NOTELF)
        return ENOEXEC;

    // allocate pages for the new task
    // Set rw permission as well, 0 as ro, 1 as rw, so that User
    // level program can't write to read-only regions
    // Supervisor can still write to uesr level read-only region 
    // if WP (write protection, bit 16 of %cr0) isn't set

    // The last param for new_region, is_ZFOD, if it's 0, then actual frames 
    // are allocated, otherwise if it's 1, then a system-wide all zero page 
    // is used, NO need to memset() the region after new_resion() returns.
    if (new_region(simple_elf.e_txtstart, simple_elf.e_txtlen, 0, 0, 0) < 0)
        return ENOMEM;
    if (new_region(simple_elf.e_datstart, simple_elf.e_datlen, 1, 0, 0) < 0)
        return ENOMEM;
    if (new_region(simple_elf.e_rodatstart, simple_elf.e_rodatlen, 0, 0, 0) < 0)
        return ENOMEM;
    if (new_region(simple_elf.e_bssstart, simple_elf.e_bsslen, 1, 0, 1) < 0)
        return ENOMEM;

    // copy bytes from elf to memory
    getbytes(filename, (int)simple_elf.e_txtoff, 
            (int)simple_elf.e_txtlen, 
            (char*)simple_elf.e_txtstart);
    getbytes(filename, (int)simple_elf.e_datoff, 
            (int)simple_elf.e_datlen, 
            (char*)simple_elf.e_datstart);
    getbytes(filename, (int)simple_elf.e_rodatoff, 
            (int)simple_elf.e_rodatlen, 
            (char*)simple_elf.e_rodatstart);
    // ZFOD is used, NO need to memset() the bss region


    // calculate total bytes needed to prepare user program stack
    int i, len = 0;
    // user stack arguments
    len += SIZE_USER_STACK_ARG;
    // space for argv
    len += argc * sizeof(char*);
    // sapce for argv[]
    for (i = 0; i < argc; i++)
        len += strlen(argv[i]) + 1;
    // deal with alignment
    len += ALIGNMENT - len % ALIGNMENT;

    // calculate pages needed for user program stack initially
    int page_num = len / PAGE_SIZE + 1;
    // allocate page
    if (new_region(MAX_ADDR - page_num * PAGE_SIZE + 1, page_num * PAGE_SIZE, 
                                                                   1, 0, 0) < 0)
        return ENOMEM;

    // put argv[]
    int arg_len;
    uint32_t addr = MAX_ADDR;
    for (i = 0; i < argc; i++) {
        arg_len = strlen(argv[i]) + 1;
        memcpy((void*)(addr - arg_len), argv[i], arg_len);
        addr -= arg_len;
    }
    addr -= addr % ALIGNMENT;

    // put argv
    addr -= argc * sizeof(char*);
    uint32_t arg_addr = MAX_ADDR;
    for (i = 0; i < argc; i++) {
        arg_len = strlen(argv[i]) + 1;
        arg_addr -= arg_len;
        memcpy((void*)addr, &arg_addr, sizeof(char*));
        addr += sizeof(char*);
    }
    addr -= argc * sizeof(char*);

    // set user stack for _main()
    void* user_esp = (void*)addr;
    // push stack_low
    user_esp = push_to_stack(user_esp, MAX_ADDR - page_num * PAGE_SIZE + 1);
    // push stack_high
    user_esp = push_to_stack(user_esp, MAX_ADDR);
    // push argv
    user_esp = push_to_stack(user_esp, addr);
    // push argc
    user_esp = push_to_stack(user_esp, argc);

    *usr_esp = user_esp - 4;
    *my_program = (void*)simple_elf.e_entry;

    return 0;
}

/** @brief Prepare the kernel stack for a newly loaded process
 *
 *  This function will prepare the kernel stack for a new process and jump to 
 *  that stack and execute iret.
 *
 *  @param k_stack_esp The initial value of esp for kernel stack  
 *  @param u_stack_esp The initial value of esp for user stack  
 *  @param program The entry point of user program
 *  @param is_idle Indicate if it is the idle process that will be loaded
 *
 *  @return Should never return
 */
void load_kernel_stack(void* k_stack_esp, void* u_stack_esp, void* program, 
                                                                int is_idle) {
    //set esp0
    set_esp0((uint32_t)(k_stack_esp));

    // prepare the kernel stack

    // push SS
    k_stack_esp = push_to_stack(k_stack_esp, SEGSEL_USER_DS);
    // push esp
    k_stack_esp = push_to_stack(k_stack_esp, (uint32_t)u_stack_esp);
    // push EFLAGS
    k_stack_esp = push_to_stack(k_stack_esp, init_eflags);
    // push CS
    k_stack_esp = push_to_stack(k_stack_esp, SEGSEL_USER_CS);
    // push EIP
    k_stack_esp = push_to_stack(k_stack_esp, (uint32_t)program);
    // push DS
    k_stack_esp = push_to_stack(k_stack_esp, SEGSEL_USER_DS);

    // set esp and call iret
    if (is_idle)
        asm_idle_process_iret(k_stack_esp);
    else
        asm_new_process_iret(k_stack_esp);

    // should never reach here
}

/** @brief Push a value to the stack specified by esp 
 *  @param value The value that will be pushed to stack
 *  @param esp The esp of the stack to push the value
 *  @return The new value of esp after push
 */
void* push_to_stack(void *esp, uint32_t value) {
    void* new_esp = (void*)((uint32_t)esp - 4);
    memcpy(new_esp, &value, 4);
    return new_esp;
}

/** @brief Initialize idle task
 *
 *  Idle task is responsible to fork() a new process and exec() init task. This
 *  is what this function does. 
 *
 *  @return Void
 */
void idle_process_init() {
    lprintf("Initializing idle process for cpu%d", smp_get_cpu());

    // let cpu1 to fork init
    if (smp_get_cpu() != 1)
        return;
    
    // fork
    context_switch(OP_FORK, 0);
    if (tcb_get_entry((void*)asm_get_esp())->result == 0) {  

        // child process, exec(init)
        char my_execname[] = "init";
        char *argv[] = {my_execname, 0};

        uint32_t old_pd = get_cr3();

        // create new page table
        uint32_t new_pd = create_pd();
        if(new_pd == ERROR_MALLOC_LIB) {
            panic("create_pd() in idle_process_init() failed");
        }
        tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
        this_thr->pcb->page_table_base = new_pd;
        set_cr3(new_pd);
        
        // load task
        void *my_program, *usr_esp;
        int rv;
        if ((rv = loadTask(my_execname, 1, (const char**)argv, &usr_esp, 
                                                            &my_program)) < 0) {
            panic("load init task failed");
        }

        int need_unreserve_frames = 1;
        free_entire_space(old_pd, need_unreserve_frames);

        // modify tcb
        this_thr->k_stack_esp = tcb_get_high_addr((void*)asm_get_esp());

        // set init_pcb (who-to-reap-orphan-process) 
        if (set_init_pcb(this_thr->pcb) < 0) {
            panic("set_init_pcb() failed");
        }

        lprintf("Ready to load init process");
        // load kernel stack, jump to new program
        load_kernel_stack(this_thr->k_stack_esp, usr_esp, my_program, 0);
    } else {
        // parent process(idle)
        return;
    }
}


/** @brief Load the mailbox task for BSP
 *
 *  This function will be invoked by kernel_main() to load mailbox task.
 *
 *  @return Should never return
 */
void loadMailboxTask() {  
    init_eflags = get_eflags();

    // create new process
    tcb_t *thread = tcb_create_idle_process(NORMAL, get_cr3());
    if (thread == NULL)
        panic("Load mailbox task failed");

    // set idle thread as NULL for BSP
    idle_thr[0] = NULL;

    asm_mailbox_process_load(thread->k_stack_esp);
    // should never reach here
}
