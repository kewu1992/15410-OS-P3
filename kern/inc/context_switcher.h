#ifndef _CONTEXT_SWITCHER_H_
#define _CONTEXT_SWITCHER_H_

void context_switch();
void context_switch_load(const char *filename);
void context_switch_set_esp0(int offset, uint32_t esp);

#endif