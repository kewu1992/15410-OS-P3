#include <simics.h>
#include <syscall.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    lprintf("I am sleep test program:%d", gettid());

    if (fork() == 0) {
        if (fork() == 0) {
            lprintf("I am child1, ready to sleep at %d", (int)get_ticks());
            sleep(10);
            lprintf("I am child1, wake up at %d", (int)get_ticks());

            int i = 0;
            while (1) {
                i++;
                if (i % 30000 == 0)
                    lprintf("current ticks:%d from child1", (int)get_ticks());
            }
        } else {
            if (fork() == 0) {
                lprintf("I am child2, ready to sleep at %d", (int)get_ticks());
                sleep(20);
                lprintf("I am child2, wake up at %d", (int)get_ticks());

                int i = 0;
                while (1) {
                    i++;
                    if (i % 30000 == 0)
                        lprintf("current ticks:%d from child2", (int)get_ticks());
                }
            } else {
                lprintf("I am child3, ready to sleep at %d", (int)get_ticks());
                sleep(30);
                lprintf("I am child3, wake up at %d", (int)get_ticks());

                int i = 0;
                while (1) {
                    i++;
                    if (i % 30000 == 0)
                        lprintf("current ticks:%d from child3", (int)get_ticks());
                }
            }
        }
        
    } else {
        int i = 0;
        while (1) {
            i++;
            if (i % 30000 == 0)
                lprintf("current ticks:%d from parent", (int)get_ticks());
        }
    }
    return 0;
}
