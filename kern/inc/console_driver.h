/** @file console_driver.h
 *  @brief Function prototypes for console_driver.c
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */
#ifndef _CONSOLE_DRIVER_H_
#define _CONSOLE_DRIVER_H_

#include <stdint.h>

void init_console_driver();

void set_hardware_cursor(uint16_t offset);

void scrollup();

int putbyte(char ch);

void putbytes(const char *s, int len);

void draw_char(int row, int col, int ch, int color);

char get_char(int row, int col);

int set_term_color(int color);

void get_term_color(int *color);

int set_cursor(int row, int col);

void get_cursor(int *row, int *col);

void hide_cursor();

void show_cursor();

void clear_console();

#endif
