#include <simics.h>
#include <syscall.h>
#include <stdio.h>



int main(int argc, char *argv[]) {
    
    if (fork() == 0) {
        if (fork() == 0) {
            lprintf("I am %d, ready to deschedule()", gettid());
            int i = 0;
            deschedule(&i);
            lprintf("here");
        } else {
            lprintf("I am %d", gettid());
            int rv = make_runnable(9);
            lprintf("make_runnable rv:%d", rv);
        }
    } else {
        if (fork() == 0) {
            lprintf("I am %d", gettid());
        } else {
            lprintf("I am %d", gettid());
        }
    }


    return 0;
}
