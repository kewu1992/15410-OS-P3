/** @file 410user/progs/init.c
 *  @author ?
 *  @brief Initial program.
 *  @public yes
 *  @for p2 p3
 *  @covers fork exec wait print
 *  @status done
 */

#include <syscall.h>
#include <stdio.h>
#include <simics.h>

int main()
{
  /*
  int pid, exitstatus;
  char shell[] = "shell";
  char * args[] = {shell, 0};

  
  while(1) {
    pid = fork();
    if (!pid)
      exec(shell, args);
    
    while (pid != wait(&exitstatus));
  
    printf("Shell exited with status %d; starting it back up...", exitstatus);
  }
  */

  //if(fork() == 0) 
  //  lprintf("I am child");
  //else
    lprintf("I am parent");

  while(1);
}
