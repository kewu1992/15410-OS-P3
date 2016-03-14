#include <simics.h>
#include <syscall.h>
#include <stdio.h>

char c0[14] = "test"; // data segment
char *c1 = "test"; // rodata segment

void func() {
    func();
}

int main() {
    lprintf("I am switched program!");

    long i = 1;
    while (i++) {
        if (i % 10000 == 0)
            lprintf("switched_program:%ld", i/10000);
    }
    

    return 0;
}
