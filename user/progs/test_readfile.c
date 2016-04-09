

#include <syscall.h>
#include <simics.h>
#include <stdlib.h>
#include <string.h>


#define EXEC_MAX_ARGC   32
#define EXEC_MAX_ARG_SIZE 128

char *args[2];

int main() {

    char buf[1024];

    readfile("foo", buf, 1024, 0);
    lprintf("contents of foo:\n%s", buf);

    return 0;
}
