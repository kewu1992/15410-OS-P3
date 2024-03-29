/** @file 410user/progs/fork_exit_bomb.c
 *  @author ?
 *  @brief Tests many invocations of fork/exit
 *  @public yes
 *  @for p3
 *  @covers fork set_status vanish
 *  @status done
 */

#include <syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include "410_tests.h"
#include <report.h>

DEF_TEST_NAME("fork_exit_bomb:");

int main(int argc, char *argv[]) {
    int pid = 0;
    int count = 0;

    report_start(START_CMPLT);

    lprintf("parent pid: %d", gettid());

    while(count < 1000) {
        if((pid = fork()) == 0) {
            exit(42);
        }
        if(pid < 0) {
            lprintf("pid < 0?! pid: %x", (unsigned)pid);
            MAGIC_BREAK;
            break;
        }
        count++;
        if(pid == 0) {
            lprintf("pid is 0, why?");
            MAGIC_BREAK;
        }
        report_fmt("child: %d", pid);
    }

    report_end(END_SUCCESS);
    exit(42);
}

