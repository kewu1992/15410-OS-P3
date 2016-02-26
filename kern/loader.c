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

/* The number of user executables in the table of contents. */
extern const int exec2obj_userapp_count;

/* The table of contents. */
extern const exec2obj_userapp_TOC_entry exec2obj_userapp_TOC[MAX_NUM_APP_ENTRIES];

extern void asm_new_process_iret(void *esp);
extern uint32_t asm_get_esp();

static int id_count = 0;

/* --- Local function prototypes --- */ 
void* push_to_stack(void *esp, uint32_t value);


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


int loadExeFile(const char *filename) {
    if (elf_check_header(filename) == ELF_NOTELF)
        return -1;

    simple_elf_t simple_elf;
    if (elf_load_helper(&simple_elf, filename) == ELF_NOTELF)
        return -1;

    // the following code should finish in VM
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

    void (*my_program) (void) = (void*)simple_elf.e_entry;
    //my_program();



    pcb_t *process = malloc(sizeof(pcb_t));
    process->pid = id_count++;
    process->page_table_base = 0;
    process->state = RUNNING;

    tcb_t *thread = malloc(sizeof(tcb_t));
    thread->tid = process->pid;
    thread->pcb = process;
    thread->k_stack_esp = malloc(K_STACK_SIZE) + K_STACK_SIZE;

    lprintf("The last byte of new stack: %p", thread->k_stack_esp);

    // push pid
    thread->k_stack_esp = push_to_stack(thread->k_stack_esp, thread->tid);
    //set esp0
    set_esp0((uint32_t)thread->k_stack_esp);

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

/*@}*/
