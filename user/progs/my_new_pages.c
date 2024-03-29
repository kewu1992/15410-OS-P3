/** @file 410user/progs/new_pages.c
 *  @author de0u
 *  @brief Tests new_pages() a lot.
 *  @public yes
 *  @for p3
 *  @covers new_pages
 *  @status done
 */

/* Here's what we do */
int new_stack(void);    /* new_pages() into the stack */
int new_data(void);     /* new_pages() into data region */
int lotsa_luck(void);   /* new_pages() "too much" memory */

#include <syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "410_tests.h"
#include <report.h>
#include <test.h>
#include <stdint.h>

/* Yes, I mean this--see below */
#define TEST_NAME "new_pages:"
DEF_TEST_NAME(TEST_NAME);

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define PAGE_BASE(a) (((int)(a)) & ~(PAGE_SIZE - 1))

int main(int argc, char *argv[]) {
    int rets[3], i;
    char mbuf[256];

    REPORT_START_CMPLT;

    rets[0] = new_stack();
    lprintf("rets 0: %d", rets[0]);
    rets[1] = new_data();
    lprintf("rets 1: %d", rets[1]);
    rets[2] = lotsa_luck();
    lprintf("rets 2: %d", rets[2]);

    for (i = 0; i < sizeof (rets) / sizeof (rets[0]); ++i) {
        if (rets[i] == 0) {
            REPORT_MISC("I *want* my outs to count!!!"); /* Christine Lavin */
            report_fmt("died on %d", i);
            REPORT_END_FAIL;
            exit(71);
        }
    }

    lprintf("Check finished");
    format_end(mbuf, sizeof(mbuf), END_SUCCESS);
    lprintf("format_end finished");

    exhaustion(exit_success, mbuf);
    lprintf("exhaustion finished");

    REPORT_END_SUCCESS;
    exit(0);
}

    int
new_stack(void)
{
    int answer = 42;
    int ret;

    void *base = (void *)PAGE_BASE((&answer));
    lprintf("new_stack: base is 0x%x", (unsigned)base);
    ret = new_pages(base, PAGE_SIZE);
    if (answer != 42) {
        REPORT_MISC("My brain hurts!"); /* T.F. Gumby */
        REPORT_END_FAIL;
        exit(71);
    }
    return (ret);
}

    int
new_data(void)
{
    int ret;

    void *base = (void *)PAGE_BASE(test_name);
    lprintf("new_data: base is 0x%x", (unsigned)base);
    ret = new_pages(base, PAGE_SIZE);
    if (strcmp(test_name, TEST_NAME)) {
        report_misc("new_pages() killed .data?");
        REPORT_END_FAIL;
        exit(71);
    }
    return (ret);
}

#define ADDR 0x40000000
#define GIGABYTE (1024*1024*1024)

    int
lotsa_luck(void)
{
    return (new_pages((void *)ADDR, GIGABYTE));
}

