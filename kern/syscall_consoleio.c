#include <console.h>
#include <simics.h>
#include <mutex.h>
#include <control_block.h>
#include <asm_helper.h>
#include <keyboard_driver.h>
#include <context_switcher.h>

#define NULL 0

static mutex_t print_lock;

static mutex_t read_lock;

static tcb_t* read_waiting_thr;
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
    // CHECK validation of PARAMETERS

    mutex_lock(&print_lock);
    putbytes((const char*)buf, len);
    mutex_unlock(&print_lock);

    return 0;
}

int readline_syscall_handler(int len, char *buf) {
    // CHECK validation of PARAMETERS
    mutex_lock(&read_lock);
    
    int count = 0;
    while (count < len) {
        spinlock_lock(&reading_lock);
        int ch = readchar();
        if (ch == -1) {
            read_waiting_thr = tcb_get_entry((void*)asm_get_esp());
            spinlock_unlock(&reading_lock);

            context_switch(3, 0); // no input available, block itself
        } else {
            spinlock_unlock(&reading_lock);
            buf[count++] = (char)ch;
            putbyte((char)ch);
            if ((char)ch == '\n')
                break;
            else if ((char)ch == '\b')
                count = (count < 2) ? 0 : (count - 2);
        }
    }
    mutex_unlock(&read_lock);

    return count;
}

// should only be called by keyboard interrupt, so this function call 
// will not be interrupted
void make_reading_thr_runnable() {
    if (read_waiting_thr != NULL) {
        // HOW ABOUT resume to it ???????
        context_switch(4, (uint32_t)read_waiting_thr); // make waiting thread runnable
        read_waiting_thr = NULL;
    }
}