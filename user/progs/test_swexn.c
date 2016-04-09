#include <syscall.h>
#include <simics.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <stdint.h>

#include <simics.h>

void test_yield_failure() {

    // Run without shell, should return -1
    // Because there's only one single thread, 
    // no other thread can yield
    int ret = yield(-1);

    lprintf("ret %d", ret);

}

void test_yield_success() {

    int pid = fork();
    if(pid == 0) {
        // In pid 1
        // Without shell parent should be pid 0
        int ret = yield(0);
        lprintf("Parent hasn't exited, ret should be 0, ret = %d", ret);
        ret = yield(4);
        lprintf("Try a non existent tid, ret should be -1, ret = %d", ret);
        lprintf("Test ends, pid 1 is going to exit");
        exit(43);
    }

    // Parent task
    int i;
    for(i = 0; i < 100000; i++) {
        // Loop forever
        ;
    }

}

void dump_ureg(ureg_t *ureg) {

    lprintf("This is the ureg dump");
    lprintf("cause: %x, cr2: %x, ds: %x, es: %x, fs: %x, gs: %x, "
            "edi: %x, esi: %x, ebp: %x, zero: %x, ebx: %x, edx: %x, "
            "ecx: %x, eax: %x, error_code: %x, eip: %x, cs: %x, eflags: %x, "
            "esp: %x, ss: %x", 
            (unsigned)ureg->cause, (unsigned)ureg->cr2, (unsigned)ureg->ds, 
            (unsigned)ureg->es, (unsigned)ureg->fs, (unsigned)ureg->gs, 
            (unsigned)ureg->edi, (unsigned)ureg->esi, (unsigned)ureg->ebp, 
            (unsigned)ureg->zero, (unsigned)ureg->ebx, (unsigned)ureg->edx,
            (unsigned)ureg->ecx, (unsigned)ureg->eax, 
            (unsigned)ureg->error_code, (unsigned)ureg->eip,
            (unsigned)ureg->cs, (unsigned)ureg->eflags, (unsigned)ureg->esp, 
            (unsigned)ureg->ss);
}

void swexn_handler(void *arg, ureg_t *ureg) {

    lprintf("This is the user space swexn handler");
    lprintf("arg: %x", (unsigned)arg);
    dump_ureg(ureg);

    lprintf("swexn_handler returns directly");
    return;


    // MAGIC_BREAK;

}

void test_division_zero() {

    int a = 0;
    lprintf("Will divide by 0");
    lprintf("divide by 0: %d", 3/a);

    lprintf("survived 0 division?!");
    MAGIC_BREAK;

}

void test_zfod() {
    uint32_t base = 0x8000000;
    int len = 4096;
    int ret = new_pages((void *)base, len);
    if(ret < 0) {
        lprintf("new_pages failed");
        MAGIC_BREAK;
    } else {
        lprintf("new_pages succeed");
    }

    lprintf("About to write to new memory");
    *((char *)base) = '1';
    lprintf("Wrote to new memory, new memory: %c", 
            *((char *)base));

    MAGIC_BREAK;
}

void test_swexn() {

    uint32_t base = 0x5000000;
    int len = 4096;
    if(new_pages((void *)base, len) < 0) {
        lprintf("new_pages failed");
        MAGIC_BREAK;
    }

    // Test very small stack, 4 bytes: 0x5000004 to 0x5000000
    // char exn_stack_high[4];
    // uint32_t esp3 = (uint32_t)(base + 4);
    uint32_t esp3 = (uint32_t)(0x5001000 - 4);
    // Register exception handler
    if(swexn((void *)esp3, swexn_handler, (void *)3, NULL) < 0) {
        lprintf("Register exception handler failed");
        MAGIC_BREAK;
    } else {
        lprintf("Register exception handler succeeded");
    }

    test_division_zero();

    // test_zfod();
}

void test_noswexn() {
    int pid = fork();
    if(pid == 0) {
        test_division_zero();
    }
    
    if(pid > 0) {
        while(1);
    }

}


int main() {

    // test_yield_failure();

    // test_yield_success();

    test_swexn();


    // test_noswexn();

    lprintf("test ends");

    while(1) ;

}


