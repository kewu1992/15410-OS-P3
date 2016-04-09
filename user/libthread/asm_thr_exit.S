#include <syscall_int.h>

.global asm_thr_exit

asm_thr_exit:
    movl    4(%esp), %ebx       # %ebx = &mutex_arraytcb->inner_lock
    movl    8(%esp), %edi       # %edi = page_remove_info

    # start removing page, should not use stack anymore

    movl    4(%edi), %eax       # %eax = page_remove_info[1] (is_remove of base1 page)
    testl   %eax, %eax          # check if base1 page need to remove
    je      .L1                 # is_remove == 0, don't need to remove
    movl    0(%edi), %esi       # is_remove != 0, remove base1 page
    int     $REMOVE_PAGES_INT   # call remove_page()
  .L1:
    movl    12(%edi), %eax      # %eax = page_remove_info[3] (is_remove of base2 page)
    testl   %eax, %eax          # check if base2 page need to remove
    je      .L2                 # is_remove == 0, don't need to remove
    movl    8(%edi), %esi       # is_remove != 0, remove base2 page
    int     $REMOVE_PAGES_INT   # call remove_page()
  .L2:
    movl    20(%edi), %eax      # %eax = page_remove_info[5] (is_remove of base3 page)
    testl   %eax, %eax          # check if base3 page need to remove
    je      .L3                 # is_remove == 0, don't need to remove
    movl    16(%edi), %esi      # is_remove != 0, remove base3 page
    int     $REMOVE_PAGES_INT   # call remove_page()
  .L3:
    movl    $1, %eax            # %eax = 1
    xchg    (%ebx), %eax        # atomically do mutex_arraytcb->inner_lock = 1
    int     $VANISH_INT         # Syscall of vanish
    ret                         # should never reach here though
