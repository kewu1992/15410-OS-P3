#include <simics.h>
#include <vm.h>

int new_pages_syscall_handler(void *base, int len) {
    
    lprintf("new_pages base: %x", (unsigned)base);
    int ret = new_pages(base, len);
    //lprintf("ret: %d", ret);
    return ret;
}

int remove_pages_syscall_handler(void *base) {
    
    lprintf("remove_pages base: %x", (unsigned)base);
    int ret = remove_pages(base);
    //lprintf("ret: %d", ret);
    return ret;
}

