#include <syscall.h>
#include <simics.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <stdint.h>

#include <simics.h>


void test_division_zero() {

    int a = 0;
    lprintf("Will divide by 0");
    lprintf("divide by 0: %d", 3/a);

    lprintf("survived 0 division?!");
    MAGIC_BREAK;

}


int main() {

    // test_yield_failure();

    // test_yield_success();

    // test_swexn();

    // test_fork_swexn();

    lprintf("new execed program will divide by 0"); 
    test_division_zero();


    // test_noswexn();

    lprintf("test ends");

    while(1) ;

}


