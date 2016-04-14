/** @file syscall_consoleio.c
 *  @brief System calls related to console I/O.
 *
 *  This file contains implementations of system calls that are related to 
 *  console I/O.
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

/** @brief At most half of the kernel stack can be used as buffer for 
 *         readline() */
#define MAX_READLINE_BUF (K_STACK_SIZE>>1)

/** @brief For print() syscall.
*          The mutex to prevent mulitple print() from interleaving */
static mutex_t print_lock;

/** @brief For readline() syscall.
 *         The mutex to prevent mulitple readline() from interleaving */
static mutex_t read_lock;

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


/** @brief Initialize data structure for print() syscall */
int syscall_print_init() {
    return mutex_init(&print_lock);
}

/** @brief Initialize data structure for readline() syscall */
int syscall_read_init() {
    read_waiting_thr = NULL;

    if (spinlock_init(&reading_lock) < 0)
        return -1;

    if (mutex_init(&read_lock) < 0)
        return -2;

    return 0;   
}

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

    // using mutex lock to avoid multiple print() interleaving
    mutex_lock(&print_lock);
    putbytes((const char*)buf, len);
    mutex_unlock(&print_lock);

    return 0;
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

    // using mutex lock to avoid multiple readline() interleaving
    mutex_lock(&read_lock);

    int rv = 0;

    // kernel stack space is used to store bytes read by readline().
    char kernel_buf[MAX_READLINE_BUF];

    reading_count = 0;
    reading_length = len;
    reading_buf = kernel_buf;
    
    while (reading_count < reading_length) {
        // spinlock is used to disable keyboard interrupt when manipulate data
        // structures of keyboard driver and readline() syscall. 
        spinlock_lock(&reading_lock);
        int ch = readchar();
        if (ch == -1) {
            // there is no data, block this thread. Keyboard interrupt handler
            // will put data to buffer when input comes in. It will 
            // wake up this thread when this readline() syscall completes.
            read_waiting_thr = tcb_get_entry((void*)asm_get_esp());
            
            spinlock_unlock(&reading_lock);

            context_switch(OP_BLOCK, 0);

            break;
        } else {
            if (!((char)ch == '\b' && reading_count == 0))
                putbyte((char)ch);
            spinlock_unlock(&reading_lock);
            
            if ((char)ch == '\b')
                reading_count = (reading_count == 0) ? 0 : (reading_count - 1);
            else
                reading_buf[reading_count++] = (char)ch;

            if ((char)ch == '\n')
                break;
        }
    }
    
    // copy data from kernel buffer to user buffer
    memcpy(buf, reading_buf, reading_count);
    rv = reading_count;

    mutex_unlock(&read_lock);

    return rv;
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
        tcb_t* rv = read_waiting_thr;
        read_waiting_thr = NULL;
        return (void*)rv;
    }

    return NULL;
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


/** @brief Set terminal color syscall handler
 *
 * @param color The value of color to set
 *
 * @return 0 on success; -1 on error
 *
 */
int set_term_color_syscall_handler(int color) {

    // Wait while other threads are printing stuff
    int ret;
    mutex_lock(&print_lock);
    ret = set_term_color(color);
    mutex_unlock(&print_lock);

    return ret;

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

    // Wait while other threads are printing stuff
    int ret;
    mutex_lock(&print_lock);
    ret = set_cursor(row, col);
    mutex_unlock(&print_lock);

    return ret;

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
    if(check_mem_validness((char *)row,max_len,is_check_null,need_writable)<0 ||
       check_mem_validness((char *)col,max_len,is_check_null,need_writable)<0) {
        return -1;
    }

    // Since row and col are fetched in two steps, should be an atomic
    // operation to make row and col related to one point
    mutex_lock(&print_lock);
    get_cursor(row, col);
    mutex_unlock(&print_lock);

    return 0;

}

