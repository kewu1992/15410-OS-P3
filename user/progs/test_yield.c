#include <syscall.h>
#include <simics.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

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


int main() {

    test_yield_failure();

    // test_yield_success();

    lprintf("test ends");

    while(1) ;

}


