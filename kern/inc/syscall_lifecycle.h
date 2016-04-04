#ifndef _SYSCALL_LIFECYCLE_H_
#define _SYSCALL_LIFECYCLE_H_

#include <list.h>

/** @brief The struct to store exit status for a task */
typedef struct {
    /** @brief The vanished task's pid */
    int pid;
    /** @brief The vanished task's exit status */
    int status;
} exit_status_t;

int get_next_zombie(tcb_t **thread_zombie);
int put_next_zombie(tcb_t *thread_zombie);
int vanish_wipe_thread(tcb_t *thread_zombie);
void ht_put_task(int pid, pcb_t *process);


#endif
