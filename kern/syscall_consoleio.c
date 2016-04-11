#include <console.h>
#include <simics.h>
#include <mutex.h>
#include <control_block.h>
#include <asm_helper.h>
#include <keyboard_driver.h>
#include <context_switcher.h>
#include <vm.h>
#include <syscall_errors.h>

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
    if(check_mem_validness(buf, max_len, is_check_null, need_writable) < 0) {
        return -1;
    }
    // Finish argument check

    mutex_lock(&print_lock);
    putbytes((const char*)buf, len);
    mutex_unlock(&print_lock);

    return 0;
}

/** @brief System call handler for new_pages()
 *
 *  This function will be invoked by new_pages_wrapper().
 *
 *  Allocates new memory to the invoking task, starting at base and extending 
 *  for len bytes.
 *
 *  @param base The starting address for new pages, it should be page-aligned
 *  @param len The length of bytes to be allocated, it should be multiple of
 *             the system page size.
 *
 *  @return On success, return zero.
 *          On error, returning a negative integer error code.
 *          1) If base is not page-aligned or if len is not a positive integral 
 *             multiple of the system page size, EINVAL will be returned.
 *          2) If any portion of the region represents memory already in the 
 *             task’s address space, EALLOCATED will be returned.
 *          3) If any portion of the region intersects a part of the address 
 *             space reserved by the kernel, EFAULT will be returned.
 *          4) If the operating system has insufficient resources to satisfy the
 *             request, ENOMEM will be returned. 
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
    
    memcpy(buf, reading_buf, reading_count);
    rv = reading_count;

    mutex_unlock(&read_lock);

    return rv;
}

// should only be called by keyboard interrupt, so this function call 
// will not be interrupted
void* resume_reading_thr(char ch) {
    if (!(ch == '\b' && reading_count == 0))
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
    if(check_mem_validness((char *)row, max_len, is_check_null, need_writable) < 0 ||
       check_mem_validness((char *)col, max_len, is_check_null, need_writable) < 0) {
        return -1;
    }

    // Since row and col are fetched in two steps, should be an atomic
    // operation to make row and col related to one point
    mutex_lock(&print_lock);
    get_cursor(row, col);
    mutex_unlock(&print_lock);

    return 0;

}

