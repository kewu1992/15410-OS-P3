/** @file syscall_inter.h
 *
 *  @brief This file contains definitions for syscall internals
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _SYSCALL_INTER_H_
#define _SYSCALL_INTER_H_

#include <mutex.h>
#include <control_block.h>


int malloc_init(int cpu_id);

int syscall_sleep_init();

int syscall_vanish_init();

int syscall_deschedule_init();

int syscall_readfile_init();

void* resume_reading_thr(char ch);

void* timer_callback(unsigned int ticks);

int has_read_waiting_thr();

simple_node_t* get_next_zombie();

int put_next_zombie(simple_node_t* node);

mutex_t *get_zombie_list_lock();

int set_init_pcb(pcb_t *init_pcb);

void vanish_syscall_handler(int is_kernel_kill);

int print_syscall_handler(int len, char *buf, int is_kernel_call);

int fork_create_process(tcb_t* new_thr, tcb_t* old_thr);

#endif
