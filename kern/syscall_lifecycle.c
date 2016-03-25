#include <context_switcher.h>
#include <control_block.h>
#include <asm_helper.h>
#include <string.h>
#include <stdlib.h>
#include <asm_helper.h>
#include <cr.h>
#include <loader.h>
#include <vm.h>

#define EXEC_MAX_ARGC   32
#define EXEC_MAX_ARG_SIZE   128

int fork_syscall_handler() {
    context_switch(-2);
    return tcb_get_entry((void*)asm_get_esp())->fork_result;
}

int exec_syscall_handler(char* execname, char **argvec) {

    // PAGE_FAULT ?????????
    int i, argc = 0;
    while (argc < EXEC_MAX_ARGC && argvec[argc]){
        for(i = 0; i < EXEC_MAX_ARG_SIZE; i++)
            if(argvec[argc][i] == '\0')
                break;
        // check argument length 
        if (i == EXEC_MAX_ARG_SIZE)
            return -1;
        argc++;
    }
    // check arguments number
    if (argc == EXEC_MAX_ARGC)
        return -1;

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