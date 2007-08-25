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
#ifndef STATIC_LINK
#include <dlfcn.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>

#include <threadwrap.h>

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
    int fd;
    int i;
	void *dl;

	/* Ensure we haven't already initialized */
	if(initialized)
		return(0);

	/* Set up the pipe for local events */
	if(pipe(local_pipe))
		return(-1);

	/* And the keyboard pipe */
	if(pipe(key_pipe))
		return(-1);

	/* Load X11 functions */
#ifdef STATIC_LINK
	x11.XChangeGC=XChangeGC;
	x11.XCopyPlane=XCopyPlane;
	x11.XFillRectangle=XFillRectangle;
	x11.XDrawPoint=XDrawPoint;
	x11.XFlush=XFlush;
	x11.XSync=XSync;
	x11.XBell=XBell;
	x11.XLookupString=XLookupString;
	x11.XNextEvent=XNextEvent;
	x11.XAllocSizeHints=XAllocSizeHints;
	x11.XSetWMNormalHints=XSetWMNormalHints;
	x11.XResizeWindow=XResizeWindow;
	x11.XMapWindow=XMapWindow;
	x11.XFree=XFree;
	x11.XFreePixmap=XFreePixmap;
	x11.XCreatePixmap=XCreatePixmap;
	x11.XCopyArea=XCopyArea;
	x11.XCreateBitmapFromData=XCreateBitmapFromData;
	x11.XAllocColor=XAllocColor;
	x11.XOpenDisplay=XOpenDisplay;
	x11.XCreateSimpleWindow=XCreateSimpleWindow;
	x11.XCreateGC=XCreateGC;
	x11.XSelectInput=XSelectInput;
	x11.XStoreName=XStoreName;
	x11.XGetSelectionOwner=XGetSelectionOwner;
	x11.XConvertSelection=XConvertSelection;
	x11.XGetWindowProperty=XGetWindowProperty;
	x11.XChangeProperty=XChangeProperty;
	x11.XSendEvent=XSendEvent;
	x11.XPutImage=XPutImage;
#ifndef XPutPixel
	x11.XPutPixel=XPutPixel;
#endif
#ifndef XDestroyImage
	x11.XDestroyImage=XDestroyImage;
#endif
	x11.XCreateImage=XCreateImage;
	x11.XSetSelectionOwner=XSetSelectionOwner;
	x11.XSetIconName=XSetIconName;
	x11.XSynchronize=XSynchronize;
	x11.XGetWindowAttributes=XGetWindowAttributes;
#else
#if defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__)
	if((dl=dlopen("/usr/X11R6/lib/libX11.dylib",RTLD_LAZY|RTLD_GLOBAL))==NULL)
#else
	if((dl=dlopen("libX11.so",RTLD_LAZY))==NULL)
#endif
		return(-1);
	if((x11.XChangeGC=dlsym(dl,"XChangeGC"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XCopyPlane=dlsym(dl,"XCopyPlane"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XFillRectangle=dlsym(dl,"XFillRectangle"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XDrawPoint=dlsym(dl,"XDrawPoint"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XFlush=dlsym(dl,"XFlush"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XSync=dlsym(dl,"XSync"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XBell=dlsym(dl,"XBell"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XLookupString=dlsym(dl,"XLookupString"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XNextEvent=dlsym(dl,"XNextEvent"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XAllocSizeHints=dlsym(dl,"XAllocSizeHints"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XSetWMNormalHints=dlsym(dl,"XSetWMNormalHints"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XResizeWindow=dlsym(dl,"XResizeWindow"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XMapWindow=dlsym(dl,"XMapWindow"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XFree=dlsym(dl,"XFree"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XFreePixmap=dlsym(dl,"XFreePixmap"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XCreatePixmap=dlsym(dl,"XCreatePixmap"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XCopyArea=dlsym(dl,"XCopyArea"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XCreateBitmapFromData=dlsym(dl,"XCreateBitmapFromData"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XAllocColor=dlsym(dl,"XAllocColor"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XOpenDisplay=dlsym(dl,"XOpenDisplay"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XCreateSimpleWindow=dlsym(dl,"XCreateSimpleWindow"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XCreateGC=dlsym(dl,"XCreateGC"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XSelectInput=dlsym(dl,"XSelectInput"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XStoreName=dlsym(dl,"XStoreName"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XGetSelectionOwner=dlsym(dl,"XGetSelectionOwner"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XConvertSelection=dlsym(dl,"XConvertSelection"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XGetWindowProperty=dlsym(dl,"XGetWindowProperty"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XChangeProperty=dlsym(dl,"XChangeProperty"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XSendEvent=dlsym(dl,"XSendEvent"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XPutImage=dlsym(dl,"XPutImage"))==NULL) {
		dlclose(dl);
		return(-1);
	}
#ifndef XDestroyImage
	if((x11.XDestroyImage=dlsym(dl,"XDestroyImage"))==NULL) {
		dlclose(dl);
		return(-1);
	}
#endif
#ifndef XPutPixel
	if((x11.XPutPixel=dlsym(dl,"XPutPixel"))==NULL) {
		dlclose(dl);
		return(-1);
	}
#endif
	if((x11.XCreateImage=dlsym(dl,"XCreateImage"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XSetSelectionOwner=dlsym(dl,"XSetSelectionOwner"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XSetIconName=dlsym(dl,"XSetIconName"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XSynchronize=dlsym(dl,"XSynchronize"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((x11.XGetWindowAttributes=dlsym(dl,"XGetWindowAttributes"))==NULL) {
		dlclose(dl);
		return(-1);
	}
#endif

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
	if(!initialized) {
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
	if(initialized) {
		ev.data.rect.x=xoffset;
		ev.data.rect.y=yoffset;
		ev.data.rect.width=width;
		ev.data.rect.height=height;
		ev.data.rect.data=data;
		while(write(local_pipe[1], &ev, sizeof(ev))==-1);
	}
}
