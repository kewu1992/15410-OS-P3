#include <simics.h>
#include <syscall.h>

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

    return 0;
}