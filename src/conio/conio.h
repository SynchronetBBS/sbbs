/* $Id$ */

#ifndef _CIOWRAP_H_
#define _CIOWRAP_H_

enum {
	 CIOWRAP_AUTO_MODE
	,CIOWRAP_CURSES_MODE
	,CIOWRAP_CURSES_IBM_MODE
	,CIOWRAP_ANSI_MODE
	,CIOWRAP_X_MODE
	,CIOWRAP_CONIO_MODE
};

#ifndef __unix__
#include <conio.h>
#else

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

#endif

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
} cioapi_t;

extern cioapi_t cio_api;
extern int _wscroll;
extern int directvideo;

#define _conio_kbhit()		kbhit()

#ifdef __cplusplus
extern "C" {
#endif
int initciowrap(int mode);
#ifdef __cplusplus
}
#endif

#ifndef CIOWRAP_NO_MACROS
	#define cscanf					ciowrap_cscanf
	#define cprintf					ciowrap_cprintf

	#define movetext(a,b,c,d,e,f)	ciowrap_movetext(a,b,c,d,e,f)
	#define cgets(a)				ciowrap_cgets(a)
	#define kbhit()					ciowrap_kbhit()
	#define getch()					ciowrap_getch()
	#define getchr()				ciowrap_getche()
	#define ungetch(a)				ciowrap_ungetch(a)
	#define gettextinfo(a)			ciowrap_gettextinfo(a)
	#define wherex()				ciowrap_wherex()
	#define wherey()				ciowrap_wherey()
	#define	wscroll()				ciowrap_wscroll()
	#define gotoxy(a,b)				ciowrap_gotoxy(a,b)
	#define clreol()				ciowrap_clreol()
	#define clrscr()				ciowrap_clrscr()
	#define cputs(a)				ciowrap_cputs(a)
	#define textbackground(a)		ciowrap_textbackground(a)
	#define textcolor(a)			ciowrap_textcolor(a)
	#define highvideo()				ciowrap_highvideo()
	#define lowvideo()				ciowrap_lowvideo()
	#define normvideo()				ciowrap_normvideo()
	#define puttext(a,b,c,d,e)		ciowrap_puttext(a,b,c,d,e)
	#define gettext(a,b,c,d,e)		ciowrap_gettext(a,b,c,d,e)
	#define textattr(a)				ciowrap_textattr(a)
	#define delay(a)				ciowrap_delay(a)
	#define putch(a)				ciowrap_putch(a)
	#define _setcursortype(a)		ciowrap_setcursortype(a)
	#define textmode(a)				ciowrap_textmode(a)
	#define window(a,b,c,d)			ciowrap_window(a,b,c,d)
	#define delline()				ciowrap_delline()
	#define insline					ciowrap_insline()
	#define getpass(a)				ciowrap_getpass(a);
#endif

#endif	/* Do not add anything after this line */
