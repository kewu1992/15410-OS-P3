#include <simics.h>
#include <syscall.h>
#include <stdio.h>

char c0[14] = "test"; // data segment
char *c1 = "test"; // rodata segment

void func() {
    func();
}

int main() {
    int ret = new_pages((void*)0xFF0000, 8192);
    lprintf("%d", ret);

    return 0;
}
