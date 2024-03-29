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
#include <syscall_inter.h>

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

/** @brief The total number of lapic timer interrupts that handler has caught 
 * on different cores.
 */
static unsigned int *apic_num_ticks[MAX_CPUS];

/** @brief The total number of PIC timer interrupts that handler has caught */
static unsigned int numTicks;

/** @brief Initialize timer device driver.
 *
 *  Calculate interrupt rate, configure timer mode and rate. Set callback
 *  function and set numTicks to zero.
 *         
 *  @return Void.
 */
void init_timer_driver() {
    uint16_t interrupt_rate = (int)(TIMER_RATE / FREQ);

    outb(TIMER_MODE_IO_PORT, TIMER_SQUARE_WAVE);
    outb(TIMER_PERIOD_IO_PORT, (uint8_t)interrupt_rate);
    outb(TIMER_PERIOD_IO_PORT, (uint8_t)(interrupt_rate >> 8));

    numTicks = 0;
}


/** @brief Initialize lapic timer driver
 *
 *  @return Void.
 */
void init_lapic_timer_driver() {

    int cur_cpu = smp_get_cpu();

    apic_num_ticks[cur_cpu] = malloc(sizeof(unsigned int));
    if(apic_num_ticks[cur_cpu] == NULL) {
        panic("init_lapic_timer_driver failed");
    }

    *apic_num_ticks[cur_cpu] = 0;

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

/** @brief APIC timer interrupt handler
 *
 *  The function is called when a APIC timer interrupt comes in. it will update
 *  apic_num_ticks, invoke callback function and tell APIC the interrupt is 
 *  processed. If there is thread that should wake up from sleep(), timer 
 *  interrupt handler will resume to that pariticular thread. Otherwise, a 
 *  normal context switch will happen and the scheduler will choose the next 
 *  thread to run. 
 *
 *  This function will also check if the interruped thread is stack overflow. 
 *  When it does, kernel will panic. 
 *         
 *  @return Void.
 */
void apic_timer_interrupt_handler() {

    // Update ticks
    int cur_cpu = smp_get_cpu();
    int ticks = ++(*apic_num_ticks[cur_cpu]);

    tcb_t* next_thr = (tcb_t*)timer_callback(ticks);

    // Acknowledge interrupt
    apic_eoi();

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

/** @brief PIC timer interrupt handler
 *
 *  Only used once to calibrate APIC timer and then be disabled.
 *
 *  @return void
 */
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
            free(apic_num_ticks[0]);
            apic_num_ticks[0] = NULL;

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

/** @brief Get the current APIC timer ticks 
  *
  * @return Current APIC timer ticks
  *
  */
unsigned int timer_get_ticks() {

    int cur_cpu = smp_get_cpu();
    return *apic_num_ticks[cur_cpu];
}

