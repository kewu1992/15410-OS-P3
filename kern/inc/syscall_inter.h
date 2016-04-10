#ifndef _SYSCALL_INTER_H_
#define _SYSCALL_INTER_H_

#include <mutex.h>
#include <control_block.h>


int malloc_init();

int syscall_print_init();

int syscall_read_init();

int syscall_sleep_init();

int syscall_vanish_init();

void* resume_reading_thr(char ch);

void* timer_callback(unsigned int ticks);

int syscall_deschedule_init();

int syscall_readfile_init();

int has_read_waiting_thr();

tcb_t* get_next_zombie();

int put_next_zombie(tcb_t *thread_zombie);

void vanish_wipe_thread(tcb_t *thread);

void ht_put_task(int pid, pcb_t *process);

mutex_t *get_zombie_list_lock();

void set_init_pcb(pcb_t *init_task);

void vanish_syscall_handler(int is_kernel_kill);

#endif
