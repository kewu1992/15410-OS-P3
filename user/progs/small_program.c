#include <simics.h>
#include <syscall.h>

char c0[14] = "test"; // data segment
char *c1 = "test"; // rodata segment

void func() {
    func();
}

int main() {
    lprintf("I am in user program!");
    lprintf("my id: %d", gettid());

    /*
    lprintf("trying to write to kernal memory:");
    int *p = (int*)0x00100000;
    *p = 123;
    */

    /*
    lprintf("trying to stack overflow");
    func();
    */

    lprintf("Trying to write to data segment");
    c0[1] = 'h';
    // Should reach here
    lprintf("Writing to data segment succeeded!");

    lprintf("trying to write to rodata segment, will page fault");
    c1[1] = 'h';
    // Shouldn't reach here
    lprintf("Writing to rodata segment succeeded!");

    return 0;
}
