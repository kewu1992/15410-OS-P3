/**
 * The 15-410 kernel project.
 * @name loader.c
 *
 * Functions for the loading
 * of user programs from binary 
 * files should be written in
 * this file. The function 
 * elf_load_helper() is provided
 * for your use.
 */
/*@{*/

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

#define MAX_ADDR 0xFFFFFFFF

#define SIZE_USER_STACK  20

#define ALIGNMENT 4

/* The number of user executables in the table of contents. */
extern const int exec2obj_userapp_count;

/* The table of contents. */
extern const exec2obj_userapp_TOC_entry exec2obj_userapp_TOC[MAX_NUM_APP_ENTRIES];

extern void asm_new_process_iret(void *esp);

extern void asm_idle_process_iret(void *esp);

static uint32_t init_eflags;


tcb_t* idle_thr;

/* --- Local function prototypes --- */ 
static void* push_to_stack(void *esp, uint32_t value);

/** @brief Return default eflags
  * 
  * @return void
  */
uint32_t get_init_eflags() {
    return init_eflags;
}

int is_file_exist(const char *filename) {
    int i;
    for (i = 0; i < exec2obj_userapp_count; i++)
        if (strcmp(filename, exec2obj_userapp_TOC[i].execname) == 0) {
            return 1;
        }
    return 0;
}

/**
 * Copies data from a file into a buffer.
 *
 * @param filename   the name of the file to copy data from
 * @param offset     the location in the file to begin copying from
 * @param size       the number of bytes to be copied
 * @param buf        the buffer to copy the data into
 *
 * @return returns the number of bytes copied on succes; -1 on failure
 */
int getbytes( const char *filename, int offset, int size, char *buf )
{
    //lprintf("filename:%s; offset:%d; size:%d; buf:%p", filename, offset, size, buf);
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

void loadFirstTask(const char *filename) {  
    init_eflags = get_eflags();

    void *my_program, *usr_esp;
    int rv;

    // create new process
    tcb_t *thread = tcb_create_process(NORMAL, get_cr3());

    lprintf("HERERERERE");
    MAGIC_BREAK;

    const char *argv[1] = {filename};
    if ((rv = loadTask(filename, 1, argv, &usr_esp, &my_program)) < 0)
        panic("Load first task failed");

    // set init pcb (who-to-reap-orphan-process) as the first pcb 
    // (will reset if the first pcb is idle)
    set_init_pcb(thread->pcb);

    // set idle thread as NULL (will reset if the first thread is idle)
    idle_thr = NULL;

    load_kernel_stack(thread->k_stack_esp, usr_esp, my_program, strcmp(filename, "idle") == 0);

    // should never reach here
}

int loadTask(const char *filename, int argc, const char **argv, void** usr_esp, void** my_program) {

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
    new_region(simple_elf.e_txtstart, simple_elf.e_txtlen, 0, 0, 0);
    new_region(simple_elf.e_datstart, simple_elf.e_datlen, 1, 0, 0);
    new_region(simple_elf.e_rodatstart, simple_elf.e_rodatlen, 0, 0, 0);
    new_region(simple_elf.e_bssstart, simple_elf.e_bsslen, 1, 0, 1);

    //lprintf("txtstart:%p, txtlen:%d", (void*)simple_elf.e_txtstart, (int)simple_elf.e_txtlen);
    //lprintf("datstart:%p, datlen:%d", (void*)simple_elf.e_datstart, (int)simple_elf.e_datlen);
    //lprintf("rodatstart:%p, rodatslen:%d", (void*)simple_elf.e_rodatstart, (int)simple_elf.e_rodatlen);
    //lprintf("bssstart:%p, bsslen:%d", (void*)simple_elf.e_bssstart, (int)simple_elf.e_bsslen);

    // copy bytes from elf
    getbytes(filename, (int)simple_elf.e_txtoff, 
            (int)simple_elf.e_txtlen, 
            (char*)simple_elf.e_txtstart);
    getbytes(filename, (int)simple_elf.e_datoff, 
            (int)simple_elf.e_datlen, 
            (char*)simple_elf.e_datstart);
    getbytes(filename, (int)simple_elf.e_rodatoff, 
            (int)simple_elf.e_rodatlen, 
            (char*)simple_elf.e_rodatstart);
    // ZFOD is used, NO need to memset() the region after new_resion() returns.
    //memset((void*)simple_elf.e_bssstart, 0, (size_t)simple_elf.e_bsslen);

    // calculate total bytes needed to prepare user program
    int i, len = 0;
    // user stack arguments
    len += SIZE_USER_STACK;
    // space for argv
    len += argc * sizeof(char*);
    // sapce for argv[]
    for (i = 0; i < argc; i++)
        len += strlen(argv[i]) + 1;
    // deal with alignment
    len += ALIGNMENT - len % ALIGNMENT;

    // calculate pages needed initially
    int page_num = len / PAGE_SIZE + 1;
    // allocate page
    new_region(MAX_ADDR - page_num * PAGE_SIZE, page_num * PAGE_SIZE, 1, 0, 0);

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
    user_esp = push_to_stack(user_esp, MAX_ADDR - page_num * PAGE_SIZE);
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

void load_kernel_stack(void* k_stack_esp, void* u_stack_esp, void* program, int is_idle) {
    //set esp0
    set_esp0((uint32_t)(k_stack_esp));

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

    lprintf("loggg");

    // set esp and call iret
    if (is_idle)
        asm_idle_process_iret(k_stack_esp);
    else
        asm_new_process_iret(k_stack_esp);

    // should never reach here
}

void* push_to_stack(void *esp, uint32_t value) {
    void* new_esp = (void*)((uint32_t)esp - 4);
    memcpy(new_esp, &value, 4);
    return new_esp;
}

 void idle_process_init() {
    lprintf("Initializing idle process");

    idle_thr = tcb_get_entry((void*)asm_get_esp());
    
    // fork
    context_switch(1, 0);
    if (tcb_get_entry((void*)asm_get_esp())->result == 0) {        
        // child process, exec(init)
        char my_execname[] = "init";
        char *argv[] = {my_execname, 0};

        uint32_t old_pd = get_cr3();

        // create new page table
        uint32_t new_pd = create_pd();
        if(new_pd == ERROR_MALLOC_LIB) {
            panic("create_pd failed");
        }
        tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
        this_thr->pcb->page_table_base = new_pd;
        set_cr3(new_pd);
        
        // load task
        void *my_program, *usr_esp;
        int rv;
        if ((rv = loadTask(my_execname, 1, (const char**)argv, &usr_esp, &my_program)) < 0) {
            panic("load init failed");
        }

        free_entire_space(old_pd);

        // modify tcb
        this_thr->k_stack_esp = tcb_get_high_addr((void*)asm_get_esp());

        // reset init_pcb (who-to-reap-orphan-process) as the second pcb 
        set_init_pcb(this_thr->pcb);

        lprintf("ready to load init process");
        // load kernel stack, jump to new program
        load_kernel_stack(this_thr->k_stack_esp, usr_esp, my_program, 0);
    } else {
        // parent process(idle)
        return;
    }
 }

/*@}*/
