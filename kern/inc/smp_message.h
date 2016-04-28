#ifndef _SMP_MESSAGE_H_
#define _SMP_MESSAGE_H_

#include <simple_queue.h>

/* Request data */

/** @brief Message data for fork */
typedef struct {
    /** @brief tcb of the newly created thread*/
    void* new_thr;
    /** @brief The times of retrying fork() if the previous are failed */
    int retry_times; 
    /** @brief tid of the newly created thread, it is also the pid of the
     *         new process */
    int new_tid;
    /** @brief pid of the parent task */
    int ppid;
} msg_data_fork_t;

/** @brief Message data for wait */
typedef struct {
    int pid;
} msg_data_wait_t;

/** @brief Message data for vanish */
typedef struct {
    int pid;
    int ppid;
    int status;
} msg_data_vanish_t;

/** @brief Message data for set_term_color */
typedef struct {
    int color;
} msg_data_set_term_color_t;

/** @brief Message data for print */
typedef struct {
    int len;
    char *buf;
} msg_data_print_t;

/** @brief Message data for set_cursor_pos */
typedef struct {
    int row;
    int col;
} msg_data_set_cursor_pos_t;

/** @brief Message data for readline */
typedef struct {
    int len;
    char *kernel_buf;
} msg_data_readline_t;


/** @brief Message data for make_runnable */
typedef struct {
    int tid;
    int next_core;
    int result;
} msg_data_make_runnable_t;

/** @brief Message data for get_cursor_pos */
typedef struct {
    int row;
    int col;
} msg_data_get_cursor_pos_response_t;

/* Response data */

/** @brief Message response data for syscalls that have one integer return 
  * value.
  */
typedef struct {
    int result;
} msg_data_response_t;

/** @brief Message response data for fork */
typedef struct {
    void* req_msg;
    int result;
} msg_data_fork_response_t;

/** @brief Message response data for wait */
typedef struct {
    int status;
    int pid;
}msg_data_wait_response_t;

/** @brief Message response data for set_init_pcb */
typedef struct {
    int pid;
} msg_data_set_init_pcb_t;

/** @brief Message response data for vanish */
typedef struct {
    int ori_cpu;
} msg_data_vanish_back_t;

/** @brief Message response data for yield */
typedef struct {
    int tid;
    int next_core;
    int result;
} msg_data_yield_t;

/** @brief Message type */
typedef enum {
    FORK,           // 0
    THREAD_FORK,    // 1
    VANISH,         // 2
    WAIT,           // 3
    YIELD,          // 4
    MAKE_RUNNABLE,  // 5
    READLINE,       // 6
    PRINT,          // 7
    SET_TERM_COLOR, // 8
    SET_CURSOR_POS, // 9
    GET_CURSOR_POS, // 10
    SET_INIT_PCB,   // 11
    RESPONSE,       // 12
    FORK_RESPONSE,  // 13
    WAIT_RESPONSE,  // 14
    VANISH_BACK,    // 15
    HALT,           // 16
    NONE
} msg_type_t;

/** @brief Message type */
typedef struct {
    /** @brief A node to enable this message be put in a queue somewhere */
    simple_node_t node;  // 12 bytes
    /** @brief The tcb of the thread that issues a interprocessor syscall */
    void* req_thr;  // 4 bytes
    /** @brief The index of the core where the requesting thread resides */
    int req_cpu;    // 4 bytes
    /** @brief Type of the message: can be request or response type */
    msg_type_t type;  // 4 bytes
    /** @brief Data field of the message, can be request or response data
     *  Use union to limit message size.
     */
    union {
        /* Request data */
        msg_data_fork_t fork_data;
        msg_data_wait_t wait_data;
        msg_data_vanish_t vanish_data;
        msg_data_fork_response_t fork_response_data;
        msg_data_set_term_color_t set_term_color_data;
        msg_data_set_cursor_pos_t set_cursor_pos_data;
        msg_data_readline_t readline_data;
        msg_data_print_t print_data;
        msg_data_make_runnable_t make_runnable_data;
        msg_data_yield_t yield_data;
        /* Response data */
        msg_data_wait_response_t wait_response_data;
        msg_data_get_cursor_pos_response_t get_cursor_pos_response_data;
        msg_data_response_t response_data;
        msg_data_set_init_pcb_t set_init_pcb_data;
        msg_data_vanish_back_t vanish_back_data;
    } data; // 16 bytes
} msg_t;


int msg_init();

int init_ap_msg();

void msg_synchronize();

void worker_send_msg(msg_t* msg);

msg_t* worker_recv_msg();

void manager_send_msg(msg_t* msg, int dest_cpu);

msg_t* manager_recv_msg();

void* get_thr_from_msg_queue();

#endif
