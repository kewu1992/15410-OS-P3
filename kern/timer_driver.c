/** @file timer_driver.c
 *  @brief Contains timer interrupt handler and driver initialization function
 *
 *  This file contains timer interrupt handler and driver initialization 
 *  function.
 *
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <interrupt_defines.h>
#include <asm.h>
#include <timer_defines.h>
#include <context_switcher.h>
#include <simics.h>
#include <control_block.h>

#define NULL 0

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
    uint16_t interrupt_rate = (int)(TIMER_RATE * 0.01);

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
 *         
 *  @return Void.
 */
void timer_interrupt_handler() {
    tcb_t* next_thr = (tcb_t*)callback(++numTicks);

    outb(INT_CTL_PORT, INT_ACK_CURRENT);

    enable_interrupts();

    if (next_thr == NULL)
        context_switch(0, -1);
    else
        context_switch(5, (uint32_t)next_thr);
}

unsigned int timer_get_ticks() {
    return numTicks;
}

