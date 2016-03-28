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
