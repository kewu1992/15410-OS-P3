#include <simics.h>
#include <syscall.h>
#include <stdio.h>
#include <stdlib.h>

char c0[14] = "test"; // data segment
char *c1 = "test"; // rodata segment

void func() {
    func();
}

int main() {
    int *a = malloc(sizeof(int));
    printf("here:%d", *a);

    return 0;
}
