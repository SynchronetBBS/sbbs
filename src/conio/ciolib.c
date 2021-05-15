/* $Id: ciolib.c,v 1.204 2020/07/18 18:15:28 rswindell Exp $ */

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

/* Icon file! */
#ifdef __BORLANDC__
#pragma resource "ciolib.res"		/* DO NOT REMOVE THE EXTRA \r! */
#endif

#include <stdarg.h>
#include <stdlib.h>	/* alloca */
#include <stdio.h>
#if defined(_WIN32)
 #include <malloc.h>	/* alloca() on Win32 */
#endif

#include <threadwrap.h>
#include <genwrap.h>
#include <xpbeep.h>

#define CIOLIB_NO_MACROS
#include "ciolib.h"

#if defined(WITH_SDL)
 #include "sdl_con.h"
 #include "sdlfuncs.h"
#endif
#ifdef _WIN32
 #include "win32cio.h"
#else
 #ifndef NO_X
  #include "x_cio.h"
 #endif
 #include "curs_cio.h"
 #undef getch
#endif

#include "bitmap_con.h"
#include "ansi_cio.h"
#include "syncicon64.h"

CIOLIBEXPORT cioapi_t	cio_api;

static const int tabs[]={1,9,17,25,33,41,49,57,65,73,81,89,97,105,113,121,129,137,145};
static int ungotch;
struct text_info cio_textinfo;
uint32_t ciolib_fg;
uint32_t ciolib_bg;
static int lastmode=C80;
CIOLIBEXPORT int _wscroll=1;
CIOLIBEXPORT int directvideo=0;
CIOLIBEXPORT int hold_update=0;
CIOLIBEXPORT int puttext_can_move=0;
CIOLIBEXPORT int ciolib_reaper=TRUE;
CIOLIBEXPORT char *ciolib_appname=NULL;
CIOLIBEXPORT int ciolib_initial_window_height = -1;
CIOLIBEXPORT int ciolib_initial_window_width = -1;
static int initialized=0;

CIOLIBEXPORT int ciolib_movetext(int sx, int sy, int ex, int ey, int dx, int dy);
CIOLIBEXPORT char * ciolib_cgets(char *str);
CIOLIBEXPORT int ciolib_cscanf (char *format , ...);
CIOLIBEXPORT int ciolib_kbhit(void);
CIOLIBEXPORT int ciolib_getch(void);
CIOLIBEXPORT int ciolib_getche(void);
CIOLIBEXPORT int ciolib_ungetch(int ch);
CIOLIBEXPORT void ciolib_gettextinfo(struct text_info *info);
CIOLIBEXPORT int ciolib_wherex(void);
CIOLIBEXPORT int ciolib_wherey(void);
CIOLIBEXPORT void ciolib_wscroll(void);
CIOLIBEXPORT void ciolib_gotoxy(int x, int y);
CIOLIBEXPORT void ciolib_clreol(void);
CIOLIBEXPORT void ciolib_clrscr(void);
CIOLIBEXPORT int ciolib_cputs(const char *str);
CIOLIBEXPORT int	ciolib_cprintf(const char *fmat, ...);
CIOLIBEXPORT void ciolib_textbackground(int colour);
CIOLIBEXPORT void ciolib_textcolor(int colour);
CIOLIBEXPORT void ciolib_highvideo(void);
CIOLIBEXPORT void ciolib_lowvideo(void);
CIOLIBEXPORT void ciolib_normvideo(void);
CIOLIBEXPORT int ciolib_puttext(int a,int b,int c,int d,void *e);
CIOLIBEXPORT int ciolib_vmem_puttext(int a,int b,int c,int d,struct vmem_cell *e);
CIOLIBEXPORT int ciolib_gettext(int a,int b,int c,int d,void *e);
CIOLIBEXPORT int ciolib_vmem_gettext(int a,int b,int c,int d,struct vmem_cell *e);
CIOLIBEXPORT void ciolib_textattr(int a);
CIOLIBEXPORT void ciolib_delay(long a);
CIOLIBEXPORT int ciolib_putch(int a);
CIOLIBEXPORT void ciolib_setcursortype(int a);
CIOLIBEXPORT void ciolib_textmode(int mode);
CIOLIBEXPORT void ciolib_window(int sx, int sy, int ex, int ey);
CIOLIBEXPORT void ciolib_delline(void);
CIOLIBEXPORT void ciolib_insline(void);
CIOLIBEXPORT char * ciolib_getpass(const char *prompt);
CIOLIBEXPORT void ciolib_copytext(const char *text, size_t buflen);
CIOLIBEXPORT char * ciolib_getcliptext(void);
CIOLIBEXPORT int ciolib_get_window_info(int *width, int *height, int *xpos, int *ypos);
CIOLIBEXPORT void ciolib_setscaling(int new_value);
CIOLIBEXPORT int ciolib_getscaling(void);
CIOLIBEXPORT int ciolib_setpalette(uint32_t entry, uint16_t r, uint16_t g, uint16_t b);
CIOLIBEXPORT int ciolib_attr2palette(uint8_t attr, uint32_t *fg, uint32_t *bg);
CIOLIBEXPORT int ciolib_setpixel(uint32_t x, uint32_t y, uint32_t colour);
CIOLIBEXPORT struct ciolib_pixels * ciolib_getpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, int force);
CIOLIBEXPORT int ciolib_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, struct ciolib_pixels *pixels, void *mask);
CIOLIBEXPORT void ciolib_freepixels(struct ciolib_pixels *pixels);
CIOLIBEXPORT struct ciolib_screen * ciolib_savescreen(void);
CIOLIBEXPORT void ciolib_freescreen(struct ciolib_screen *);
CIOLIBEXPORT int ciolib_restorescreen(struct ciolib_screen *scrn);
CIOLIBEXPORT void ciolib_setcolour(uint32_t fg, uint32_t bg);
CIOLIBEXPORT int ciolib_get_modepalette(uint32_t p[16]);
CIOLIBEXPORT int ciolib_set_modepalette(uint32_t p[16]);
CIOLIBEXPORT void ciolib_set_vmem(struct vmem_cell *cell, uint8_t ch, uint8_t attr, uint8_t font);
CIOLIBEXPORT void ciolib_set_vmem_attr(struct vmem_cell *cell, uint8_t attr);
CIOLIBEXPORT void ciolib_setwinsize(int width, int height);
CIOLIBEXPORT void ciolib_setwinposition(int x, int y);
CIOLIBEXPORT enum ciolib_codepage ciolib_getcodepage(void);

#if defined(WITH_SDL)
int sdl_video_initialized = 0;
#endif

#define CIOLIB_INIT()		{ if(initialized != 1) initciolib(CIOLIB_MODE_AUTO); }

#if defined(WITH_SDL)
static int try_sdl_init(int mode)
{
	if(!sdl_initciolib(mode)) {
		cio_api.mouse=1;
		cio_api.puttext=bitmap_puttext;
		cio_api.vmem_puttext=bitmap_vmem_puttext;
		cio_api.vmem_gettext=bitmap_vmem_gettext;
		cio_api.gotoxy=bitmap_gotoxy;
		cio_api.setcursortype=bitmap_setcursortype;
		cio_api.setfont=bitmap_setfont;
		cio_api.getfont=bitmap_getfont;
		cio_api.loadfont=bitmap_loadfont;
		cio_api.movetext=bitmap_movetext;
		cio_api.clreol=bitmap_clreol;
		cio_api.clrscr=bitmap_clrscr;
		cio_api.getcustomcursor=bitmap_getcustomcursor;
		cio_api.setcustomcursor=bitmap_setcustomcursor;
		cio_api.getvideoflags=bitmap_getvideoflags;
		cio_api.setvideoflags=bitmap_setvideoflags;

		cio_api.kbhit=sdl_kbhit;
		cio_api.getch=sdl_getch;
		cio_api.textmode=sdl_textmode;
		cio_api.showmouse=sdl_showmouse;
		cio_api.hidemouse=sdl_hidemouse;
		cio_api.setname=sdl_setname;
		cio_api.seticon=sdl_seticon;
		cio_api.settitle=sdl_settitle;
		cio_api.copytext=sdl_copytext;
		cio_api.getcliptext=sdl_getcliptext;
		cio_api.get_window_info=sdl_get_window_info;
		cio_api.setwinsize=sdl_setwinsize;
		cio_api.setwinposition=sdl_setwinposition;
		cio_api.setpalette=bitmap_setpalette;
		cio_api.attr2palette=bitmap_attr2palette;
		cio_api.setpixel=bitmap_setpixel;
		cio_api.getpixels=bitmap_getpixels;
		cio_api.setpixels=bitmap_setpixels;
		cio_api.get_modepalette=bitmap_get_modepalette;
		cio_api.set_modepalette=bitmap_set_modepalette;
		cio_api.map_rgb = bitmap_map_rgb;
		cio_api.replace_font = bitmap_replace_font;
		cio_api.beep = sdl_beep;
		cio_api.mousepointer=sdl_mousepointer;
		return(1);
	}
	return(0);
}
#endif

#ifndef NO_X
static int try_x_init(int mode)
{
#if defined(WITH_SDL)
	if (sdl_video_initialized) {
		sdl.QuitSubSystem(SDL_INIT_VIDEO);
	}
#endif

	if(!x_init()) {
		cio_api.mode=CIOLIB_MODE_X;
		cio_api.mouse=1;
		cio_api.puttext=bitmap_puttext;
		cio_api.vmem_puttext=bitmap_vmem_puttext;
		cio_api.vmem_gettext=bitmap_vmem_gettext;
		cio_api.gotoxy=bitmap_gotoxy;
		cio_api.setcursortype=bitmap_setcursortype;
		cio_api.setfont=bitmap_setfont;
		cio_api.getfont=bitmap_getfont;
		cio_api.loadfont=bitmap_loadfont;
		cio_api.beep=x_beep;
		cio_api.movetext=bitmap_movetext;
		cio_api.clreol=bitmap_clreol;
		cio_api.clrscr=bitmap_clrscr;
		cio_api.getcustomcursor=bitmap_getcustomcursor;
		cio_api.setcustomcursor=bitmap_setcustomcursor;
		cio_api.getvideoflags=bitmap_getvideoflags;
		cio_api.setvideoflags=bitmap_setvideoflags;

		cio_api.kbhit=x_kbhit;
		cio_api.getch=x_getch;
		cio_api.textmode=x_textmode;
		cio_api.setname=x_setname;
		cio_api.settitle=x_settitle;
		cio_api.copytext=x_copytext;
		cio_api.getcliptext=x_getcliptext;
		cio_api.get_window_info=x_get_window_info;
		cio_api.setscaling=x_setscaling;
		cio_api.getscaling=x_getscaling;
		cio_api.seticon=x_seticon;
		cio_api.setpalette=bitmap_setpalette;
		cio_api.attr2palette=bitmap_attr2palette;
		cio_api.setpixel=bitmap_setpixel;
		cio_api.getpixels=bitmap_getpixels;
		cio_api.setpixels=bitmap_setpixels;
		cio_api.get_modepalette=bitmap_get_modepalette;
		cio_api.set_modepalette=bitmap_set_modepalette;
		cio_api.map_rgb = bitmap_map_rgb;
		cio_api.replace_font = bitmap_replace_font;
		cio_api.mousepointer=x_mousepointer;
		return(1);
	}
	return(0);
}
#endif

#ifndef _WIN32
static int try_curses_init(int mode)
{
#if defined(WITH_SDL)
	if (sdl_video_initialized) {
		sdl.QuitSubSystem(SDL_INIT_VIDEO);
	}
#endif

	if(curs_initciolib(mode)) {
		if(mode==CIOLIB_MODE_AUTO)
			mode=CIOLIB_MODE_CURSES;
		cio_api.mode=mode;
		cio_api.puttext=curs_puttext;
		cio_api.vmem_puttext=curs_vmem_puttext;
		cio_api.vmem_gettext=curs_vmem_gettext;
		cio_api.textattr=curs_textattr;
		cio_api.kbhit=curs_kbhit;
		cio_api.gotoxy=curs_gotoxy;
		cio_api.setcursortype=curs_setcursortype;
		cio_api.getch=curs_getch;
		cio_api.textmode=curs_textmode;
		cio_api.showmouse=curs_showmouse;
		cio_api.hidemouse=curs_hidemouse;
		cio_api.suspend=curs_suspend;
		cio_api.resume=curs_resume;
		cio_api.beep=curs_beep;
		cio_api.setvideoflags=curs_setvideoflags;
		cio_api.getvideoflags=curs_getvideoflags;
#if defined(NCURSES_VERSION_MAJOR) || defined (__NetBSD__)
		cio_api.escdelay=&ESCDELAY;
#endif
		cio_api.setfont = curs_setfont;
		cio_api.getfont = curs_getfont;
		cio_api.setpalette = curs_setpalette;
		cio_api.get_modepalette = curs_get_modepalette;
		cio_api.set_modepalette = curs_set_modepalette;
		cio_api.attr2palette = curs_attr2palette;
		return(1);
	}
	return(0);
}
#endif

static int try_ansi_init(int mode)
{
#if defined(WITH_SDL)
	if (sdl_video_initialized) {
		sdl.QuitSubSystem(SDL_INIT_VIDEO);
	}
#endif

	if(ansi_initciolib(mode)) {
		cio_api.mode=CIOLIB_MODE_ANSI;
		cio_api.mouse=0;
		cio_api.puttext=ansi_puttext;
		cio_api.gettext=ansi_gettext;
		cio_api.textattr=ansi_textattr;
		cio_api.kbhit=ansi_kbhit;
		cio_api.gotoxy=ansi_gotoxy;
		cio_api.setcursortype=ansi_setcursortype;
		cio_api.getch=ansi_getch;
		cio_api.textmode=ansi_textmode;
		cio_api.escdelay=&CIOLIB_ANSI_TIMEOUT;
		cio_api.beep=ansi_beep;
		cio_api.suspend=ansi_suspend;
		return(1);
	}
	return(0);
}

#ifdef _WIN32
#if defined(__BORLANDC__)
        #pragma argsused
#endif
static int try_conio_init(int mode)
{
#if defined(WITH_SDL)
	if (sdl_video_initialized) {
		sdl.QuitSubSystem(SDL_INIT_VIDEO);
	}
#endif

	/* This should test for something or other */
	if(win32_initciolib(mode)) {
		if(mode==CIOLIB_MODE_AUTO)
			cio_api.mode=CIOLIB_MODE_CONIO;
		else
			cio_api.mode=mode;	/* CIOLIB_MODE_CONIO or CIOLIB_MODE_CONIO_FULLSCREEN */
		cio_api.mouse=1;
		cio_api.puttext=win32_puttext;
		cio_api.gettext=win32_gettext;
		cio_api.kbhit=win32_kbhit;
		cio_api.gotoxy=win32_gotoxy;
		cio_api.setcursortype=win32_setcursortype;
		cio_api.getch=win32_getch;
		cio_api.textmode=win32_textmode;
		cio_api.showmouse=win32_showmouse;
		cio_api.hidemouse=win32_hidemouse;
		cio_api.setname=win32_settitle;
		cio_api.settitle=win32_settitle;
		cio_api.copytext=win32_copytext;
		cio_api.getcliptext=win32_getcliptext;
		cio_api.suspend=win32_suspend;
		cio_api.resume=win32_resume;
		cio_api.getcustomcursor=win32_getcustomcursor;
		cio_api.setcustomcursor=win32_setcustomcursor;
		cio_api.getvideoflags=win32_getvideoflags;
		cio_api.setpalette=win32_setpalette;
		return(1);
	}
	return(0);
}
#endif

/* Optional */
CIOLIBEXPORT void suspendciolib(void)
{
	ciolib_clrscr();
	if(cio_api.suspend != NULL)
		cio_api.suspend();
	initialized=-1;
}

CIOLIBEXPORT int initciolib(int mode)
{
	switch(initialized) {
		case 1:
			return(0);
		case -1:
			initialized=1;
			if(cio_api.resume != NULL)
				cio_api.resume();
			ciolib_clrscr();
			return(0);
	}

	memset(&cio_api,0,sizeof(cio_api));

	switch(mode) {
		case CIOLIB_MODE_AUTO:
#ifndef NO_X
			if(!try_x_init(mode))
#endif
#if defined(WITH_SDL)
				if(!try_sdl_init(CIOLIB_MODE_SDL))
#endif
#ifdef _WIN32
					if(!try_conio_init(mode))
#else
					if(!try_curses_init(mode))
#endif
						try_ansi_init(mode);
			break;
#ifdef _WIN32
		case CIOLIB_MODE_CONIO:
		case CIOLIB_MODE_CONIO_FULLSCREEN:
			try_conio_init(mode);
			break;
#else
		case CIOLIB_MODE_CURSES:
		case CIOLIB_MODE_CURSES_IBM:
		case CIOLIB_MODE_CURSES_ASCII:
			try_curses_init(mode);
			break;

		case CIOLIB_MODE_X:
#ifndef NO_X
			try_x_init(mode);
#endif
			break;
#endif
		case CIOLIB_MODE_ANSI:
			try_ansi_init(mode);
			break;

#if defined(WITH_SDL)
		case CIOLIB_MODE_SDL:
		case CIOLIB_MODE_SDL_FULLSCREEN:
			try_sdl_init(mode);
			break;
#endif
	}
	if(cio_api.mode==CIOLIB_MODE_AUTO) {
		fprintf(stderr,"CIOLIB initialization failed!\n");
		return(-1);
	}

	initialized=1;
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;

	/* Default C64 is Lt Blue on Dark Blue (As per Every picture ever) */
	switch(cio_textinfo.currmode) {
		case C64_40X25:
			cio_textinfo.normattr=0x6e;
			break;
		case C128_40X25:
			cio_textinfo.normattr=0xbd;
			break;
		default:
			cio_textinfo.normattr=LIGHTGRAY;
			break;
	}
	ciolib_seticon(syncicon64, SYNCICON64_WIDTH);
	ciolib_textattr(cio_textinfo.normattr);

	_beginthread(ciolib_mouse_thread,0,NULL);
	return(0);
}

/* **MUST** be implemented */
/*
 * Returns non-zero if a key is hit
 */
CIOLIBEXPORT int ciolib_kbhit(void)
{
	CIOLIB_INIT();
	if(ungotch)
		return(1);
	return(cio_api.kbhit());
}

/* **MUST** be implemented */
CIOLIBEXPORT int ciolib_getch(void)
{
	int ch;

	CIOLIB_INIT();

	if(ungotch) {
		ch=ungotch;
		ungotch=0;
		return(ch);
	}
	return(cio_api.getch());
}

/* Optional */
CIOLIBEXPORT int ciolib_getche(void)
{
	int ch;

	CIOLIB_INIT();

	if(ungotch) {
		ch=ungotch;
		ungotch=0;
		ciolib_putch(ch);
		return(ch);
	}
	if(cio_api.getche)
		return(cio_api.getche());
	else {
		while(1) {
			ch=ciolib_getch();
			if(ch != 0 && ch != 0xe0) {
				ciolib_putch(ch);
				return(ch);
			}
			/* Eat extended chars - except ESC which is an abort */
			if(ciolib_getch()==1)
				return(EOF);
		}
	}
}

/* Optional */
/*
 * On success, returns ch, on error, returns EOF
 */
CIOLIBEXPORT int ciolib_ungetch(int ch)
{
	CIOLIB_INIT();

	if(ungotch)
		return(EOF);
	if(cio_api.ungetch)
		return(cio_api.ungetch(ch));
	ungotch=ch;
	return(ch);
}

/* Optional */
/*
 * Returns non-zero on success
 */
CIOLIBEXPORT int ciolib_movetext(int sx, int sy, int ex, int ey, int dx, int dy)
{
	int width;
	int height;
	void *buf;

	CIOLIB_INIT();

	if(cio_api.movetext != NULL)
		return(cio_api.movetext(sx, sy, ex, ey, dx, dy));

	width=ex-sx;
	height=ey-sy;
	if (cio_api.vmem_gettext) {
		buf=malloc((width+1)*(height+1)*sizeof(struct vmem_cell));
		if (buf == NULL)
			return 0;
		if(!ciolib_vmem_gettext(sx,sy,ex,ey,buf))
			goto fail;
		if(!ciolib_vmem_puttext(dx,dy,dx+width,dy+height,buf))
			goto fail;
	}
	else {
		buf=malloc((width+1)*(height+1)*2);
		if (buf == NULL)
			return 0;
		if(!ciolib_gettext(sx,sy,ex,ey,buf))
			goto fail;
		if(!ciolib_puttext(dx,dy,dx+width,dy+height,buf))
			goto fail;
	}

	return(1);

fail:
	free(buf);
	return 0;
}

/* Optional */
/*
 * Returns &str[2]
 * Cannot fail
 */
CIOLIBEXPORT char * ciolib_cgets(char *str)
{
	int	maxlen;
	int len=0;
	int ch;

	CIOLIB_INIT();

	if(cio_api.cgets)
		return(cio_api.cgets(str));

	maxlen=*(unsigned char *)str;
	while((ch=ciolib_getch())!='\n' && ch !='\r') {
		switch(ch) {
			case 0:		/* Skip extended keys */
			case 0xe0:	/* Skip extended keys */
				if(ciolib_getche()==1)
					goto early_return;
				break;
			case '\b':
				if(len==0) {
					ciolib_putch(7);
					break;
				}
				ciolib_putch('\b');
				len--;
				break;
			default:
				ciolib_putch(ch);
				str[(len++)+2]=ch;
				if(len==maxlen) {
					str[len+2]=0;
					*((unsigned char *)(str+1))=(unsigned char)len;
					ciolib_putch('\r');
					ciolib_putch('\n');
					return(&str[2]);
				}
				break;
		}
	}
early_return:
	str[len+2]=0;
	*((unsigned char *)(str+1))=(unsigned char)len;
	ciolib_putch('\r');
	ciolib_putch('\n');
	return(&str[2]);
}

#if defined(_MSC_VER) && (_MSC_VER < 1800)	/* Use lame vsscanf() implementation */
/* This is a way to do _vsscanf without using fancy stack tricks or using the
 * "_input" method provided by Microsoft, which is no longer exported as of .NET.
 * The function has a limit of 25 arguments (or less if you run out of stack space),
 *  but how many arguments do you need?
 */
/* From "krabsheva" - http://www.codeguru.com/Cpp/Cpp/string/comments.php/c5631/?thread=1051 */
int vsscanf( const char *buffer, const char *format, va_list arg_ptr )
{
	int i, ret;
	void *arg_arr[25];

	/* Do exception handling in case we go too far // */
	__try
	{
		for ( i = 0; i < 25; i++ )
			arg_arr[i] = va_arg( arg_ptr, void * );
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
	}

	/* This is lame, but the extra arguments won't be used by sscanf */
	ret = sscanf( buffer, format, arg_arr[0], arg_arr[1], arg_arr[2], arg_arr[3],
		arg_arr[4], arg_arr[5], arg_arr[6], arg_arr[7], arg_arr[8], arg_arr[9],
		arg_arr[10], arg_arr[11], arg_arr[12], arg_arr[13], arg_arr[14],
		arg_arr[15], arg_arr[16], arg_arr[17], arg_arr[18], arg_arr[19],
		arg_arr[20], arg_arr[21], arg_arr[22], arg_arr[23], arg_arr[24] );

	return ret;
}
#endif

/* Can't be overridden */
/* Returns the number of fields converted */
CIOLIBEXPORT int ciolib_cscanf (char *format , ...)
{
	char str[255];
    va_list argptr;
	int ret;

	CIOLIB_INIT();

	str[0]=-1;
	va_start(argptr,format);
	ret=vsscanf(ciolib_cgets(str),format,argptr);
	va_end(argptr);
	return(ret);
}

/* Optional */
/* So dumb */
CIOLIBEXPORT char * ciolib_getpass(const char *prompt)
{
	static char pass[9];
	int len=0;
	int ch;

	CIOLIB_INIT();

	if(cio_api.getpass)
		return(cio_api.getpass(prompt));

	ciolib_cputs(prompt);
	while((ch=ciolib_getch())!='\n') {
		switch(ch) {
			case 0:		/* Skip extended keys */
			case 0xe0:	/* Skip extended keys */
				if(ciolib_getch()==1)
					goto early_return;
				break;
			case '\r':	/* Skip \r (ToDo: Should this be treeated as a \n? */
				break;
			case '\b':
				if(len==0) {
					ciolib_putch(7);
					break;
				}
				len--;
				break;
			default:
				if(len==8)
					ciolib_putch(7);
				else
					pass[len++]=ch;
				break;
		}
	}
early_return:
	pass[len]=0;
	return(pass);
}

/* TODO: Hackery here... must fix */
/* Optional */
CIOLIBEXPORT void ciolib_gettextinfo(struct text_info *info)
{
	CIOLIB_INIT()

	if(cio_api.gettextinfo)
		cio_api.gettextinfo(&cio_textinfo);

	if(info!=&cio_textinfo)
		*info=cio_textinfo;
}

/* Optional */
/* Not part of Borland conio? */
CIOLIBEXPORT void ciolib_wscroll(void)
{
	int os;

	CIOLIB_INIT();

	if(cio_api.wscroll!=NULL) {
		cio_api.wscroll();
		return;
	}
	if(!_wscroll)
		return;
	ciolib_movetext(cio_textinfo.winleft
			,cio_textinfo.wintop+1
			,cio_textinfo.winright
			,cio_textinfo.winbottom
			,cio_textinfo.winleft
			,cio_textinfo.wintop);
	ciolib_gotoxy(1,cio_textinfo.winbottom-cio_textinfo.wintop+1);
	os=_wscroll;
	_wscroll=0;
	/* ciolib_cprintf("%*s",ti.winright-ti.winleft+1,""); */
	ciolib_clreol();
	_wscroll=os;
	ciolib_gotoxy(cio_textinfo.curx,cio_textinfo.cury);
}

/* Optional */
/* Cannot fail */
CIOLIBEXPORT int ciolib_wherex(void)
{
	int x;

	CIOLIB_INIT();

	if(cio_api.wherex) {
		/* TODO: This is old hackery... beware */
		x=cio_api.wherex();
		x=x-cio_textinfo.winleft+1;
	}
	else
		x=cio_textinfo.curx;
	return(x);
}

/* Optional */
/* Cannot fail */
CIOLIBEXPORT int ciolib_wherey(void)
{
	int y;

	CIOLIB_INIT();

	if(cio_api.wherey) {
		/* TODO: This is old hackery... beware */
		y=cio_api.wherey();
		y=y-cio_textinfo.wintop+1;
	}
	else
		y=cio_textinfo.cury;
	return(y);
}

/* **MUST** be implemented */
CIOLIBEXPORT void ciolib_gotoxy(int x, int y)
{
	CIOLIB_INIT();

	if(		x < 1
			|| x > cio_textinfo.winright-cio_textinfo.winleft+1
			|| y < 1
			|| y > cio_textinfo.winbottom-cio_textinfo.wintop+1)
		return;
	cio_api.gotoxy(x,y);
}

/* **MUST** be implemented */
CIOLIBEXPORT void ciolib_textmode(int mode)
{
	CIOLIB_INIT();

	if(mode==LASTMODE) {
		cio_api.textmode(lastmode);
		lastmode=cio_textinfo.currmode;
	}
	else {
		if(mode==64)
			mode=C80X50;
		if(mode==_ORIGMODE)
			mode=C80;
		lastmode=cio_textinfo.currmode;
		cio_api.textmode(mode);
	}
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;

	switch(cio_textinfo.currmode) {
		case C64_40X25:
			cio_textinfo.normattr=0x6e;
			break;
		case C128_40X25:
			cio_textinfo.normattr=0xbd;
			break;
		default:
			cio_textinfo.normattr=LIGHTGRAY;
			break;
	}
	ciolib_textattr(cio_textinfo.normattr);
}

/* Optional */
CIOLIBEXPORT void ciolib_window(int sx, int sy, int ex, int ey)
{
	CIOLIB_INIT();

	if(cio_api.window)
		cio_api.window(sx,sy,ex,ey);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > cio_textinfo.screenwidth
			|| sy > cio_textinfo.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > cio_textinfo.screenwidth
			|| ey > cio_textinfo.screenheight) {
		return;
	}
	cio_textinfo.winleft=sx;
	cio_textinfo.wintop=sy;
	cio_textinfo.winright=ex;
	cio_textinfo.winbottom=ey;
	ciolib_gotoxy(1,1);
}

/* Optional */
CIOLIBEXPORT void ciolib_clreol(void)
{
	struct vmem_cell *buf;
	int i;
	int width,height;

	CIOLIB_INIT();

	if(cio_api.clreol) {
		cio_api.clreol();
		return;
	}

	width=cio_textinfo.winright-cio_textinfo.winleft+1-cio_textinfo.curx+1;
	height=1;
	buf=malloc(width*height*sizeof(*buf));
	if (!buf)
		return;
	for(i=0;i<width*height;i++) {
		buf[i].ch = ' ';
		buf[i].legacy_attr = cio_textinfo.attribute;
		buf[i].fg = ciolib_fg;
		buf[i].bg = ciolib_bg;
		buf[i].font = ciolib_attrfont(cio_textinfo.attribute);
	}
	ciolib_vmem_puttext(
			cio_textinfo.curx+cio_textinfo.winleft-1,
			cio_textinfo.cury+cio_textinfo.wintop-1,
			cio_textinfo.winright,
			cio_textinfo.cury+cio_textinfo.wintop-1,
			buf);
	free(buf);
}

/* Optional */
CIOLIBEXPORT void ciolib_clrscr(void)
{
	struct vmem_cell *buf;
	int i;
	int width,height;
	int old_ptcm=puttext_can_move;

	CIOLIB_INIT();
	if(cio_api.clrscr) {
		cio_api.clrscr();
		return;
	}

	width=cio_textinfo.winright-cio_textinfo.winleft+1;
	height=cio_textinfo.winbottom-cio_textinfo.wintop+1;
	buf=malloc(width*height*sizeof(*buf));
	if(!buf)
		return;
	for(i=0;i<width*height;i++) {
		buf[i].ch = ' ';
		buf[i].legacy_attr = cio_textinfo.attribute;
		buf[i].fg = ciolib_fg;
		buf[i].bg = ciolib_bg;
		buf[i].font = ciolib_attrfont(cio_textinfo.attribute);
	}
	puttext_can_move=1;
	ciolib_vmem_puttext(cio_textinfo.winleft,cio_textinfo.wintop,cio_textinfo.winright,cio_textinfo.winbottom,buf);
	ciolib_gotoxy(1,1);
	puttext_can_move=old_ptcm;

	free(buf);
}

/* Optional */
CIOLIBEXPORT void ciolib_delline(void)
{
	CIOLIB_INIT();

	if(cio_api.delline) {
		cio_api.delline();
		return;
	}
	ciolib_movetext(cio_textinfo.winleft
			,cio_textinfo.cury+1
			,cio_textinfo.winright
			,cio_textinfo.winbottom
			,cio_textinfo.winleft
			,cio_textinfo.cury);
	ciolib_gotoxy(1,cio_textinfo.winbottom-cio_textinfo.wintop+1);
	ciolib_clreol();
	ciolib_gotoxy(cio_textinfo.curx,cio_textinfo.cury);
}

/* Optional */
CIOLIBEXPORT void ciolib_insline(void)
{
	CIOLIB_INIT();

	if(cio_api.insline) {
		cio_api.insline();
		return;
	}
	ciolib_movetext(cio_textinfo.winleft,cio_textinfo.cury,cio_textinfo.winright,cio_textinfo.winbottom,cio_textinfo.winleft,cio_textinfo.cury+1);
	ciolib_gotoxy(1,cio_textinfo.cury);
	ciolib_clreol();
	ciolib_gotoxy(cio_textinfo.curx,cio_textinfo.cury);
}

/* Not overridable due to varargs */
/* Returns the number of characters or EOF on error */
CIOLIBEXPORT int ciolib_cprintf(const char *fmat, ...)
{
    va_list argptr;
	int		ret;
#if defined(_MSC_VER) || defined(__MSVCRT__)	/* Can't figure out a way to allocate a "big enough" buffer for Win32. */
	char	str[16384];
#else
	char	*str;
#ifndef HAVE_VASPRINTF
	va_list argptr2;
#endif
#endif

	CIOLIB_INIT();

    va_start(argptr,fmat);

#ifdef HAVE_VASPRINTF
	ret=vasprintf(&str, fmat, argptr);
	if(ret>=0)
		ciolib_cputs(str);
	else
		ret=EOF;
	free(str);
    va_end(argptr);
#else

#if defined(_MSC_VER) || defined(__MSVCRT__)
	ret=_vsnprintf(str,sizeof(str)-1,fmat,argptr);
#else

#ifdef __BORLANDC__
	argptr2=argptr;
#else
	va_copy(argptr2, argptr);
#endif
    ret=vsnprintf(NULL,0,fmat,argptr);
	if(ret<0)
		return(EOF);
	str=(char *)alloca(ret+1);
	if(str==NULL)
		return(EOF);
	ret=vsprintf(str,fmat,argptr2);
#endif
    va_end(argptr);
#if !(defined(_MSC_VER) || defined(__MSVCRT__))
    va_end(argptr2);
#endif
	if(ret>=0)
		ciolib_cputs(str);
	else
		ret=EOF;

#endif

    return(ret);
}

/* Optional */
/* The Borland version does not translate \n into \r\n... this does.
 * Returns last character printed (!)
 */
CIOLIBEXPORT int ciolib_cputs(const char *str)
{
	int		pos;
	int		ret=0;
	int		olddmc;

	CIOLIB_INIT();

	if(cio_api.cputs)
		return(cio_api.cputs(str));

	olddmc=hold_update;
	hold_update=1;
	for(pos=0;str[pos];pos++)
	{
		ret=str[pos];
		if(str[pos]=='\n')
			ciolib_putch('\r');
		ciolib_putch(str[pos]);
	}
	hold_update=olddmc;
	ciolib_gotoxy(ciolib_wherex(),ciolib_wherey());
	return(ret);
}

/* Optional... and overriding is silly */
CIOLIBEXPORT void ciolib_textbackground(int colour)
{
	unsigned char attr;
	unsigned char col;

	CIOLIB_INIT();

	if(cio_api.textbackground) {
		cio_api.textbackground(colour);
		return;
	}

	attr=cio_textinfo.attribute;
	attr&=143;
	col=(colour & 0x07);
	attr|=(col<<4);
	ciolib_textattr(attr);
}

/* Optional... and overriding is silly */
CIOLIBEXPORT void ciolib_textcolor(int colour)
{
	unsigned char attr;
	unsigned char col;

	CIOLIB_INIT();

	if(cio_api.textcolor) {
		cio_api.textcolor(colour);
		return;
	}

	attr=cio_textinfo.attribute;
	attr&=240;
	col=colour&0x0f;
	attr|=col;
	ciolib_textattr(attr);
}

/* Optional... and overriding is silly */
CIOLIBEXPORT void ciolib_highvideo(void)
{
	int attr;

	CIOLIB_INIT();

	if(cio_api.highvideo) {
		cio_api.highvideo();
		return;
	}

	attr=cio_textinfo.attribute;
	attr |= 8;
	ciolib_textattr(attr);
}

/* Optional... and overriding is silly */
CIOLIBEXPORT void ciolib_lowvideo(void)
{
	int attr;

	CIOLIB_INIT();

	if(cio_api.lowvideo) {
		cio_api.lowvideo();
		return;
	}

	attr=cio_textinfo.attribute;
	attr &= 0xf7;
	ciolib_textattr(attr);
}

/* Optional... and overriding is silly */
CIOLIBEXPORT void ciolib_normvideo(void)
{
	CIOLIB_INIT();

	if(cio_api.normvideo) {
		cio_api.normvideo();
		return;
	}
	ciolib_textattr(cio_textinfo.normattr);
}

/* **MUST** be implemented */
/*
 * Non-zero on success
 */
CIOLIBEXPORT int ciolib_puttext(int a,int b,int c,int d,void *e)
{
	char	*buf=e;
	int		ret;
	CIOLIB_INIT();

	ret = cio_api.puttext(a,b,c,d,(void *)buf);
	if (buf != e)
		free(buf);
	return ret;
}

/* **MUST** be implemented */
/* 1 on success, 0 on failure */
CIOLIBEXPORT int ciolib_gettext(int a,int b,int c,int d,void *e)
{
	char	*ch;
	int		i;
	int		ret;
	struct vmem_cell *buf;
	CIOLIB_INIT();

	if (cio_api.gettext == NULL) {
		buf = malloc((c-a+1)*(d-b+1)*sizeof(*buf));
		if (buf == NULL)
			return 0;
		ch = e;
		ret = cio_api.vmem_gettext(a,b,c,d,buf);
		for (i=0; i<(c-a+1)*(d-b+1); i++) {
			*(ch++)=buf[i].ch;
			*(ch++)=buf[i].legacy_attr;
		}
		free(buf);
	}
	else
		ret = cio_api.gettext(a,b,c,d,e);
	return ret;
}

/* Optional */
/* 1 on success, 0 on failure */
CIOLIBEXPORT int ciolib_vmem_gettext(int a,int b,int c,int d,struct vmem_cell *e)
{
	int ret;
	uint16_t *buf;
	int i;
	CIOLIB_INIT();

	if (cio_api.vmem_gettext == NULL) {
		buf = malloc((c-a+1)*(d-b+1)*sizeof(*buf));
		if (buf == NULL)
			return 0;
		ret = ciolib_gettext(a, b, c, d, buf);
		if (ret) {
			for (i=0; i<(c-a+1)*(d-b+1); i++) {
				memset(&e[i], 0, sizeof(e[0]));
				e[i].ch = buf[i] & 0xff;
				e[i].legacy_attr = buf[i] >> 8;
			}
		}
		free(buf);
		return ret;
	}
	return cio_api.vmem_gettext(a,b,c,d,e);
}

/* Optional */
/* Non-zero on success */
CIOLIBEXPORT int ciolib_vmem_puttext(int a,int b,int c,int d,struct vmem_cell *e)
{
	int i;
	int ret;
	uint16_t *buf;
	CIOLIB_INIT();

	if (cio_api.vmem_puttext == NULL) {
		buf = malloc((c-a+1)*(d-b+1)*sizeof(*buf));
		if (buf == NULL)
			return 0;
		for (i=0; i<(c-a+1)*(d-b+1); i++)
			buf[i] = (e[i].legacy_attr << 8) | (e[i].ch);
		ret = ciolib_puttext(a, b, c, d, buf);
		free(buf);
		return ret;
	}
	return cio_api.vmem_puttext(a,b,c,d,e);
}

/* Optional */
CIOLIBEXPORT void ciolib_textattr(int a)
{
	CIOLIB_INIT();

	if(cio_api.textattr) {
		cio_api.textattr(a);
		return;
	}

	cio_textinfo.attribute=a;
	ciolib_attr2palette(a, &ciolib_fg, &ciolib_bg);
}

/* Optional */
CIOLIBEXPORT void ciolib_delay(long a)
{
	CIOLIB_INIT();

	if(cio_api.delay) {
		cio_api.delay(a);
		return;
	}
	SLEEP(a);
}

/* Optional */
/* Returns ch on success, EOF on error */
CIOLIBEXPORT int ciolib_putch(int ch)
{
	unsigned char a1=ch;
	struct vmem_cell buf[1];
	int i;
	int old_puttext_can_move=puttext_can_move;

	CIOLIB_INIT();

	if(cio_api.putch)
		return(cio_api.putch(a1));

	puttext_can_move=1;

	buf[0].ch=a1;
	buf[0].legacy_attr=cio_textinfo.attribute;
	buf[0].fg = ciolib_fg;
	buf[0].bg = ciolib_bg;
	buf[0].font = ciolib_attrfont(cio_textinfo.attribute);

	switch(a1) {
		case '\r':
			ciolib_gotoxy(1,cio_textinfo.cury);
			break;
		case '\n':
			if(cio_textinfo.cury==cio_textinfo.winbottom-cio_textinfo.wintop+1)
				ciolib_wscroll();
			else
				ciolib_gotoxy(cio_textinfo.curx, cio_textinfo.cury+1);
			break;
		case '\b':
			if(cio_textinfo.curx>1) {
				ciolib_gotoxy(cio_textinfo.curx-1,cio_textinfo.cury);
				buf[0].ch=' ';
				ciolib_vmem_puttext(cio_textinfo.curx+cio_textinfo.winleft-1
						,cio_textinfo.cury+cio_textinfo.wintop-1
						,cio_textinfo.curx+cio_textinfo.winleft-1
						,cio_textinfo.cury+cio_textinfo.wintop-1
						,buf);
			}
			break;
		case 7:		/* Bell */
			ciolib_beep();
			break;
		case '\t':
			for(i=0;i<(sizeof(tabs)/sizeof(int));i++) {
				if(tabs[i]>cio_textinfo.curx) {
					buf[0].ch=' ';
					while(cio_textinfo.curx<tabs[i]) {
						ciolib_vmem_puttext(cio_textinfo.curx+cio_textinfo.winleft-1
								,cio_textinfo.cury+cio_textinfo.wintop-1
								,cio_textinfo.curx+cio_textinfo.winleft-1
								,cio_textinfo.cury+cio_textinfo.wintop-1
								,buf);
						ciolib_gotoxy(cio_textinfo.curx+1,cio_textinfo.cury);
						if(cio_textinfo.curx==cio_textinfo.screenwidth)
							break;
					}
					break;
				}
			}
			if(cio_textinfo.curx==cio_textinfo.screenwidth) {
				ciolib_gotoxy(1,cio_textinfo.cury);
				if(cio_textinfo.cury==cio_textinfo.winbottom-cio_textinfo.wintop+1)
					ciolib_wscroll();
				else
					ciolib_gotoxy(cio_textinfo.curx, cio_textinfo.cury+1);
			}
			break;
		default:
			if(cio_textinfo.cury==cio_textinfo.winbottom-cio_textinfo.wintop+1
					&& cio_textinfo.curx==cio_textinfo.winright-cio_textinfo.winleft+1) {
				ciolib_vmem_puttext(ciolib_wherex()+cio_textinfo.winleft-1
						,ciolib_wherey()+cio_textinfo.wintop-1
						,ciolib_wherex()+cio_textinfo.winleft-1
						,ciolib_wherey()+cio_textinfo.wintop-1
						,buf);
				ciolib_wscroll();
				ciolib_gotoxy(1, cio_textinfo.winbottom-cio_textinfo.wintop+1);
			}
			else {
				if(cio_textinfo.curx==cio_textinfo.winright-cio_textinfo.winleft+1) {
					ciolib_vmem_puttext(ciolib_wherex()+cio_textinfo.winleft-1
							,ciolib_wherey()+cio_textinfo.wintop-1
							,ciolib_wherex()+cio_textinfo.winleft-1
							,ciolib_wherey()+cio_textinfo.wintop-1
							,buf);
					ciolib_gotoxy(1,cio_textinfo.cury+1);
				}
				else {
					ciolib_vmem_puttext(ciolib_wherex()+cio_textinfo.winleft-1
							,ciolib_wherey()+cio_textinfo.wintop-1
							,ciolib_wherex()+cio_textinfo.winleft-1
							,ciolib_wherey()+cio_textinfo.wintop-1
							,buf);
					ciolib_gotoxy(cio_textinfo.curx+1, cio_textinfo.cury);
				}
			}
			break;
	}

	puttext_can_move=old_puttext_can_move;

	return(a1);

}

/* **MUST** be implemented */
CIOLIBEXPORT void ciolib_setcursortype(int a)
{
	CIOLIB_INIT();

	cio_api.setcursortype(a);
}

/* Optional */
/* Return Non-zero on success */
CIOLIBEXPORT int ciolib_showmouse(void) {
	CIOLIB_INIT();

	if(cio_api.showmouse!=NULL)
		return(cio_api.showmouse());
	return(0);
}

/* Optional */
/* Return Non-zero on success */
CIOLIBEXPORT int ciolib_hidemouse(void) {
	CIOLIB_INIT();

	if(cio_api.hidemouse!=NULL)
		return(cio_api.hidemouse());
	return(0);
}

/* Optional */
CIOLIBEXPORT void ciolib_setname(const char *name) {
	CIOLIB_INIT();

	if(cio_api.setname!=NULL)
		cio_api.setname(name);
}

/* Optional */
CIOLIBEXPORT void ciolib_seticon(const void *icon, unsigned long size) {
	CIOLIB_INIT();

	if(cio_api.seticon!=NULL)
		cio_api.seticon(icon,size);
}

/* Optional */
CIOLIBEXPORT void ciolib_settitle(const char *title) {
	CIOLIB_INIT();

	if(cio_api.settitle!=NULL)
		cio_api.settitle(title);
}

/* Optional */
CIOLIBEXPORT void ciolib_copytext(const char *text, size_t buflen)
{
	CIOLIB_INIT();

	if(cio_api.copytext!=NULL)
		cio_api.copytext(text,buflen);
}

/* Optional */
/* Returns NULL on error */
CIOLIBEXPORT char * ciolib_getcliptext(void)
{
	CIOLIB_INIT();

	if(cio_api.getcliptext!=NULL)
		return(cio_api.getcliptext());
	else
		return(NULL);
}

/* Optional */
/* Return Non-zero on success */
CIOLIBEXPORT int ciolib_setfont(int font, int force, int font_num)
{
	CIOLIB_INIT();

	if(cio_api.setfont!=NULL)
		return(cio_api.setfont(font,force,font_num));
	else
		return(0);
}

/* Optional */
/* Return -1 if not implemented */
CIOLIBEXPORT int ciolib_getfont(int font_num)
{
	CIOLIB_INIT();

	if(cio_api.getfont!=NULL)
		return(cio_api.getfont(font_num));
	else
		return(-1);
}

/* Optional */
/* Return Non-zero on success */
CIOLIBEXPORT int ciolib_loadfont(char *filename)
{
	CIOLIB_INIT();

	if(cio_api.loadfont!=NULL)
		return(cio_api.loadfont(filename));
	else
		return(0);
}

/* Optional */
/* Return Non-zero on success */
CIOLIBEXPORT int ciolib_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	CIOLIB_INIT();

	if(cio_api.get_window_info!=NULL)
		return(cio_api.get_window_info(width,height,xpos,ypos));
	else {
		if(width)
			*width=-1;
		if(height)
			*height=-1;
		if(xpos)
			*xpos=-1;
		if(ypos)
			*ypos=-1;
	}
	return(0);
}

/* Optional */
CIOLIBEXPORT void ciolib_beep(void)
{
	CIOLIB_INIT();

	if(cio_api.beep)
		cio_api.beep();
	else
		BEEP(440,100);
	return;
}

/* Optional */
CIOLIBEXPORT void ciolib_getcustomcursor(int *start, int *end, int *range, int *blink, int *visible)
{
	CIOLIB_INIT();

	if(cio_api.getcustomcursor)
		cio_api.getcustomcursor(start,end,range,blink,visible);
}

/* Optional */
CIOLIBEXPORT void ciolib_setcustomcursor(int start, int end, int range, int blink, int visible)
{
	CIOLIB_INIT();

	if(cio_api.setcustomcursor)
		cio_api.setcustomcursor(start,end,range,blink,visible);
}

/* Optional */
CIOLIBEXPORT void ciolib_setvideoflags(int flags)
{
	CIOLIB_INIT();

	if(cio_api.setvideoflags)
		cio_api.setvideoflags(flags);
}

/* Optional */
CIOLIBEXPORT int ciolib_getvideoflags(void)
{
	CIOLIB_INIT();

	if(cio_api.getvideoflags)
		return(cio_api.getvideoflags());
	return(0);
}

/* Optional */
CIOLIBEXPORT void ciolib_setscaling(int new_value)
{
	CIOLIB_INIT();

	if(cio_api.setscaling)
		cio_api.setscaling(new_value);
}

/* Optional */
/* Returns zero on error */
CIOLIBEXPORT int ciolib_getscaling(void)
{
	CIOLIB_INIT();

	if(cio_api.getscaling)
		return(cio_api.getscaling());
	return(1);
}

/* Optional */
/* Returns non-zero on success */
CIOLIBEXPORT int ciolib_setpalette(uint32_t entry, uint16_t r, uint16_t g, uint16_t b)
{
	CIOLIB_INIT();

	if(cio_api.setpalette)
		return(cio_api.setpalette(entry, r, g, b));
	return(0);
}

/* Returns non-zero on success */
CIOLIBEXPORT int ciolib_attr2palette(uint8_t attr, uint32_t *fg, uint32_t *bg)
{
	CIOLIB_INIT();

	if (cio_api.attr2palette)
		return cio_api.attr2palette(attr, fg, bg);
	/*
	 * TODO: If we want to be able to cross screens, we need some
	 * mapping for non-plaette aware things.
	 */
	return 0;
}

/* Returns non-zero on success */
CIOLIBEXPORT int ciolib_setpixel(uint32_t x, uint32_t y, uint32_t colour)
{
	CIOLIB_INIT();

	if (cio_api.setpixel)
		return cio_api.setpixel(x, y, colour);
	return 0;
}

/* Returns NULL on failure */
CIOLIBEXPORT struct ciolib_pixels * ciolib_getpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, int force)
{
	CIOLIB_INIT();

	if (cio_api.getpixels)
		return cio_api.getpixels(sx, sy, ex, ey, force);
	return NULL;
}

/* Returns non-zero on success */
CIOLIBEXPORT int ciolib_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, struct ciolib_pixels *pixels, void *mask)
{
	CIOLIB_INIT();

	if (cio_api.setpixels)
		return cio_api.setpixels(sx, sy, ex, ey, x_off, y_off, pixels, mask);
	return 0;
}

CIOLIBEXPORT void ciolib_freepixels(struct ciolib_pixels *pixels)
{
	if (pixels == NULL)
		return;

	FREE_AND_NULL(pixels->pixels);
	FREE_AND_NULL(pixels->pixelsb);
	FREE_AND_NULL(pixels);
}

/* Returns NULL on failure */
CIOLIBEXPORT struct ciolib_screen * ciolib_savescreen(void)
{
	struct ciolib_screen *ret;
	int vmode;
	int i;

	CIOLIB_INIT();

	ret = malloc(sizeof(*ret));
	if (ret == NULL)
		return NULL;

	ciolib_gettextinfo(&ret->text_info);
	vmode = find_vmode(ret->text_info.currmode);
	ret->vmem = malloc(ret->text_info.screenwidth * ret->text_info.screenheight * sizeof(struct vmem_cell));
	if (ret->vmem == NULL) {
		free(ret);
		return NULL;
	}

	if (vmode != -1) {
		ret->pixels = ciolib_getpixels(0, 0, vparams[vmode].charwidth * vparams[vmode].cols - 1, vparams[vmode].charheight * vparams[vmode].rows - 1, TRUE);
	}
	ciolib_vmem_gettext(1, 1, ret->text_info.screenwidth, ret->text_info.screenheight, ret->vmem);
	ret->fg_colour = ciolib_fg;
	ret->bg_colour = ciolib_bg;
	for (i=0; i<5; i++)
		ret->fonts[i] = ciolib_getfont(i);
	ret->flags = ciolib_getvideoflags();
	ciolib_get_modepalette(ret->palette);

	return ret;
}

CIOLIBEXPORT void ciolib_freescreen(struct ciolib_screen *scrn)
{
	if (scrn == NULL)
		return;

	ciolib_freepixels(scrn->pixels);
	FREE_AND_NULL(scrn->vmem);
	free(scrn);
}

/* Returns non-zero on success */
CIOLIBEXPORT int ciolib_restorescreen(struct ciolib_screen *scrn)
{
	struct text_info ti;
	int vmode;
	int i;

	CIOLIB_INIT();

	ciolib_gettextinfo(&ti);

	if (ti.currmode != scrn->text_info.currmode)
		ciolib_textmode(scrn->text_info.currmode);
	ciolib_set_modepalette(scrn->palette);
	ciolib_vmem_puttext(1, 1, scrn->text_info.screenwidth, scrn->text_info.screenheight, scrn->vmem);
	ciolib_textcolor(scrn->text_info.attribute);
	ciolib_window(scrn->text_info.winleft, scrn->text_info.wintop, scrn->text_info.winright, scrn->text_info.winbottom);
	vmode = find_vmode(scrn->text_info.currmode);
	if (vmode != -1 && scrn->pixels != NULL)
		ciolib_setpixels(0, 0, vparams[vmode].charwidth * vparams[vmode].cols - 1, vparams[vmode].charheight * vparams[vmode].rows - 1, 0, 0, scrn->pixels, NULL);
	for (i=0; i<5; i++)
		ciolib_setfont(scrn->fonts[i], FALSE, i);
	ciolib_setvideoflags(scrn->flags);
	ciolib_setcolour(scrn->fg_colour, scrn->bg_colour);
	ciolib_gotoxy(scrn->text_info.curx, scrn->text_info.cury);
	return 1;
}

CIOLIBEXPORT void ciolib_setcolour(uint32_t fg, uint32_t bg)
{
	ciolib_fg = fg;
	ciolib_bg = bg;
}

/* Returns non-zero on success */
CIOLIBEXPORT int ciolib_get_modepalette(uint32_t p[16])
{
	CIOLIB_INIT();

	if (cio_api.get_modepalette)
		return cio_api.get_modepalette(p);
	return 0;
}

/* Returns non-zero on success */
CIOLIBEXPORT int ciolib_set_modepalette(uint32_t p[16])
{
	CIOLIB_INIT();

	if (cio_api.set_modepalette)
		return cio_api.set_modepalette(p);
	return 0;
}

/* Returns non-zero on success */
CIOLIBEXPORT uint32_t ciolib_map_rgb(uint16_t r, uint16_t g, uint16_t b)
{
	CIOLIB_INIT();

	if (cio_api.map_rgb)
		return cio_api.map_rgb(r,g,b);
	return 0;
}

CIOLIBEXPORT void ciolib_replace_font(uint8_t id, char *name, void *data, size_t size)
{
	CIOLIB_INIT();

	if (cio_api.replace_font) {
		cio_api.replace_font(id, name, data, size);
		return;
	}
	free(name);
	free(data);
}

CIOLIBEXPORT int ciolib_attrfont(uint8_t attr)
{
	int flags;
	int font = 0;
	CIOLIB_INIT();

	flags = ciolib_getvideoflags();
	if ((flags & CIOLIB_VIDEO_ALTCHARS) && (attr & 0x08))
		font |= 1;
	if ((flags & CIOLIB_VIDEO_BLINKALTCHARS) && (attr & 0x80))
		font |= 2;
	return ciolib_getfont(font+1);
}

/* Returns non-zero if fontnum is supported in this mode */
CIOLIBEXPORT int ciolib_checkfont(int fontnum)
{
	int vmode;
	struct text_info ti;

	CIOLIB_INIT();

	if (fontnum < 0 || fontnum > 255)
		return 0;

	// Font 0 is always supported.
	if (fontnum == 0)
		return 1;

	ciolib_gettextinfo(&ti);
	vmode = find_vmode(ti.currmode);

	if (cio_api.checkfont != NULL)
		return cio_api.checkfont(fontnum);
	if (vmode == -1)
		return 0;

	if (cio_api.options & CONIO_OPT_FONT_SELECT) {
		switch (vparams[vmode].charheight) {
			case 8:
				if (conio_fontdata[fontnum].eight_by_eight)
					return 1;
				return 0;
			case 14:
				if (conio_fontdata[fontnum].eight_by_fourteen)
					return 1;
				return 0;
			case 16:
				if (conio_fontdata[fontnum].eight_by_sixteen)
					return 1;
				return 0;
			default:
				return 0;
		}
	}
	return 0;

}

CIOLIBEXPORT void
ciolib_set_vmem(struct vmem_cell *cell, uint8_t ch, uint8_t attr, uint8_t font)
{
	CIOLIB_INIT();

	if (cell == NULL)
		return;
	cell->ch = ch;
	cell->font = font;
	ciolib_set_vmem_attr(cell, attr);
}

CIOLIBEXPORT void
ciolib_set_vmem_attr(struct vmem_cell *cell, uint8_t attr)
{
	CIOLIB_INIT();

	if (cell == NULL)
		return;
	cell->legacy_attr = attr;
	ciolib_attr2palette(attr, &cell->fg, &cell->bg);
}

/* Optional */
CIOLIBEXPORT void ciolib_setwinsize(int w, int h)
{
	CIOLIB_INIT();

	if(cio_api.setwinsize)
		cio_api.setwinsize(w, h);
}

/* Optional */
CIOLIBEXPORT void ciolib_setwinposition(int x, int y)
{
	CIOLIB_INIT();

	if(cio_api.setwinposition)
		cio_api.setwinposition(x, y);
}

CIOLIBEXPORT enum ciolib_codepage ciolib_getcodepage(void)
{
	int font = ciolib_getfont(1);

	if (font < 0)
		return CIOLIB_CP437;
	if (font >= sizeof(conio_fontdata) / sizeof(conio_fontdata[0]))
		return CIOLIB_CP437;
	return conio_fontdata[font].cp;
}

#if defined(__DARWIN__)
#ifdef main
#undef main
#endif

sem_t startsdl_sem;
sem_t main_sem;
int initsdl_ret = -1;

struct main_args {
	int argc;
	char **argv;
	int ret;
	int no_sdl;
};

int CIOLIB_main(int, char **);
void main_stub(void *argptr)
{
	struct main_args *args = (struct main_args *)argptr;
	args->ret = CIOLIB_main(args->argc, args->argv);
	args->no_sdl = 1;
	sem_post(&startsdl_sem);
	sem_post(&main_sem);
	exit_sdl_con();
}

int main(int argc, char **argv)
{
	struct main_args ma = {argc, argv, -1, 0};
	sem_init(&startsdl_sem, 0, 0);
	sem_init(&main_sem, 0, 0);
	initsdl_ret = init_sdl_video();
	_beginthread(main_stub, 0, &ma);
	if (!ma.no_sdl) {
		if (initsdl_ret != -1) {
			sem_wait(&startsdl_sem);
			sdl_video_event_thread(NULL);
		}
	}
	sem_wait(&main_sem);
	return ma.ret;
}
#endif

/* Returns non-zero on success */
CIOLIBEXPORT uint32_t ciolib_mousepointer(enum ciolib_mouse_ptr type)
{
	CIOLIB_INIT();

	if (cio_api.mousepointer)
		return cio_api.mousepointer(type);
	return 0;
}
