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

#ifndef WL_EVENTS_H
#define WL_EVENTS_H

#include "ciolib.h"
#include "bitmap_con.h"

enum wl_local_events {
	WL_LOCAL_SETMODE,
	WL_LOCAL_SETNAME,
	WL_LOCAL_SETTITLE,
	WL_LOCAL_DRAWRECT,
	WL_LOCAL_FLUSH,
	WL_LOCAL_BEEP,
	WL_LOCAL_SETICON,
	WL_LOCAL_SETSCALING,
	WL_LOCAL_SETSCALING_TYPE,
	WL_LOCAL_COPY,
	WL_LOCAL_PASTE,
	WL_LOCAL_MOUSEPOINTER,
};

struct wl_icon_data {
	uint32_t *pixels;	/* ARGB, caller-allocated, event thread frees */
	unsigned long width;	/* square icon: width == height */
};

struct wl_local_event {
	enum wl_local_events type;
	union {
		int mode;
		char name[81];
		char title[81];
		struct rectlist *rect;
		struct wl_icon_data icon;
		double scaling;
		enum ciolib_scaling st;
		enum ciolib_mouse_ptr ptr;
	} data;
};

extern int wl_key_pipe[2];
extern int wl_local_pipe[2];
extern int wl_initialized;
extern bool wl_internal_scaling;
extern sem_t wl_init_complete;
extern sem_t wl_mode_set;
extern sem_t wl_pastebuf_set;
extern sem_t wl_pastebuf_used;
extern char *wl_pastebuf;
extern pthread_mutex_t wl_copybuf_mutex;
extern char *wl_copybuf;

void wl_event_thread(void *args);
void wl_mouse_thread(void *data);
int wl_available_opts(void);
bool wl_has_clipboard(void);

#endif
