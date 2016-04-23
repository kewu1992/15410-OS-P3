#ifndef _SMP_SYSCALL_H_
#define _SMP_SYSCALL_H_

#include <smp_message.h>

void smp_syscall_fork(msg_t* msg);

void smp_fork_response(msg_t* msg);

int smp_syscall_vanish_init();

void smp_syscall_vanish(msg_t* msg);

void smp_syscall_wait(msg_t* msg);
#endif