/* $Id$ */

#include "curs_fix.h"

#define MONO	1
#define	BW80	MONO
#define COLOR_MODE	2

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

enum
{
	_NOCURSOR,
	_SOLIDCURSOR,
	_NORMALCURSOR
};

struct text_info {
    unsigned char currmode;
    unsigned char screenheight;
	unsigned char screenwidth;
};

short curses_color(short color);
int puttext(int sx, int sy, int ex, int ey, unsigned char *fill);
#define gettext(x1,y1,x2,y2,z)	cio_gettext(x1,y1,x2,y2,z)
int cio_gettext(int sx, int sy, int ex, int ey, unsigned char *fill);
void textattr(unsigned char attr);
int kbhit(void);
#ifndef __QNX__
void delay(long msec);
#endif
int wherey(void);
int wherex(void);
void _putch(unsigned char ch, BOOL refresh_now);
int cprintf(char *fmat, ...);
void cputs(unsigned char *str);
void gotoxy(int x, int y);
void clrscr(void);
void initciowrap(long inmode);
void gettextinfo(struct text_info *info);
void _setcursortype(int type);
