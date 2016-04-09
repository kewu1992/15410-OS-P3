#include <console.h>
#include <simics.h>
#include <mutex.h>
#include <control_block.h>
#include <asm_helper.h>
#include <keyboard_driver.h>
#include <context_switcher.h>
#include <vm.h>

/** @brief At most half of the kernel stack to be used as buffer of readline() */
#define MAX_READLINE_BUF (K_STACK_SIZE>>1)

static mutex_t print_lock;

static mutex_t read_lock;

static tcb_t* read_waiting_thr;

static int reading_count;

static int reading_length;

static char *reading_buf;

// lock time is short and need to avoid keyboard interrupt durring operation
static spinlock_t reading_lock;

int syscall_print_init() {
    return mutex_init(&print_lock);
}

int syscall_read_init() {
    read_waiting_thr = NULL;

    if (spinlock_init(&reading_lock) < 0)
        return -1;

    if (mutex_init(&read_lock) < 0)
        return -2;

    return 0;   
}


int print_syscall_handler(int len, char *buf) {

    // Start argument check
    int is_check_null = 0;
    int need_writable = 0;
    int max_len = len;
    if(!is_mem_valid(buf, max_len, is_check_null, need_writable)) {
        return -1;
    }
    // Finish argument check

    mutex_lock(&print_lock);
    putbytes((const char*)buf, len);
    mutex_unlock(&print_lock);

    return 0;
}

int readline_syscall_handler(int len, char *buf) {

    // Start argument check
    int is_check_null = 0;
    int max_len = len;
    int need_writable = 1;
    if(!is_mem_valid(buf, max_len, is_check_null, need_writable)) {
        return -1;
    }

    if (len > MAX_READLINE_BUF)
        return -2;
    // Finish argument check

    mutex_lock(&read_lock);

    int rv = 0;

    char kernel_buf[MAX_READLINE_BUF];

    reading_count = 0;
    reading_length = len;
    reading_buf = kernel_buf;
    
    while (reading_count < reading_length) {
        spinlock_lock(&reading_lock);
        int ch = readchar();
        if (ch == -1) {
            read_waiting_thr = tcb_get_entry((void*)asm_get_esp());
            
            spinlock_unlock(&reading_lock);

            context_switch(3, 0); // no input available, block itself

            break;
        } else {
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
    
    memcpy(buf, reading_buf, reading_count);
    rv = reading_count;

    mutex_unlock(&read_lock);

    return rv;
}

// should only be called by keyboard interrupt, so this function call 
// will not be interrupted
void* resume_reading_thr(char ch) {
    putbyte(ch);
    if (ch == '\b')
        reading_count = (reading_count == 0) ? 0 : (reading_count - 1);
    else{
        reading_buf[reading_count++] = ch;
    }

    if (reading_count == reading_length || ch == '\n') {
        tcb_t* rv = read_waiting_thr;
        read_waiting_thr = NULL;
        return (void*)rv;
    }

    return NULL;
}

// should only be called by keyboard interrupt, so this function call 
// will not be interrupted
int has_read_waiting_thr() {
    return (read_waiting_thr != NULL);
}

int set_term_color_syscall_handler(int color) {

    // Wait while other threads are printing stuff
    int ret;
    mutex_lock(&print_lock);
    ret = set_term_color(color);
    mutex_unlock(&print_lock);

    return ret;

}

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
    if(!is_mem_valid((char *)row, max_len, is_check_null, need_writable) ||
        !is_mem_valid((char *)col, max_len, is_check_null, need_writable)) {
        return -1;
    }

    // Since row and col are fetched in two steps, should be an atomic
    // operation to make row and col related to one point
    mutex_lock(&print_lock);
    get_cursor(row, col);
    mutex_unlock(&print_lock);

    return 0;

}

