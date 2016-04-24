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
410TESTS = ack actual_wait cat cho cho_variant cho2 chow ck1 coolness deschedule_hang exec_basic exec_basic_helper exec_nonexist fib fork_bomb fork_exit_bomb fork_wait_bomb fork_test1 fork_wait getpid_test1 halt_test knife loader_test1 loader_test2 make_crash make_crash_helper mem_eat_test mem_permissions merchant minclone_mem new_pages peon print_basic readline_basic register_test remove_pages_test1 remove_pages_test2 slaughter sleep_test1 stack_test1 swexn_basic_test swexn_cookie_monster swexn_dispatch swexn_regs swexn_stands_for_swextensible swexn_uninstall_test wait_getpid wild_test1 work yield_desc_mkrun

###########################################################################
# Test programs you have written which you wish to run
###########################################################################
# A list of the test programs you want compiled in from the user/progs
# directory.
#
STUDENTTESTS = agility_drill beady_test cvar_test cyclone excellent hello_world io_test join_specific_test juggle largetest mandelbrot multitest mutex_test my_cho my_fork my_new_pages my_wild_test1 my_yield_desc_mkrun paraguay param_check racer rwlock_downgrade_read_test sleep_test small_program startle switched_program switzerland test_fork test_readfile test_swexn test_swexn_helper test_yield thr_exit_join vanish_check


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
THREAD_OBJS = arraytcb.o asm_get_ebp.o asm_get_esp.o asm_thr_exit.o asm_xchg.o cond_var.o hashtable.o malloc.o mutex.o panic.o queue.o rwlock.o sem.o thr_create_kernel.o thr_lib_helper.o thr_lib.o


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
SYSCALL_OBJS = deschedule.o exec.o fork.o get_cursor_pos.o get_ticks.o gettid.o halt.o make_runnable.o new_pages.o print.o readfile.o readline.o remove_pages.o set_cursor_pos.o set_status.o set_term_color.o sleep.o swexn.o syscall.o vanish.o wait.o yield.o


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
KERNEL_OBJS = asm_atomic.o asm_context_switch.o asm_helper.o asm_invalidate_tlb.o asm_new_process_iret.o asm_ret_newureg.o asm_ret_swexn_handler.o console_driver.o context_switcher.o control_block.o exception_handler.o handler_wrapper.o hashtable.o init_IDT.o kernel.o keyboard_driver.o loader.o malloc_wrappers.o mutex.o pm.o priority_queue.o scheduler.o seg_tree.o simple_queue.o spinlock.o syscall_consoleio.o syscall_lifecycle.o syscall_memory.o syscall_misc.o syscall_thr_management.o timer_driver.o vm.o ap_kernel.o smp_manager_scheduler.o smp_message.o smp_syscall_lifecycle.o smp_syscall_consoleio.o smp_syscall_thr_management.o

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
