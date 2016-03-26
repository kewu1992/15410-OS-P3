#ifndef _CONTEXT_SWITCHER_H_
#define _CONTEXT_SWITCHER_H_

#include <stdint.h>

void context_switch(int op, uint32_t arg);

void context_switch_set_esp0(int offset, uint32_t esp);

#endif