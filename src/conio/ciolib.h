/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifdef CIOLIBEXPORT
        #undef CIOLIBEXPORT
#endif

#ifdef _WIN32
        #ifdef __BORLANDC__
                #define CIOLIBCALL __stdcall
        #else
                #define CIOLIBCALL
        #endif
        #if defined(CIOLIB_IMPORTS) || defined(CIOLIB_EXPORTS)
                #if defined(CIOLIB_IMPORTS)
                        #define CIOLIBEXPORT __declspec( dllimport )
                        #define CIOLIBEXPORTVAR __declspec( dllimport )
                #else
                        #define CIOLIBEXPORT __declspec( dllexport )
                        #define CIOLIBEXPORTVAR __declspec( dllexport )
                #endif
        #else   /* self-contained executable */
                #define CIOLIBEXPORT
                #define CIOLIBEXPORTVAR	extern
        #endif
#elif defined __unix__
        #define CIOLIBCALL
        #define CIOLIBEXPORT
        #define CIOLIBEXPORTVAR	extern
#else
        #define CIOLIBCALL
        #define CIOLIBEXPORT
        #define CIOLIBEXPORTVAR	extern
#endif

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

#ifndef __COLORS
#define __COLORS

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

#endif	/* __COLORS */

#ifndef BLINK
#define BLINK 128
#endif

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
	void	(*suspend)		(void);
	void	(*resume)		(void);
	int		(*setfont)		(int font, int force);
} cioapi_t;

CIOLIBEXPORTVAR cioapi_t cio_api;
CIOLIBEXPORTVAR int _wscroll;
CIOLIBEXPORTVAR int directvideo;
CIOLIBEXPORTVAR int hold_update;

#define _conio_kbhit()		kbhit()

#ifdef __cplusplus
extern "C" {
#endif
CIOLIBEXPORT int CIOLIBCALL initciolib(int mode);
CIOLIBEXPORT void CIOLIBCALL suspendciolib(void);

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
CIOLIBEXPORT void CIOLIBCALL ciolib_settitle(const char *title);
CIOLIBEXPORT void CIOLIBCALL ciolib_setname(const char *title);
CIOLIBEXPORT int CIOLIBCALL ciolib_showmouse(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_hidemouse(void);
CIOLIBEXPORT void CIOLIBCALL ciolib_copytext(const char *text, size_t buflen);
CIOLIBEXPORT char * CIOLIBCALL ciolib_getcliptext(void);
CIOLIBEXPORT int CIOLIBCALL ciolib_setfont(int font, int force);
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
	#define getcliptext()			ciolib_getcliptext()
	#define setfont(a,b)			ciolib_setfont(a,b)
#endif

/* Special hackery for SDL */
#ifdef WITH_SDL
	#define	main	CIOLIB_main
#endif

#endif	/* Do not add anything after this line */
