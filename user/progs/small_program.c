#include <simics.h>
#include <syscall.h>

int main() {
    lprintf("I am in user program!");
    lprintf("my id: %d", gettid());

    return 0;
}