#include <simics.h>
#include <stdlib.h>
#include <exec2obj.h>
#include <string.h>
#include <vm.h>

/** @brief Halt by calling simics command 
  *
  * @return No return
  */
extern void sim_halt(void);

/** @brief Halt 
  *
  * Disable interrupt and execute halt instruction
  *
  * @return No return
  */
extern void asm_hlt(void);

/** @brief The "." file that contains a list of the files that readfile()
  * can access.
  */
static char *dot_file;

/** @brief Length of dot file */
static int dot_file_length;

/** @brief Halt syscall handler 
  *
  * @return No return
  *
  */
void halt_syscall_handler() {
    sim_halt();

    // if kernel is run on real hardware....
    asm_hlt();
}

/** @brief Init readfile syscall and construct "." file
  * 
  * @return 0 on success; -1 on error
  *
  */
int syscall_readfile_init() {
    dot_file_length = 1;
    int i;
    for (i = 0; i < exec2obj_userapp_count; i++) {
        dot_file_length += strlen(exec2obj_userapp_TOC[i].execname) + 1;
    }

    dot_file = malloc(dot_file_length);
    if (dot_file == NULL)
        return -1;

    int count = 0;
    for (i = 0; i < exec2obj_userapp_count; i++) {
        memcpy(dot_file + count, exec2obj_userapp_TOC[i].execname, 
                strlen(exec2obj_userapp_TOC[i].execname));
        count += strlen(exec2obj_userapp_TOC[i].execname);
        dot_file[count] = '\0';
        count++;
    }
    dot_file[count] = '\0';

    return 0;
}

/** @brief Readfile syscall handler
  *
  * @param filename The RAM disk file to read
  * @param buf The buffer to fill in  
  * @param count Number of bytes to fill in  
  * @param offset The offset from the beginning of the RAM disk file  
  *
  * @return number of bytes stored into the buffer is returned on success; 
  * -1 on error
  *
  */
int readfile_syscall_handler(char* filename, char *buf, int count, int offset)
{

    if (count < 0 || offset < 0)
        return -1;

    // Make sure buf is valid
    int is_check_null = 0;
    int max_len = count;
    int need_writable = 1;
    if(check_mem_validness(buf, max_len, is_check_null, need_writable) < 0) {
        return -1;
    } 

    if (strcmp(filename, ".") == 0) {
        if (offset > dot_file_length)
                return -1;

        if (offset + count > dot_file_length)
            count = dot_file_length - offset;

        memcpy(buf, dot_file+offset, count);
        return count;
    }

    int i;
    for (i = 0; i < exec2obj_userapp_count; i++) {
        if(strcmp(exec2obj_userapp_TOC[i].execname, filename) == 0) {
            if (offset > exec2obj_userapp_TOC[i].execlen)
                return -1;

            if (offset + count > exec2obj_userapp_TOC[i].execlen)
                count = exec2obj_userapp_TOC[i].execlen - offset;

            memcpy(buf, exec2obj_userapp_TOC[i].execbytes+offset, count);
            return count;
        }
    }

    return -1;
}
