#ifndef _EXCEPTION_HANDLER_H_
#define _EXCEPTION_HANDLER_H_

#include <stdint.h>
#include <syscall_inter.h>

/**************** Eflags bits *********************/
// Reserved bits
#define EFLAGS_GET_RSV(n) ((n) & 0xffc0802a)
#define EFLAGS_EX_VAL_RSV 2

// Get IOPL value: I/O Privilege Level
#define EFLAGS_GET_IOPL(n) (((n) >> 12) & 3)
#define EFLAGS_EX_VAL_IOPL 0

// TF value: Trap flag
#define EFLAGS_GET_TF(n) (((n) >> 8) & 1)
#define EFLAGS_EX_VAL_TF 0

// IF value: Interrupt enable flag
#define EFLAGS_GET_IF(n) (((n) >> 9) & 1)
#define EFLAGS_EX_VAL_IF 1

// NT value: Nested Task Flag
#define EFLAGS_GET_NT(n) (((n) >> 14) & 1)
#define EFLAGS_EX_VAL_NT 0

// Other values
#define EFLAGS_GET_OTHER(n) (((n) >> 16) & 0x3f)
#define EFLAGS_EX_VAL_OTHER 0

void asm_ret_swexn_handler(swexn_handler_t eip, uint32_t cs, uint32_t eflags, 
        uint32_t esp, uint32_t ss);
void asm_ret_newureg(ureg_t *newureg);
void expcetion_handler(int exception_type);

#endif
