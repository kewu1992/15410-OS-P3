#ifndef _SYSCALL_INTER_H_
#define _SYSCALL_INTER_H_

#include <simple_queue.h>
#include <mutex.h>

int malloc_init();

int syscall_print_init();

int syscall_read_init();

int syscall_sleep_init();

int syscall_vanish_init();

void* resume_reading_thr();

void* timer_callback(unsigned int ticks);

int fork_syscall_handler();

/* @brief Data structure for wait() syscall */
typedef struct {
    int num_alive, num_zombie;
    simple_queue_t wait_queue;
    mutex_t lock;
} task_wait_t;

#endif
