#ifndef _SMP_SYSCALL_H_
#define _SMP_SYSCALL_H_

#include <smp_message.h>

void smp_syscall_fork(msg_t* msg);

void smp_fork_response(msg_t* msg);

int smp_syscall_vanish_init();

void smp_syscall_vanish(msg_t* msg);

void smp_syscall_wait(msg_t* msg);

int smp_syscall_read_init();

void smp_syscall_readline(msg_t *msg);

void smp_syscall_get_cursor_pos(msg_t *msg);

int smp_syscall_print_init();

void smp_syscall_print(msg_t *msg);

void smp_syscall_set_cursor_pos(msg_t *msg);

void smp_syscall_set_term_color(msg_t *msg);

void smp_make_runnable_syscall_handler(msg_t *msg);

void smp_set_init_pcb(msg_t* msg);

#endif
