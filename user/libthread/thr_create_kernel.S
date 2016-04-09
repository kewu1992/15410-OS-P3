#include <syscall_int.h>

.global thr_create_kernel
.global thr_ret2exit

thr_create_kernel:
    movl    4(%esp), %ecx       # save addr of func
    movl    8(%esp), %edx       # save value of new esp (stack)
    int     $THREAD_FORK_INT    # fork a new thread
    testl   %eax, %eax          # check if %eax is zero
    je      .L2                 # return value == 0, new thread
    ret                         # return value != 0, original thread
  .L2:              
    movl    %edx, %esp          # 1. set esp to new stack
    int     $GETTID_INT         # 2. get its ktid
    movl    %eax, 4(%esp)       # 3. "push" its ktid to stack
    call    arraytcb_set_ktid   # 4. set its ktid in arraytcb
    addl    $8, %esp            # 5. "pop" index and ktid 
    call    %ecx                # 6. call func
  thr_ret2exit:
    pushl   %eax                # push func's return value as the new param
    call    thr_exit            # call thr_exit if func doesn't call itself

    
