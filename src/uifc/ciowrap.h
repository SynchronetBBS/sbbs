#include "curs_fix.h"

#define MONO	1
#define	BW80	MONO
#define COLOR_MODE	2
#define kbhit()		_kbhit()

#ifndef BOOL
#define BOOL    int
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif
#endif

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

int inkey(int mode);
short curses_color(short color);
int puttext(int sx, int sy, int ex, int ey, unsigned char *fill);
int gettext(int sx, int sy, int ex, int ey, unsigned char *fill);
void textattr(unsigned char attr);
int _kbhit(void);
#ifndef __QNX__
void delay(long msec);
#endif
int wherey(void);
int wherex(void);
void _putch(unsigned char ch, BOOL refresh_now);
int cprintf(char *fmat, ...);
void cputs(char *str);
void gotoxy(int x, int y);
void clrscr(void);
void initciowrap(long inmode);
void gettextinfo(struct text_info *info);
void _setcursortype(int type);
