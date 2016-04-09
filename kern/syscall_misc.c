#include <simics.h>
#include <stdlib.h>
#include <exec2obj.h>
#include <string.h>
#include <vm.h>

extern void sim_halt(void);

static char *dot_file;
static int dot_file_length;

void halt_syscall_handler() {
    sim_halt();

    // if kernel is run on real hardware....
    panic("kernel is halt!");
}

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
        memcpy(dot_file + count, exec2obj_userapp_TOC[i].execname, strlen(exec2obj_userapp_TOC[i].execname));
        count += strlen(exec2obj_userapp_TOC[i].execname);
        dot_file[count] = '\0';
        count++;
    }
    dot_file[count] = '\0';

    return 0;
}

int readfile_syscall_handler(char* filename, char *buf, int count, int offset) {

    if (count < 0 || offset < 0)
        return -1;

    // Make sure buf is valid
    int is_check_null = 0;
    int max_len = count;
    int need_writable = 1;
    if(!is_mem_valid(buf, max_len, is_check_null, need_writable)) {
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