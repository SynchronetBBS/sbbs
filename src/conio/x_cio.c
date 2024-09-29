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

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <threadwrap.h>
#include <xp_dl.h>
#include <genwrap.h>

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#define BITMAP_CIOLIB_DRIVER
#include "ciolib.h"
#include "x_cio.h"
#include "x_events.h"

#include "bitmap_con.h"

#include "x_cio.h"

pthread_mutex_t scalinglock;
double newscaling;

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

static void write_event(struct x11_local_event *ev)
{
	size_t sent = 0;
	char *buf = (char *)ev;
	fd_set	wfd;
	int rv;

	FD_ZERO(&wfd);
	FD_SET(local_pipe[1], &wfd);

	while (sent < sizeof(*ev)) {
		select(local_pipe[1]+1, NULL, &wfd, NULL, NULL);
		rv = write(local_pipe[1], buf + sent, sizeof(*ev) - sent);
		if (rv > 0)
			sent += rv;
	}
}

void x_beep(void)
{
	struct x11_local_event ev = {0};

	ev.type=X11_LOCAL_BEEP;
	write_event(&ev);
}

void x_textmode(int mode)
{
	struct x11_local_event ev = {0};

	ev.type=X11_LOCAL_SETMODE;
	ev.data.mode = mode;
	write_event(&ev);
	sem_wait(&mode_set);
}

void x_setname(const char *name)
{
	struct x11_local_event ev = {0};

	ev.type=X11_LOCAL_SETNAME;
	strlcpy(ev.data.name, name, sizeof(ev.data.name));
	write_event(&ev);
}

void x_settitle(const char *title)
{
	struct x11_local_event ev = {0};

	ev.type=X11_LOCAL_SETTITLE;
	strlcpy(ev.data.title, title, sizeof(ev.data.title));
	write_event(&ev);
}

void x_seticon(const void *icon, unsigned long size)
{
	const uint32_t *icon32 = icon;
	struct x11_local_event ev = {0};
	int i;

	ev.data.icon_data = malloc((size*size + 2)*sizeof(ev.data.icon_data[0]));
	if (ev.data.icon_data != NULL) {
		ev.type=X11_LOCAL_SETICON;
		for (i = 0; i < size*size; i++)
			ev.data.icon_data[i + 2] = ((icon32[i] & 0xff000000))|((icon32[i] & 0x00ff0000) >> 16)|((icon32[i] & 0x0000ff00))|((icon32[i] & 0x000000ff)<<16);
		ev.data.icon_data[0] = size;
		ev.data.icon_data[1] = size;
		write_event(&ev);
	}
}

void x_copytext(const char *text, size_t buflen)
{
	struct x11_local_event ev = {0};

	pthread_mutex_lock(&copybuf_mutex);
	FREE_AND_NULL(copybuf);

	copybuf=strdup(text);
	if(copybuf) {
		ev.type=X11_LOCAL_COPY;
		write_event(&ev);
		copybuf_format = XA_STRING;
	}
	pthread_mutex_unlock(&copybuf_mutex);
	return;
}

char *x_getcliptext(void)
{
	char *ret=NULL;
	struct x11_local_event ev = {0};

	ev.type=X11_LOCAL_PASTE;
	write_event(&ev);
	sem_wait(&pastebuf_set);
	if(pastebuf!=NULL)
		ret=strdup(pastebuf);
	sem_post(&pastebuf_used);
	return(ret);
}

/* Mouse event/keyboard thread */
void x11_mouse_thread(void *data)
{
	//uint16_t	key=((CIO_KEY_MOUSE&0xFF)<<8)|((CIO_KEY_MOUSE>>8)&0xFF);
	uint16_t	key=CIO_KEY_MOUSE;

	SetThreadName("X11 Mouse");
	while(1) {
		if(mouse_wait())
			write(key_pipe[1], &key, 2);
	}
}

int x_initciolib(int mode)
{
	dll_handle	dl;
	const char *libnames[]={"X11",NULL};
#ifdef WITH_XRENDER
	dll_handle	dl2;
	const char *libnames2[]={"Xrender",NULL};
#endif
#ifdef WITH_XINERAMA
	dll_handle	dl3;
	const char *libnames3[]={"Xinerama",NULL};
#endif
#ifdef WITH_XRANDR
	dll_handle	dl4;
	const char *libnames4[]={"Xrandr",NULL};
#endif
	Status (*xit)(void);
	int *_Xdebug;

	/* Ensure we haven't already initialized */
	if(x11_initialized)
		return(0);

	pthread_mutex_init(&scalinglock, NULL);

	/* Set up the pipe for local events */
	if(pipe(local_pipe))
		return(-1);

	/* And the keyboard pipe */
	if(pipe(key_pipe))
		return(-1);

	/* Load X11 functions */
	if((dl=xp_dlopen(libnames,RTLD_LAZY,7))==NULL)
		return(-1);
	if ((_Xdebug = xp_dlsym(dl,_Xdebug))!=NULL)
		*_Xdebug=1;
	if((xit=xp_dlsym(dl,XInitThreads))!=NULL)
		xit();
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
	if((x11.XCloseDisplay=xp_dlsym(dl,XCloseDisplay))==NULL) {
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
	if((x11.XFreeGC=xp_dlsym(dl,XFreeGC))==NULL) {
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
	if((x11.XAllocWMHints=xp_dlsym(dl,XAllocWMHints))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetWMProperties=xp_dlsym(dl,XSetWMProperties))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetWMProtocols=xp_dlsym(dl,XSetWMProtocols))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XInternAtom=xp_dlsym(dl,XInternAtom))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XFreeColors=xp_dlsym(dl,XFreeColors))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XGetVisualInfo=xp_dlsym(dl,XGetVisualInfo))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCreateWindow=xp_dlsym(dl,XCreateWindow))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCreateColormap=xp_dlsym(dl,XCreateColormap))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XInstallColormap=xp_dlsym(dl,XInstallColormap))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XAllocClassHint=xp_dlsym(dl,XAllocClassHint))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetForeground=xp_dlsym(dl,XSetForeground))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetLocaleModifiers=xp_dlsym(dl,XSetLocaleModifiers))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XOpenIM=xp_dlsym(dl,XOpenIM))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCreateIC=xp_dlsym(dl,XCreateIC))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XwcLookupString=xp_dlsym(dl,XwcLookupString))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetICFocus=xp_dlsym(dl,XSetICFocus))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XUnsetICFocus=xp_dlsym(dl,XUnsetICFocus))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XFilterEvent=xp_dlsym(dl,XFilterEvent))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XCreateFontCursor=xp_dlsym(dl,XCreateFontCursor))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XDefineCursor=xp_dlsym(dl,XDefineCursor))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XFreeCursor=xp_dlsym(dl,XFreeCursor))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XGetGeometry=xp_dlsym(dl,XGetGeometry))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
#ifndef DefaultDepth
	if((x11.DefaultDepth=xp_dlsym(dl,DefaultDepth))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
#endif
#ifndef DefaultVisual
	if((x11.DefaultVisual=xp_dlsym(dl,DefaultVisual))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
#endif
	if((x11.XGetWMHints=xp_dlsym(dl,XGetWMHints))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XSetWMHints=xp_dlsym(dl,XSetWMHints))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XTranslateCoordinates=xp_dlsym(dl,XTranslateCoordinates))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XChangeWindowAttributes=xp_dlsym(dl,XChangeWindowAttributes))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XConfigureWindow=xp_dlsym(dl,XConfigureWindow))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XMoveWindow=xp_dlsym(dl,XMoveWindow))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XGetWMNormalHints=xp_dlsym(dl,XGetWMNormalHints))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
	if((x11.XMoveResizeWindow=xp_dlsym(dl,XMoveResizeWindow))==NULL) {
		xp_dlclose(dl);
		return(-1);
	}
#ifdef WITH_XRENDER
	xrender_found = true;
	if ((dl2 = xp_dlopen(libnames2,RTLD_LAZY,1)) == NULL)
		xrender_found = false;
	if (xrender_found && ((x11.XRenderFindStandardFormat = xp_dlsym(dl2, XRenderFindStandardFormat)) == NULL)) {
		xp_dlclose(dl2);
		xrender_found = false;
	}
	if (xrender_found && ((x11.XRenderCreatePicture = xp_dlsym(dl2, XRenderCreatePicture)) == NULL)) {
		xp_dlclose(dl2);
		xrender_found = false;
	}
	if (xrender_found && ((x11.XRenderFreePicture = xp_dlsym(dl2, XRenderFreePicture)) == NULL)) {
		xp_dlclose(dl2);
		xrender_found = false;
	}
	if (xrender_found && ((x11.XRenderSetPictureTransform = xp_dlsym(dl2, XRenderSetPictureTransform)) == NULL)) {
		xp_dlclose(dl2);
		xrender_found = false;
	}
	if (xrender_found && ((x11.XRenderComposite = xp_dlsym(dl2, XRenderComposite)) == NULL)) {
		xp_dlclose(dl2);
		xrender_found = false;
	}
	if (xrender_found && ((x11.XRenderFindVisualFormat = xp_dlsym(dl2, XRenderFindVisualFormat)) == NULL)) {
		xp_dlclose(dl2);
		xrender_found = false;
	}
	if (xrender_found && ((x11.XRenderQueryVersion = xp_dlsym(dl2, XRenderQueryVersion)) == NULL)) {
		xp_dlclose(dl2);
		xrender_found = false;
	}
	if (xrender_found && ((x11.XRenderSetPictureFilter = xp_dlsym(dl2, XRenderSetPictureFilter)) == NULL)) {
		xp_dlclose(dl2);
		xrender_found = false;
	}
#endif
#ifdef WITH_XINERAMA
	xinerama_found = true;
	if ((dl3 = xp_dlopen(libnames3,RTLD_LAZY,1)) == NULL)
		xinerama_found = false;
	if (xinerama_found && ((x11.XineramaQueryVersion = xp_dlsym(dl3, XineramaQueryVersion)) == NULL)) {
		xp_dlclose(dl3);
		xinerama_found = false;
	}
	if (xinerama_found && ((x11.XineramaQueryScreens = xp_dlsym(dl3, XineramaQueryScreens)) == NULL)) {
		xp_dlclose(dl3);
		xinerama_found = false;
	}
#endif
#ifdef WITH_XRANDR
	xrandr_found = true;
	if ((dl4 = xp_dlopen(libnames4,RTLD_LAZY,2)) == NULL)
		xrandr_found = false;
	if (xrandr_found && ((x11.XRRQueryVersion = xp_dlsym(dl4, XRRQueryVersion)) == NULL)) {
		xp_dlclose(dl4);
		xrandr_found = false;
	}
	if (xrandr_found && ((x11.XRRGetScreenResources = xp_dlsym(dl4, XRRGetScreenResources)) == NULL)) {
		xp_dlclose(dl4);
		xrandr_found = false;
	}
	if (xrandr_found && ((x11.XRRGetCrtcInfo = xp_dlsym(dl4, XRRGetCrtcInfo)) == NULL)) {
		xp_dlclose(dl4);
		xrandr_found = false;
	}
	if (xrandr_found && ((x11.XRRFreeCrtcInfo = xp_dlsym(dl4, XRRFreeCrtcInfo)) == NULL)) {
		xp_dlclose(dl4);
		xrandr_found = false;
	}
	if (xrandr_found && ((x11.XRRFreeScreenResources = xp_dlsym(dl4, XRRFreeScreenResources)) == NULL)) {
		xp_dlclose(dl4);
		xrandr_found = false;
	}
#endif
	setlocale(LC_ALL, "");
	x11.XSetLocaleModifiers("@im=none");

	if(sem_init(&pastebuf_set, 0, 0)) {
		xp_dlclose(dl);
#ifdef WITH_XRENDER
		if (xrender_found)
			xp_dlclose(dl2);
		xrender_found = false;
#endif
#ifdef WITH_XINERAMA
		if (xinerama_found)
			xp_dlclose(dl3);
		xinerama_found = false;
#endif
#ifdef WITH_XRANDR
		if (xrandr_found)
			xp_dlclose(dl4);
		xrandr_found = false;
#endif
		return(-1);
	}
	if(sem_init(&pastebuf_used, 0, 0)) {
		xp_dlclose(dl);
#ifdef WITH_XRENDER
		if (xrender_found)
			xp_dlclose(dl2);
		xrender_found = false;
#endif
#ifdef WITH_XINERAMA
		if (xinerama_found)
			xp_dlclose(dl3);
		xinerama_found = false;
#endif
#ifdef WITH_XRANDR
		if (xrandr_found)
			xp_dlclose(dl4);
		xrandr_found = false;
#endif
		sem_destroy(&pastebuf_set);
		return(-1);
	}
	if(sem_init(&init_complete, 0, 0)) {
		xp_dlclose(dl);
#ifdef WITH_XRENDER
		if (xrender_found)
			xp_dlclose(dl2);
		xrender_found = false;
#endif
#ifdef WITH_XINERAMA
		if (xinerama_found)
			xp_dlclose(dl3);
		xinerama_found = false;
#endif
#ifdef WITH_XRANDR
		if (xrandr_found)
			xp_dlclose(dl4);
		xrandr_found = false;
#endif
		sem_destroy(&pastebuf_set);
		sem_destroy(&pastebuf_used);
		return(-1);
	}
	if(sem_init(&mode_set, 0, 0)) {
		xp_dlclose(dl);
#ifdef WITH_XRENDER
		if (xrender_found)
			xp_dlclose(dl2);
		xrender_found = false;
#endif
#ifdef WITH_XINERAMA
		if (xinerama_found)
			xp_dlclose(dl3);
		xinerama_found = false;
#endif
#ifdef WITH_XRANDR
		if (xrandr_found)
			xp_dlclose(dl4);
		xrandr_found = false;
#endif
		sem_destroy(&pastebuf_set);
		sem_destroy(&pastebuf_used);
		sem_destroy(&init_complete);
		return(-1);
	}

	if(pthread_mutex_init(&copybuf_mutex, 0)) {
		xp_dlclose(dl);
#ifdef WITH_XRENDER
		if (xrender_found)
			xp_dlclose(dl2);
		xrender_found = false;
#endif
#ifdef WITH_XINERAMA
		if (xinerama_found)
			xp_dlclose(dl3);
		xinerama_found = false;
#endif
#ifdef WITH_XRANDR
		if (xrandr_found)
			xp_dlclose(dl4);
		xrandr_found = false;
#endif
		sem_destroy(&pastebuf_set);
		sem_destroy(&pastebuf_used);
		sem_destroy(&init_complete);
		sem_destroy(&mode_set);
		return(-1);
	}

	_beginthread(x11_event_thread,1<<16,(void *)(intptr_t)mode);
	sem_wait(&init_complete);
	if(!x11_initialized) {
		xp_dlclose(dl);
#ifdef WITH_XRENDER
		if (xrender_found)
			xp_dlclose(dl2);
		xrender_found = false;
#endif
#ifdef WITH_XINERAMA
		if (xinerama_found)
			xp_dlclose(dl3);
		xinerama_found = false;
#endif
#ifdef WITH_XRANDR
		if (xrandr_found)
			xp_dlclose(dl4);
		xrandr_found = false;
#endif
		sem_destroy(&pastebuf_set);
		sem_destroy(&pastebuf_used);
		sem_destroy(&init_complete);
		sem_destroy(&mode_set);
		pthread_mutex_destroy(&copybuf_mutex);
		return(-1);
	}
	_beginthread(x11_mouse_thread,1<<16,NULL);
	cio_api.options |= CONIO_OPT_SET_TITLE | CONIO_OPT_SET_NAME | CONIO_OPT_SET_ICON | CONIO_OPT_EXTERNAL_SCALING;
	return(0);
}

void x11_drawrect(struct rectlist *data)
{
	struct x11_local_event ev = {0};

	ev.type=X11_LOCAL_DRAWRECT;
	if(x11_initialized) {
		ev.data.rect=data;
		write_event(&ev);
	}
	else {
		bitmap_drv_free_rect(data);
	}
}

void x11_flush(void)
{
	struct x11_local_event ev = {0};

	ev.type=X11_LOCAL_FLUSH;
	if(x11_initialized)
		write_event(&ev);
}

void x_setscaling(double newval)
{
	if (newval < 1)
		newval = 1;
	pthread_mutex_lock(&scalinglock);
	newscaling = newval;
	pthread_mutex_unlock(&scalinglock);
}

double x_getscaling(void)
{
	double ret;

	pthread_mutex_lock(&vstatlock);
	ret = vstat.scaling;
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

int x_mousepointer(enum ciolib_mouse_ptr type)
{
	struct x11_local_event ev = {0};

	ev.type=X11_LOCAL_MOUSEPOINTER;
	if(x11_initialized) {
		ev.data.ptr=type;
		write_event(&ev);
		return 1;
	}
	return 0;
}

enum ciolib_scaling
x_getscaling_type(void)
{
	enum ciolib_scaling ret;

	ret = (x_internal_scaling ? CIOLIB_SCALING_INTERNAL : CIOLIB_SCALING_EXTERNAL);
	return ret;
}

void
x_setscaling_type(enum ciolib_scaling newval)
{
	struct x11_local_event ev = {0};

	if ((newval == CIOLIB_SCALING_INTERNAL) != x_internal_scaling) {
		ev.type = X11_LOCAL_SETSCALING_TYPE;
		ev.data.st = newval;
		write_event(&ev);
	}
}
