/** @file exception_handler.h 
 *
 *  @brief Contains macro definitions assembly helper function definitions 
 *  for exception handling.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs
 */
#ifndef _EXCEPTION_HANDLER_H_
#define _EXCEPTION_HANDLER_H_

#include <stdint.h>
#include <syscall_inter.h>

/* Eflags bits */
/** @brief Get reserved bits */
#define EFLAGS_GET_RSV(n) ((n) & 0xffc0802a)
/** @brief Reserved bits expected value */
#define EFLAGS_EX_VAL_RSV 2

/** @brief Get IOPL value: I/O Privilege Level */
#define EFLAGS_GET_IOPL(n) (((n) >> 12) & 3)
/** @brief IOPL expected value */
#define EFLAGS_EX_VAL_IOPL 0

/** @brief Get TF value: Trap flag */
#define EFLAGS_GET_TF(n) (((n) >> 8) & 1)
/** @brief TF expected value */
#define EFLAGS_EX_VAL_TF 0

/** @brief Get IF value: Interrupt enable flag */
#define EFLAGS_GET_IF(n) (((n) >> 9) & 1)
/** @brief IF expected value */
#define EFLAGS_EX_VAL_IF 1

/** @brief Get NT value: Nested Task Flag */
#define EFLAGS_GET_NT(n) (((n) >> 14) & 1)
/** @brief NT expected value */
#define EFLAGS_EX_VAL_NT 0

/** @brief Get other values */
#define EFLAGS_GET_OTHER(n) (((n) >> 17) & 0x1f)
/** @brief Other expected value */
#define EFLAGS_EX_VAL_OTHER 0

/** @brief Set up kernel exception handler's stack and return to user 
  *  mode to run swexn hanlder.
  *
  *  @param  eip The swexn handler's address
  *  @param  cs The user cs: SEGSEL_USER_CS
  *  @param  eflags The default initial eflags when the first task loads
  *  @param  esp The return address of the swexn handler
  *  @param  ss The user ss: SEGSEL_USER_DS
  *
  *  @return No return
  */
void asm_ret_swexn_handler(swexn_handler_t eip, uint32_t cs, uint32_t eflags, 
        uint32_t esp, uint32_t ss);

/** @brief Adopt register values in newureg and change to user mode.
  *
  *  @param  newureg The ureg struct that contains the register values to adopt
  *
  *  @return No return
  */
void asm_ret_newureg(ureg_t *newureg);

/** @brief Generic exception handler
 *
 * Whatever exception happens, this exception handler executes first
 * after registers are pushed on the stack.
 *
 * @param exception_type The type of exception
 *
 * @return void
 */
void expcetion_handler(int exception_type);

#endif
