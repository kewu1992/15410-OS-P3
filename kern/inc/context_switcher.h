#ifndef _CONTEXT_SWITCHER_H_
#define _CONTEXT_SWITCHER_H_

#include <stdint.h>

void context_switch(int mode);

void context_switch_load();

void context_switch_set_esp0(int offset, uint32_t esp);

#endif