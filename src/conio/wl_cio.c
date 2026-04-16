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
#include "wl_cio.h"
#include "wl_events.h"
#include "wl_dynload.h"

#include "bitmap_con.h"

int wl_key_pipe[2];
int wl_local_pipe[2];
int wl_initialized;
sem_t wl_init_complete;
sem_t wl_mode_set;

int wl_kbwait(int ms)
{
	fd_set rfd;
	struct timeval tv = {
		.tv_sec = ms / 1000,
		.tv_usec = (ms % 1000) * 1000
	};

	FD_ZERO(&rfd);
	FD_SET(wl_key_pipe[0], &rfd);
	return (select(wl_key_pipe[0] + 1, &rfd, NULL, NULL, &tv) == 1);
}

int wl_kbhit(void)
{
	fd_set rfd;
	struct timeval tv;

	memset(&tv, 0, sizeof(tv));
	FD_ZERO(&rfd);
	FD_SET(wl_key_pipe[0], &rfd);
	return (select(wl_key_pipe[0] + 1, &rfd, NULL, NULL, &tv) == 1);
}

int wl_getch(void)
{
	unsigned char ch;

	while (read(wl_key_pipe[0], &ch, 1) != 1)
		;
	return (ch);
}

static void write_event(struct wl_local_event *ev)
{
	size_t sent = 0;
	char *buf = (char *)ev;
	fd_set wfd;

	FD_ZERO(&wfd);
	FD_SET(wl_local_pipe[1], &wfd);

	while (sent < sizeof(*ev)) {
		select(wl_local_pipe[1] + 1, NULL, &wfd, NULL, NULL);
		int rv = write(wl_local_pipe[1], buf + sent, sizeof(*ev) - sent);
		if (rv > 0)
			sent += rv;
	}
}

void wl_textmode(int mode)
{
	struct wl_local_event ev = {0};

	ev.type = WL_LOCAL_SETMODE;
	ev.data.mode = mode;
	write_event(&ev);
	sem_wait(&wl_mode_set);
}

void wl_settitle(const char *title)
{
	struct wl_local_event ev = {0};

	ev.type = WL_LOCAL_SETTITLE;
	SAFECOPY(ev.data.title, title);
	write_event(&ev);
}

void wl_setname(const char *name)
{
	struct wl_local_event ev = {0};

	ev.type = WL_LOCAL_SETNAME;
	SAFECOPY(ev.data.name, name);
	write_event(&ev);
}

void wl_beep(void)
{
	struct wl_local_event ev = {0};

	ev.type = WL_LOCAL_BEEP;
	if (wl_initialized)
		write_event(&ev);
}

void wl_seticon(const void *icon, unsigned long size)
{
	struct wl_local_event ev = {0};
	const uint32_t *src = icon;
	uint32_t *pixels;
	unsigned long i;

	if (!wl_initialized || !src || size == 0)
		return;

	pixels = malloc(size * size * sizeof(uint32_t));
	if (!pixels)
		return;

	/*
	 * ciolib icons are ABGR (A in high byte, B next, G, R in low
	 * byte).  WL_SHM_FORMAT_ARGB8888 expects ARGB, so swap R and B.
	 */
	for (i = 0; i < size * size; i++)
		pixels[i] = (src[i] & 0xff00ff00)
		    | ((src[i] & 0x00ff0000) >> 16)
		    | ((src[i] & 0x000000ff) << 16);

	ev.type = WL_LOCAL_SETICON;
	ev.data.icon.pixels = pixels;
	ev.data.icon.width = size;
	write_event(&ev);
}

void wl_drawrect(struct rectlist *data)
{
	struct wl_local_event ev = {0};

	ev.type = WL_LOCAL_DRAWRECT;
	if (wl_initialized) {
		ev.data.rect = data;
		write_event(&ev);
	}
	else {
		bitmap_drv_free_rect(data);
	}
}

void wl_flush(void)
{
	struct wl_local_event ev = {0};

	ev.type = WL_LOCAL_FLUSH;
	if (wl_initialized)
		write_event(&ev);
}

double wl_getscaling(void)
{
	double ret;

	assert_rwlock_rdlock(&vstatlock);
	ret = bitmap_double_mult_inside(vstat.winwidth, vstat.winheight);
	assert_rwlock_unlock(&vstatlock);
	return ret;
}

void wl_setscaling(double newval)
{
	struct wl_local_event ev = {0};

	if (newval < 1)
		newval = 1;
	ev.type = WL_LOCAL_SETSCALING;
	ev.data.scaling = newval;
	write_event(&ev);
}

enum ciolib_scaling wl_getscaling_type(void)
{
	return wl_internal_scaling ? CIOLIB_SCALING_INTERNAL
	    : CIOLIB_SCALING_EXTERNAL;
}

void wl_setscaling_type(enum ciolib_scaling newval)
{
	struct wl_local_event ev = {0};

	if ((newval == CIOLIB_SCALING_INTERNAL) != wl_internal_scaling) {
		ev.type = WL_LOCAL_SETSCALING_TYPE;
		ev.data.st = newval;
		write_event(&ev);
	}
}

void wl_setwinsize(int w, int h)
{
	double s = bitmap_double_mult_inside(w, h);
	wl_setscaling(s);
}

void wl_copytext(const char *text, size_t buflen)
{
	struct wl_local_event ev = {0};

	assert_pthread_mutex_lock(&wl_copybuf_mutex);
	FREE_AND_NULL(wl_copybuf);
	wl_copybuf = strdup(text);
	int ok = (wl_copybuf != NULL);
	assert_pthread_mutex_unlock(&wl_copybuf_mutex);

	if (ok) {
		ev.type = WL_LOCAL_COPY;
		write_event(&ev);
	}
}

char *wl_getcliptext(void)
{
	struct wl_local_event ev = {0};
	char *ret = NULL;

	ev.type = WL_LOCAL_PASTE;
	write_event(&ev);
	sem_wait(&wl_pastebuf_set);
	if (wl_pastebuf != NULL)
		ret = strdup(wl_pastebuf);
	sem_post(&wl_pastebuf_used);
	return ret;
}

int wl_mousepointer(enum ciolib_mouse_ptr type)
{
	struct wl_local_event ev = {0};

	if (wl_initialized) {
		ev.type = WL_LOCAL_MOUSEPOINTER;
		ev.data.ptr = type;
		write_event(&ev);
		return 1;
	}
	return 0;
}

void wl_mouse_thread(void *data)
{
	uint16_t key = CIO_KEY_MOUSE;

	SetThreadName("WL Mouse");
	while (1) {
		if (mouse_wait())
			write(wl_key_pipe[1], &key, 2);
	}
}

int wl_initciolib(int mode)
{
	if (wl_initialized)
		return 0;

	if (wl_dynload_init())
		return -1;

	if (pipe(wl_local_pipe))
		return -1;

	if (pipe(wl_key_pipe)) {
		close(wl_local_pipe[0]);
		close(wl_local_pipe[1]);
		return -1;
	}

	if (sem_init(&wl_init_complete, 0, 0)) {
		close(wl_local_pipe[0]);
		close(wl_local_pipe[1]);
		close(wl_key_pipe[0]);
		close(wl_key_pipe[1]);
		return -1;
	}

	if (sem_init(&wl_mode_set, 0, 0)) {
		sem_destroy(&wl_init_complete);
		close(wl_local_pipe[0]);
		close(wl_local_pipe[1]);
		close(wl_key_pipe[0]);
		close(wl_key_pipe[1]);
		return -1;
	}

	sem_init(&wl_pastebuf_set, 0, 0);
	sem_init(&wl_pastebuf_used, 0, 0);
	assert_pthread_mutex_init(&wl_copybuf_mutex, NULL);

	_beginthread(wl_event_thread, 1 << 18, (void *)(intptr_t)mode);
	sem_wait(&wl_init_complete);
	if (!wl_initialized) {
		sem_destroy(&wl_init_complete);
		sem_destroy(&wl_mode_set);
		sem_destroy(&wl_pastebuf_set);
		sem_destroy(&wl_pastebuf_used);
		pthread_mutex_destroy(&wl_copybuf_mutex);
		close(wl_local_pipe[0]);
		close(wl_local_pipe[1]);
		close(wl_key_pipe[0]);
		close(wl_key_pipe[1]);
		return -1;
	}
	_beginthread(wl_mouse_thread, 1 << 16, NULL);
	cio_api.options |= wl_available_opts();
	return 0;
}
