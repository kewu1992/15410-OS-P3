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
    halt();
    
    char* argv[] = {"small_program", "233333", NULL};
    exec("small_program", argv);
    
    return 0;
}
