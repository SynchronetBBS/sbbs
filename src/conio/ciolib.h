/* $Id: ciolib.h,v 1.125 2020/06/27 00:04:44 deuce Exp $ */

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

#ifndef _CIOLIB_H_
#define _CIOLIB_H_

#include <string.h>	/* size_t */
#if defined(__DARWIN__)
#include <semwrap.h>
#endif
#include "gen_defs.h"
#include "utf8_codepages.h"

#ifdef CIOLIBEXPORT
        #undef CIOLIBEXPORT
#endif

#ifdef _WIN32
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
        #define CIOLIBEXPORT
        #define CIOLIBEXPORTVAR	extern
#else
        #define CIOLIBEXPORT
        #define CIOLIBEXPORTVAR	extern
#endif

enum {
	 CIOLIB_MODE_AUTO
	,CIOLIB_MODE_CURSES
	,CIOLIB_MODE_CURSES_IBM
	,CIOLIB_MODE_CURSES_ASCII
	,CIOLIB_MODE_ANSI
	,CIOLIB_MODE_X
	,CIOLIB_MODE_CONIO
	,CIOLIB_MODE_CONIO_FULLSCREEN
	,CIOLIB_MODE_SDL
	,CIOLIB_MODE_SDL_FULLSCREEN
};

enum ciolib_mouse_ptr {
	 CIOLIB_MOUSEPTR_ARROW
	,CIOLIB_MOUSEPTR_BAR
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

#define CIOLIB_VIDEO_ALTCHARS             (1<<0)	// Attribute bit 3 selects alternate char set
#define CIOLIB_VIDEO_NOBRIGHT             (1<<1)	// Attribute bit 3 does not increase intensity
#define CIOLIB_VIDEO_BGBRIGHT             (1<<2)	// Attribute bit 7 selects high intensity background, not blink
#define CIOLIB_VIDEO_BLINKALTCHARS        (1<<3)	// Attribute bit 7 selects alternate char set
#define CIOLIB_VIDEO_NOBLINK              (1<<4)	// Attribute bit 7 has no effect
#define CIOLIB_VIDEO_EXPAND               (1<<5)	// Use an extra blank column between characters from the font
#define CIOLIB_VIDEO_LINE_GRAPHICS_EXPAND (1<<6)	// Per VGA, when using CIOLIB_VIDEO_EXPAND, repeat the last column for chars 0xC0 - 0xDF inclusive

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
    C80X30,
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
    MONO60,		// 38

	/* New modes we've added 'cause they're cool */

	ST132X37_16_9,
	ST132X52_5_4,

	/* New modes we've added 'cause they're stupid */
	VGA80X25,

	/* Cruft... */

	C4350    = C80X50,	/* this is actually "64" in the "real" conio */

    _ORIGMODE = 65,      /* original mode at program startup */

	EGA80X25,	/* 80x25 in 640x350 screen */
    C64_40X25 = 147,	/* Commodore 64 40x25 colour mode */
    C128_40X25,		/* Commodore 128 40x25 colour mode */
    C128_80X25,		/* Commodore 128 40x25 colour mode */
	ATARI_40X24,	/* Atari 800 40x24 colour text mode */
	ATARI_80X25,	/* Atari 800 XEP80 80x25 mono text mode */

	/* VESA Modes */
	VESA_132X21	= 235,
	VESA_132X25	= 231,
	VESA_132X28	= 228,
	VESA_132X30	= 226,
	VESA_132X34	= 222,
	VESA_132X43	= 213,
	VESA_132X50	= 206,
	VESA_132X60	= 196,

	/* Custom Mode */
	CIOLIB_MODE_CUSTOM = 255,	// Last mode... if it's over 255, text_info can't hold it.
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

CIOLIBEXPORTVAR struct text_info cio_textinfo;
CIOLIBEXPORTVAR uint32_t ciolib_fg;
CIOLIBEXPORTVAR uint32_t ciolib_bg;

struct mouse_event {
	int event;
	int bstate;
	int kbsm;
	int startx;
	int starty;
	int endx;
	int endy;
	int startx_res;
	int starty_res;
	int endx_res;
	int endy_res;
};

struct conio_font_data_struct {
        char 	*eight_by_sixteen;
        char 	*eight_by_fourteen;
        char 	*eight_by_eight;
        char 	*desc;
        enum ciolib_codepage cp;
};

CIOLIBEXPORTVAR struct conio_font_data_struct conio_fontdata[257];

struct ciolib_pixels {
	uint32_t	*pixels;
	uint32_t	*pixelsb;
	uint32_t	width;
	uint32_t	height;
};

struct vmem_cell {
	uint8_t legacy_attr;
	uint8_t ch;
	uint8_t font;
	uint32_t fg;	// RGB 00RRGGBB High bit indicates palette colour
	uint32_t bg;	// RGB 00RRGGBB High bit indicates palette colour
};

struct ciolib_screen {
	uint32_t		fg_colour;
	uint32_t		bg_colour;
	int			flags;
	int			fonts[5];
	struct ciolib_pixels	*pixels;
	struct vmem_cell	*vmem;
	struct text_info	text_info;
	uint32_t		palette[16];
};

#define CONIO_FIRST_FREE_FONT	43

typedef struct {
	int		mode;
	int		mouse;
	uint64_t	options;
#define	CONIO_OPT_LOADABLE_FONTS	1
#define CONIO_OPT_BLINK_ALT_FONT	2
#define CONIO_OPT_BOLD_ALT_FONT		4
#define CONIO_OPT_BRIGHT_BACKGROUND	8
#define CONIO_OPT_PALETTE_SETTING	16
#define CONIO_OPT_SET_PIXEL			32
#define CONIO_OPT_CUSTOM_CURSOR		64
#define CONIO_OPT_FONT_SELECT		128
#define CONIO_OPT_SET_TITLE			256
#define CONIO_OPT_SET_NAME			512
#define CONIO_OPT_SET_ICON			1024
#define CONIO_OPT_EXTENDED_PALETTE	2048
#define CONIO_OPT_BLOCKY_SCALING	4096
	void	(*clreol)		(void);
	int		(*puttext)		(int,int,int,int,void *);
	int		(*vmem_puttext)		(int,int,int,int,struct vmem_cell *);
	int		(*gettext)		(int,int,int,int,void *);
	int		(*vmem_gettext)		(int,int,int,int,struct vmem_cell *);
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
	void	(*beep)			(void);
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
	int		(*cprintf)		(const char *,...);
	int		(*cputs)		(const char *);
	void	(*textbackground)	(int);
	void	(*textcolor)	(int);
	int		(*getmouse)		(struct mouse_event *mevent);
	int		(*ungetmouse)	(struct mouse_event *mevent);
	int		(*hidemouse)	(void);
	int		(*showmouse)	(void);
	int		(*mousepointer)	(enum ciolib_mouse_ptr);
	void	(*settitle)		(const char *);
	void	(*setname)		(const char *);
	void	(*seticon)		(const void *, unsigned long);
	void	(*copytext)		(const char *, size_t);
	char 	*(*getcliptext)	(void);
	void	(*suspend)		(void);
	void	(*resume)		(void);
	int		(*setfont)		(int font, int force, int font_num);
	int		(*getfont)		(int font_num);
	int		(*loadfont)		(char *filename);
	int		(*get_window_info)		(int* width, int* height, int* xpos, int* ypos);
	void	(*getcustomcursor)	(int *startline, int *endline, int *range, int *blink, int *visible);
	void	(*setcustomcursor)	(int startline, int endline, int range, int blink, int visible);
	void	(*setvideoflags)	(int flags);
	int		(*getvideoflags)	(void);
	void	(*setscaling)	(int new_value);
	int		(*getscaling)	(void);
	int		*escdelay;
	int		(*setpalette)	(uint32_t entry, uint16_t r, uint16_t g, uint16_t b);
	int		(*attr2palette)	(uint8_t attr, uint32_t *fg, uint32_t *bg);
	int		(*setpixel)	(uint32_t x, uint32_t y, uint32_t colour);
	struct ciolib_pixels *(*getpixels)(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, int force);
	int		(*setpixels)(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, struct ciolib_pixels *pixels, void *mask);
	int 	(*get_modepalette)(uint32_t[16]);
	int	(*set_modepalette)(uint32_t[16]);
	uint32_t	(*map_rgb)(uint16_t r, uint16_t g, uint16_t b);
	void	(*replace_font)(uint8_t id, char *name, void *data, size_t size);
	int	(*checkfont)(int font_num);
	void	(*setwinsize)	(int width, int height);
	void	(*setwinposition)	(int x, int y);
} cioapi_t;

CIOLIBEXPORTVAR cioapi_t cio_api;
CIOLIBEXPORTVAR int _wscroll;
CIOLIBEXPORTVAR int directvideo;
CIOLIBEXPORTVAR int hold_update;
CIOLIBEXPORTVAR int puttext_can_move;
CIOLIBEXPORTVAR int ciolib_reaper;
CIOLIBEXPORTVAR char *ciolib_appname;
CIOLIBEXPORTVAR int ciolib_initial_window_height;
CIOLIBEXPORTVAR int ciolib_initial_window_width;

#define _conio_kbhit()		kbhit()

#ifdef __cplusplus
extern "C" {
#endif
CIOLIBEXPORT int initciolib(int mode);
CIOLIBEXPORT void suspendciolib(void);

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
CIOLIBEXPORT int ciolib_cprintf(const char *fmat, ...);
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
CIOLIBEXPORT void ciolib_settitle(const char *title);
CIOLIBEXPORT void ciolib_setname(const char *title);
CIOLIBEXPORT void ciolib_seticon(const void *icon,unsigned long size);
CIOLIBEXPORT int ciolib_showmouse(void);
CIOLIBEXPORT int ciolib_hidemouse(void);
CIOLIBEXPORT int ciolib_mousepointeer(enum ciolib_mouse_ptr);
CIOLIBEXPORT void ciolib_copytext(const char *text, size_t buflen);
CIOLIBEXPORT char * ciolib_getcliptext(void);
CIOLIBEXPORT int ciolib_setfont(int font, int force, int font_num);
CIOLIBEXPORT int ciolib_getfont(int font_num);
CIOLIBEXPORT int ciolib_loadfont(char *filename);
CIOLIBEXPORT int ciolib_get_window_info(int *width, int *height, int *xpos, int *ypos);
CIOLIBEXPORT void ciolib_beep(void);
CIOLIBEXPORT void ciolib_getcustomcursor(int *startline, int *endline, int *range, int *blink, int *visible);
CIOLIBEXPORT void ciolib_setcustomcursor(int startline, int endline, int range, int blink, int visible);
CIOLIBEXPORT void ciolib_setvideoflags(int flags);
CIOLIBEXPORT int ciolib_getvideoflags(void);
CIOLIBEXPORT void ciolib_setscaling(int flags);
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
CIOLIBEXPORT int ciolib_get_modepalette(uint32_t[16]);
CIOLIBEXPORT int ciolib_set_modepalette(uint32_t[16]);
CIOLIBEXPORT uint32_t ciolib_map_rgb(uint16_t r, uint16_t g, uint16_t b);
CIOLIBEXPORT void ciolib_replace_font(uint8_t id, char *name, void *data, size_t size);
CIOLIBEXPORT int ciolib_attrfont(uint8_t attr);
CIOLIBEXPORT int ciolib_checkfont(int font_num);
CIOLIBEXPORT void ciolib_set_vmem(struct vmem_cell *cell, uint8_t ch, uint8_t attr, uint8_t font);
CIOLIBEXPORT void ciolib_set_vmem_attr(struct vmem_cell *cell, uint8_t attr);
CIOLIBEXPORT void ciolib_setwinsize(int width, int height);
CIOLIBEXPORT void ciolib_setwinposition(int x, int y);
CIOLIBEXPORT enum ciolib_codepage ciolib_getcodepage(void);

/* DoorWay specific stuff that's only applicable to ANSI mode. */
CIOLIBEXPORT void ansi_ciolib_setdoorway(int enable);
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
	#define vmem_puttext(a,b,c,d,e)	ciolib_vmem_puttext(a,b,c,d,e)
	#define gettext(a,b,c,d,e)		ciolib_gettext(a,b,c,d,e)
	#define vmem_gettext(a,b,c,d,e)	ciolib_vmem_gettext(a,b,c,d,e)
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
	#define mousepointer(a)				ciolib_mousepointer(a)
	#define setname(a)				ciolib_setname(a)
	#define seticon(a,b)			ciolib_seticon(a,b)
	#define settitle(a)				ciolib_settitle(a)
	#define copytext(a,b)			ciolib_copytext(a,b)
	#define getcliptext()			ciolib_getcliptext()
	#define setfont(a,b,c)			ciolib_setfont(a,b,c)
	#define getfont(a)				ciolib_getfont(a)
	#define loadfont(a)				ciolib_loadfont(a)
	#define get_window_info(a,b,c,d)	ciolib_get_window_info(a,b,c,d)
	#define beep()				ciolib_beep()
	#define getcustomcursor(a,b,c,d,e)	ciolib_getcustomcursor(a,b,c,d,e)
	#define setcustomcursor(a,b,c,d,e)	ciolib_setcustomcursor(a,b,c,d,e)
	#define setvideoflags(a)		ciolib_setvideoflags(a)
	#define getvideoflags()			ciolib_getvideoflags()
	#define setscaling(a)			ciolib_setscaling(a)
	#define getscaling()			ciolib_getscaling()
	#define setpalette(e,r,g,b)		ciolib_setpalette(e,r,g,b)
	#define attr2palette(a,b,c)		ciolib_attr2palette(a,b,c)
	#define setpixel(a,b,c)			ciolib_setpixel(a,b,c)
	#define getpixels(a,b,c,d, e)		ciolib_getpixels(a,b,c,d, e)
	#define setpixels(a,b,c,d,e,f,g,h)	ciolib_setpixels(a,b,c,d,e,f,g,h)
	#define freepixels(a)			ciolib_freepixels(a)
	#define savescreen()			ciolib_savescreen()
	#define freescreen(a)			ciolib_freescreen(a)
	#define restorescreen(a)		ciolib_restorescreen(a)
	#define setcolour(a,b)			ciolib_setcolour(a,b)
	#define get_modepalette(a)		ciolib_get_modepalette(a)
	#define set_modepalette(a)		ciolib_set_modepalette(a)
	#define map_rgb(a,b,c)			ciolib_map_rgb(a,b,c)
	#define replace_font(a,b,c,d)	ciolib_replace_font(a,b,c,d)
	#define attrfont(a)				ciolib_attrfont(a)
	#define checkfont(a)			ciolib_checkfont(a)
	#define set_vmem(a, b, c, d)		ciolib_set_vmem(a, b, c, d)
	#define set_vmem_attr(a, b)		ciolib_set_vmem_attr(a, b)
	#define setwinsize(a,b)			ciolib_setwinsize(a,b)
	#define setwinposition(a,b)		ciolib_setwinposition(a,b)
	#define getcodepage()			ciolib_getcodepage()
#endif

#ifdef WITH_SDL
	#include <gen_defs.h>
	#include <SDL.h>

#if defined(_WIN32) || defined(__DARWIN__)
	#ifdef main
		#undef main
	#endif
	#define main	CIOLIB_main
#endif

#if defined(__DARWIN__)
	extern sem_t main_sem;
	extern sem_t startsdl_sem;
	extern int initsdl_ret;
#endif
#endif

#define CIOLIB_BUTTON_1	1
#define CIOLIB_BUTTON_2	2
#define CIOLIB_BUTTON_3	4
#define CIOLIB_BUTTON_4	8
#define CIOLIB_BUTTON_5	16

#define CIOLIB_BUTTON(x)	(1<<(x-1))

enum {
	 CIOLIB_MOUSE_MOVE			/* 0 */
	,CIOLIB_BUTTON_1_PRESS
	,CIOLIB_BUTTON_1_RELEASE
	,CIOLIB_BUTTON_1_CLICK
	,CIOLIB_BUTTON_1_DBL_CLICK
	,CIOLIB_BUTTON_1_TRPL_CLICK
	,CIOLIB_BUTTON_1_QUAD_CLICK
	,CIOLIB_BUTTON_1_DRAG_START
	,CIOLIB_BUTTON_1_DRAG_MOVE
	,CIOLIB_BUTTON_1_DRAG_END
	,CIOLIB_BUTTON_2_PRESS			/* 10 */
	,CIOLIB_BUTTON_2_RELEASE
	,CIOLIB_BUTTON_2_CLICK
	,CIOLIB_BUTTON_2_DBL_CLICK
	,CIOLIB_BUTTON_2_TRPL_CLICK
	,CIOLIB_BUTTON_2_QUAD_CLICK
	,CIOLIB_BUTTON_2_DRAG_START
	,CIOLIB_BUTTON_2_DRAG_MOVE
	,CIOLIB_BUTTON_2_DRAG_END
	,CIOLIB_BUTTON_3_PRESS
	,CIOLIB_BUTTON_3_RELEASE		/* 20 */
	,CIOLIB_BUTTON_3_CLICK
	,CIOLIB_BUTTON_3_DBL_CLICK
	,CIOLIB_BUTTON_3_TRPL_CLICK
	,CIOLIB_BUTTON_3_QUAD_CLICK
	,CIOLIB_BUTTON_3_DRAG_START
	,CIOLIB_BUTTON_3_DRAG_MOVE
	,CIOLIB_BUTTON_3_DRAG_END
	,CIOLIB_BUTTON_4_PRESS
	,CIOLIB_BUTTON_4_RELEASE
	,CIOLIB_BUTTON_4_CLICK			/* 30 */
	,CIOLIB_BUTTON_4_DBL_CLICK
	,CIOLIB_BUTTON_4_TRPL_CLICK
	,CIOLIB_BUTTON_4_QUAD_CLICK
	,CIOLIB_BUTTON_4_DRAG_START
	,CIOLIB_BUTTON_4_DRAG_MOVE
	,CIOLIB_BUTTON_4_DRAG_END
	,CIOLIB_BUTTON_5_PRESS
	,CIOLIB_BUTTON_5_RELEASE
	,CIOLIB_BUTTON_5_CLICK
	,CIOLIB_BUTTON_5_DBL_CLICK		/* 40 */
	,CIOLIB_BUTTON_5_TRPL_CLICK
	,CIOLIB_BUTTON_5_QUAD_CLICK
	,CIOLIB_BUTTON_5_DRAG_START
	,CIOLIB_BUTTON_5_DRAG_MOVE
	,CIOLIB_BUTTON_5_DRAG_END		/* 45 */
};

#define CIOLIB_BUTTON_PRESS(x)		((x-1)*9+1)
#define CIOLIB_BUTTON_RELEASE(x)	((x-1)*9+2)
#define CIOLIB_BUTTON_CLICK(x)		((x-1)*9+3)
#define CIOLIB_BUTTON_DBL_CLICK(x)	((x-1)*9+4)
#define CIOLIB_BUTTON_TRPL_CLICK(x)	((x-1)*9+5)
#define CIOLIB_BUTTON_QUAD_CLICK(x)	((x-1)*9+6)
#define CIOLIB_BUTTON_DRAG_START(x)	((x-1)*9+7)
#define CIOLIB_BUTTON_DRAG_MOVE(x)	((x-1)*9+8)
#define CIOLIB_BUTTON_DRAG_END(x)	((x-1)*9+9)

#define CIOLIB_BUTTON_NUMBER(x)		((x+8)/9)

#define CIOLIB_BUTTON_BASE(x)		(x!=CIOLIB_MOUSE_MOVE?x-9*(CIOLIB_BUTTON_NUMBER(x)-1):CIOLIB_MOUSE_MOVE)

extern int ciolib_mouse_initialized;

#ifdef __cplusplus
extern "C" {
#endif
CIOLIBEXPORT void ciomouse_gotevent(int event, int x, int y, int x_res, int y_res);
CIOLIBEXPORT int mouse_trywait(void);
CIOLIBEXPORT int mouse_wait(void);
CIOLIBEXPORT int mouse_pending(void);
CIOLIBEXPORT int ciolib_getmouse(struct mouse_event *mevent);
CIOLIBEXPORT int ciolib_ungetmouse(struct mouse_event *mevent);
CIOLIBEXPORT void ciolib_mouse_thread(void *data);
CIOLIBEXPORT uint64_t ciomouse_setevents(uint64_t events);
CIOLIBEXPORT uint64_t ciomouse_addevents(uint64_t events);
CIOLIBEXPORT uint64_t ciomouse_delevents(uint64_t events);
CIOLIBEXPORT uint64_t ciomouse_addevent(uint64_t event);
CIOLIBEXPORT uint64_t ciomouse_delevent(uint64_t event);
CIOLIBEXPORT uint32_t ciolib_mousepointer(enum ciolib_mouse_ptr type);
CIOLIBEXPORT void mousestate(int *x, int *y, uint8_t *buttons);
CIOLIBEXPORT void mousestate_res(int *x_res, int *y_res, uint8_t *buttons);
#ifdef __cplusplus
}
#endif

#define CIO_KEY_HOME      (0x47 << 8)
#define CIO_KEY_UP        (0x48 << 8)
#define CIO_KEY_END       (0x4f << 8)
#define CIO_KEY_DOWN      (0x50 << 8)
#define CIO_KEY_F(x)      ((x<11)?((0x3a+x) << 8):((0x7a+x) << 8))
#define CIO_KEY_IC        (0x52 << 8)
#define CIO_KEY_DC        (0x53 << 8)
#define CIO_KEY_SHIFT_IC  (0x05 << 8)	/* Shift-Insert */
#define CIO_KEY_SHIFT_DC  (0x07 << 8)	/* Shift-Delete */
#define CIO_KEY_CTRL_IC   (0x04 << 8)	/* Ctrl-Insert */
#define CIO_KEY_CTRL_DC   (0x06 << 8)	/* Ctrl-Delete */
#define CIO_KEY_ALT_IC    (0xA2 << 8)	/* Alt-Insert */
#define CIO_KEY_ALT_DC    (0xA3 << 8)	/* Alt-Delete */
#define CIO_KEY_LEFT      (0x4b << 8)
#define CIO_KEY_RIGHT     (0x4d << 8)
#define CIO_KEY_PPAGE     (0x49 << 8)
#define CIO_KEY_NPAGE     (0x51 << 8)
#define CIO_KEY_SHIFT_F(x)((x<11)?((0x53 + x) << 8):((0x7c + x) << 8))
#define CIO_KEY_CTRL_F(x) ((x<11)?((0x5d + x) << 8):((0x7e + x) << 8))
#define CIO_KEY_ALT_F(x)  ((x<11)?((0x67 + x) << 8):((0x80 + x) << 8))
#define CIO_KEY_BACKTAB   (0x0f << 8)

#define CIO_KEY_MOUSE     0x7dE0	// This is the right mouse on Schneider/Amstrad PC1512 PC keyboards "F-14"
#define CIO_KEY_QUIT	  0x7eE0	// "F-15"
#define CIO_KEY_ABORTED   0x01E0	// ESC key by scancode

#endif	/* Do not add anything after this line */
