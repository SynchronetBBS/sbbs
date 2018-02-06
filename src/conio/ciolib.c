/* $Id$ */

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

#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
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
CIOLIBEXPORT int ciolib_xlat=0;
CIOLIBEXPORT int ciolib_reaper=TRUE;
static int initialized=0;

CIOLIBEXPORT int CIOLIBCALL ciolib_movetext(int sx, int sy, int ex, int ey, int dx, int dy);
CIOLIBEXPORT char * CIOLIBCALL ciolib_cgets(char *str);
CIOLIBEXPORT int CIOLIBCALL ciolib_cscanf (char *format , ...);
CIOLIBEXPORT int CIOLIBCALL ciolib_kbhit(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_getch(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_getche(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_ungetch(int ch);
CIOLIBEXPORT void CIOLIBCALL ciolib_gettextinfo(struct text_info *info);
CIOLIBEXPORT int CIOLIBCALL ciolib_wherex(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_wherey(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_wscroll(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_gotoxy(int x, int y);
CIOLIBEXPORT void CIOLIBCALL ciolib_clreol(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_clrscr(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_cputs(char *str);
CIOLIBEXPORT int	CIOLIBCALL ciolib_cprintf(const char *fmat, ...);
CIOLIBEXPORT void CIOLIBCALL ciolib_textbackground(int colour);
CIOLIBEXPORT void CIOLIBCALL ciolib_textcolor(int colour);
CIOLIBEXPORT void CIOLIBCALL ciolib_highvideo(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_lowvideo(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_normvideo(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_puttext(int a,int b,int c,int d,void *e);
CIOLIBEXPORT int CIOLIBCALL ciolib_pputtext(int a,int b,int c,int d,void *e,uint32_t *f, uint32_t *g);
CIOLIBEXPORT int CIOLIBCALL ciolib_gettext(int a,int b,int c,int d,void *e);
CIOLIBEXPORT int CIOLIBCALL ciolib_pgettext(int a,int b,int c,int d,void *e, uint32_t *f, uint32_t *g);
CIOLIBEXPORT void CIOLIBCALL ciolib_textattr(int a);
CIOLIBEXPORT void CIOLIBCALL ciolib_delay(long a);
CIOLIBEXPORT int CIOLIBCALL ciolib_putch(int a);
CIOLIBEXPORT void CIOLIBCALL ciolib_setcursortype(int a);
CIOLIBEXPORT void CIOLIBCALL ciolib_textmode(int mode);
CIOLIBEXPORT void CIOLIBCALL ciolib_window(int sx, int sy, int ex, int ey);
CIOLIBEXPORT void CIOLIBCALL ciolib_delline(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_insline(void);
CIOLIBEXPORT char * CIOLIBCALL ciolib_getpass(const char *prompt);
CIOLIBEXPORT void CIOLIBCALL ciolib_copytext(const char *text, size_t buflen);
CIOLIBEXPORT char * CIOLIBCALL ciolib_getcliptext(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_get_window_info(int *width, int *height, int *xpos, int *ypos);
CIOLIBEXPORT void CIOLIBCALL ciolib_setscaling(int new_value);
CIOLIBEXPORT int CIOLIBCALL ciolib_getscaling(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_setpalette(uint32_t entry, uint16_t r, uint16_t g, uint16_t b);
CIOLIBEXPORT int CIOLIBCALL ciolib_cputch(uint32_t fg_palette, uint32_t bg_palette, int a);
CIOLIBEXPORT int CIOLIBCALL ciolib_ccputs(uint32_t fg_palette, uint32_t bg_palette, const char *str);
CIOLIBEXPORT int CIOLIBCALL ciolib_attr2palette(uint8_t attr, uint32_t *fg, uint32_t *bg);
CIOLIBEXPORT int CIOLIBCALL ciolib_setpixel(uint32_t x, uint32_t y, uint32_t colour);
CIOLIBEXPORT struct ciolib_pixels * CIOLIBCALL ciolib_getpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey);
CIOLIBEXPORT int CIOLIBCALL ciolib_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, struct ciolib_pixels *pixels, void *mask);
CIOLIBEXPORT void CIOLIBCALL ciolib_freepixels(struct ciolib_pixels *pixels);
CIOLIBEXPORT struct ciolib_screen * CIOLIBCALL ciolib_savescreen(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_freescreen(struct ciolib_screen *);
CIOLIBEXPORT int CIOLIBCALL ciolib_restorescreen(struct ciolib_screen *scrn);
CIOLIBEXPORT void CIOLIBCALL ciolib_setcolour(uint32_t fg, uint32_t bg);

#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
int sdl_video_initialized = 0;
#endif

#define CIOLIB_INIT()		{ if(initialized != 1) initciolib(CIOLIB_MODE_AUTO); }

#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
int try_sdl_init(int mode)
{
	if(!sdl_initciolib(mode)) {
		cio_api.mouse=1;
		cio_api.puttext=bitmap_puttext;
		cio_api.pputtext=bitmap_pputtext;
		cio_api.pgettext=bitmap_pgettext;
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
#ifdef _WIN32
		cio_api.copytext=win32_copytext;
		cio_api.getcliptext=win32_getcliptext;
#else
		cio_api.copytext=sdl_copytext;
		cio_api.getcliptext=sdl_getcliptext;
#endif
		cio_api.get_window_info=sdl_get_window_info;
		cio_api.setscaling=sdl_setscaling;
		cio_api.getscaling=sdl_getscaling;
		cio_api.setpalette=sdl_setpalette;
		cio_api.attr2palette=bitmap_attr2palette;
		cio_api.setpixel=bitmap_setpixel;
		cio_api.getpixels=bitmap_getpixels;
		cio_api.setpixels=bitmap_setpixels;
		return(1);
	}
	return(0);
}
#endif

#ifndef _WIN32
 #ifndef NO_X
int try_x_init(int mode)
{
#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
	if (sdl_video_initialized) {
		sdl.QuitSubSystem(SDL_INIT_VIDEO);
		sdl_video_initialized = FALSE;
	}
#endif

	if(!x_init()) {
		cio_api.mode=CIOLIB_MODE_X;
		cio_api.mouse=1;
		cio_api.puttext=bitmap_puttext;
		cio_api.pputtext=bitmap_pputtext;
		cio_api.pgettext=bitmap_pgettext;
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
		cio_api.setpalette=x_setpalette;
		cio_api.attr2palette=bitmap_attr2palette;
		cio_api.setpixel=bitmap_setpixel;
		cio_api.setpixels=bitmap_setpixels;
		return(1);
	}
	return(0);
}
 #endif

int try_curses_init(int mode)
{
#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
	if (sdl_video_initialized) {
		sdl.QuitSubSystem(SDL_INIT_VIDEO);
		sdl_video_initialized = FALSE;
	}
#endif

	if(curs_initciolib(mode)) {
		if(mode==CIOLIB_MODE_AUTO)
			mode=CIOLIB_MODE_CURSES;
		cio_api.mode=mode;
		cio_api.puttext=curs_puttext;
		cio_api.gettext=curs_gettext;
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
#if defined(NCURSES_VERSION_MAJOR) || defined (__NetBSD__)
		cio_api.ESCDELAY=&ESCDELAY;
#endif
		return(1);
	}
	return(0);
}
#endif

int try_ansi_init(int mode)
{
#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
	if (sdl_video_initialized) {
		sdl.QuitSubSystem(SDL_INIT_VIDEO);
		sdl_video_initialized = FALSE;
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
		cio_api.ESCDELAY=&CIOLIB_ANSI_TIMEOUT;
		cio_api.beep=ansi_beep;
		return(1);
	}
	return(0);
}

#ifdef _WIN32
#if defined(__BORLANDC__)
        #pragma argsused
#endif
int try_conio_init(int mode)
{
#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
	if (sdl_video_initialized) {
		sdl.QuitSubSystem(SDL_INIT_VIDEO);
		sdl_video_initialized = FALSE;
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
CIOLIBEXPORT void CIOLIBCALL suspendciolib(void)
{
	ciolib_clrscr();
	if(cio_api.suspend != NULL)
		cio_api.suspend();
	initialized=-1;
}

CIOLIBEXPORT int CIOLIBCALL initciolib(int mode)
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
#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
			if(!try_sdl_init(CIOLIB_MODE_SDL))
#endif
#ifdef _WIN32
				if(!try_conio_init(mode))
#else
#ifndef NO_X
				if(!try_x_init(mode))
#endif
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

#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
		case CIOLIB_MODE_SDL:
		case CIOLIB_MODE_SDL_FULLSCREEN:
		case CIOLIB_MODE_SDL_YUV:
		case CIOLIB_MODE_SDL_YUV_FULLSCREEN:
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
	/* Default C64 is Lt Blue on Black (As per CGTerm) */
	switch(cio_textinfo.currmode) {
		case C64_40X25:
		case C128_40X25:
		case C128_80X25:
			cio_textinfo.normattr=14;
			break;
		default:
			cio_textinfo.normattr=LIGHTGRAY;
	}
	_beginthread(ciolib_mouse_thread,0,NULL);
	return(0);
}

/* **MUST** be implemented */
CIOLIBEXPORT int CIOLIBCALL ciolib_kbhit(void)
{
	CIOLIB_INIT();
	if(ungotch)
		return(1);
	return(cio_api.kbhit());
}

/* **MUST** be implemented */
CIOLIBEXPORT int CIOLIBCALL ciolib_getch(void)
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
CIOLIBEXPORT int CIOLIBCALL ciolib_getche(void)
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
CIOLIBEXPORT int CIOLIBCALL ciolib_ungetch(int ch)
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
CIOLIBEXPORT int CIOLIBCALL ciolib_movetext(int sx, int sy, int ex, int ey, int dx, int dy)
{
	int width;
	int height;
	unsigned char *buf;
	uint32_t *fgb = NULL;
	uint32_t *bgb = NULL;

	CIOLIB_INIT();

	if(cio_api.movetext != NULL)
		return(cio_api.movetext(sx, sy, ex, ey, dx, dy));

	width=ex-sx;
	height=ey-sy;
	buf=(unsigned char *)malloc((width+1)*(height+1)*2);
	if(buf==NULL)
		return(0);
	if (cio_api.pgettext) {
		fgb=(uint32_t *)malloc((width+1)*(height+1)*sizeof(fgb[0]));
		if (fgb == NULL) {
			free(buf);
			return 0;
		}

		bgb=(uint32_t *)malloc((width+1)*(height+1)*sizeof(bgb[0]));
		if (bgb == NULL) {
			free(fgb);
			free(buf);
			return 0;
		}
		if(!ciolib_pgettext(sx,sy,ex,ey,buf,fgb,bgb))
			goto fail;
		if(!ciolib_pputtext(dx,dy,dx+width,dy+height,buf,fgb,bgb))
			goto fail;
	}
	else {
		if(!ciolib_gettext(sx,sy,ex,ey,buf))
			goto fail;
		if(!ciolib_puttext(dx,dy,dx+width,dy+height,buf))
			goto fail;
	}

	return(1);

fail:
	free(buf);
	if (fgb)
		free(fgb);
	if (bgb)
		free(bgb);
	return 0;
}

/* Optional */
CIOLIBEXPORT char * CIOLIBCALL ciolib_cgets(char *str)
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

/* Optional... in fact, since it's varargs, you can't override it */
CIOLIBEXPORT int CIOLIBCALL ciolib_cscanf (char *format , ...)
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
CIOLIBEXPORT char * CIOLIBCALL ciolib_getpass(const char *prompt)
{
	static char pass[9];
	int len=0;
	int ch;

	CIOLIB_INIT();

	if(cio_api.getpass)
		return(cio_api.getpass(prompt));

	ciolib_cputs((char *)prompt);
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
CIOLIBEXPORT void CIOLIBCALL ciolib_gettextinfo(struct text_info *info)
{

	CIOLIB_INIT()

	if(cio_api.gettextinfo) {
		cio_api.gettextinfo(&cio_textinfo);
		return;
	}
	
	if(info!=&cio_textinfo)
		*info=cio_textinfo;
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_wscroll(void)
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
CIOLIBEXPORT int CIOLIBCALL ciolib_wherex(void)
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
CIOLIBEXPORT int CIOLIBCALL ciolib_wherey(void)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_gotoxy(int x, int y)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_textmode(int mode)
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
		case C128_40X25:
		case C128_80X25:
			cio_textinfo.normattr=14;
			break;
		default:
			cio_textinfo.normattr=7;
	}
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_window(int sx, int sy, int ex, int ey)
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
			|| ey > cio_textinfo.screenheight)
		return;
	cio_textinfo.winleft=sx;
	cio_textinfo.wintop=sy;
	cio_textinfo.winright=ex;
	cio_textinfo.winbottom=ey;
	ciolib_gotoxy(1,1);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_clreol(void)
{
	unsigned char *buf;
	uint32_t *fgbuf = NULL;
	uint32_t *bgbuf = NULL;
	int i;
	int width,height;

	CIOLIB_INIT();

	if(cio_api.clreol) {
		cio_api.clreol();
		return;
	}

	width=cio_textinfo.winright-cio_textinfo.winleft+1-cio_textinfo.curx+1;
	height=1;
	buf=(unsigned char *)malloc(width*height*2);
	if (!buf)
		return;
	if (cio_api.pputtext) {
		fgbuf = malloc(width*height*sizeof(fgbuf[0]));
		if (!fgbuf) {
			free(buf);
			return;
		}
		bgbuf = malloc(width*height*sizeof(bgbuf[0]));
		if (!bgbuf) {
			free(fgbuf);
			free(buf);
			return;
		}
		for (i = 0; i < width*height; i++) {
			fgbuf[i] = ciolib_fg;
			bgbuf[i] = ciolib_bg;
		}
	}
	for(i=0;i<width*height*2;) {
		buf[i++]=' ';
		buf[i++]=cio_textinfo.attribute;
	}
	ciolib_pputtext(
			cio_textinfo.curx+cio_textinfo.winleft-1,
			cio_textinfo.cury+cio_textinfo.wintop-1,
			cio_textinfo.winright,
			cio_textinfo.cury+cio_textinfo.wintop-1,
			buf, fgbuf, bgbuf);
	if (fgbuf)
		free(fgbuf);
	if (bgbuf)
		free(bgbuf);
	free(buf);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_clrscr(void)
{
	unsigned char *buf;
	uint32_t *fgbuf = NULL;
	uint32_t *bgbuf = NULL;
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
	buf=(unsigned char *)malloc(width*height*2);
	if(!buf)
		return;
	if (cio_api.pputtext) {
		fgbuf = malloc(width*height*sizeof(fgbuf[0]));
		if (!fgbuf) {
			free(buf);
			return;
		}
		bgbuf = malloc(width*height*sizeof(bgbuf[0]));
		if (!bgbuf) {
			free(fgbuf);
			free(buf);
			return;
		}
		for (i = 0; i < width*height; i++) {
			fgbuf[i] = ciolib_fg;
			bgbuf[i] = ciolib_bg;
		}
	}
	for(i=0;i<width*height*2;) {
		buf[i++]=' ';
		buf[i++]=cio_textinfo.attribute;
	}
	puttext_can_move=1;
	ciolib_pputtext(cio_textinfo.winleft,cio_textinfo.wintop,cio_textinfo.winright,cio_textinfo.winbottom,buf,fgbuf,bgbuf);
	ciolib_gotoxy(1,1);
	puttext_can_move=old_ptcm;

	if (fgbuf)
		free(fgbuf);
	if (bgbuf)
		free(bgbuf);
	free(buf);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_delline(void)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_insline(void)
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
CIOLIBEXPORT int CIOLIBCALL ciolib_cprintf(const char *fmat, ...)
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
CIOLIBEXPORT int CIOLIBCALL ciolib_cputs(char *str)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_textbackground(int colour)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_textcolor(int colour)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_highvideo(void)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_lowvideo(void)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_normvideo(void)
{
	CIOLIB_INIT();
	
	if(cio_api.normvideo) {
		cio_api.normvideo();
		return;
	}
	ciolib_textattr(cio_textinfo.normattr);
}

static char c64_bg_xlat(char colour)
{
	switch(colour) {
		case BLACK:
			return 0;
		case BLUE:
			return 6;
		case GREEN:
			return 5;
		case CYAN:
			return 3;
		case RED:
			return 2;
		case MAGENTA:
			return 4;
		case BROWN:
			return 7;
		case LIGHTGRAY:
			return 1;
	}
	return 0;
}

static char c64_bg_rev(char colour)
{
	switch(colour) {
		case 0:
			return BLACK;
		case 6:
			return BLUE;
		case 5:
			return GREEN;
		case 3:
			return CYAN;
		case 2:
			return RED;
		case 4:
			return MAGENTA;
		case 7:
			return BROWN;
		case 1:
			return LIGHTGRAY;
	}
	return 0;
}

static char c64_color_xlat(char colour)
{
	switch(colour) {
		case BLACK:
			return 0;
		case BLUE:
			return 6;
		case GREEN:
			return 5;
		case CYAN:
			return 3;
		case RED:
			return 2;
		case MAGENTA:
			return 4;
		case BROWN:
			return 9;
		case LIGHTGRAY:
			return 15;
		case DARKGRAY:
			return 11;
		case LIGHTBLUE:	
			return 14;
		case LIGHTGREEN:
			return 13;
		case LIGHTCYAN:
			return 12;	// Gray...
		case LIGHTRED:
			return 10;
		case LIGHTMAGENTA:
			return 8;
		case YELLOW:
			return 7;
		case WHITE:
			return 1;
	}
	return 0;
}

static char c64_color_rev(char colour)
{
	switch(colour) {
		case 0:
			return BLACK;
		case 6:
			return BLUE;
		case 5:
			return GREEN;
		case 3:
			return CYAN;
		case 2:
			return RED;
		case 4:
			return MAGENTA;
		case 9:
			return BROWN;
		case 15:
			return LIGHTGRAY;
		case 11:
			return DARKGRAY;
		case 14:
			return LIGHTBLUE;
		case 13:
			return LIGHTGREEN;
		case 12:
			return LIGHTCYAN;
		case 10:
			return LIGHTRED;
		case 8:
			return LIGHTMAGENTA;
		case 7:
			return YELLOW;
		case 1:
			return WHITE;
	}
	return 0;
}

static char c64_attr_xlat(unsigned char orig)
{
	return (orig & 0x80) | c64_color_xlat(orig & 0x0f) | (c64_bg_xlat((orig&0x70)>>4)<<4);
}

static char c64_attr_rev(unsigned char orig)
{
	return (orig & 0x80) | c64_color_rev(orig & 0x0f) | (c64_bg_rev((orig&0x70)>>4)<<4);
}

/* **MUST** be implemented */
CIOLIBEXPORT int CIOLIBCALL ciolib_puttext(int a,int b,int c,int d,void *e)
{
	char	*buf=e;
	int		i;
	int		font;
	int		ret;
	CIOLIB_INIT();

	if(ciolib_xlat) {
		font = ciolib_getfont();
		if (font >= 0) {
			buf=malloc((c-a+1)*(d-b+1)*2);
			if(!buf)
				return 0;
			if (conio_fontdata[font].put_xlat == NULL && cio_textinfo.currmode != C64_40X25) {
				memcpy(buf, e, (c-a+1)*(d-b+1)*2);
			}
			else {
				for (i=0; i<(c-a+1)*(d-b+1)*2; i+=2) {
					if (((char *)e)[i] > 31 && ((char *)e)[i] < 127 && conio_fontdata[font].put_xlat != NULL)
						buf[i] = conio_fontdata[font].put_xlat[((char *)e)[i]-32];
					else
						buf[i] = ((char *)e)[i];
					if (cio_textinfo.currmode == C64_40X25)
						buf[i+1]=c64_attr_xlat(((char *)e)[i+1]);
					else
						buf[i+1]=((char *)e)[i+1];
				}
			}
		}
	}
	ret = cio_api.puttext(a,b,c,d,(void *)buf);
	if (buf != e)
		free(buf);
	return ret;
}

/* **MUST** be implemented */
CIOLIBEXPORT int CIOLIBCALL ciolib_gettext(int a,int b,int c,int d,void *e)
{
	char	*ch;
	char	xlat;
	int		i;
	int		font;
	int		ret;
	CIOLIB_INIT();

	if (cio_api.gettext == NULL)
		ret = cio_api.pgettext(a,b,c,d,e,NULL,NULL);
	else
		ret = cio_api.gettext(a,b,c,d,e);
	if(ciolib_xlat) {
		font = ciolib_getfont();
		if (font >= 0) {
			if (conio_fontdata[font].put_xlat || cio_textinfo.currmode == C64_40X25) {
				for (i=0; i<(c-a+1)*(d-b+1)*2; i+=2) {
					if (conio_fontdata[font].put_xlat) {
						xlat = ((char *)e)[i];
						if ((ch = memchr(conio_fontdata[font].put_xlat, ((char *)e)[i], 128))!=NULL)
							xlat = (char)(ch-conio_fontdata[font].put_xlat)+32;
						((char *)e)[i] = xlat;
					}
					if (cio_textinfo.currmode == C64_40X25) {
							((char *)e)[i+1] = c64_attr_rev(((char *)e)[i+1]);;
					}
				}
			}
		}
	}
	return ret;
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_pgettext(int a,int b,int c,int d,void *e,uint32_t *f, uint32_t *g)
{
	CIOLIB_INIT();

	if (cio_api.pgettext == NULL)
		return ciolib_gettext(a, b, c, d, e);
	return cio_api.pgettext(a,b,c,d,e,f,g);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_pputtext(int a,int b,int c,int d,void *e,uint32_t *f, uint32_t *g)
{
	CIOLIB_INIT();

	if (cio_api.pputtext == NULL)
		return ciolib_puttext(a, b, c, d, e);
	return cio_api.pputtext(a,b,c,d,e,f,g);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_textattr(int a)
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
CIOLIBEXPORT void CIOLIBCALL ciolib_delay(long a)
{
	CIOLIB_INIT();

	if(cio_api.delay) {
		cio_api.delay(a);
		return;
	}
	SLEEP(a);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_cputch(uint32_t fg, uint32_t bg, int a)
{
	unsigned char a1=a;
	unsigned char buf[2];
	uint32_t fgbuf[1];
	uint32_t bgbuf[1];
	int i;
	int old_puttext_can_move=puttext_can_move;

	CIOLIB_INIT();

	if(cio_api.cputch)
		return(cio_api.cputch(fg, bg, a1));

	puttext_can_move=1;

	buf[0]=a1;
	buf[1]=cio_textinfo.attribute;
	fgbuf[0] = fg;
	bgbuf[0] = bg;

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
				buf[0]=' ';
				ciolib_pputtext(cio_textinfo.curx+cio_textinfo.winleft-1
						,cio_textinfo.cury+cio_textinfo.wintop-1
						,cio_textinfo.curx+cio_textinfo.winleft-1
						,cio_textinfo.cury+cio_textinfo.wintop-1
						,buf,fgbuf,bgbuf);
			}
			break;
		case 7:		/* Bell */
			ciolib_beep();
			break;
		case '\t':
			for(i=0;i<(sizeof(tabs)/sizeof(int));i++) {
				if(tabs[i]>cio_textinfo.curx) {
					buf[0]=' ';
					while(cio_textinfo.curx<tabs[i]) {
						ciolib_pputtext(cio_textinfo.curx+cio_textinfo.winleft-1
								,cio_textinfo.cury+cio_textinfo.wintop-1
								,cio_textinfo.curx+cio_textinfo.winleft-1
								,cio_textinfo.cury+cio_textinfo.wintop-1
								,buf,bgbuf,fgbuf);
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
				ciolib_pputtext(ciolib_wherex()+cio_textinfo.winleft-1
						,ciolib_wherey()+cio_textinfo.wintop-1
						,ciolib_wherex()+cio_textinfo.winleft-1
						,ciolib_wherey()+cio_textinfo.wintop-1
						,buf,fgbuf,bgbuf);
				ciolib_wscroll();
				ciolib_gotoxy(1, cio_textinfo.winbottom-cio_textinfo.wintop+1);
			}
			else {
				if(cio_textinfo.curx==cio_textinfo.winright-cio_textinfo.winleft+1) {
					ciolib_pputtext(ciolib_wherex()+cio_textinfo.winleft-1
							,ciolib_wherey()+cio_textinfo.wintop-1
							,ciolib_wherex()+cio_textinfo.winleft-1
							,ciolib_wherey()+cio_textinfo.wintop-1
							,buf,fgbuf,bgbuf);
					ciolib_gotoxy(1,cio_textinfo.cury+1);
				}
				else {
					ciolib_pputtext(ciolib_wherex()+cio_textinfo.winleft-1
							,ciolib_wherey()+cio_textinfo.wintop-1
							,ciolib_wherex()+cio_textinfo.winleft-1
							,ciolib_wherey()+cio_textinfo.wintop-1
							,buf,fgbuf,bgbuf);
					ciolib_gotoxy(cio_textinfo.curx+1, cio_textinfo.cury);
				}
			}
			break;
	}

	puttext_can_move=old_puttext_can_move;

	return(a1);
	
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_putch(int a)
{
	CIOLIB_INIT();

	if (cio_api.putch == NULL)
		return ciolib_cputch(ciolib_fg, ciolib_bg, a);
	return cio_api.putch(a);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_ccputs(uint32_t fg_palette, uint32_t bg_palette, const char *s)
{
	int		pos;
	int		ret=0;
	int		olddmc;

	CIOLIB_INIT();

	if (cio_api.ccputs != NULL)
		return cio_api.ccputs(fg_palette, bg_palette, s);

	olddmc=hold_update;
	hold_update=1;
	for(pos=0;s[pos];pos++)
	{
		ret=s[pos];
		if(s[pos]=='\n')
			ciolib_putch('\r');
		ciolib_cputch(fg_palette, bg_palette, s[pos]);
	}
	hold_update=olddmc;
	ciolib_gotoxy(ciolib_wherex(),ciolib_wherey());
	return(ret);
}

/* **MUST** be implemented */
CIOLIBEXPORT void CIOLIBCALL ciolib_setcursortype(int a)
{
	CIOLIB_INIT();

	cio_api.setcursortype(a);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_showmouse(void) {
	CIOLIB_INIT();

	if(cio_api.showmouse!=NULL)
		return(cio_api.showmouse());
	return(-1);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_hidemouse(void) {
	CIOLIB_INIT();

	if(cio_api.hidemouse!=NULL)
		return(cio_api.hidemouse());
	return(-1);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_setname(const char *name) {
	CIOLIB_INIT();

	if(cio_api.setname!=NULL)
		cio_api.setname(name);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_seticon(const void *icon, unsigned long size) {
	CIOLIB_INIT();

	if(cio_api.seticon!=NULL)
		cio_api.seticon(icon,size);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_settitle(const char *title) {
	CIOLIB_INIT();

	if(cio_api.settitle!=NULL)
		cio_api.settitle(title);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_copytext(const char *text, size_t buflen)
{
	CIOLIB_INIT();

	if(cio_api.copytext!=NULL)
		cio_api.copytext(text,buflen);
}

/* Optional */
CIOLIBEXPORT char * CIOLIBCALL ciolib_getcliptext(void)
{
	CIOLIB_INIT();

	if(cio_api.getcliptext!=NULL)
		return(cio_api.getcliptext());
	else
		return(NULL);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_setfont(int font, int force, int font_num)
{
	CIOLIB_INIT();

	if(cio_api.setfont!=NULL)
		return(cio_api.setfont(font,force,font_num));
	else
		return(CIOLIB_SETFONT_NOT_SUPPORTED);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_getfont(void)
{
	CIOLIB_INIT();

	if(cio_api.getfont!=NULL)
		return(cio_api.getfont());
	else
		return(-1);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_loadfont(char *filename)
{
	CIOLIB_INIT();

	if(cio_api.loadfont!=NULL)
		return(cio_api.loadfont(filename));
	else
		return(-1);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_get_window_info(int *width, int *height, int *xpos, int *ypos)
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
	return(-1);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_beep(void)
{
	if(cio_api.beep)
		return(cio_api.beep());
	BEEP(440,100);
	return(0);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_getcustomcursor(int *start, int *end, int *range, int *blink, int *visible)
{
	if(cio_api.getcustomcursor)
		cio_api.getcustomcursor(start,end,range,blink,visible);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_setcustomcursor(int start, int end, int range, int blink, int visible)
{
	if(cio_api.setcustomcursor)
		cio_api.setcustomcursor(start,end,range,blink,visible);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_setvideoflags(int flags)
{
	if(cio_api.setvideoflags)
		cio_api.setvideoflags(flags);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_getvideoflags(void)
{
	if(cio_api.getvideoflags)
		return(cio_api.getvideoflags());
	return(0);
}

/* Optional */
CIOLIBEXPORT void CIOLIBCALL ciolib_setscaling(int new_value)
{
	if(cio_api.setscaling)
		cio_api.setscaling(new_value);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_getscaling(void)
{
	if(cio_api.getscaling)
		return(cio_api.getscaling());
	return(1);
}

/* Optional */
CIOLIBEXPORT int CIOLIBCALL ciolib_setpalette(uint32_t entry, uint16_t r, uint16_t g, uint16_t b)
{
	if(cio_api.setpalette)
		return(cio_api.setpalette(entry, r, g, b));
	return(1);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_attr2palette(uint8_t attr, uint32_t *fg, uint32_t *bg)
{
	if (cio_api.attr2palette)
		return cio_api.attr2palette(attr, fg, bg);
	/*
	 * TODO: If we want to be able to cross screens, we need some
	 * mapping for non-plaette aware things.
	 */
	return -1;
}

CIOLIBEXPORT int CIOLIBCALL ciolib_setpixel(uint32_t x, uint32_t y, uint32_t colour)
{
	CIOLIB_INIT();

	if (cio_api.setpixel)
		return cio_api.setpixel(x, y, colour);
	return 0;
}

CIOLIBEXPORT struct ciolib_pixels * CIOLIBCALL ciolib_getpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey)
{
	CIOLIB_INIT();

	if (cio_api.getpixels)
		return cio_api.getpixels(sx, sy, ex, ey);
	return NULL;
}

CIOLIBEXPORT int CIOLIBCALL ciolib_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, struct ciolib_pixels *pixels, void *mask)
{
	CIOLIB_INIT();

	if (cio_api.setpixels)
		return cio_api.setpixels(sx, sy, ex, ey, x_off, y_off, pixels, mask);
	return 0;
}

CIOLIBEXPORT void CIOLIBCALL ciolib_freepixels(struct ciolib_pixels *pixels)
{
	if (pixels == NULL)
		return;

	FREE_AND_NULL(pixels->pixels);
	FREE_AND_NULL(pixels);
}

CIOLIBEXPORT struct ciolib_screen * CIOLIBCALL ciolib_savescreen(void)
{
	struct ciolib_screen *ret;
	int vmode;

	CIOLIB_INIT();

	ret = malloc(sizeof(*ret));
	if (ret == NULL)
		return NULL;

	ciolib_gettextinfo(&ret->text_info);
	vmode = find_vmode(ret->text_info.currmode);
	ret->vmem = malloc(vparams[vmode].cols * vparams[vmode].rows * 2);
	if (ret->vmem == NULL) {
		free(ret);
		return NULL;
	}
	ret->foreground = malloc(vparams[vmode].cols * vparams[vmode].rows * sizeof(ret->foreground[0]));
	if (ret->foreground == NULL) {
		free(ret->vmem);
		free(ret);
		return NULL;
	}
	ret->background = malloc(vparams[vmode].cols * vparams[vmode].rows * sizeof(ret->background[0]));
	if (ret->background == NULL) {
		free(ret->foreground);
		free(ret->vmem);
		free(ret);
		return NULL;
	}

	ret->pixels = ciolib_getpixels(0, 0, vparams[vmode].charwidth * vparams[vmode].cols - 1, vparams[vmode].charheight * vparams[vmode].rows - 1);
	ciolib_pgettext(1, 1, vparams[vmode].cols, vparams[vmode].rows, ret->vmem, ret->foreground, ret->background);
	ret->fg_colour = ciolib_fg;
	ret->bg_colour = ciolib_bg;

	return ret;
}

CIOLIBEXPORT void CIOLIBCALL ciolib_freescreen(struct ciolib_screen *scrn)
{
	if (scrn == NULL)
		return;

	ciolib_freepixels(scrn->pixels);
	FREE_AND_NULL(scrn->background);
	FREE_AND_NULL(scrn->foreground);
	FREE_AND_NULL(scrn->vmem);
	free(scrn);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_restorescreen(struct ciolib_screen *scrn)
{
	struct text_info ti;
	int vmode;

	CIOLIB_INIT();

	ciolib_gettextinfo(&ti);

	if (ti.currmode != scrn->text_info.currmode)
		ciolib_textmode(scrn->text_info.currmode);
	ciolib_pputtext(1, 1, scrn->text_info.screenwidth, scrn->text_info.screenheight, scrn->vmem, scrn->foreground, scrn->background);
	ciolib_gotoxy(scrn->text_info.curx, scrn->text_info.cury);
	ciolib_textcolor(scrn->text_info.attribute);
	ciolib_window(scrn->text_info.winleft, scrn->text_info.wintop, scrn->text_info.winright, scrn->text_info.winbottom);
	vmode = find_vmode(scrn->text_info.currmode);
	ciolib_setpixels(0, 0, vparams[vmode].charwidth * vparams[vmode].cols - 1, vparams[vmode].charheight * vparams[vmode].rows - 1, 0, 0, scrn->pixels, NULL);
	ciolib_setcolour(scrn->fg_colour, scrn->bg_colour);
	return 1;
}

CIOLIBEXPORT void CIOLIBCALL ciolib_setcolour(uint32_t fg, uint32_t bg)
{
	ciolib_fg = fg;
	ciolib_bg = bg;
}
