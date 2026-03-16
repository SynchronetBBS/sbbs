/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>

#include <threadwrap.h>
#include <genwrap.h>

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#define BITMAP_CIOLIB_DRIVER
#include "ciolib.h"

#undef BOOL
#import <AppKit/NSGraphics.h>
#include "cg_cio.h"

#include "bitmap_con.h"

/* Pipes for keyboard events (from event thread to app thread) */
static int cg_key_pipe[2];

/* Synchronization */
sem_t cg_init_complete;
sem_t cg_mode_set;
sem_t cg_launched_sem;
int cg_initialized;

/* Main thread override state */
static sem_t cg_main_sem;

static struct cg_main_args {
	int argc;
	char **argv;
	int ret;
} cg_ma;

int CIOLIB_main(int, char **);

static void
cg_main_stub(void *argptr)
{
	struct cg_main_args *args = (struct cg_main_args *)argptr;
	args->ret = CIOLIB_main(args->argc, args->argv);
	/*
	 * App logic is done.  Tell the NSApplication event loop to stop
	 * so main() can return the exit code.
	 */
	extern void cg_events_stop(void);
	cg_events_stop();
	sem_post(&cg_main_sem);
}

int
cg_start_app_thread(int argc, char **argv)
{
	cg_ma.argc = argc;
	cg_ma.argv = argv;
	cg_ma.ret = -1;
	sem_init(&cg_main_sem, 0, 0);
	sem_init(&cg_launched_sem, 0, 0);
	_beginthread(cg_main_stub, 0, &cg_ma);

	/* Enter the Cocoa event loop on the main thread.
	 * This blocks until cg_events_stop() is called. */
	extern void cg_run_event_loop(void);
	cg_run_event_loop();

	/* Wait for the app thread to fully finish */
	sem_wait(&cg_main_sem);
	return cg_ma.ret;
}

int cg_kbwait(int ms)
{
	fd_set rfd;
	struct timeval tv = {
		.tv_sec = ms / 1000,
		.tv_usec = (ms % 1000) * 1000
	};

	FD_ZERO(&rfd);
	FD_SET(cg_key_pipe[0], &rfd);
	return (select(cg_key_pipe[0] + 1, &rfd, NULL, NULL, &tv) == 1);
}

int cg_kbhit(void)
{
	fd_set rfd;
	struct timeval tv;

	memset(&tv, 0, sizeof(tv));
	FD_ZERO(&rfd);
	FD_SET(cg_key_pipe[0], &rfd);
	return (select(cg_key_pipe[0] + 1, &rfd, NULL, NULL, &tv) == 1);
}

int cg_getch(void)
{
	unsigned char ch;

	while (read(cg_key_pipe[0], &ch, 1) != 1)
		;
	return (ch);
}

void
cg_send_key(uint16_t key)
{
	unsigned char buf[2];
	size_t len;

	if (key == 0xffff)
		return;
	if (key == 0xe0)
		key = CIO_KEY_LITERAL_E0;
	if (key < 256) {
		buf[0] = (uint8_t)key;
		len = 1;
	}
	else {
		buf[0] = key & 0xff;
		buf[1] = key >> 8;
		len = 2;
	}
	write(cg_key_pipe[1], buf, len);
}

static void
cg_mouse_thread(void *data)
{
	uint16_t key = CIO_KEY_MOUSE;

	SetThreadName("CG Mouse");
	while (1) {
		if (mouse_wait())
			write(cg_key_pipe[1], &key, 2);
	}
}

/* These are implemented in cg_events.m */
extern void cg_events_textmode(int mode);
extern void cg_events_settitle(const char *title);
extern void cg_events_setname(const char *name);
extern void cg_events_seticon(const void *icon, unsigned long size);
extern void cg_events_setscaling(double newval);
extern void cg_events_setscaling_type(enum ciolib_scaling newval);
extern void cg_events_setwinposition(int x, int y);
extern void cg_events_setwinsize(int w, int h);
extern void cg_events_drawrect(struct rectlist *data);
extern void cg_events_flush(void);
extern void cg_events_copytext(const char *text, size_t buflen);
extern char *cg_events_getcliptext(void);
extern int cg_events_get_window_info(int *width, int *height, int *xpos, int *ypos);
extern int cg_events_mousepointer(enum ciolib_mouse_ptr type);
extern double cg_events_getscaling(void);
extern enum ciolib_scaling cg_events_getscaling_type(void);

void cg_textmode(int mode)
{
	cg_events_textmode(mode);
	sem_wait(&cg_mode_set);
}

void cg_settitle(const char *title)
{
	cg_events_settitle(title);
}

void cg_setname(const char *name)
{
	cg_events_setname(name);
}

void cg_beep(void)
{
	NSBeep();
}

void cg_seticon(const void *icon, unsigned long size)
{
	cg_events_seticon(icon, size);
}

void cg_drawrect(struct rectlist *data)
{
	if (cg_initialized)
		cg_events_drawrect(data);
	else
		bitmap_drv_free_rect(data);
}

void cg_flush(void)
{
	if (cg_initialized)
		cg_events_flush();
}

double cg_getscaling(void)
{
	return cg_events_getscaling();
}

void cg_setscaling(double newval)
{
	if (newval < 1)
		newval = 1;
	cg_events_setscaling(newval);
}

enum ciolib_scaling cg_getscaling_type(void)
{
	return cg_events_getscaling_type();
}

void cg_setscaling_type(enum ciolib_scaling newval)
{
	cg_events_setscaling_type(newval);
}

void cg_copytext(const char *text, size_t buflen)
{
	cg_events_copytext(text, buflen);
}

char *cg_getcliptext(void)
{
	return cg_events_getcliptext();
}

int cg_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	return cg_events_get_window_info(width, height, xpos, ypos);
}

int cg_mousepointer(enum ciolib_mouse_ptr type)
{
	if (cg_initialized)
		return cg_events_mousepointer(type);
	return 0;
}

void cg_setwinposition(int x, int y)
{
	cg_events_setwinposition(x, y);
}

void cg_setwinsize(int w, int h)
{
	cg_events_setwinsize(w, h);
}

int cg_initciolib(int mode)
{
	if (cg_initialized)
		return 0;

	if (pipe(cg_key_pipe))
		return -1;

	if (sem_init(&cg_init_complete, 0, 0)) {
		close(cg_key_pipe[0]);
		close(cg_key_pipe[1]);
		return -1;
	}

	if (sem_init(&cg_mode_set, 0, 0)) {
		sem_destroy(&cg_init_complete);
		close(cg_key_pipe[0]);
		close(cg_key_pipe[1]);
		return -1;
	}

	/*
	 * On macOS, all AppKit operations must happen on the main thread.
	 * The main thread is already running the NSApplication event loop
	 * (started by cg_run_event_loop in main()), so we dispatch
	 * initialization to it from here (the app thread).
	 *
	 * Wait for applicationDidFinishLaunching: before creating any
	 * NSWindow — Cocoa asserts if windows are created too early.
	 */
	extern void cg_events_init(int mode);
	sem_wait(&cg_launched_sem);
	cg_events_init(mode);
	sem_wait(&cg_init_complete);

	if (!cg_initialized) {
		sem_destroy(&cg_init_complete);
		sem_destroy(&cg_mode_set);
		close(cg_key_pipe[0]);
		close(cg_key_pipe[1]);
		return -1;
	}

	_beginthread(cg_mouse_thread, 1 << 16, NULL);
	cio_api.options |= CONIO_OPT_SET_TITLE | CONIO_OPT_SET_NAME
	    | CONIO_OPT_SET_ICON | CONIO_OPT_EXTERNAL_SCALING;
	return 0;
}
