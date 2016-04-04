/** @file timer_driver.h
 *  @brief Function prototype for timer_driver.c
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */
#ifndef _TIMER_DRIVER_H_
#define _TIMER_DRIVER_H_

void init_timer_driver(void (*tickback)(unsigned int));

unsigned int timer_get_ticks();

#endif
