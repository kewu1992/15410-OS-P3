#ifndef _SYSCALL_INTER_H_
#define _SYSCALL_INTER_H_

#include <simple_queue.h>
#include <mutex.h>
#include <ureg.h>

int malloc_init();

int syscall_print_init();

int syscall_read_init();

int syscall_sleep_init();

int syscall_vanish_init();

void* resume_reading_thr();

void* timer_callback(unsigned int ticks);

int syscall_deschedule_init();

/* @brief Data structure for wait() syscall */
typedef struct {
    int num_alive, num_zombie;
    simple_queue_t wait_queue;
    mutex_t lock;
} task_wait_t;

/** @brief The swexn handler type */
typedef void (*swexn_handler_t)(void *arg, ureg_t *ureg);

/** @brief The parameters for registered swexn handler */
typedef struct swexn_t {
    void *esp3;
    swexn_handler_t eip;
    void *arg;
} swexn_t;

#endif
