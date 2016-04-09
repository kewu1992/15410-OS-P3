/*
 *
 *    #####          #######         #######         ######            ###
 *   #     #            #            #     #         #     #           ###
 *   #                  #            #     #         #     #           ###
 *    #####             #            #     #         ######             #
 *         #            #            #     #         #
 *   #     #            #            #     #         #                 ###
 *    #####             #            #######         #                 ###
 *
 *
 *   You should probably NOT EDIT THIS FILE in any way!
 *
 *   You should probably DELETE this file, insert all of your
 *   Project 2 stub files, and edit config.mk accordingly.
 *
 *   Alternatively, you can DELETE pieces from this file as
 *   you write your stubs.  But if you forget half-way through
 *   that that's the plan, you'll have a fun debugging problem!
 *
 */

#include <syscall.h>
#include <simics.h>


char getchar(void)
{
	return -1;
}

void task_vanish(int status)
{
	status ^= status;
	status /= status;
	while (1)
		continue;
}

void misbehave(int mode)
{
	return;
}
