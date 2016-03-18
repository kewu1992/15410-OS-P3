#ifndef _ASM_HELPER_H_
#define _ASM_HELPER_H_

#include <stdint.h>

uint32_t asm_get_esp();
uint32_t asm_get_ebp();
uint32_t asm_get_cs();

void asm_set_esp_w_ret(uint32_t new_esp);

void asm_popa();
void asm_pusha();
void asm_popf();
void asm_pushf();

int asm_xchg(int *lock_available, int val);

#endif
