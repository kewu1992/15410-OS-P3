/** @file smp_syscall_consoleio.c
 *  @brief SMP version of system calls related to console I/O.
 *
 *  This file contains implementations of system calls that are related to 
 *  console I/O that are parts handled on manager core side.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <console.h>
#include <simics.h>
#include <mutex.h>
#include <control_block.h>
#include <asm_helper.h>
#include <keyboard_driver.h>
#include <context_switcher.h>
#include <vm.h>
#include <syscall_errors.h>

#include <smp.h>

/** @brief For print() syscall.
 *          The mutex to prevent mulitple print() from interleaving */
static mutex_t print_lock;

/** @brief For readline() syscall.
 *         The thread that is blocked on readline() and waiting for input. 
 *         If there is no thread waiting for input, this variable should be 
 *         NULL. */
static tcb_t* read_waiting_thr;

/** @brief For readline() syscall.
 *         The number of bytes that have been read by one readline() syscall */
static int reading_count;

/** @brief For readline() syscall.
 *         The maximum number of bytes that cen be read by one readline() 
 *         syscall, which equals 'len' argument of readline() syscall */
static int reading_length;

/** @brief For readline() syscall.
 *         The buffer to store bytes read by readline(). This buffer using
 *         kernel stack space */
static char *reading_buf;

/** @brief For readline() syscall.
 *         lock to avoid any interrupt durring operation. Because interrupt 
 *         handlers are using interrupt gate which disable all interrupts, here
 *         we can not use mutex for protection. Because in mutex_lock(), it will
 *         call enable_interrupts() and make interrupt gate useless. */
static spinlock_t reading_lock;

/** @brief A queue for threads requesting readline() */
static simple_queue_t readline_queue;

/** @brief Initialize data structure for print() syscall */
int syscall_print_init() {
    return mutex_init(&print_lock);
}

/** @brief Initialize data structure for readline() syscall */
int syscall_read_init() {
    read_waiting_thr = NULL;

    if (spinlock_init(&reading_lock) < 0)
        return -1;

    if(simple_queue_init(&readline_queue) < 0)
        return -1;

    return 0;   
}

/** @brief Check if there is any thread blocked on readline(), waiting for input
 *
 *  This function should only be called by keyboard interrupt, so this function
 *  call will not be interrupted. It can read value of read_waiting_thr safely.
 * 
 *  @return Return 1 if there is thread waiting, return zero otherwise */
int has_read_waiting_thr() {
    return (read_waiting_thr != NULL);
}


/** @brief Multi-core version of readline syscall handler that's on manager 
 *  core side
 *
 *  @param msg The message that contains the readline request
 *
 *  @return void
 */
void smp_readline_syscall_handler(msg_t *msg) {

    // Check if some other thread is requesting readline(), if so, enqueue 
    // current request. This checking should be locked by spinlock to avoid
    // keyboard interrupt during checking
    spinlock_lock(&reading_lock, 1);
    if(has_read_waiting_thr()) {
        simple_queue_enqueue(&readline_queue, &(msg->node));
        spinlock_unlock(&reading_lock, 1);
        return;
    }
    spinlock_unlock(&reading_lock, 1);

    int len = msg->data.readline_data.len;
    char *kernel_buf = msg->data.readline_data.kernel_buf;

    reading_count = 0;
    reading_length = len;
    reading_buf = kernel_buf;

    while (reading_count < reading_length) {
        // spinlock is used to disable keyboard interrupt when manipulate data
        // structures of keyboard driver and readline() syscall. 
        spinlock_lock(&reading_lock, 1);
        int ch = readchar();
        if (ch == -1) {
            // there is no data, block this thread. Keyboard interrupt handler
            // will put data to buffer when input comes in. It will 
            // wake up this thread when this readline() syscall completes.
            read_waiting_thr = msg->req_thr;

            // No need to enqueue since there's no other request

            spinlock_unlock(&reading_lock, 1);

            return;

        } else {
            if (!((char)ch == '\b' && reading_count == 0))
                putbyte((char)ch);
            spinlock_unlock(&reading_lock, 1);

            if ((char)ch == '\b')
                reading_count = (reading_count == 0) ? 0 : (reading_count - 1);
            else
                reading_buf[reading_count++] = (char)ch;

            if ((char)ch == '\n')
                break;
        }
    }

    msg->type = RESPONSE;
    msg->data.response_data.result = reading_count;
    manager_send_msg(msg, msg->req_cpu);

}

/** @brief Put input byte to the buffer of readline() and wake up blocked thread
 *         if readline() completes
 *
 *  This function should only be called by keyboard interrupt, so this function
 *  call will not be interrupted. It can manipulate data structure of readline()
 *  safely.
 * 
 *  @return Return the blocked thread that should be wakened up if readline() 
 *          completes, return NULL otherwise */
void* resume_reading_thr(char ch) {
    // echo input consumed by readline() to screen
    if (!(ch == '\b' && reading_count == 0))
        putbyte(ch);

    if (ch == '\b')
        reading_count = (reading_count == 0) ? 0 : (reading_count - 1);
    else{
        reading_buf[reading_count++] = ch;
    }

    // check if readline() completes
    if (reading_count == reading_length || ch == '\n') {

        // readline() completes
        msg_t* msg = read_waiting_thr->my_msg;
        msg->type = RESPONSE;
        msg->data.response_data.result = reading_count;
        tcb_t* rv = read_waiting_thr;

        // Serve next readline request
        simple_node_t *readline_node = 
                simple_queue_dequeue(&readline_queue);
        if(readline_node != NULL) {
            msg_t* next_msg = (msg_t *)(readline_node->thr);
            int len = next_msg->data.readline_data.len;
            char *kernel_buf = next_msg->data.readline_data.kernel_buf;
            reading_count = 0;
            reading_length = len;
            reading_buf = kernel_buf;
            read_waiting_thr = msg->req_thr;
        } else {
            read_waiting_thr = NULL;
        }

        return (void*)rv;

    }

    return NULL;
}


/** @brief Multi-core version of get_cursor_pos syscall handler that's on 
 *  manager core side
 *
 *  @param msg The message that contains the get_cursor_pos request
 *
 *  @return void
 */
void smp_get_cursor_pos_syscall_handler(msg_t *msg) {

    int row;
    int col;

    // Since row and col are fetched in two steps, should be an atomic
    // operation to make row and col related to one point
    mutex_lock(&print_lock);
    get_cursor(&row, &col);
    mutex_unlock(&print_lock);

    msg->type = RESPONSE;
    msg->data.get_cursor_pos_response_data.row = row;
    msg->data.get_cursor_pos_response_data.col = col;
    manager_send_msg(msg, msg->req_cpu);

}


/** @brief Multi-core version of print syscall handler that's on manager 
 *  core side
 *
 *  @param msg The message that contains the print request
 *
 *  @return void
 */
void smp_print_syscall_handler(msg_t *msg) {

    int len = msg->data.print_data.len;
    char *buf = msg->data.print_data.buf;

    // using mutex lock to avoid multiple print() interleaving
    mutex_lock(&print_lock);
    putbytes((const char*)buf, len);
    mutex_unlock(&print_lock);

    msg->type = RESPONSE;
    msg->data.response_data.result = 0;
    manager_send_msg(msg, msg->req_cpu);

}


/** @brief Multi-core version of set_cursor_pos syscall handler that's on 
 *  manager core side
 *
 *  @param msg The message that contains the set_cursor_pos request
 *
 *  @return void
 */
void smp_set_cursor_pos_syscall_handler(msg_t *msg) {

    int row = msg->data.set_cursor_pos_data.row;
    int col = msg->data.set_cursor_pos_data.col;

    // Wait while other threads are printing stuff
    int ret;
    mutex_lock(&print_lock);
    ret = set_cursor(row, col);
    mutex_unlock(&print_lock);

    msg->type = RESPONSE;
    msg->data.response_data.result = ret;
    manager_send_msg(msg, msg->req_cpu);
}


/** @brief Multi-core version of set_term_color syscall handler that's on 
 *  manager core side
 *
 *  @param msg The message that contains the set_term_color request
 *
 *  @return void
 */
void smp_set_term_color_syscall_handler(msg_t *msg) {

    int color = msg->data.set_term_color_data.color;

    int ret;
    mutex_lock(&print_lock);
    ret = set_term_color(color);
    mutex_unlock(&print_lock);

    msg->type = RESPONSE;
    msg->data.response_data.result = ret;
    manager_send_msg(msg, msg->req_cpu);
}


