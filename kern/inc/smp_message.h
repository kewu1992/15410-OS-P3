#ifndef _SMP_MESSAGE_H_
#define _SMP_MESSAGE_H_

#include <simple_queue.h>

typedef struct {
    void* new_thr;
    void* old_task_pd;
} msg_data_fork_t;

typedef struct {
    int color;
} msg_data_set_term_color_t;

typedef struct {
    int row;
    int column;
} msg_data_set_cursor_pos_t;

typedef enum {
    FORK,
    THREAD_FORK,
    VANISH,
    WAIT,
    YIELD,
    DESCHEDULE,
    MAKE_RUNNABLE,
    READLINE,
    PRINT,
    SET_TERM_COLOR,
    SET_CURSOR_POS,
    GET_CURSOR_POS,
    RESPONSE
} msg_type_t;

typedef struct {
    simple_node_t node;  // 12 bytes
    void* req_thr;  // 4 bytes
    int req_cpu;    // 4 bytes
    msg_type_t type;  // 4 bytes
    union {
        msg_data_fork_t fork_data;
        msg_data_set_term_color_t set_term_color_data;
        msg_data_set_cursor_pos_t set_cursor_pos_data;
    } data;
} msg_t;


int msg_init();

int init_ap_msg();

void msg_synchronize();

void worker_send_msg(msg_t* msg);

msg_t* worker_recv_msg();

void manager_send_msg(msg_t* msg);

msg_t* manager_recv_msg();

#endif
