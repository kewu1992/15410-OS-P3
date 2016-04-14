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
 *  It has four instructions: save all registers (using pusha), call interrupt
 *  handler function, restore all registers (using popa) and return from
 *  interrupt.
 *
 *  @return Void
 */
void timer_wrapper();

/** @brief Gettid syscall handler wrapper
 *
 *  @return Void
 */
void gettid_wrapper();

/** @brief Fork syscall handler wrapper
 *
 *  @return Void
 */
void fork_wrapper();

/** @brief Exec syscall handler wrapper
 *
 *  @return Void
 */
void exec_wrapper();

/** @brief Print syscall handler wrapper
 *
 *  @return Void
 */
void print_wrapper();

/** @brief New_pages syscall handler wrapper
 *
 *  @return Void
 */
void new_pages_wrapper();

/** @brief Remove_pages syscall handler wrapper
 *
 *  @return Void
 */
void remove_pages_wrapper();

/** @brief Swexn syscall handler wrapper
 *
 *  @return Void
 */
void swexn_wrapper();

/** @brief Halt syscall handler wrapper
 *
 *  @return Void
 */
void halt_wrapper();

/** @brief Readline syscall handler wrapper
 *
 *  @return Void
 */
void readline_wrapper();

/** @brief Set_term_color syscall handler wrapper
 *
 *  @return Void
 */
void set_term_color_wrapper();

/** @brief Set_cursor_pos syscall handler wrapper
 *
 *  @return Void
 */
void set_cursor_pos_wrapper();

/** @brief Get_cursor_pos syscall handler wrapper
 *
 *  @return Void
 */
void get_cursor_pos_wrapper();

/** @brief Sleep syscall handler wrapper
 *
 *  @return Void
 */
void sleep_wrapper();

/** @brief Get_ticks syscall handler wrapper
 *
 *  @return Void
 */
void get_ticks_wrapper();

/** @brief Vanish syscall handler wrapper
 *
 *  @return Void
 */
void vanish_wrapper();

/** @brief Wait syscall handler wrapper
 *
 *  @return Void
 */
void wait_wrapper();

/** @brief Set_status syscall handler wrapper
 *
 *  @return Void
 */
void set_status_wrapper();

/** @brief Yield syscall handler wrapper
 *
 *  @return Void
 */
void yield_wrapper();

/** @brief Tread_fork syscall handler wrapper
 *
 *  @return Void
 */
void thread_fork_wrapper();

/** @brief Deschedule syscall handler wrapper
 *
 *  @return Void
 */
void deschedule_wrapper();

/** @brief Make_runnable syscall handler wrapper
 *
 *  @return Void
 */
void make_runnable_wrapper();

/** @brief Readfile syscall handler wrapper
 *
 *  @return Void
 */
void readfile_wrapper();

/* Exception wrappers */

/** @brief Devision Error wrapper
 *
 *  @return Void
 */
void de_wrapper();

/** @brief DB exception wrapper
 *
 *  @return Void
 */
void db_wrapper();

/** @brief NMI exception wrapper
 *
 *  @return Void
 */
void nmi_wrapper();

/** @brief BP exception wrapper
 *
 *  @return Void
 */
void bp_wrapper();

/** @brief OF exception wrapper
 *
 *  @return Void
 */
void of_wrapper();

/** @brief BR exception wrapper
 *
 *  @return Void
 */
void br_wrapper();

/** @brief DE exception wrapper
 *
 *  @return Void
 */
void de_wrapper();

/** @brief NM exception wrapper
 *
 *  @return Void
 */
void nm_wrapper();

/** @brief DF exception wrapper
 *
 *  @return Void
 */
void df_wrapper();

/** @brief CSO exception wrapper
 *
 *  @return Void
 */
void cso_wrapper();

/** @brief TS exception wrapper
 *
 *  @return Void
 */
void ts_wrapper();

/** @brief NP exception wrapper
 *
 *  @return Void
 */
void np_wrapper();

/** @brief SS exception wrapper
 *
 *  @return Void
 */
void ss_wrapper();

/** @brief GP exception wrapper
 *
 *  @return Void
 */
void gp_wrapper();

/** @brief PF exception wrapper
 *
 *  @return Void
 */
void pf_wrapper();

/** @brief MF exception wrapper
 *
 *  @return Void
 */
void mf_wrapper();

/** @brief AC exception wrapper
 *
 *  @return Void
 */
void ac_wrapper();

/** @brief MC exception wrapper
 *
 *  @return Void
 */
void mc_wrapper();

/** @brief XF exception wrapper
 *
 *  @return Void
 */
void xf_wrapper ();

#endif
