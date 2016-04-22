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

#include <apic.h>

#include <timer_driver.h>

#include <smp.h>

/** @brief Frequency */
#define FREQ 100

/** @brief A flag indicating if init_vm has finished */
extern int finished_init_vm;

/** @brief A flag indicating if APIC timer has been calibrated */
extern int finished_cal_apic_timer;

/** @brief The numTicks at the time we start calibrating APIC timer */
static unsigned int start_numTicks;

/** @brief Initial counter value of lapic timer 
  * Initially set it to a large value to for calibrating, set it to the 
  * desired value that can generate interrupts per 10 ms with the
  * corresponding divider value.
  */
static uint32_t lapic_timer_init = 0xffffffff;


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


void init_lapic_timer_driver() {

    uint32_t lapic_lvt_timer = lapic_read(LAPIC_LVT_TIMER);

    // Timer initial count 
    lapic_write(LAPIC_TIMER_INIT, lapic_timer_init);

    // Frequency divider of the timer
    uint32_t lapic_timer_div = LAPIC_X1;
    lapic_write(LAPIC_TIMER_DIV, lapic_timer_div);

    // Set as periodic mode
    lapic_lvt_timer |= LAPIC_PERIODIC;
    // Enable lapic timer interrupt
    lapic_lvt_timer &= ~LAPIC_IMASK;
    // Set idt vector 
    lapic_lvt_timer |= APIC_TIMER_IDT_ENTRY;
    // Write the value back
    lapic_write(LAPIC_LVT_TIMER, lapic_lvt_timer);

}

void apic_timer_interrupt_handler() {

    // Acknowledge interrupt
    apic_eoi();

    enable_interrupts();
    
    context_switch(OP_CONTEXT_SWITCH, -1);

}

void pic_timer_interrupt_handler() {
    
    ++numTicks;

    if(finished_init_vm) {

        if(start_numTicks == 0) {
            start_numTicks = numTicks;

            init_lapic_timer_driver();
        } else if(numTicks == start_numTicks + 10) {
            // The PIC is configured to generate an interrupt every 10ms,
            // So numTicks gets incremented every 10ms
            // Evaluate APIC frequency after 100ms

            uint32_t lapic_timer_cur = lapic_read(LAPIC_TIMER_CUR);

            // Stop lapic timer for the moment
            lapic_write(LAPIC_TIMER_INIT, 0);

            uint32_t diff = 0xffffffff - lapic_timer_cur;

            // Given that the lapic divider value is 1, diff in 100ms divided 
            // by 10 is the desired lapic_timer_init value to generate 
            // interrupts every 10ms
            lapic_timer_init = diff / 10;

            // Disable PIC
            outb(TIMER_MODE_IO_PORT, TIMER_ONE_SHOT);

            // Mark finishing calibrating APIC timer
            finished_cal_apic_timer = 1;
        }

    }

    outb(INT_CTL_PORT, INT_ACK_CURRENT);

    enable_interrupts();
    return;
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
        //context_switch(OP_CONTEXT_SWITCH, -1);
    } else {
        // there is a thread that should wake up from sleep(), resume
        // the sleeping thread
        //context_switch(OP_RESUME, (uint32_t)next_thr);
    }
}

/** @brief Get the current ticks */
unsigned int timer_get_ticks() {
    return numTicks;
}

