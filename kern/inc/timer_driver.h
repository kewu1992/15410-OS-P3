/** @file timer_driver.h
 *  @brief Function prototype for timer_driver.c
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */
#ifndef _TIMER_DRIVER_H_
#define _TIMER_DRIVER_H_

#define APIC_TIMER_IDT_ENTRY 0x22

void init_timer_driver(void* (*tickback)(unsigned int));

void init_lapic_timer_driver();

unsigned int timer_get_ticks();

#endif
