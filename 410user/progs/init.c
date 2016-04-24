/** @file 410user/progs/init.c
 *  @author ?
 *  @brief Initial program.
 *  @public yes
 *  @for p2 p3
 *  @covers fork exec wait print
 *  @status done
 */


#include <syscall.h>
#include <stdlib.h>
#include "410_tests.h"
#include <report.h>

DEF_TEST_NAME("fork_wait_bomb:");

int main(int argc, char *argv[]) {
    int pid = 0;
    int count = 0;
    int ret_val;
    int wpid;

    report_start(START_CMPLT);
    report_fmt("parent: %d", gettid());

  while(count < 1000) {
    if((pid = fork()) == 0) {
      exit(42);
    }
    if(pid < 0) {
      break;
    }

        count++;

        report_fmt("child: %d", pid);

    wpid = wait(&ret_val);

    if(wpid != pid || ret_val != 42) {
            report_end(END_FAIL);
            exit(42);
    }
  }

    report_end(END_SUCCESS);
  
  while(1);
}

