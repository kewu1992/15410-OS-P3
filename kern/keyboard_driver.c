/** @file keyboard_driver.c
 *  @brief Keyboard device driver and driver initialization function
 *
 *  This file contains keyboard device driver and driver initialization
 *  function. The device driver can be divided into two parts. The bottom part
 *  is interrupt handler which handle keyboard interrupt, get scancode from I/O
 *  port and store scancode in a buffer. It will also check if there is any 
 *  thread that is blocked by readline() and wake up the blocked thread if the 
 *  readline() syscall can complete. The top part is readchar() which get 
 *  scancode from buffer, process it by invoking process_scancode() and return
 *  the input character based on augmented character. 
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <interrupt_defines.h>
#include <asm.h>
#include <keyhelp.h>
#include <syscall_inter.h>
#include <simics.h>
#include <context_switcher.h>

/** @brief Size of buffer used to store scancode */
#define KEY_BUF_SIZE 256

/** @brief Buffer used to store scancode */
static uint8_t keybuf[KEY_BUF_SIZE];

/** @brief Two pointers for accessing buffer, front indicates where the next 
 *         data will be placed in buffer, rear indicates where is the next 
 *         available data. When front == rear, it means buffer is empty. When
 *         (front + 1) % KEY_BUF_SIZE == rear, it means buffer is full. Note
 *         that a buffer slot is wasted so that we can distinguish between 
 *         buffer empty and buffer full.
 */
static short front;
/** @brief The rear pointer */
static short rear;

/** @brief Initialize keyboard device driver
 *  
 *  Set two pointers to zero, indicating that buffer is empty.
 *
 *  @return Void.
 */
void init_keyboard_driver() {
    front = 0;
    rear = 0;
}


/** @brief Interrupt handler for keyboard interrupt
 *  
 *  The bottom part of kayboard driver. Read scancode from I/O port and store
 *  it in buffer. If there is any thread blocked on readline(), this function 
 *  will also process scancode, put the input characters into buffer of 
 *  readline() and wake up blocked thread if readline() completes. 
 *
 *  This interrupt handler should use interrupt gate. Imagine that if trap gate
 *  is used and a timer interrupt comes in while this function is manipulating
 *  variables and buffer of readline() in resume_reading_thr(). The timer 
 *  interrupt causes a context switch and the next thread is also running at 
 *  readline_syscall_handler() and about to manipulate variables 
 *  and buffer of readline(). In this case, the data of readline() might be in 
 *  an inconsistent state and some bad things will happen. 
 *
 *  @return Void.
 */
void keyboard_interrupt_handler(){
    uint8_t scancode = inb(KEYBOARD_PORT);

    // check if keyboard buffer is full
    if ((front + 1) % KEY_BUF_SIZE != rear){
        keybuf[front] = scancode;
        front = (front + 1) % KEY_BUF_SIZE;
    }

    void* thr = 0;

    if (has_read_waiting_thr()) {
        // some threads are waiting for input, try to process scancode and
        // fill buffer of readline()
        int ch = -1;

        while (ch == -1 && front != rear) {
            uint8_t scancode = keybuf[rear];
            rear = (rear + 1) % KEY_BUF_SIZE;

            // process code
            kh_type augchar = process_scancode(scancode);
            if (KH_HASDATA(augchar) && KH_ISMAKE(augchar))
                ch = (int)KH_GETCHAR(augchar);
        } 

        // there is an available character, put it to the buffer of readline()
        if (ch != -1)
            thr = resume_reading_thr((char)ch);
    }

    outb(INT_CTL_PORT, INT_ACK_CURRENT);

    enable_interrupts();

    // if thr != NULL, it means readline() completes and the blocked thread 
    // should be wakened up
    if (thr) {

        msg_t *msg = ((tcb_t *)thr)->my_msg;
        manager_send_msg(msg, msg->req_cpu);
    }
}

/** @brief Returns the next character in the keyboard buffer
 *
 *  The top part of keyboard driver. It get scancode from buffer and process it.
 *  This function does not block if there are no characters in the keyboard
 *  buffer. Note that before it access the shared data structure buffer, it will
 *  disable all interrupts to avoid interrupt-related concurrency problem 
 *  (this is not done in readchar() but in the function that invoke readchar(), 
 *  e.g. readline() and getchar()).
 *
 *  @return The next character in the keyboard buffer, or -1 if the keyboard
 *          buffer is currently empty
 **/
int readchar(void) {
    int ret = -1;

    // check if keryboard buffer is empty
    while (ret == -1 && front != rear) {
        uint8_t scancode = keybuf[rear];
        rear = (rear + 1) % KEY_BUF_SIZE;

        // process code
        kh_type augchar = process_scancode(scancode);
        if (KH_HASDATA(augchar) && KH_ISMAKE(augchar))
            ret = (int)KH_GETCHAR(augchar);
    } 
    return ret;
}
