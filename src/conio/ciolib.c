#include <stdarg.h>
#include <stdlib.h>	/* malloc */
#include <stdio.h>

#define CIOLIB_NO_MACROS
#include "ciolib.h"

#ifndef _WIN32
 #ifndef NO_X
  #include "x_cio.h"
 #endif
 #include "curs_cio.h"
 #undef getch
#endif

#include "ansi_cio.h"

cioapi_t	cio_api;

static int ungotch;
static struct text_info cio_textinfo;
static int lastmode=3;
int _wscroll=1;
int directvideo=0;
static int initialized=0;

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
void ciolib_textattr(unsigned char a);
void ciolib_delay(long a);
int ciolib_putch(unsigned char a);
void ciolib_setcursortype(int a);
void ciolib_textmode(int mode);
void ciolib_window(int sx, int sy, int ex, int ey);
void ciolib_delline(void);
void ciolib_insline(void);
char *ciolib_getpass(const char *prompt);

#define CIOLIB_INIT()		{ if(!initialized) initciolib(CIOLIB_MODE_AUTO); }

#ifndef _WIN32
 #ifndef NO_X
int try_x_init(int mode)
{
	if(!console_init()) {
		cio_api.mode=CIOLIB_MODE_X;
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
		return(1);
	}
	fprintf(stderr,"X init failed\n");
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
		return(1);
	}
	fprintf(stderr,"Curses init failed\n");
	return(0);
}
#endif

int try_ansi_init(int mode)
{
	if(ansi_initciolib(mode)) {
		cio_api.mode=CIOLIB_MODE_ANSI;
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
		return(1);
	}
	fprintf(stderr,"ANSI init failed\n");
	return(0);
}

#ifdef _WIN32
#if defined(__BORLANDC__)
        #pragma argsused
#endif
int try_conio_init(int mode)
{
	/* This should test for something or other */
	if(isatty(fileno(stdout))) {
		cio_api.mode=CIOLIB_MODE_CONIO;
		cio_api.puttext=puttext;
		cio_api.gettext=gettext;
		cio_api.textattr=textattr;
		cio_api.kbhit=kbhit;
		cio_api.wherey=wherey;
		cio_api.wherex=wherex;
		cio_api.putch=putch;
		cio_api.gotoxy=gotoxy;
		cio_api.gettextinfo=gettextinfo;
		cio_api.setcursortype=_setcursortype;
		cio_api.getch=getch;
		cio_api.getche=getche;
		cio_api.textmode=textmode;
		return(1);
	}
	fprintf(stderr,"CONIO init failed\n");
	return(0);
}
#endif


int initciolib(int mode)
{
	switch(mode) {
		case CIOLIB_MODE_AUTO:
#ifdef _WIN32
			if(!try_conio_init(mode))
#else
			if(!try_x_init(mode))
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
			try_x_init(mode);
			break;
#endif
		case CIOLIB_MODE_ANSI:
			try_ansi_init(mode);
			break;
	}
	if(cio_api.mode==CIOLIB_MODE_AUTO) {
		fprintf(stderr,"CIOLIB initialization failed!");
		return(-1);
	}

	initialized=1;
	ciolib_gettextinfo(&cio_textinfo);
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
	cio_textinfo.normattr=7;
	return(0);
}

int ciolib_kbhit(void)
{
	CIOLIB_INIT();
	if(ungotch)
		return(1);
	return(cio_api.kbhit());
}

int ciolib_getch(void)
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

int ciolib_getche(void)
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

int ciolib_ungetch(int ch)
{
	CIOLIB_INIT();
	
	if(ungotch)
		return(EOF);
	ungotch=ch;
	return(ch);
}

int ciolib_movetext(int sx, int sy, int ex, int ey, int dx, int dy)
{
	int width;
	int height;
	char *buf;

	CIOLIB_INIT();
	
	width=ex-sx;
	height=ey-sy;
	buf=(char *)malloc((width+1)*(height+1)*2);
	if(buf==NULL)
		return(0);
	if(!ciolib_gettext(sx,sy,ex,ey,buf)) {
		free(buf);
		return(0);
	}
	if(!ciolib_puttext(dx,dy,dx+width,dy+height,buf)) {
		free(buf);
		return(0);
	}
	free(buf);
	return(1);
}

char *ciolib_cgets(char *str)
{
	int	maxlen;
	int len=0;
	int chars;
	int ch;

	CIOLIB_INIT();
	
	maxlen=*(unsigned char *)str;
	while((ch=ciolib_getche())!='\n') {
		switch(ch) {
			case 0:	/* Skip extended keys */
				ciolib_getche();
				break;
			case '\r':	/* Skip \r (ToDo: Should this be treeated as a \n? */
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
				str[(len++)+2]=ch;
				if(len==maxlen) {
					str[len+2]=0;
					*((unsigned char *)(str+1))=(unsigned char)len;
					return(&str[2]);
				}
				break;
		}
	}
	str[len+2]=0;
	*((unsigned char *)(str+1))=(unsigned char)len;
	return(&str[2]);
}

int ciolib_cscanf (char *format , ...)
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

char *ciolib_getpass(const char *prompt)
{
	static char pass[9];
	int len=0;
	int chars;
	int ch;

	CIOLIB_INIT();
	
	ciolib_cputs((char *)prompt);
	while((ch=getch())!='\n') {
		switch(ch) {
			case 0:	/* Skip extended keys */
				getch();
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

void ciolib_gettextinfo(struct text_info *info)
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
		info->curx=cio_textinfo.curx;           /* x-coordinate in current window */
		info->cury=cio_textinfo.cury;           /* y-coordinate in current window */
	}
}

void ciolib_wscroll(void)
{
	char *buf;
	int os;
	struct text_info ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);
	if(!_wscroll)
		return;
	ciolib_movetext(ti.winleft,ti.wintop+1,ti.winright,ti.winbottom,ti.winleft,ti.wintop);
	ciolib_gotoxy(1,ti.winbottom-ti.winleft+1);
	os=_wscroll;
	_wscroll=0;
	ciolib_cprintf("%*s",ti.winright-ti.winleft+1,"");
	_wscroll=os;
	ciolib_gotoxy(ti.curx,ti.cury);
}

int ciolib_wherex(void)
{
	int x;

	CIOLIB_INIT();
	
	x=cio_api.wherex();
	x=x-cio_textinfo.winleft+1;
	return(x);
}

int ciolib_wherey(void)
{
	int y;

	CIOLIB_INIT();
	
	y=cio_api.wherey();
	y=y-cio_textinfo.wintop+1;
	return(y);
}

void ciolib_gotoxy(int x, int y)
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

void ciolib_textmode(mode)
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
}

void ciolib_window(int sx, int sy, int ex, int ey)
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

void ciolib_clreol(void)
{
	int os;
	struct text_info	ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);
	os=_wscroll;
	_wscroll=0;
	ciolib_cprintf("%*s",ti.winright-ti.curx+1,"");
	_wscroll=os;
	ciolib_gotoxy(ti.curx,ti.cury);
}

void ciolib_clrscr(void)
{
	char *buf;
	int i;
	int width,height;
	struct text_info ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);

	width=ti.winright-ti.winleft+1;
	height=ti.winbottom-ti.wintop+1;
	buf=(char *)malloc(width*height*2);
	for(i=0;i<width*height*2;) {
		buf[i++]=' ';
		buf[i++]=ti.attribute;
	}
	ciolib_puttext(ti.winleft,ti.wintop,ti.winright,ti.winbottom,buf);
	free(buf);
}

void ciolib_delline(void)
{
	struct text_info ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);

	ciolib_movetext(ti.winleft,ti.cury+1,ti.winright,ti.winbottom,ti.winleft,ti.cury);
	ciolib_gotoxy(1,ti.winbottom-ti.wintop+1);
	ciolib_clreol();
	ciolib_gotoxy(ti.curx,ti.cury);
}

void ciolib_insline(void)
{
	struct text_info ti;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&ti);

	ciolib_movetext(ti.winleft,ti.cury,ti.winright,ti.winbottom,ti.winleft,ti.cury+1);
	ciolib_gotoxy(1,ti.cury);
	ciolib_clreol();
	ciolib_gotoxy(ti.curx,ti.cury);
}

int ciolib_cprintf(char *fmat, ...)
{
    va_list argptr;
	int		pos;
	int		ret;
#ifdef _WIN32			/* Can't figure out a way to allocate a "big enough" buffer for Win32. */
	char	str[16384];
#else
	char	*str;
#endif

	CIOLIB_INIT();
	
    va_start(argptr,fmat);
#ifdef _WIN32
	ret=vsnprintf(str,sizeof(str)-1,fmat,argptr);
#else
    ret=vsnprintf(NULL,0,fmat,argptr);
	str=(char *)malloc(ret+1);
	if(str==NULL)
		return(EOF);
	ret=vsprintf(str,fmat,argptr);
#endif
    va_end(argptr);
	if(ret>=0)
		ciolib_cputs(str);
	else
		ret=EOF;
#ifndef _WIN32
	free(str);
#endif
    return(ret);
}

int ciolib_cputs(char *str)
{
	int		pos;
	int		ret=0;

	CIOLIB_INIT();
	
	for(pos=0;str[pos];pos++)
	{
		ret=str[pos];
		if(str[pos]=='\n')
			ciolib_putch('\r');
		ciolib_putch(str[pos]);
	}
	return(ret);
}

void ciolib_textbackground(int colour)
{
	unsigned char attr;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr&=143;
	attr|=(colour<<4);
	ciolib_textattr(attr);
}

void ciolib_textcolor(int colour)
{
	unsigned char attr;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr&=240;
	attr|=colour;
	ciolib_textattr(attr);
}

void ciolib_highvideo(void)
{
	int attr;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr |= 8;
	ciolib_textattr(attr);
}

void ciolib_lowvideo(void)
{
	int attr;

	CIOLIB_INIT();
	
	ciolib_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr &= 0xf7;
	ciolib_textattr(attr);
}

void ciolib_normvideo(void)
{
	CIOLIB_INIT();
	
	ciolib_textattr(0x07);
}

int ciolib_puttext(int a,int b,int c,int d,unsigned char *e)
{
	CIOLIB_INIT();
	
	return(cio_api.puttext(a,b,c,d,e));
}

int ciolib_gettext(int a,int b,int c,int d,unsigned char *e)
{
	CIOLIB_INIT();
	
	return(cio_api.gettext(a,b,c,d,e));
}

void ciolib_textattr(unsigned char a)
{
	CIOLIB_INIT();
	
	cio_api.textattr(a);
}

void ciolib_delay(long a)
{
	CIOLIB_INIT();
	
	cio_api.delay(a);
}

int ciolib_putch(unsigned char a)
{
	CIOLIB_INIT();
	
	return(cio_api.putch(a));
}

void ciolib_setcursortype(int a)
{
	CIOLIB_INIT();
	
	cio_api.setcursortype(a);
}
