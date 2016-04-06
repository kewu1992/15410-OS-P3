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

    int i;
    if(pid == 0) {

        exit(42);
    }

    for(i = 0; i < 1000000; i++) {
        ;
    }
    exit(43);  

}
