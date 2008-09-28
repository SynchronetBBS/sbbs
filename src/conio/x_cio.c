/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <threadwrap.h>
#include <xp_dl.h>

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "x_cio.h"
#include "x_events.h"

int x_kbhit(void)
{
	fd_set	rfd;
	struct timeval tv;

	memset(&tv, 0, sizeof(tv));
	FD_ZERO(&rfd);
	FD_SET(key_pipe[0], &rfd);
	return(select(key_pipe[0]+1, &rfd, NULL, NULL, &tv)==1);
}

int x_getch(void)
{
	unsigned char ch;

	while(read(key_pipe[0], &ch, 1)!=1);
	return(ch);
}

int x_beep(void)
{
	struct x11_local_event ev;

	ev.type=X11_LOCAL_BEEP;
	while(write(local_pipe[1], &ev, sizeof(ev))==-1);
	return(0);
}

void x_textmode(int mode)
{
	struct x11_local_event ev;

	ev.type=X11_LOCAL_SETMODE;
	ev.data.mode = mode;
	while(write(local_pipe[1], &ev, sizeof(ev))==-1);
	sem_wait(&mode_set);
}

void x_setname(const char *name)
{
	struct x11_local_event ev;

	ev.type=X11_LOCAL_SETNAME;
	SAFECOPY(ev.data.name, name);
	while(write(local_pipe[1], &ev, sizeof(ev))==-1);
}

void x_settitle(const char *title)
{
	struct x11_local_event ev;

	ev.type=X11_LOCAL_SETTITLE;
	SAFECOPY(ev.data.title, title);
	while(write(local_pipe[1], &ev, sizeof(ev))==-1);
}

void x_copytext(const char *text, size_t buflen)
{
	struct x11_local_event ev;

	pthread_mutex_lock(&copybuf_mutex);
	FREE_AND_NULL(copybuf);

	copybuf=strdup(text);
	if(copybuf) {
		ev.type=X11_LOCAL_COPY;
		while(write(local_pipe[1], &ev, sizeof(ev))==-1);
	}
	pthread_mutex_unlock(&copybuf_mutex);
	return;
}

char *x_getcliptext(void)
{
	char *ret=NULL;
	struct x11_local_event ev;

	ev.type=X11_LOCAL_PASTE;
	while(write(local_pipe[1], &ev, sizeof(ev))==-1);
	sem_wait(&pastebuf_set);
	if(pastebuf!=NULL)
		ret=strdup(pastebuf);
	sem_post(&pastebuf_used);
	return(ret);
}

int x_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	if(width)
		*width=x11_window_width;
	if(height)
		*height=x11_window_height;
	if(xpos)
		*xpos=x11_window_xpos;
	if(ypos)
		*ypos=x11_window_ypos;
	
	return(0);
}

int x_init(void)
{
	dll_handle	dl;
	const char *libnames[]={"X11",NULL};

	/* Ensure we haven't already initialized */
	if(x11_initialized)
		return(0);

	/* Set up the pipe for local events */
	if(pipe(local_pipe))
		return(-1);

	/* And the keyboard pipe */
	if(pipe(key_pipe))
		return(-1);

	/* Load X11 functions */
	if((dl=xp_dlopen(libnames,RTLD_LAZY,7))==NULL)
		return(-1);
	if((x11.XChangeGC=xp_dlsym(dl,XChangeGC))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCopyPlane=xp_dlsym(dl,XCopyPlane))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XFillRectangle=xp_dlsym(dl,XFillRectangle))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XDrawPoint=xp_dlsym(dl,XDrawPoint))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XFlush=xp_dlsym(dl,XFlush))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSync=xp_dlsym(dl,XSync))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XBell=xp_dlsym(dl,XBell))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XLookupString=xp_dlsym(dl,XLookupString))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XNextEvent=xp_dlsym(dl,XNextEvent))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XAllocSizeHints=xp_dlsym(dl,XAllocSizeHints))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetWMNormalHints=xp_dlsym(dl,XSetWMNormalHints))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XResizeWindow=xp_dlsym(dl,XResizeWindow))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XMapWindow=xp_dlsym(dl,XMapWindow))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XFree=xp_dlsym(dl,XFree))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XFreePixmap=xp_dlsym(dl,XFreePixmap))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCreatePixmap=xp_dlsym(dl,XCreatePixmap))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCopyArea=xp_dlsym(dl,XCopyArea))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCreateBitmapFromData=xp_dlsym(dl,XCreateBitmapFromData))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XAllocColor=xp_dlsym(dl,XAllocColor))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XOpenDisplay=xp_dlsym(dl,XOpenDisplay))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCreateSimpleWindow=xp_dlsym(dl,XCreateSimpleWindow))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCreateGC=xp_dlsym(dl,XCreateGC))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSelectInput=xp_dlsym(dl,XSelectInput))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XStoreName=xp_dlsym(dl,XStoreName))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XGetSelectionOwner=xp_dlsym(dl,XGetSelectionOwner))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XConvertSelection=xp_dlsym(dl,XConvertSelection))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XGetWindowProperty=xp_dlsym(dl,XGetWindowProperty))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XChangeProperty=xp_dlsym(dl,XChangeProperty))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSendEvent=xp_dlsym(dl,XSendEvent))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XPutImage=xp_dlsym(dl,XPutImage))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
#ifndef XDestroyImage
	if((x11.XDestroyImage=xp_dlsym(dl,XDestroyImage))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
#endif
#ifndef XPutPixel
	if((x11.XPutPixel=xp_dlsym(dl,XPutPixel))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
#endif
	if((x11.XCreateImage=xp_dlsym(dl,XCreateImage))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetSelectionOwner=xp_dlsym(dl,XSetSelectionOwner))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetIconName=xp_dlsym(dl,XSetIconName))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSynchronize=xp_dlsym(dl,XSynchronize))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XGetWindowAttributes=xp_dlsym(dl,XGetWindowAttributes))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}

	if(sem_init(&pastebuf_set, 0, 0))
		return(-1);
	if(sem_init(&pastebuf_used, 0, 0)) {
		sem_destroy(&pastebuf_set);
		return(-1);
	}
	if(sem_init(&init_complete, 0, 0)) {
		sem_destroy(&pastebuf_set);
		sem_destroy(&pastebuf_used);
		return(-1);
	}
	if(sem_init(&mode_set, 0, 0)) {
		sem_destroy(&pastebuf_set);
		sem_destroy(&pastebuf_used);
		sem_destroy(&init_complete);
		return(-1);
	}

	if(pthread_mutex_init(&copybuf_mutex, 0)) {
		sem_destroy(&pastebuf_set);
		sem_destroy(&pastebuf_used);
		sem_destroy(&init_complete);
		sem_destroy(&mode_set);
		return(-1);
	}

	_beginthread(x11_event_thread,1<<16,NULL);
	sem_wait(&init_complete);
	if(!x11_initialized) {
		sem_destroy(&pastebuf_set);
		sem_destroy(&pastebuf_used);
		sem_destroy(&init_complete);
		sem_destroy(&mode_set);
		pthread_mutex_destroy(&copybuf_mutex);
		return(-1);
	}
	return(0);
}

void x11_drawrect(int xoffset,int yoffset,int width,int height,unsigned char *data)
{
	struct x11_local_event ev;

	ev.type=X11_LOCAL_DRAWRECT;
	if(x11_initialized) {
		ev.data.rect.x=xoffset;
		ev.data.rect.y=yoffset;
		ev.data.rect.width=width;
		ev.data.rect.height=height;
		ev.data.rect.data=data;
		while(write(local_pipe[1], &ev, sizeof(ev))==-1);
	}
}

void x11_flush(void)
{
	struct x11_local_event ev;

	ev.type=X11_LOCAL_FLUSH;
	if(x11_initialized)
		while(write(local_pipe[1], &ev, sizeof(ev))==-1);
}
