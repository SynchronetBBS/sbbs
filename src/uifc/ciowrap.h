/* $Id$ */

#ifndef _CIOWRAP_H_
#define _CIOWRAP_H_

#ifndef __unix__
#include "conio.h"
#define initciowrap(x)
#else
#include "curs_fix.h"
#ifndef NEED_CURSES_GETCH
 #undef getch
#endif

#ifndef BOOL
#define BOOL    int
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif
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

#define LASTMODE	-1
#define BW40		0
#define C40			1
#define BW80		2
#define	C80			3
#define MONO		7
#define C4350		64

#define COLOR_MODE	C80

enum
{
	_NOCURSOR,
	_SOLIDCURSOR,
	_NORMALCURSOR
};

enum
{
	 X_MODE
	,CURSES_MODE
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
	void	(*clreol)		(void);
	int		(*puttext)		(int,int,int,int,unsigned char *);
	int		(*gettext)		(int,int,int,int,unsigned char *);
	void	(*textattr)		(unsigned char);
	int		(*kbhit)		(void);
	void	(*delay)		(long);
	int		(*wherex)		(void);
	int		(*wherey)		(void);
	int		(*putch)		(unsigned char);
	int		(*c_printf)		(char *fmat, ...);
	int		(*cputs)		(unsigned char *);
	void	(*gotoxy)		(int,int);
	void	(*clrscr)		(void);
	void	(*gettextinfo)	(struct text_info *);
	void	(*setcursortype)(int);
	void	(*textbackground)	(int);
	void	(*textcolor)	(int);
	int		(*getch)		(void);
	int		(*getche)		(void);
	int		(*beep)			(void);
	void	(*highvideo)	(void);
	void	(*lowvideo)		(void);
	void	(*normvideo)	(void);
	void	(*textmode)		(int);
} cioapi_t;

int movetext(int sx, int sy, int ex, int ey, int dx, int dy);
char *cgets(char *str);
int cscanf (char *format , ...);
int kbhit(void);
#ifndef NEED_CURSES_GETCH
 int getch(void);
#endif
int getche(void);
int ungetch(int ch);
void gettextinfo(struct text_info *info);
int wherex(void);
int wherex(void);
void wscroll(void);
void gotoxy(int x, int y);
void clreol(void);
void clrscr(void);

extern cioapi_t cio_api;
extern int _wscroll;
extern int directvideo;

#define _conio_kbhit()		kbhit()
#define puttext(a,b,c,d,e)	cio_api.puttext(a,b,c,d,e)
#define gettext(a,b,c,d,e)	cio_api.gettext(a,b,c,d,e)
#define textattr(a)			cio_api.textattr(a)
#define delay(a)			cio_api.delay(a)
#define putch(a)			cio_api.putch(a)
#define cprintf				cio_api.c_printf
#define cputs(a)			cio_api.cputs(a)
#define _setcursortype(a)	cio_api.setcursortype(a)
#define textbackground(a)	cio_api.textbackground(a)
#define textcolor(a)		cio_api.textcolor(a)
#define beep()				cio_api.beep()
#define highvideo()			cio_api.highvideo()
#define lowvideo()			cio_api.lowvideo()
#define normvideo()			cio_api.normvideo()

#endif

#endif	/* Do not add anything after this line */
