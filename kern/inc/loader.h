/* The 15-410 kernel project
 *
 *     loader.h
 *
 * Structure definitions, #defines, and function prototypes
 * for the user process loader.
 */

#ifndef _LOADER_H
#define _LOADER_H
     

/* --- Prototypes --- */

int getbytes( const char *filename, int offset, int size, char *buf );

/*
 * Declare your loader prototypes here.
 */

void loadFirstTask(const char *filename);

void* loadTask(const char *filename, int argc, const char **argv, void** usr_esp);

void load_kernel_stack(void* k_stack_esp, void* u_stack_esp, void* program);

#endif /* _LOADER_H */
