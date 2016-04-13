/** @file loader.h
 *  @brief Structure definitions and function prototypes for the user process 
 *         loader.
 *
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */


#ifndef _LOADER_H
#define _LOADER_H

#include <stdint.h>
     
uint32_t get_init_eflags();

int getbytes( const char *filename, int offset, int size, char *buf );

void loadFirstTask(const char *filename);

int loadTask(const char *filename, int argc, const char **argv, void** usr_esp, 
                                                            void** my_program);

void load_kernel_stack(void* k_stack_esp, void* u_stack_esp, void* program, 
                                                                  int is_idle);

#endif /* _LOADER_H */
