#include <console_driver.h>
#include <simics.h>
#include <mutex.h>

static mutex_t print_lock;

int syscall_print_init() {
    return mutex_init(&print_lock);
}

int print_syscall_handler(int len, char *buf) {
    // CHECK validation of PARAMETERS

    mutex_lock(&print_lock);
    putbytes((const char*)buf, len);
    mutex_unlock(&print_lock);

    return 0;
}