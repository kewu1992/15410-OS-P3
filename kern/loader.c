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

#include <vm.h> // For vm

/* The number of user executables in the table of contents. */
extern const int exec2obj_userapp_count;

/* The table of contents. */
extern const exec2obj_userapp_TOC_entry exec2obj_userapp_TOC[MAX_NUM_APP_ENTRIES];

extern void asm_new_process_iret(void *esp);
extern uint32_t asm_get_esp();

static int id_count = 0;

static tcb_t **tcb_table;

/* --- Local function prototypes --- */ 
static void* push_to_stack(void *esp, uint32_t value);


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


int loadFirstTask(const char *filename) {

    // Create pd for a new task
    uint32_t new_pd = create_pd();
    set_cr3(new_pd);

    /***********
     *
     *  Initialize tcb_table, should move to proper location later
     *
     ***********/
    tcb_table = calloc(USER_MEM_START/K_STACK_SIZE, sizeof(tcb_t*));



    if (elf_check_header(filename) == ELF_NOTELF)
        return -1;

    simple_elf_t simple_elf;
    if (elf_load_helper(&simple_elf, filename) == ELF_NOTELF)
        return -1;


    // Enable mappings
    // Set rw permission as well, 0 as ro, 1 as rw, so that User
    // level program can't write to read-only regions
    // Supervisor can still write to uesr level read-only region 
    // if WP (write protection, bit 16 of %cr0) isn't set
    new_region(simple_elf.e_txtstart, simple_elf.e_txtlen, 0);
    new_region(simple_elf.e_datstart, simple_elf.e_datlen, 1);
    new_region(simple_elf.e_rodatstart, simple_elf.e_rodatlen, 0);
    new_region(simple_elf.e_bssstart, simple_elf.e_bsslen, 1);
    new_region(0x8000000 - PAGE_SIZE, 2 * PAGE_SIZE, 1);


    // the following code should run in VM
    getbytes(filename, (int)simple_elf.e_txtoff, 
            (int)simple_elf.e_txtlen, 
            (char*)simple_elf.e_txtstart);
    getbytes(filename, (int)simple_elf.e_datoff, 
            (int)simple_elf.e_datlen, 
            (char*)simple_elf.e_datstart);
    getbytes(filename, (int)simple_elf.e_rodatoff, 
            (int)simple_elf.e_rodatlen, 
            (char*)simple_elf.e_rodatstart);
    memset((void*)simple_elf.e_bssstart, 0, (size_t)simple_elf.e_bsslen);

    // Update TLB for new task by resetting %cr3 value 
    set_cr3(new_pd);;

    void (*my_program) (void) = (void*)simple_elf.e_entry;

    /*
     * NEED TO TEST TO MAKE SURE malloc() AND smemalign() WORKS FINE
     * WHEN VM IS OPEN !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */ 
    pcb_t *process = malloc(sizeof(pcb_t));
    process->pid = id_count++;
    process->page_table_base = 0;
    process->state = RUNNING;

    tcb_t *thread = malloc(sizeof(tcb_t));
    thread->tid = process->pid;
    thread->pcb = process;
    thread->k_stack_esp = smemalign(K_STACK_SIZE, K_STACK_SIZE) + K_STACK_SIZE;

    // set tcb_table
    tcb_table[GET_K_STACK_INDEX(thread->k_stack_esp-1)] = thread;

    //set esp0
    set_esp0((uint32_t)(thread->k_stack_esp));

    // push SS
    thread->k_stack_esp = push_to_stack(thread->k_stack_esp, SEGSEL_USER_DS);
    // push esp
    thread->k_stack_esp = push_to_stack(thread->k_stack_esp, 0x8000000);
    // push EFLAGS
    thread->k_stack_esp = push_to_stack(thread->k_stack_esp, get_eflags());
    // push CS
    thread->k_stack_esp = push_to_stack(thread->k_stack_esp, SEGSEL_USER_CS);
    // push EIP
    thread->k_stack_esp = push_to_stack(thread->k_stack_esp, (uint32_t)my_program);
    // push DS
    thread->k_stack_esp = push_to_stack(thread->k_stack_esp, SEGSEL_USER_DS);

    // set esp and call iret
    asm_new_process_iret(thread->k_stack_esp);

    return 0;
}

void* push_to_stack(void *esp, uint32_t value) {
    void* new_esp = (void*)((uint32_t)esp - 4);
    memcpy(new_esp, &value, 4);
    return new_esp;
}

int gettid_syscall_handler() {
    return tcb_table[GET_K_STACK_INDEX(asm_get_esp())]->tid;
}

/*@}*/
