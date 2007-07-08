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

/* Icon file! */
#ifdef __BORLANDC__
#pragma resource "ciolib.res"
#endif

#include <stdarg.h>
#include <stdlib.h>	/* alloca */
#include <stdio.h>
#if defined(_WIN32)
 #include <malloc.h>	/* alloca() on Win32 */
#endif

#include <threadwrap.h>

#define CIOLIB_NO_MACROS
#include "ciolib.h"

#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
 #include "sdl_con.h"
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

#include "ansi_cio.h"

CIOLIBEXPORT cioapi_t	cio_api;

static int ungotch;
static struct text_info cio_textinfo;
static int lastmode=3;
CIOLIBEXPORT int _wscroll=1;
CIOLIBEXPORT int directvideo=0;
CIOLIBEXPORT int hold_update=0;
CIOLIBEXPORT int puttext_can_move=0;
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
CIOLIBEXPORT int	CIOLIBCALL ciolib_cprintf(char *fmat, ...);
CIOLIBEXPORT void CIOLIBCALL ciolib_textbackground(int colour);
CIOLIBEXPORT void CIOLIBCALL ciolib_textcolor(int colour);
CIOLIBEXPORT void CIOLIBCALL ciolib_highvideo(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_lowvideo(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_normvideo(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_puttext(int a,int b,int c,int d,unsigned char *e);
CIOLIBEXPORT int CIOLIBCALL ciolib_gettext(int a,int b,int c,int d,unsigned char *e);
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

#define CIOLIB_INIT()		{ if(initialized != 1) initciolib(CIOLIB_MODE_AUTO); }

#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
int try_sdl_init(int mode)
{
	if(!sdl_initciolib(mode)) {
		cio_api.mouse=1;
		cio_api.puttext=sdl_puttext;
		cio_api.gettext=sdl_gettext;
		cio_api.textattr=sdl_textattr;
		cio_api.kbhit=sdl_kbhit;
		cio_api.delay=sdl_delay;
		cio_api.wherey=sdl_wherey;
		cio_api.wherex=sdl_wherex;
		cio_api.putch=sdl_putch;
		cio_api.gotoxy=sdl_gotoxy;
		cio_api.gettextinfo=sdl_gettextinfo;
		cio_api.setcursortype=sdl_setcursortype;
		cio_api.getch=sdl_getch;
		cio_api.getche=sdl_getche;
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
		cio_api.setfont=sdl_setfont;
		cio_api.getfont=sdl_getfont;
		cio_api.loadfont=sdl_loadfont;
		return(1);
	}
	return(0);
}
#endif

#ifndef _WIN32
 #ifndef NO_X
int try_x_init(int mode)
{
	if(!console_init()) {
		cio_api.mode=CIOLIB_MODE_X;
		cio_api.mouse=1;
		cio_api.puttext=x_puttext;
		cio_api.gettext=x_gettext;
		cio_api.textattr=x_textattr;
		cio_api.kbhit=x_kbhit;
		cio_api.delay=x_delay;
		cio_api.wherey=x_wherey;
		cio_api.wherex=x_wherex;
		cio_api.putch=x_putch;
		cio_api.gotoxy=x_gotoxy;
		cio_api.gettextinfo=x_gettextinfo;
		cio_api.setcursortype=x_setcursortype;
		cio_api.getch=x_getch;
		cio_api.getche=x_getche;
		cio_api.textmode=x_textmode;
		cio_api.setname=x_setname;
		cio_api.settitle=x_settitle;
		cio_api.copytext=x_copytext;
		cio_api.getcliptext=x_getcliptext;
		cio_api.setfont=x_setfont;
		cio_api.getfont=x_getfont;
		cio_api.loadfont=x_loadfont;
		return(1);
	}
	return(0);
}
 #endif

int try_curses_init(int mode)
{
	if(curs_initciolib(mode)) {
		cio_api.mode=CIOLIB_MODE_CURSES_IBM;
		cio_api.puttext=curs_puttext;
		cio_api.gettext=curs_gettext;
		cio_api.textattr=curs_textattr;
		cio_api.kbhit=curs_kbhit;
		cio_api.delay=curs_delay;
		cio_api.wherey=curs_wherey;
		cio_api.wherex=curs_wherex;
		cio_api.putch=curs_putch;
		cio_api.gotoxy=curs_gotoxy;
		cio_api.gettextinfo=curs_gettextinfo;
		cio_api.setcursortype=curs_setcursortype;
		cio_api.getch=curs_getch;
		cio_api.getche=curs_getche;
		cio_api.textmode=curs_textmode;
		cio_api.showmouse=curs_showmouse;
		cio_api.hidemouse=curs_hidemouse;
		cio_api.suspend=curs_suspend;
		cio_api.resume=curs_resume;
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
	if(ansi_initciolib(mode)) {
		cio_api.mode=CIOLIB_MODE_ANSI;
		cio_api.mouse=0;
		cio_api.puttext=ansi_puttext;
		cio_api.gettext=ansi_gettext;
		cio_api.textattr=ansi_textattr;
		cio_api.kbhit=ansi_kbhit;
		cio_api.delay=ansi_delay;
		cio_api.wherey=ansi_wherey;
		cio_api.wherex=ansi_wherex;
		cio_api.putch=ansi_putch;
		cio_api.gotoxy=ansi_gotoxy;
		cio_api.gettextinfo=ansi_gettextinfo;
		cio_api.setcursortype=ansi_setcursortype;
		cio_api.getch=ansi_getch;
		cio_api.getche=ansi_getche;
		cio_api.textmode=ansi_textmode;
		cio_api.ESCDELAY=&CIOLIB_ANSI_TIMEOUT;
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
	/* This should test for something or other */
	if(win32_initciolib(mode)) {
		cio_api.mode=CIOLIB_MODE_CONIO;
		cio_api.mouse=1;
		cio_api.puttext=win32_puttext;
		cio_api.gettext=win32_gettext;
		cio_api.textattr=win32_textattr;
		cio_api.kbhit=win32_kbhit;
		cio_api.delay=win32_delay;
		cio_api.wherey=win32_wherey;
		cio_api.wherex=win32_wherex;
		cio_api.putch=win32_putch;
		cio_api.gotoxy=win32_gotoxy;
		cio_api.gettextinfo=win32_gettextinfo;
		cio_api.setcursortype=win32_setcursortype;
		cio_api.getch=win32_getch;
		cio_api.getche=win32_getche;
		cio_api.textmode=win32_textmode;
		cio_api.showmouse=win32_showmouse;
		cio_api.hidemouse=win32_hidemouse;
		cio_api.setname=win32_settitle;
		cio_api.settitle=win32_settitle;
		cio_api.copytext=win32_copytext;
		cio_api.getcliptext=win32_getcliptext;
		cio_api.suspend=win32_suspend;
		cio_api.resume=win32_resume;
		return(1);
	}
	return(0);
}
#endif

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
			if(!try_sdl_init(mode))
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
			try_sdl_init(mode);
			break;
#endif
	}
	if(cio_api.mode==CIOLIB_MODE_AUTO) {
		fprintf(stderr,"CIOLIB initialization failed!\n");
		return(-1);
	}

	initialized=1;
	ciolib_gettextinfo(&cio_textinfo);
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
			cio_textinfo.normattr=7;
	}
	_beginthread(ciolib_mouse_thread,0,NULL);
	return(0);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_kbhit(void)
{
	CIOLIB_INIT();
	if(ungotch)
		return(1);
	return(cio_api.kbhit());
}

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
	return(cio_api.getche());
}

CIOLIBEXPORT int CIOLIBCALL ciolib_ungetch(int ch)
{
	CIOLIB_INIT();
	
	if(ungotch)
		return(EOF);
	ungotch=ch;
	return(ch);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_movetext(int sx, int sy, int ex, int ey, int dx, int dy)
{
	int width;
	int height;
	unsigned char *buf;

	CIOLIB_INIT();

	if(cio_api.movetext != NULL)
		return(cio_api.movetext(sx, sy, ex, ey, dx, dy));

	width=ex-sx;
	height=ey-sy;
	buf=(unsigned char *)alloca((width+1)*(height+1)*2);
	if(buf==NULL)
		return(0);
	if(!ciolib_gettext(sx,sy,ex,ey,buf))
		return(0);
	if(!ciolib_puttext(dx,dy,dx+width,dy+height,buf))
		return(0);
	return(1);
}

CIOLIBEXPORT char * CIOLIBCALL ciolib_cgets(char *str)
{
	int	maxlen;
	int len=0;
	int ch;

	CIOLIB_INIT();

	maxlen=*(unsigned char *)str;
	while((ch=ciolib_getch())!='\n' && ch !='\r') {
		switch(ch) {
			case 0:	/* Skip extended keys */
				ciolib_getche();
				break;
			case '\r':	/* Skip \r (ToDo: Should this be treated as a \n? */
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
	str[len+2]=0;
	*((unsigned char *)(str+1))=(unsigned char)len;
	ciolib_putch('\r');
	ciolib_putch('\n');
	return(&str[2]);
}

#ifdef _MSC_VER	/* Use lame vsscanf() implementation */
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

CIOLIBEXPORT char * CIOLIBCALL ciolib_getpass(const char *prompt)
{
	static char pass[9];
	int len=0;
	int ch;

	CIOLIB_INIT();
	
	ciolib_cputs((char *)prompt);
	while((ch=ciolib_getch())!='\n') {
		switch(ch) {
			case 0:	/* Skip extended keys */
				ciolib_getch();
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
	pass[len]=0;
	return(pass);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_gettextinfo(struct text_info *info)
{
	if(!initialized)
		initciolib(CIOLIB_MODE_AUTO);
	else {
		cio_api.gettextinfo(&cio_textinfo);
	}
	if(info!=&cio_textinfo) {
		info->winleft=cio_textinfo.winleft;        /* left window coordinate */
		info->wintop=cio_textinfo.wintop;         /* top window coordinate */
		info->winright=cio_textinfo.winright;       /* right window coordinate */
		info->winbottom=cio_textinfo.winbottom;      /* bottom window coordinate */
		info->attribute=cio_textinfo.attribute;      /* text attribute */
		info->normattr=cio_textinfo.normattr;       /* normal attribute */
		info->currmode=cio_textinfo.currmode;       /* current video mode:
                               			 BW40, BW80, C40, C80, or C4350 */
		info->screenheight=cio_textinfo.screenheight;   /* text screen's height */
		info->screenwidth=cio_textinfo.screenwidth;    /* text screen's width */
		info->curx=cio_textinfo.curx-cio_textinfo.winleft+1;           /* x-coordinate in current window */
		info->cury=cio_textinfo.cury-cio_textinfo.wintop+1;           /* y-coordinate in current window */
	}
}

CIOLIBEXPORT void CIOLIBCALL ciolib_wscroll(void)
{
	int os;
	struct text_info ti;

	CIOLIB_INIT();

	if(cio_api.wscroll!=NULL) {
		cio_api.wscroll();
		return;
	}
	ciolib_gettextinfo(&ti);
	if(!_wscroll)
		return;
	ciolib_movetext(ti.winleft,ti.wintop+1,ti.winright,ti.winbottom,ti.winleft,ti.wintop);
	ciolib_gotoxy(1,ti.winbottom-ti.wintop+1);
	os=_wscroll;
	_wscroll=0;
	/* ciolib_cprintf("%*s",ti.winright-ti.winleft+1,""); */
	ciolib_clreol();
	_wscroll=os;
	ciolib_gotoxy(ti.curx,ti.cury);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_wherex(void)
{
	int x;

	CIOLIB_INIT();
	
	x=cio_api.wherex();
	x=x-cio_textinfo.winleft+1;
	return(x);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_wherey(void)
{
	int y;

	CIOLIB_INIT();
	
	y=cio_api.wherey();
	y=y-cio_textinfo.wintop+1;
	return(y);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_gotoxy(int x, int y)
{
	int nx;
	int ny;
	struct text_info ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);
	if(		x < 1
			|| x > ti.winright-ti.winleft+1
			|| y < 1
			|| y > ti.winbottom-ti.wintop+1)
		return;
	nx=x+ti.winleft-1;
	ny=y+ti.wintop-1;
	cio_api.gotoxy(nx,ny);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_textmode(int mode)
{
	CIOLIB_INIT();
	
	if(mode==-1) {
		ciolib_gettextinfo(&cio_textinfo);
		cio_api.textmode(lastmode);
		lastmode=cio_textinfo.currmode;
	}
	else {
		ciolib_gettextinfo(&cio_textinfo);
		lastmode=cio_textinfo.currmode;
		cio_api.textmode(mode);
	}
	ciolib_gettextinfo(&cio_textinfo);
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

CIOLIBEXPORT void CIOLIBCALL ciolib_window(int sx, int sy, int ex, int ey)
{
	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
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

CIOLIBEXPORT void CIOLIBCALL ciolib_clreol(void)
{
	unsigned char *buf;
	int i;
	int width,height;
	struct text_info ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);

	width=ti.winright-ti.winleft+1-ti.curx+1;
	height=1;
	buf=(unsigned char *)alloca(width*height*2);
	for(i=0;i<width*height*2;) {
		buf[i++]=' ';
		buf[i++]=ti.attribute;
	}
	ciolib_puttext(ti.curx+ti.winleft-1,ti.cury+ti.wintop-1,ti.winright,ti.cury+ti.wintop-1,buf);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_clrscr(void)
{
	unsigned char *buf;
	int i;
	int width,height;
	struct text_info ti;
	int old_ptcm=puttext_can_move;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);

	width=ti.winright-ti.winleft+1;
	height=ti.winbottom-ti.wintop+1;
	buf=(unsigned char *)alloca(width*height*2);
	for(i=0;i<width*height*2;) {
		buf[i++]=' ';
		buf[i++]=ti.attribute;
	}
	puttext_can_move=1;
	ciolib_puttext(ti.winleft,ti.wintop,ti.winright,ti.winbottom,buf);
	ciolib_gotoxy(1,1);
	puttext_can_move=old_ptcm;;
}

CIOLIBEXPORT void CIOLIBCALL ciolib_delline(void)
{
	struct text_info ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);

	ciolib_movetext(ti.winleft,ti.cury+1,ti.winright,ti.winbottom,ti.winleft,ti.cury);
	ciolib_gotoxy(1,ti.winbottom-ti.wintop+1);
	ciolib_clreol();
	ciolib_gotoxy(ti.curx,ti.cury);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_insline(void)
{
	struct text_info ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);

	ciolib_movetext(ti.winleft,ti.cury,ti.winright,ti.winbottom,ti.winleft,ti.cury+1);
	ciolib_gotoxy(1,ti.cury);
	ciolib_clreol();
	ciolib_gotoxy(ti.curx,ti.cury);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_cprintf(char *fmat, ...)
{
    va_list argptr;
	int		ret;
#ifdef _MSC_VER		/* Can't figure out a way to allocate a "big enough" buffer for Win32. */
	char	str[16384];
#else
	char	*str;
	va_list argptr2;
#endif

	CIOLIB_INIT();

    va_start(argptr,fmat);
#ifdef _MSC_VER
	ret=_vsnprintf(str,sizeof(str)-1,fmat,argptr);
#else
	va_copy(argptr2, argptr);
    ret=vsnprintf(NULL,0,fmat,argptr);
	if(ret<0)
		return(EOF);
	str=(char *)alloca(ret+1);
	if(str==NULL)
		return(EOF);
	ret=vsprintf(str,fmat,argptr2);
#endif
    va_end(argptr);
	if(ret>=0)
		ciolib_cputs(str);
	else
		ret=EOF;
    return(ret);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_cputs(char *str)
{
	int		pos;
	int		ret=0;
	int		olddmc;

	CIOLIB_INIT();

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

CIOLIBEXPORT void CIOLIBCALL ciolib_textbackground(int colour)
{
	unsigned char attr;
	unsigned char col;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr&=143;
	col=(colour & 0x07);
	attr|=(col<<4);
	ciolib_textattr(attr);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_textcolor(int colour)
{
	unsigned char attr;
	unsigned char col;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr&=240;
	col=colour&0x0f;
	attr|=col;
	ciolib_textattr(attr);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_highvideo(void)
{
	int attr;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr |= 8;
	ciolib_textattr(attr);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_lowvideo(void)
{
	int attr;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr &= 0xf7;
	ciolib_textattr(attr);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_normvideo(void)
{
	CIOLIB_INIT();
	
	ciolib_textattr(0x07);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_puttext(int a,int b,int c,int d,unsigned char *e)
{
	CIOLIB_INIT();
	
	return(cio_api.puttext(a,b,c,d,e));
}

CIOLIBEXPORT int CIOLIBCALL ciolib_gettext(int a,int b,int c,int d,unsigned char *e)
{
	CIOLIB_INIT();
	
	return(cio_api.gettext(a,b,c,d,e));
}

CIOLIBEXPORT void CIOLIBCALL ciolib_textattr(int a)
{
	CIOLIB_INIT();
	
	cio_api.textattr(a);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_delay(long a)
{
	CIOLIB_INIT();
	
	cio_api.delay(a);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_putch(int a)
{
	unsigned char a1=a;
	CIOLIB_INIT();

	if(a1=='\n')
		return(cio_api.putch(a1));
	return(cio_api.putch(a1));
}

CIOLIBEXPORT void CIOLIBCALL ciolib_setcursortype(int a)
{
	CIOLIB_INIT();
	
	cio_api.setcursortype(a);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_showmouse(void) {
	CIOLIB_INIT();

	if(cio_api.showmouse!=NULL)
		return(cio_api.showmouse());
	return(-1);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_hidemouse(void) {
	CIOLIB_INIT();

	if(cio_api.hidemouse!=NULL)
		return(cio_api.hidemouse());
	return(-1);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_setname(const char *name) {
	CIOLIB_INIT();

	if(cio_api.setname!=NULL)
		cio_api.setname(name);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_seticon(const void *icon, unsigned long size) {
	CIOLIB_INIT();

	if(cio_api.seticon!=NULL)
		cio_api.seticon(icon,size);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_settitle(const char *title) {
	CIOLIB_INIT();

	if(cio_api.settitle!=NULL)
		cio_api.settitle(title);
}

CIOLIBEXPORT void CIOLIBCALL ciolib_copytext(const char *text, size_t buflen)
{
	CIOLIB_INIT();

	if(cio_api.copytext!=NULL)
		cio_api.copytext(text,buflen);
}

CIOLIBEXPORT char * CIOLIBCALL ciolib_getcliptext(void)
{
	CIOLIB_INIT();

	if(cio_api.getcliptext!=NULL)
		return(cio_api.getcliptext());
	else
		return(NULL);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_setfont(int font, int force)
{
	CIOLIB_INIT();

	if(cio_api.setfont!=NULL)
		return(cio_api.setfont(font,force));
	else
		return(-1);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_getfont(void)
{
	CIOLIB_INIT();

	if(cio_api.getfont!=NULL)
		return(cio_api.getfont());
	else
		return(-1);
}

CIOLIBEXPORT int CIOLIBCALL ciolib_loadfont(char *filename)
{
	CIOLIB_INIT();

	if(cio_api.loadfont!=NULL)
		return(cio_api.loadfont(filename));
	else
		return(-1);
}
