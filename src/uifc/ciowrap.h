/* $Id$ */

#ifndef _CIOWRAP_H_
#define _CIOWRAP_H_

#ifndef __unix__
#include "conio.h"
#define initciowrap(x)
#else
#include "curs_fix.h"
#undef getch
#include "keys.h"

#define MONO	2
#define	BW80	MONO
#define COLOR_MODE	3

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

enum
{
	 X_MODE
	,CURSES_MODE
};

struct text_info {
    unsigned char currmode;
    unsigned char screenheight;
	unsigned char screenwidth;
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
} cioapi_t;

int movetext(int sx, int sy, int ex, int ey, int dx, int dy);
char *cgets(char *str);
int cscanf (char *format , ...);
int kbhit(void);
int getch(void);
int getche(void);
int ungetch(int ch);

extern cioapi_t cio_api;

#define _conio_kbhit()		kbhit()
#define clreol()			cio_api.clreol()
#define puttext(a,b,c,d,e)	cio_api.puttext(a,b,c,d,e)
#define gettext(a,b,c,d,e)	cio_api.gettext(a,b,c,d,e)
#define textattr(a)			cio_api.textattr(a)
#define delay(a)			cio_api.delay(a)
#define wherex()			cio_api.wherex()
#define wherey()			cio_api.wherey()
#define putch(a)			cio_api.putch(a)
#define cprintf				cio_api.c_printf
#define cputs(a)			cio_api.cputs(a)
#define gotoxy(a,b)			cio_api.gotoxy(a,b)
#define clrscr()			cio_api.clrscr()
#define gettextinfo(a)		cio_api.gettextinfo(a)
#define _setcursortype(a)	cio_api.setcursortype(a)
#define textbackground(a)	cio_api.textbackground(a)
#define textcolor(a)		cio_api.textcolor(a)
#define beep()				cio_api.beep()
#define highvideo()			cio_api.highvideo()
#define lowvideo()			cio_api.lowvideo()
#define normvideo()			cio_api.normvideo()

#endif

#endif	/* Do not add anything after this line */
