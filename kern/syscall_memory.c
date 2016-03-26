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


/** @brief swexn syscall
  *
  * @param esp3 The exception stack address (one word higher than the first
  * address that the kernel should use to push values onto exception stack.
  *
  * @param eip The first instruction of the handler function
  *
  * @return
  *
  */
int swexn_syscall_handler(void *esp3, swexn_handler_t eip, void *arg, 
        ureg_t *newureg) {

    lprintf("swexn called");
    MAGIC_BREAK;

    /*
    // Check new_ureg validness if it's not NULL
    if(newureg != NULL) {
        if(!is_newureg_valid()) {
            // new_ureg isn't valid
             
        }
    }

    if(esp3 == NULL || (uint32_t)eip == 0) {
        // deregister an exception handler if one is currently registered
    } else {
        // register an exception handler
    }
    */

    return 0;

}


