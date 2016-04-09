###########################################################################
#
#    #####          #######         #######         ######            ###
#   #     #            #            #     #         #     #           ###
#   #                  #            #     #         #     #           ###
#    #####             #            #     #         ######             #
#         #            #            #     #         #
#   #     #            #            #     #         #                 ###
#    #####             #            #######         #                 ###
#
#
# Please read the directions in README and in this config.mk carefully.
# Do -N-O-T- just dump things randomly in here until your kernel builds.
# If you do that, you run an excellent chance of turning in something
# which can't be graded.  If you think the build infrastructure is
# somehow restricting you from doing something you need to do, contact
# the course staff--don't just hit it with a hammer and move on.
#
# [Once you've read this message, please edit it out of your config.mk]
# [Once you've read this message, please edit it out of your config.mk]
# [Once you've read this message, please edit it out of your config.mk]
###########################################################################

###########################################################################
# This is the include file for the make file.
# You should have to edit only this file to get things to build.
###########################################################################

###########################################################################
# Tab stops
###########################################################################
# If you use tabstops set to something other than the international
# standard of eight characters, this is your opportunity to inform
# our print scripts.
TABSTOP = 8

###########################################################################
# The method for acquiring project updates.
###########################################################################
# This should be "afs" for any Andrew machine, "web" for non-andrew machines
# and "offline" for machines with no network access.
#
# "offline" is strongly not recommended as you may miss important project
# updates.
#
UPDATE_METHOD = afs

###########################################################################
# WARNING: When we test your code, the two TESTS variables below will be
# blanked.  Your kernel MUST BOOT AND RUN if 410TESTS and STUDENTTESTS
# are blank.  It would be wise for you to test that this works.
###########################################################################

###########################################################################
# Test programs provided by course staff you wish to run
###########################################################################
# A list of the test programs you want compiled in from the 410user/progs
# directory.
#
410TESTS = getpid_test1 loader_test1 loader_test2 exec_basic exec_basic_helper exec_nonexist fork_test1 new_pages remove_pages_test1 remove_pages_test2 fork_wait actual_wait sleep_test1 swexn_basic_test swexn_cookie_monster swexn_dispatch swexn_regs mem_permissions make_crash make_crash_helper cho cho2 wait_getpid wild_test1 print_basic yield_desc_mkrun deschedule_hang cho_variant stack_test1

###########################################################################
# Test programs you have written which you wish to run
###########################################################################
# A list of the test programs you want compiled in from the user/progs
# directory.
#
STUDENTTESTS = small_program switched_program mutex_test coolness peon merchant io_test param_check sleep_test vanish_check fork_exit_bomb fork_wait_bomb test_fork hello_world test_yield test_swexn my_wild_test1 my_cho my_yield_desc_mkrun test_readfile


###########################################################################
# Data files provided by course staff to build into the RAM disk
###########################################################################
# A list of the data files you want built in from the 410user/files
# directory.
#
410FILES =

###########################################################################
# Data files you have created which you wish to build into the RAM disk
###########################################################################
# A list of the data files you want built in from the user/files
# directory.
#
STUDENTFILES = foo

###########################################################################
# Object files for your thread library
###########################################################################
THREAD_OBJS = malloc.o panic.o asm_xchg.o mutex.o queue.o thr_create_kernel.o thr_lib.o thr_lib_helper.o arraytcb.o cond_var.o asm_get_esp.o hashtable.o sem.o rwlock.o asm_thr_exit.o asm_get_ebp.o


# Thread Group Library Support.
#
# Since libthrgrp.a depends on your thread library, the "buildable blank
# P3" we give you can't build libthrgrp.a.  Once you install your thread
# library and fix THREAD_OBJS above, uncomment this line to enable building
# libthrgrp.a:
410USER_LIBS_EARLY += libthrgrp.a

###########################################################################
# Object files for your syscall wrappers
###########################################################################
SYSCALL_OBJS = syscall.o gettid.o fork.o exec.o new_pages.o remove_pages.o print.o halt.o readline.o vanish.o wait.o set_cursor_pos.o set_term_color.o set_status.o sleep.o get_ticks.o yield.o swexn.o deschedule.o make_runnable.o readfile.o get_cursor_pos.o


###########################################################################
# Object files for your automatic stack handling
###########################################################################
AUTOSTACK_OBJS = autostack.o

###########################################################################
# Parts of your kernel
###########################################################################
#
# Kernel object files you want included from 410kern/
#
410KERNEL_OBJS = load_helper.o
#
# Kernel object files you provide in from kern/
#
KERNEL_OBJS = console_driver.o kernel.o loader.o malloc_wrappers.o asm_helper.o asm_new_process_iret.o init_IDT.o handler_wrapper.o vm.o control_block.o context_switcher.o scheduler.o queue.o keyboard_driver.o timer_driver.o asm_invalidate_tlb.o asm_context_switch.o syscall_thr_management.o syscall_lifecycle.o pm.o list.o asm_atomic.o spinlock.o mutex.o syscall_consoleio.o syscall_memory.o simple_queue.o syscall_misc.o hashtable.o priority_queue.o exception_handler.o asm_ret_swexn_handler.o asm_ret_newureg.o

###########################################################################
# WARNING: Do not put **test** programs into the REQPROGS variables.  Your
#          kernel will probably not build in the test harness and you will
#          lose points.
###########################################################################

###########################################################################
# Mandatory programs whose source is provided by course staff
###########################################################################
# A list of the programs in 410user/progs which are provided in source
# form and NECESSARY FOR THE KERNEL TO RUN.
#
# The shell is a really good thing to keep here.  Don't delete idle
# or init unless you are writing your own, and don't do that unless
# you have a really good reason to do so.
#
410REQPROGS = idle init shell

###########################################################################
# Mandatory programs whose source is provided by you
###########################################################################
# A list of the programs in user/progs which are provided in source
# form and NECESSARY FOR THE KERNEL TO RUN.
#
# Leave this blank unless you are writing custom init/idle/shell programs
# (not generally recommended).  If you use STUDENTREQPROGS so you can
# temporarily run a special debugging version of init/idle/shell, you
# need to be very sure you blank STUDENTREQPROGS before turning your
# kernel in, or else your tweaked version will run and the test harness
# won't.
#
STUDENTREQPROGS =
