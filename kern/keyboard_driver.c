/** @file keyboard_driver.c
 *  @brief Keyboard device driver and driver initialization function
 *
 *  This file contains keyboard device driver and driver initialization
 *  function. The device driver can be divided into two parts. The bottom part
 *  is interrupt handler which handle keyboard interrupt, get scancode from I/O
 *  port and store scancode in a buffer. The top part is readchar() which get 
 *  scancode from buffer, process it by invoking process_scancode() and return
 *  the input character based on augmented character. Note that becuase
 *  interrupt handler should finish as soon as possible, it only do the minimal
 *  task it must do (in this case, read scancode and put it to buffer) and leave
 *  the rest of work outside the interrupt handler (in this case, process 
 *  scancode). Besides, because two parts of drivers will access the same data
 *  structure (buffer), readchar() will disable all interrupts when it access 
 *  buffer to avoid interrupt-related concurrency problem.
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
static short front, rear;

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
 *  it in buffer.
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

    void* thr = resume_reading_thr();

    outb(INT_CTL_PORT, INT_ACK_CURRENT);

    if (thr) {
        enable_interrupts();
        context_switch(5, (uint32_t)thr);
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
