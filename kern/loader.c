/**
 * The 15-410 kernel project.
 * @name loader.c
 *
 * Functions for the loading
 * of user programs from binary 
 * files should be written in
 * this file. The function 
 * elf_load_helper() is provided
 * for your use.
 */
/*@{*/

/* --- Includes --- */
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <exec2obj.h>
#include <loader.h>
#include <elf_410.h>

/* The number of user executables in the table of contents. */
extern const int exec2obj_userapp_count;

/* The table of contents. */
extern const exec2obj_userapp_TOC_entry exec2obj_userapp_TOC[MAX_NUM_APP_ENTRIES];

/* --- Local function prototypes --- */ 


/**
 * Copies data from a file into a buffer.
 *
 * @param filename   the name of the file to copy data from
 * @param offset     the location in the file to begin copying from
 * @param size       the number of bytes to be copied
 * @param buf        the buffer to copy the data into
 *
 * @return returns the number of bytes copied on succes; -1 on failure
 */
int getbytes( const char *filename, int offset, int size, char *buf )
{
    int i;
    for (i = 0; i < exec2obj_userapp_count; i++)
        if (strcmp(filename, exec2obj_userapp_TOC[i].execname) == 0) {
            if (offset + size > exec2obj_userapp_TOC[i].execlen)
                return -1;
            memcpy(buf, exec2obj_userapp_TOC[i].execbytes+offset, size);
            return 0;
        }
    return -1;
}

/*@}*/
