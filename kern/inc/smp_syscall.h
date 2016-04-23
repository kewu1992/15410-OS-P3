#ifndef _SMP_SYSCALL_H_
#define _SMP_SYSCALL_H_

#include <smp_message.h>

void smp_syscall_fork(msg_t* msg);

void smp_fork_response(msg_t* msg);

#endif