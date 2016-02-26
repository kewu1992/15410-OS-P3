#include <simics.h>

int main() {
    lprintf("I am in user program!");
    MAGIC_BREAK;

    return 0;
}