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

#ifndef _CIOLIB_H_
#define _CIOLIB_H_

#include <string.h>	/* size_t */
#include "mouse.h"

enum {
	 CIOLIB_MODE_AUTO
	,CIOLIB_MODE_CURSES
	,CIOLIB_MODE_CURSES_IBM
	,CIOLIB_MODE_ANSI
	,CIOLIB_MODE_X
	,CIOLIB_MODE_CONIO
	,CIOLIB_MODE_SDL
	,CIOLIB_MODE_SDL_FULLSCREEN
};

#if defined(_WIN32)	/* presumably, Win32 */

	#include <io.h>			/* isatty */

#endif

enum {
	 BLACK
	,BLUE
	,GREEN
	,CYAN
	,RED
	,MAGENTA
	,BROWN	
	,LIGHTGRAY	
	,DARKGRAY
	,LIGHTBLUE	
	,LIGHTGREEN	
	,LIGHTCYAN
	,LIGHTRED
	,LIGHTMAGENTA
	,YELLOW
	,WHITE
};
#define BLINK 128

enum text_modes
{
    /* DOS-compatible modes */

    LASTMODE = -1,
    BW40     = 0,
    C40,
    BW80,
    C80,
    MONO     = 7,

    /* New Color modes */

    C40X14   = 8,
    C40X21,
    C40X28,
    C40X43,
    C40X50,
    C40X60,

    C80X14,
    C80X21,
    C80X28,
    C80X43,
    C80X50,
    C80X60,

    /* New Black & White modes */

    BW40X14,
    BW40X21,
    BW40X28,
    BW40X43,
    BW40X50,
    BW40X60,

    BW80X14,
    BW80X21,
    BW80X28,
    BW80X43,
    BW80X50,
    BW80X60,

    /* New Monochrome modes */

    MONO14,             /* Invalid VGA mode */
    MONO21,
    MONO28,
    MONO43,
    MONO50,
    MONO60,

    C4350    = C80X50,	/* this is actually "64" in the "real" conio */

    _ORIGMODE = 65      /* original mode at program startup */
};

#define COLOR_MODE	C80

enum
{
	_NOCURSOR,
	_SOLIDCURSOR,
	_NORMALCURSOR
};

struct text_info {
	unsigned char winleft;        /* left window coordinate */
	unsigned char wintop;         /* top window coordinate */
	unsigned char winright;       /* right window coordinate */
	unsigned char winbottom;      /* bottom window coordinate */
	unsigned char attribute;      /* text attribute */
	unsigned char normattr;       /* normal attribute */
	unsigned char currmode;       /* current video mode:
                                	 BW40, BW80, C40, C80, or C4350 */
	unsigned char screenheight;   /* text screen's height */
	unsigned char screenwidth;    /* text screen's width */
	unsigned char curx;           /* x-coordinate in current window */
	unsigned char cury;           /* y-coordinate in current window */
};

typedef struct {
	int		mode;
	int		mouse;
	void	(*clreol)		(void);
	int		(*puttext)		(int,int,int,int,void *);
	int		(*gettext)		(int,int,int,int,void *);
	void	(*textattr)		(int);
	int		(*kbhit)		(void);
	void	(*delay)		(long);
	int		(*wherex)		(void);
	int		(*wherey)		(void);
	int		(*putch)		(int);
	void	(*gotoxy)		(int,int);
	void	(*clrscr)		(void);
	void	(*gettextinfo)	(struct text_info *);
	void	(*setcursortype)(int);
	int		(*getch)		(void);
	int		(*getche)		(void);
	int		(*beep)			(void);
	void	(*highvideo)	(void);
	void	(*lowvideo)		(void);
	void	(*normvideo)	(void);
	void	(*textmode)		(int);
	int		(*ungetch)		(int);
	int		(*movetext)		(int,int,int,int,int,int);
	char	*(*cgets)		(char *);
	int		(*cscanf)		(char *,...);
	char	*(*getpass)		(const char *);
	void	(*wscroll)		(void);
	void	(*window)		(int,int,int,int);
	void	(*delline)		(void);
	void	(*insline)		(void);
	int		(*cprintf)		(char *,...);
	int		(*cputs)		(char *);
	void	(*textbackground)	(int);
	void	(*textcolor)	(int);
	int		(*getmouse)		(struct mouse_event *mevent);
	int		(*ungetmouse)	(struct mouse_event *mevent);
	int		(*hidemouse)	(void);
	int		(*showmouse)	(void);
	void	(*settitle)		(const char *);
	void	(*setname)		(const char *);
	void	(*copytext)		(const char *, size_t);
	char 	*(*getcliptext)	(void);
} cioapi_t;

extern cioapi_t cio_api;
extern int _wscroll;
extern int directvideo;
extern int hold_update;

#define _conio_kbhit()		kbhit()

#ifdef __cplusplus
extern "C" {
#endif
int initciolib(int mode);

int ciolib_movetext(int sx, int sy, int ex, int ey, int dx, int dy);
char *ciolib_cgets(char *str);
int ciolib_cscanf (char *format , ...);
int ciolib_kbhit(void);
int ciolib_getch(void);
int ciolib_getche(void);
int ciolib_ungetch(int ch);
void ciolib_gettextinfo(struct text_info *info);
int ciolib_wherex(void);
int ciolib_wherey(void);
void ciolib_wscroll(void);
void ciolib_gotoxy(int x, int y);
void ciolib_clreol(void);
void ciolib_clrscr(void);
int ciolib_cputs(char *str);
int	ciolib_cprintf(char *fmat, ...);
void ciolib_textbackground(int colour);
void ciolib_textcolor(int colour);
void ciolib_highvideo(void);
void ciolib_lowvideo(void);
void ciolib_normvideo(void);
int ciolib_puttext(int a,int b,int c,int d,unsigned char *e);
int ciolib_gettext(int a,int b,int c,int d,unsigned char *e);
void ciolib_textattr(int a);
void ciolib_delay(long a);
int ciolib_putch(int a);
void ciolib_setcursortype(int a);
void ciolib_textmode(int mode);
void ciolib_window(int sx, int sy, int ex, int ey);
void ciolib_delline(void);
void ciolib_insline(void);
char *ciolib_getpass(const char *prompt);
void ciolib_settitle(const char *title);
void ciolib_setname(const char *title);
int ciolib_showmouse(void);
int ciolib_hidemouse(void);
void ciolib_copytext(const char *text, size_t buflen);
char *ciolib_getcliptext(void);
#ifdef __cplusplus
}
#endif

#ifndef CIOLIB_NO_MACROS
	#define cscanf					ciolib_cscanf
	#define cprintf					ciolib_cprintf

	#define movetext(a,b,c,d,e,f)	ciolib_movetext(a,b,c,d,e,f)
	#define cgets(a)				ciolib_cgets(a)
	#define kbhit()					ciolib_kbhit()
	#define getch()					ciolib_getch()
	#define getche()				ciolib_getche()
	#define ungetch(a)				ciolib_ungetch(a)
	#define gettextinfo(a)			ciolib_gettextinfo(a)
	#define wherex()				ciolib_wherex()
	#define wherey()				ciolib_wherey()
	#define	wscroll()				ciolib_wscroll()
	#define gotoxy(a,b)				ciolib_gotoxy(a,b)
	#define clreol()				ciolib_clreol()
	#define clrscr()				ciolib_clrscr()
	#define cputs(a)				ciolib_cputs(a)
	#define textbackground(a)		ciolib_textbackground(a)
	#define textcolor(a)			ciolib_textcolor(a)
	#define highvideo()				ciolib_highvideo()
	#define lowvideo()				ciolib_lowvideo()
	#define normvideo()				ciolib_normvideo()
	#define puttext(a,b,c,d,e)		ciolib_puttext(a,b,c,d,e)
	#define gettext(a,b,c,d,e)		ciolib_gettext(a,b,c,d,e)
	#define textattr(a)				ciolib_textattr(a)
	#define delay(a)				ciolib_delay(a)
	#define putch(a)				ciolib_putch(a)
	#define _setcursortype(a)		ciolib_setcursortype(a)
	#define textmode(a)				ciolib_textmode(a)
	#define window(a,b,c,d)			ciolib_window(a,b,c,d)
	#define delline()				ciolib_delline()
	#define insline()				ciolib_insline()
	#define getpass(a)				ciolib_getpass(a)
	#define getmouse(a)				ciolib_getmouse(a)
	#define ungetmouse(a)			ciolib_ungetmouse(a)
	#define	hidemouse()				ciolib_hidemouse()
	#define showmouse()				ciolib_showmouse()
	#define setname(a)				ciolib_setname(a)
	#define settitle(a)				ciolib_settitle(a)
	#define copytext(a,b)			ciolib_copytext(a,b)
	#define getcliptext()		ciolib_getcliptext()
#endif

/* Special hackery for SDL */
#ifdef WITH_SDL
	#define	main	CIOLIB_main
#endif

#endif	/* Do not add anything after this line */
