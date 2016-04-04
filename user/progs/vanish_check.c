/** @file 410user/progs/coolness.c
 *  @author de0u
 *  @brief Tests fork, exec, and context-switching.
 *  @public yes
 *  @for p3
 *  @covers fork gettid exec
 *  @status done
 */

#include <syscall.h>
#include <simics.h>
#include <stdlib.h>
#include <string.h>


#define EXEC_MAX_ARGC   32
#define EXEC_MAX_ARG_SIZE 128

char *args[2];

int main() {

    int pid = fork();
    //fork();
    
    if(pid == 0) {
        // child
        lprintf("child starts to run");
        set_status(18);
        vanish();
        lprintf("Should never print this");
        while(1) {
            ;
        }
    }


    
    int status;
    int ret = wait(&status);
    lprintf("wait ret: %d", ret);
    

    //int i = 0;
    while(1) {
        ;
        //for(i = 0; i < 10000000; i++) ;
        //lprintf("main task");
    }


/*
    args[0] = program;
    //fork();


    int ret;
    ret = exec(program, args);
    if(ret < 0) {
        lprintf("test program == NULL");
    }

    char buf[EXEC_MAX_ARG_SIZE*2];
    memset(buf, 6, EXEC_MAX_ARG_SIZE + 2);
    program = buf;
    args[0] = program;
    ret = exec(program, args);
    if(ret < 0) {
        lprintf("test program not NULL terminated");
    }

    program = "\0";
    args[0] = program;
    ret = exec(program, args);
    if(ret < 0) {
        lprintf("test program empty string");
    }

    program = "peon";
    ret = exec(program, args);
    if(ret < 0) {
        lprintf("test args[0] different than pragram");
    }


    program = "peon";
    char *args2[2 * EXEC_MAX_ARGC];
    memset(args2, 6, EXEC_MAX_ARGC);
    args2[0] = program;
    ret = exec(program, args2);
    if(ret < 0) {
        lprintf("test args not NUll terminated");
    }

    MAGIC_BREAK;

    while(1)
        lprintf("ULTIMATE BADNESS");

    */
}
