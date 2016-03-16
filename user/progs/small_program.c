#include <simics.h>
#include <syscall.h>
#include <stdio.h>

char c0[14] = "test"; // data segment
char *c1 = "test"; // rodata segment

void func() {
    func();
}

int main() {
    
    lprintf("I am small program:%d", gettid());

    char *small_text;
    
    if (fork() == 0) {
        // child
        lprintf("I am child program:%d", gettid());
        small_text = "child";
    } else {
        // parent
        lprintf("I am parent program:%d", gettid());
        small_text = "parent";
    }

    long i = 1;
    while (i++) {
        if (i % 10000 == 0) {
            lprintf("%s:%ld", small_text, i/10000);
        }
    }
    

    return 0;
}
