#include <stdarg.h>
#include <stdio.h>

#define CIOWRAP_NO_MACROS
#include "conio.h"

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

int ciowrap_movetext(int sx, int sy, int ex, int ey, int dx, int dy);
char *ciowrap_cgets(char *str);
int ciowrap_cscanf (char *format , ...);
int ciowrap_kbhit(void);
int ciowrap_getch(void);
int ciowrap_getche(void);
int ciowrap_ungetch(int ch);
void ciowrap_gettextinfo(struct text_info *info);
int ciowrap_wherex(void);
int ciowrap_wherey(void);
void ciowrap_wscroll(void);
void ciowrap_gotoxy(int x, int y);
void ciowrap_clreol(void);
void ciowrap_clrscr(void);
int ciowrap_cputs(char *str);
int	ciowrap_cprintf(char *fmat, ...);
void ciowrap_textbackground(int colour);
void ciowrap_textcolor(int colour);
void ciowrap_highvideo(void);
void ciowrap_lowvideo(void);
void ciowrap_normvideo(void);
int ciowrap_puttext(int a,int b,int c,int d,unsigned char *e);
int ciowrap_gettext(int a,int b,int c,int d,unsigned char *e);
void ciowrap_textattr(unsigned char a);
void ciowrap_delay(long a);
int ciowrap_putch(unsigned char a);
void ciowrap_setcursortype(int a);
void ciowrap_textmode(int mode);
void ciowrap_window(int sx, int sy, int ex, int ey);
void ciowrap_delline(void);
void ciowrap_insline(void);
char *ciowrap_getpass(const char *prompt);

#ifndef _WIN32
 #ifndef NO_X
int try_x_init(int mode)
{
	if(!console_init()) {
		cio_api.mode=CIOWRAP_X_MODE;
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
		cio_api.beep=x_beep;
		cio_api.textmode=x_textmode;
		return(1);
	}
	fprintf(stderr,"X init failed\n");
	return(0);
}
 #endif

int try_curses_init(int mode)
{
	if(curs_initciowrap(mode)) {
		cio_api.mode=CIOWRAP_CURSES_IBM_MODE;
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
		cio_api.beep=beep;
		cio_api.textmode=curs_textmode;
		return(1);
	}
	fprintf(stderr,"Curses init failed\n");
	return(0);
}
#endif

int try_ansi_init(int mode)
{
	if(ansi_initciowrap(mode)) {
		cio_api.mode=CIOWRAP_ANSI_MODE;
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
		cio_api.beep=ansi_beep;
		cio_api.textmode=ansi_textmode;
		return(1);
	}
	fprintf(stderr,"ANSI init failed\n");
	return(0);
}

#ifdef _WIN32
int try_ciowrap_init(int mode)
{
	/* This should test for something or other */
	if(1) {
		cio_api.mode=CIOWRAP_CIOWRAP_MODE;
		cio_api.puttext=puttext;
		cio_api.gettext=gettext;
		cio_api.textattr=textattr;
		cio_api.kbhit=kbhit;
		cio_api.delay=delay;
		cio_api.wherey=wherey;
		cio_api.wherex=wherex;
		cio_api.putch=putch;
		cio_api.gotoxy=gotoxy;
		cio_api.gettextinfo=gettextinfo;
		cio_api.setcursortype=setcursortype;
		cio_api.getch=getch;
		cio_api.getche=getche;
		cio_api.beep=beep;
		cio_api.textmode=textmode;
		return(1);
	}
	fprintf(stderr,"CONIO init failed\n");
	return(0);
}
#endif


int initciowrap(int mode)
{
	switch(mode) {
		case CIOWRAP_AUTO_MODE:
#ifdef _WIN32
			if(!try_ciowrap_init(mode))
#else
			if(!try_x_init(mode))
				if(!try_curses_init(mode))
#endif
					try_ansi_init(mode);
			break;
#ifdef _WIN32
		case CIOWRAP_ciowrap_MODE:
			try_ciowrap_init(mode);
			break;
#else
		case CIOWRAP_CURSES_MODE:
		case CIOWRAP_CURSES_IBM_MODE:
			try_curses_init(mode);
			break;
		case CIOWRAP_X_MODE:
			try_x_init(mode);
			break;
#endif
		case CIOWRAP_ANSI_MODE:
			try_ansi_init(mode);
			break;
	}
	if(cio_api.mode==CIOWRAP_AUTO_MODE) {
		fprintf(stderr,"CIOWRAP initialization failed!");
		return(-1);
	}

	initialized=1;
	ciowrap_gettextinfo(&cio_textinfo);
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
	cio_textinfo.normattr=7;
	return(0);
}

int ciowrap_kbhit(void)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	if(ungotch)
		return(1);
	return(cio_api.kbhit());
}

int ciowrap_getch(void)
{
	int ch;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	if(ungotch) {
		ch=ungotch;
		ungotch=0;
		return(ch);
	}
	return(cio_api.getch());
}

int ciowrap_getche(void)
{
	int ch;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	if(ungotch) {
		ch=ungotch;
		ungotch=0;
		ciowrap_putch(ch);
		return(ch);
	}
	return(cio_api.getche());
}

int ciowrap_ungetch(int ch)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	if(ungotch)
		return(EOF);
	ungotch=ch;
	return(ch);
}

int ciowrap_movetext(int sx, int sy, int ex, int ey, int dx, int dy)
{
	int width;
	int height;
	char *buf;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	width=ex-sx;
	height=ey-sy;
	buf=(char *)malloc((width+1)*(height+1)*2);
	if(buf==NULL)
		return(0);
	if(!ciowrap_gettext(sx,sy,ex,ey,buf)) {
		free(buf);
		return(0);
	}
	if(!ciowrap_puttext(dx,dy,dx+width,dy+height,buf)) {
		free(buf);
		return(0);
	}
	free(buf);
	return(1);
}

char *ciowrap_cgets(char *str)
{
	int	maxlen;
	int len=0;
	int chars;
	int ch;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	maxlen=*(unsigned char *)str;
	while((ch=ciowrap_getche())!='\n') {
		switch(ch) {
			case 0:	/* Skip extended keys */
				ch=ciowrap_getche();
				break;
			case '\n':
				str[len+2]=0;
				*((unsigned char *)(str+1))=(unsigned char)len;
				return(&str[2]);
			case '\r':	/* Skip \r (ToDo: Should this be treeated as a \n? */
				break;
			case '\b':
				if(len==0) {
					cio_api.beep();
					break;
				}
				ciowrap_putch('\b');
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
}

int ciowrap_cscanf (char *format , ...)
{
	char str[255];
    va_list argptr;
	int ret;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	str[0]=-1;
	va_start(argptr,format);
	ret=vsscanf(ciowrap_cgets(str),format,argptr);
	va_end(argptr);
	return(ret);
}

char *ciowrap_getpass(const char *prompt)
{
	static char pass[9];
	int len=0;
	int chars;
	int ch;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	while((ch=getch())!='\n') {
		switch(ch) {
			case 0:	/* Skip extended keys */
				ch=getch();
				break;
			case '\n':
				pass[len]=0;
				return(pass);
			case '\r':	/* Skip \r (ToDo: Should this be treeated as a \n? */
				break;
			case '\b':
				if(len==0) {
					cio_api.beep();
					break;
				}
				len--;
				break;
			default:
				if(len==8)
					cio_api.beep();
				else
					pass[len++]=ch;
				break;
		}
	}
}

void ciowrap_gettextinfo(struct text_info *info)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
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

void ciowrap_wscroll(void)
{
	char *buf;
	int os;
	struct text_info ti;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&ti);
	if(!_wscroll)
		return;
	ciowrap_movetext(ti.winleft,ti.wintop+1,ti.winright,ti.winbottom,ti.winleft,ti.wintop);
	ciowrap_gotoxy(1,ti.winbottom-ti.winleft+1);
	os=_wscroll;
	_wscroll=0;
	ciowrap_cprintf("%*s",ti.winright-ti.winleft+1,"");
	_wscroll=os;
	ciowrap_gotoxy(ti.curx,ti.cury);
}

int ciowrap_wherex(void)
{
	int x;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	x=cio_api.wherex();
	x=x-cio_textinfo.winleft+1;
	return(x);
}

int ciowrap_wherey(void)
{
	int y;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	y=cio_api.wherey();
	y=y-cio_textinfo.wintop+1;
	return(y);
}

void ciowrap_gotoxy(int x, int y)
{
	int nx;
	int ny;
	struct text_info ti;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&ti);
	if(		x < 1
			|| x > ti.winright-ti.winleft+1
			|| y < 1
			|| y > ti.winbottom-ti.wintop+1)
		return;
	nx=x+ti.winleft-1;
	ny=y+ti.wintop-1;
	cio_api.gotoxy(nx,ny);
}

void ciowrap_textmode(mode)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	if(mode==-1) {
		ciowrap_gettextinfo(&cio_textinfo);
		cio_api.textmode(lastmode);
		lastmode=cio_textinfo.currmode;
	}
	else {
		ciowrap_gettextinfo(&cio_textinfo);
		lastmode=cio_textinfo.currmode;
		cio_api.textmode(mode);
	}
	ciowrap_gettextinfo(&cio_textinfo);
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
}

void ciowrap_window(int sx, int sy, int ex, int ey)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&cio_textinfo);
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
	ciowrap_gotoxy(1,1);
}

void ciowrap_clreol(void)
{
	int os;
	struct text_info	ti;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&ti);
	os=_wscroll;
	_wscroll=0;
	ciowrap_cprintf("%*s",ti.winright-ti.curx+1,"");
	_wscroll=os;
	ciowrap_gotoxy(ti.curx,ti.cury);
}

void ciowrap_clrscr(void)
{
	char *buf;
	int i;
	int width,height;
	struct text_info ti;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&ti);

	width=ti.winright-ti.winleft+1;
	height=ti.winbottom-ti.wintop+1;
	buf=(char *)malloc(width*height*2);
	for(i=0;i<width*height*2;) {
		buf[i++]=' ';
		buf[i++]=ti.attribute;
	}
	ciowrap_puttext(ti.winleft,ti.wintop,ti.winright,ti.winbottom,buf);
	free(buf);
}

void ciowrap_delline(void)
{
	struct text_info ti;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&ti);

	ciowrap_movetext(ti.winleft,ti.cury+1,ti.winright,ti.winbottom,ti.winleft,ti.cury);
	ciowrap_gotoxy(1,ti.winbottom-ti.wintop+1);
	ciowrap_clreol();
	ciowrap_gotoxy(ti.curx,ti.cury);
}

void ciowrap_insline(void)
{
	struct text_info ti;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&ti);

	ciowrap_movetext(ti.winleft,ti.cury,ti.winright,ti.winbottom,ti.winleft,ti.cury+1);
	ciowrap_gotoxy(1,ti.cury);
	ciowrap_clreol();
	ciowrap_gotoxy(ti.curx,ti.cury);
}

int ciowrap_cprintf(char *fmat, ...)
{
    va_list argptr;
	int		pos;
	int		ret;
#ifdef _WIN32			/* Can't figure out a way to allocate a "big enough" buffer for Win32. */
	char	str[16384];
#else
	char	*str;
#endif

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
    va_start(argptr,fmat);
#ifdef WIN32
	ret=vnsprintf(str,sizeof(str)-1,fmat,argptr);
#else
    ret=vsnprintf(NULL,0,fmat,argptr);
	str=(char *)malloc(ret+1);
	if(str==NULL)
		return(EOF);
	ret=vsprintf(str,fmat,argptr);
#endif
    va_end(argptr);
	if(ret>=0)
		ciowrap_cputs(str);
	else
		ret=EOF;
#ifndef WIN32
	free(str);
#endif
    return(ret);
}

int ciowrap_cputs(char *str)
{
	int		pos;
	int		ret=0;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	for(pos=0;str[pos];pos++)
	{
		ret=str[pos];
		if(str[pos]=='\n')
			ciowrap_putch('\r');
		ciowrap_putch(str[pos]);
	}
	return(ret);
}

void ciowrap_textbackground(int colour)
{
	unsigned char attr;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr&=143;
	attr|=(colour<<4);
	ciowrap_textattr(attr);
}

void ciowrap_textcolor(int colour)
{
	unsigned char attr;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr&=240;
	attr|=colour;
	ciowrap_textattr(attr);
}

void ciowrap_highvideo(void)
{
	int attr;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr |= 8;
	ciowrap_textattr(attr);
}

void ciowrap_lowvideo(void)
{
	int attr;

	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr &= 0xf7;
	ciowrap_textattr(attr);
}

void ciowrap_normvideo(void)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	ciowrap_textattr(0x07);
}

int ciowrap_puttext(int a,int b,int c,int d,unsigned char *e)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	return(cio_api.puttext(a,b,c,d,e));
}

int ciowrap_gettext(int a,int b,int c,int d,unsigned char *e)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	return(cio_api.gettext(a,b,c,d,e));
}

void ciowrap_textattr(unsigned char a)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	cio_api.textattr(a);
}

void ciowrap_delay(long a)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	cio_api.delay(a);
}

int ciowrap_putch(unsigned char a)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	return(cio_api.putch(a));
}

void ciowrap_setcursortype(int a)
{
	if(!initialized)
		initciowrap(CIOWRAP_AUTO_MODE);
	cio_api.setcursortype(a);
}
