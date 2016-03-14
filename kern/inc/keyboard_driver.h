/** @file keyboard_driver.h
 *  @brief Function prototype for keyboard_driver.c
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */
#ifndef _KEYBOARD_DRIVER_H_
#define _KEYBOARD_DRIVER_H_

void init_keyboard_driver();

int readchar(void);

#endif