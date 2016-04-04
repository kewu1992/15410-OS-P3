#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <control_block.h>

int scheduler_init();

int scheduler_enqueue_tail(tcb_t *thread);

tcb_t* scheduler_get_next(int mode);

tcb_t* simple_scheduler_get_next(int mode);

int scheduler_is_exist(int tid);

#endif
