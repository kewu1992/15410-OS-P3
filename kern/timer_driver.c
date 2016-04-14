/** @file timer_driver.c
 *  @brief Contains timer interrupt handler and driver initialization function
 *
 *  This file contains timer interrupt handler and driver initialization 
 *  function.
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug Kernel doesn't try to recovery from the error when a thread is 
 *       stack overflow. It is not easy to do so. The kernel can not just kill
 *       the thread immediately, because the thread may manipulating some 
 *       kernel data structure when it is interrupted. So our kernel will just
 *       panic() and let the developer to handle with this error. To be fair, 
 *       in our implementation of the kernel, it is very unlikely that a thread
 *       will stack overflow. When it happens, it may indicate some bugs.
 */

#include <interrupt_defines.h>
#include <asm.h>
#include <timer_defines.h>
#include <context_switcher.h>
#include <simics.h>
#include <control_block.h>
#include <asm_helper.h>

/** @brief Frequency */
#define FREQ 100

/** @brief Function pointer points to callback function of timer */
static void* (*callback)(unsigned int);

/** @brief Stores the total number of timer interrupts that handler has caught*/
static unsigned int numTicks;

/** @brief Initialize timer device driver.
 *
 *  Calculate interrupt rate, configure timer mode and rate. Set callback
 *  function and set numTicks to zero.
 *         
 *  @param tickback Pointer to clock-tick callback function
 *  @return Void.
 */
void init_timer_driver(void* (*tickback)(unsigned int)) {
    uint16_t interrupt_rate = (int)(TIMER_RATE / FREQ);

    outb(TIMER_MODE_IO_PORT, TIMER_SQUARE_WAVE);
    outb(TIMER_PERIOD_IO_PORT, (uint8_t)interrupt_rate);
    outb(TIMER_PERIOD_IO_PORT, (uint8_t)(interrupt_rate >> 8));

    callback = tickback;
    numTicks = 0;
}

/** @brief Timer interrupt handler
 *
 *  The function is called when a timer interrupt comes in. it will update 
 *  numTicks, invoke callback function and tell PIC the interrupt is processed.
 *  If there is thread that should wake up from sleep(), timer interrupt handler
 *  will resume to that pariticular thread. Otherwise, a normal context switch 
 *  will happen and the scheduler will choose the next thread to run. 
 *
 *  This function will also check if the interruped thread is stack overflow. 
 *  When it does, kernel will panic. 
 *         
 *  @return Void.
 */
void timer_interrupt_handler() {
    tcb_t* next_thr = (tcb_t*)callback(++numTicks);

    outb(INT_CTL_PORT, INT_ACK_CURRENT);

    enable_interrupts();

    if (tcb_is_stack_overflow((void*)asm_get_esp())) {
        panic("thread's kernel stack overflow!");
    }

    if (next_thr == NULL) {
        // no thread should be wakened up, just call normal context switch 
        context_switch(OP_CONTEXT_SWITCH, -1);
    } else {
        // there is a thread that should wake up from sleep(), resume
        // the sleeping thread
        context_switch(OP_RESUME, (uint32_t)next_thr);
    }
}

/** @brief Get the current ticks */
unsigned int timer_get_ticks() {
    return numTicks;
}

