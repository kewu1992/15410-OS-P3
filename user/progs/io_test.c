#include <simics.h>
#include <syscall.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    lprintf("I am io test program:%d", gettid());

    char buf[1024];

    if (fork() == 0) {
        int i = 0;
        while(1){
            i++;
            if (i % 10000000 == 0)
                lprintf("child");
        }
    } else {
        while(1) {
            lprintf("ready to readline");
            int rv = readline(1024, buf);  
            buf[rv] = '\0';
            lprintf("%s", buf);
            //sleep(1000);
        }
    }
    
    return 0;
}
