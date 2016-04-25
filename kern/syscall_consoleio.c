/** @file syscall_consoleio.c
 *  @brief System calls related to console I/O.
 *
 *  This file contains implementations of system calls that are related to 
 *  console I/O on worker cores side.
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

/** @brief At most half of the kernel stack can be used as buffer for 
 *         readline() */
#define MAX_READLINE_BUF (K_STACK_SIZE>>1)


/** @brief System call handler for print()
 *
 *  This function will be invoked by print_wrapper().
 *
 *  Prints len bytes of memory, starting at buf, to the console. The calling 
 *  thread should not continue until all characters have been printed to the 
 *  console. Output of two concurrent print()s should not be intermixed.
 *
 *  @param buf The starting address of bytes to be printed
 *  @param len The length of bytes to be printed
 *  @param is_kernel_call Flag showing if the caller is the kernel rather
 *  than the user.
 *
 *  @return On success, return zero.
 *          On error, return an integer error code less than zero.
 */
int print_syscall_handler(int len, char *buf, int is_kernel_call) {

    // Start argument check
    if(!is_kernel_call) {
        int is_check_null = 0;
        int need_writable = 0;
        int max_len = len;
        if(check_mem_validness(buf, max_len, is_check_null, 
                    need_writable) < 0) {
            return EFAULT;
        }
        // Finish argument check
    }

    // Copy to kernel memory so that manager core can access it as well
    char *kernel_buf = malloc(len);
    if(kernel_buf == NULL) {
        return ENOMEM;
    }
    memcpy(kernel_buf, buf, len);

    // Hand over to manager core
    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());

    // Construct message
    this_thr->my_msg->req_thr = this_thr;
    this_thr->my_msg->req_cpu = smp_get_cpu();
    this_thr->my_msg->type = PRINT;
    this_thr->my_msg->data.print_data.len = len;
    this_thr->my_msg->data.print_data.buf = kernel_buf;

    context_switch(OP_SEND_MSG, 0);

    free(kernel_buf);

    return this_thr->my_msg->data.response_data.result;

}

/** @brief System call handler for readline()
 *
 *  This function will be invoked by readline_wrapper().
 *
 *  Reads the next line from the console and copies it into the buffer pointed 
 *  to by buf.
 *  If there is no line of input currently available, the calling thread is 
 *  descheduled until one is. If some other thread is descheduled on a 
 *  readline() or a getchar(), then the calling thread must block and wait its 
 *  turn to access the input stream. The length of the buffer is indicated by 
 *  len. If the line is smaller than the buffer, then the complete line 
 *  including the newline character is copied into the buffer. If the length of 
 *  the line exceeds the length of the buffer, only len characters should be 
 *  copied into buf. Available characters should not be committed into buf until
 *  there is a newline character available, so the user has a chance to 
 *  backspace over typing mistakes.
 *  Characters that will be consumed by a readline() should be echoed to the 
 *  console as soon as possible. If there is no outstanding call to readline() 
 *  no characters should be echoed. Echoed user input may be interleaved with 
 *  output due to calls to print(). Characters not placed in the buffer should 
 *  remain available for other calls to readline() and/or getchar(). 
 *
 *  @param len The maximum length of buffer
 *  @param buf The buffer to store input data.
 *
 *  @return On success, returns the number of bytes copied into the buffer. 
 *          On error, returning an integer error code less than zero.
 */
int readline_syscall_handler(int len, char *buf) {

    // Start argument check
    int is_check_null = 0;
    int max_len = len;
    int need_writable = 1;
    if(check_mem_validness(buf, max_len, is_check_null, need_writable) < 0)
        return EFAULT;

    if (len > MAX_READLINE_BUF)
        return EINVAL;
    // Finish argument check

    char *kernel_buf = malloc(MAX_READLINE_BUF);
    if(kernel_buf == NULL) {
        return ENOMEM;
    }

    // Hand over to manager core
    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());

    // Construct message
    this_thr->my_msg->req_thr = this_thr;
    this_thr->my_msg->req_cpu = smp_get_cpu();
    this_thr->my_msg->type = READLINE;
    this_thr->my_msg->data.readline_data.kernel_buf = kernel_buf;
    this_thr->my_msg->data.readline_data.len = len;

    context_switch(OP_SEND_MSG, 0);

    // get results
    int reading_count = this_thr->my_msg->data.response_data.result;
    memcpy(buf, kernel_buf, reading_count);
    free(kernel_buf);

    return reading_count;
}


/** @brief Set terminal color syscall handler
 *
 * @param color The value of color to set
 *
 * @return 0 on success; -1 on error
 *
 */
int set_term_color_syscall_handler(int color) {

    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());

    // Construct message
    this_thr->my_msg->req_thr = this_thr;
    this_thr->my_msg->req_cpu = smp_get_cpu();
    this_thr->my_msg->type = SET_TERM_COLOR;
    this_thr->my_msg->data.set_term_color_data.color = color;

    context_switch(OP_SEND_MSG, 0);

    return this_thr->my_msg->data.response_data.result;

}

/** @brief Set cursor postion syscall handler
 *
 * @param row The value of row to set
 * @param col The value of col to set
 *
 * @return 0 on success; -1 on error
 *
 */
int set_cursor_pos_syscall_handler(int row, int col) {

    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());

    // Construct message
    this_thr->my_msg->req_thr = this_thr;
    this_thr->my_msg->req_cpu = smp_get_cpu();
    this_thr->my_msg->type = SET_CURSOR_POS;
    this_thr->my_msg->data.set_cursor_pos_data.row = row;
    this_thr->my_msg->data.set_cursor_pos_data.col = col;

    context_switch(OP_SEND_MSG, 0);

    return this_thr->my_msg->data.response_data.result;

}

/** @brief Get cursor postion syscall handler
 *
 * @param row The place to store row
 * @param col The place to store col
 *
 * @return 0 on success; -1 on error
 *
 */
int get_cursor_pos_syscall_handler(int *row, int *col) {

    // Check parameter
    int is_check_null = 0;
    int max_len = sizeof(int);
    int need_writable = 1;
    if(check_mem_validness((char *)row,max_len,is_check_null,need_writable)<0 
            || check_mem_validness((char *)col,max_len,is_check_null,
                need_writable)<0) {
        return -1;
    }

    // Get current thread
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());

    // Construct message
    this_thr->my_msg->req_thr = this_thr;
    this_thr->my_msg->req_cpu = smp_get_cpu();
    this_thr->my_msg->type = GET_CURSOR_POS;

    context_switch(OP_SEND_MSG, 0);

    *row = this_thr->my_msg->data.get_cursor_pos_response_data.row;
    *col = this_thr->my_msg->data.get_cursor_pos_response_data.col;

    return 0;
}

