#ifndef _SYSCALL_INTER_H_
#define _SYSCALL_INTER_H_

int malloc_init();

int syscall_print_init();

int syscall_read_init();

int syscall_sleep_init();

int syscall_vanish_init();

void make_reading_thr_runnable();

void timer_callback(unsigned int ticks);

/* @brief Data structure for wait() syscall */
typedef struct {
    int num_running, num_zombie;
    simple_queue_t wait_queue;
    mutex_t lock;
} task_wait_t;

#endif
