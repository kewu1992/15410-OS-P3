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

    char *switched_text = "switched";

    long i = 1;
    while (i++) {
        if (i % 15000 == 0){
            lprintf("%s:%ld", switched_text, i/15000);
        }
    }
    

    return 0;
}
