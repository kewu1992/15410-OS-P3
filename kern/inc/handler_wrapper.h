/** @file handler_wrapper.h
 *  @brief Function prototypes for interrupt handler wrappers
 *
 *  Function prototypes for keyboard interrupt handler wrapper and timer
 *  interrupt handler wrapper which are written by assembly. The assembly
 *  wrappers are necessary so that interrupt handler can save all the 
 *  general purpose registers before executing handler code, and after 
 *  that, resume all registers so that the user code looks same as before
 *  interrupt happens. 
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#ifndef _HANDLER_WRAPPER_H_
#define _HANDLER_WRAPPER_H_

/** @brief Keyboard interrupt handler wrapper
 *  
 *  It has frou instructions: save all registers (using pusha), call interrupt
 *  handler function, restore all registers (using popa) and return from
 *  interrupt.
 *
 *  @return Void
 */
void keyboard_wrapper();

/** @brief Timer interrupt handler wrapper
 *  
 *  It has frou instructions: save all registers (using pusha), call interrupt
 *  handler function, restore all registers (using popa) and return from
 *  interrupt.
 *
 *  @return Void
 */
void timer_wrapper();

void gettid_wrapper();

void fork_wrapper();

void exec_wrapper();

void print_wrapper();

void new_pages_wrapper();

void remove_pages_wrapper();

void swexn_wrapper();

void halt_wrapper();

void readline_wrapper();

void set_term_color_wrapper();

void set_cursor_pos_wrapper();

void sleep_wrapper();

void get_ticks_wrapper();

void vanish_wrapper();

void wait_wrapper();

void set_status_wrapper();

void yield_wrapper();

// Exceptions
void exception_division_error_wrapper();
void exception_page_fault_wrapper();


#endif
