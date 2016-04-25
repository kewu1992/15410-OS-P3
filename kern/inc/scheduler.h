/** @file scheduler.h 
 *
 *  @brief Contains public interface for scheduler
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs
 */


#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <control_block.h>

int scheduler_init();

tcb_t* scheduler_get_next(int mode);

tcb_t* scheduler_block();

void scheduler_make_runnable(tcb_t *thread);

int scheduler_is_exist_or_running(int tid);

#endif
