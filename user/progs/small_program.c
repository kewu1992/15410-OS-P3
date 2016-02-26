#include <simics.h>

int main() {
    lprintf("I am in user program!");
    
    lprintf("I can not return because exit() syscall is not implemented...");

    while(1)
        continue;
}