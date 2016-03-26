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

void set_status(int status)
{
	lprintf("I can not exit because syscall is not implemented... exit status: %d", status);

	while (1)
		continue;

	return;
}

volatile int placate_the_compiler;
void vanish(void)
{
	int blackhole = 867-5309;

	blackhole ^= blackhole;
	blackhole /= blackhole;
	*(int *) blackhole = blackhole; /* won't get here */
	while (1)
		++placate_the_compiler;
}

int wait(int *status_ptr)
{
	return -1;
}

int yield(int pid)
{
	return -1;
}

int deschedule(int *flag)
{
	return -1;
}

int make_runnable(int pid)
{
	return -1;
}

int sleep(int ticks)
{
	return -1;
}

char getchar(void)
{
	return -1;
}

int readline(int size, char *buf)
{
	return -1;
}

int set_term_color(int color)
{
	return -1;
}

int get_cursor_pos(int *row, int *col)
{
  return -1;
}

int set_cursor_pos(int row, int col)
{
	return -1;
}

void halt(void)
{
	while (1)
		continue;
}

int readfile(char *filename, char *buf, int count, int offset)
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

unsigned int get_ticks()
{
	return 1;
}

void misbehave(int mode)
{
	return;
}
