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

    char *program = (char *)0;
    //fork();
    args[0] = program;
    //fork();


    int ret;
    ret = exec(program, args);
    if(ret < 0) {
        lprintf("test program == NULL:%d",ret);
    }

    char buf[EXEC_MAX_ARG_SIZE*2];
    memset(buf, 6, EXEC_MAX_ARG_SIZE + 2);
    program = buf;
    args[0] = program;
    ret = exec(program, args);
    if(ret < 0) {
        lprintf("test program not NULL terminated:%d",ret);
    }

    program = "\0";
    args[0] = program;
    ret = exec(program, args);
    if(ret < 0) {
        lprintf("test program empty string:%d",ret);
    }

    program = "peon";
    ret = exec(program, args);
    if(ret < 0) {
        lprintf("test args[0] different than pragram:%d",ret);
    }

    program = "peon";
    ret = exec(program, (char**)0xdeadbeef);
    if(ret < 0) {
        lprintf("test args invalid memory:%d",ret);
    }

    program = "peon";
    char *args2[2 * EXEC_MAX_ARGC];
    memset(args2, 6, EXEC_MAX_ARGC);
    args2[0] = program;
    ret = exec(program, args2);
    if(ret < 0) {
        lprintf("test args[] invalid memory:%d",ret);
    }


    program = "peon";
    args2[0] = program;
    int i = 0;
    char *tmp = "abc";
    for (i = 1; i < 2 * EXEC_MAX_ARGC; i++)
        args2[i] = tmp;
    ret = exec(program, args2);
    if(ret < 0) {
        lprintf("test args too long args:%d",ret);
    }

    program = "peon";
    args2[0] = program;
    for (i = 1; i < 2 * EXEC_MAX_ARGC; i++)
        args2[i] = buf;
    ret = exec(program, args2);
    if(ret < 0) {
        lprintf("test args not NULL terminated:%d",ret);
    }

    MAGIC_BREAK;

    while(1)
        lprintf("ULTIMATE BADNESS");
}
