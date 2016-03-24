#include <simics.h>
#include <vm.h>

int new_pages_syscall_handler(void *base, int len) {
    
    int ret = new_pages(base, len);
    lprintf("new_pages base: %x, ret: %d", (unsigned)base, ret);
    return ret;
}

int remove_pages_syscall_handler(void *base) {
    
    lprintf("remove pages called!");
    int ret = remove_pages(base);
    lprintf("remove_pages base: %x, ret: %d", (unsigned)base, ret);
    return ret;
}


