#ifndef _INIT_IDT_H_
#define _INIT_IDT_H_

int init_IDT(void* (*tickback)(unsigned int));

#endif