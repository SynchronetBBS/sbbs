#include <stdarg.h>
#include <stdio.h>

#include "conio.h"
#ifndef NO_X
 #include "x_cio.h"
#endif
#include "curs_cio.h"
#undef getch

cioapi_t	cio_api;

static int ungotch;
static struct text_info cio_textinfo;
static int lastmode=3;
int _wscroll=1;
int directvideo=0;
static int initialized=0;

void initciowrap(int mode)
{
#ifndef NO_X
	if(!console_init()) {
		cio_api.mode=X_MODE;
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
	}
	else {
		fprintf(stderr,"X init failed\n");
#endif
		curs_initciowrap(mode);
		cio_api.mode=CURSES_MODE;
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
#ifndef NO_X
	}
#endif
	initialized=1;
	gettextinfo(&cio_textinfo);
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
	cio_textinfo.normattr=7;
}

int kbhit(void)
{
	if(!initialized)
		initciowrap(3);
	if(ungotch)
		return(1);
	return(cio_api.kbhit());
}

int getch(void)
{
	int ch;

	if(!initialized)
		initciowrap(3);
	if(ungotch) {
		ch=ungotch;
		ungotch=0;
		return(ch);
	}
	return(cio_api.getch());
}

int getche(void)
{
	int ch;

	if(!initialized)
		initciowrap(3);
	if(ungotch) {
		ch=ungotch;
		ungotch=0;
		putch(ch);
		return(ch);
	}
	return(cio_api.getche());
}

int ungetch(int ch)
{
	if(!initialized)
		initciowrap(3);
	if(ungotch)
		return(EOF);
	ungotch=ch;
	return(ch);
}

int movetext(int sx, int sy, int ex, int ey, int dx, int dy)
{
	int width;
	int height;
	char *buf;

	if(!initialized)
		initciowrap(3);
	width=ex-sx;
	height=ey-sy;
	buf=(char *)malloc((width+1)*(height+1)*2);
	if(buf==NULL)
		return(0);
	if(!gettext(sx,sy,ex,ey,buf)) {
		free(buf);
		return(0);
	}
	if(!puttext(dx,dy,dx+width,dy+height,buf)) {
		free(buf);
		return(0);
	}
	free(buf);
	return(1);
}

char *cgets(char *str)
{
	int	maxlen;
	int len=0;
	int chars;
	int ch;

	if(!initialized)
		initciowrap(3);
	maxlen=*(unsigned char *)str;
	while((ch=getche())!='\n') {
		switch(ch) {
			case 0:	/* Skip extended keys */
				ch=getche();
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
				putch('\b');
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

int cscanf (char *format , ...)
{
	char str[255];
    va_list argptr;
	int ret;

	if(!initialized)
		initciowrap(3);
	str[0]=-1;
	va_start(argptr,format);
	ret=vsscanf(cgets(str),format,argptr);
	va_end(argptr);
	return(ret);
}

char *getpass(const char *prompt)
{
	static char pass[9];
	int len=0;
	int chars;
	int ch;

	if(!initialized)
		initciowrap(3);
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

void gettextinfo(struct text_info *info)
{
	if(!initialized)
		initciowrap(3);
	else {
		cio_api.gettextinfo(&cio_textinfo);
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
}

void wscroll(void)
{
	char *buf;
	int os;
	struct text_info ti;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&ti);
	if(!_wscroll)
		return;
	movetext(ti.winleft,ti.wintop+1,ti.winright,ti.winbottom,ti.winleft,ti.wintop);
	gotoxy(1,ti.winbottom-ti.winleft+1);
	os=_wscroll;
	_wscroll=0;
	cprintf("%*s",ti.winright-ti.winleft+1,"");
	_wscroll=os;
	gotoxy(ti.curx,ti.cury);
}

int wherex(void)
{
	int x;

	if(!initialized)
		initciowrap(3);
	x=cio_api.wherex();
	x=x-cio_textinfo.winleft+1;
	return(x);
}

int wherey(void)
{
	int y;

	if(!initialized)
		initciowrap(3);
	y=cio_api.wherey();
	y=y-cio_textinfo.wintop+1;
	return(y);
}

void gotoxy(int x, int y)
{
	int nx;
	int ny;
	struct text_info ti;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&ti);
	if(		x < 1
			|| x > ti.winright-ti.winleft+1
			|| y < 1
			|| y > ti.winbottom-ti.wintop+1)
		return;
	nx=x+ti.winleft-1;
	ny=y+ti.wintop-1;
	cio_api.gotoxy(nx,ny);
}

void textmode(mode)
{
	if(!initialized)
		initciowrap(3);
	if(mode==-1) {
		gettextinfo(&cio_textinfo);
		cio_api.textmode(lastmode);
		lastmode=cio_textinfo.currmode;
	}
	else {
		gettextinfo(&cio_textinfo);
		lastmode=cio_textinfo.currmode;
		cio_api.textmode(mode);
	}
	gettextinfo(&cio_textinfo);
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
}

void window(int sx, int sy, int ex, int ey)
{
	if(!initialized)
		initciowrap(3);
	gettextinfo(&cio_textinfo);
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
	gotoxy(1,1);
}

void clreol(void)
{
	int os;
	struct text_info	ti;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&ti);
	os=_wscroll;
	_wscroll=0;
	cprintf("%*s",ti.winright-ti.curx+1,"");
	_wscroll=os;
	gotoxy(ti.curx,ti.cury);
}

void clrscr(void)
{
	char *buf;
	int i;
	struct text_info ti;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&ti);

	buf=(char *)malloc(ti.screenheight*ti.screenwidth*2);
	for(i=0;i<ti.screenheight*ti.screenwidth*2;) {
		buf[i++]=' ';
		buf[i++]=ti.attribute;
	}
	puttext(1,1,ti.screenwidth,ti.screenheight,buf);
	free(buf);
}

void delline(void)
{
	struct text_info ti;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&ti);

	movetext(ti.winleft,ti.cury+1,ti.winright,ti.winbottom,ti.winleft,ti.cury);
	gotoxy(1,ti.winbottom-ti.wintop+1);
	clreol();
	gotoxy(ti.curx,ti.cury);
}

void insline(void)
{
	struct text_info ti;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&ti);

	movetext(ti.winleft,ti.cury,ti.winright,ti.winbottom,ti.winleft,ti.cury+1);
	gotoxy(1,ti.cury);
	clreol();
	gotoxy(ti.curx,ti.cury);
}

int cprintf(char *fmat, ...)
{
    va_list argptr;
	char	str[4097];
	int		pos;
	int		ret;

	if(!initialized)
		initciowrap(3);
    va_start(argptr,fmat);
    ret=vsprintf(str,fmat,argptr);
    va_end(argptr);
	if(ret>=0)
		cputs(str);
	else
		ret=EOF;
    return(ret);
}

int cputs(char *str)
{
	int		pos;
	int		ret=0;

	if(!initialized)
		initciowrap(3);
	for(pos=0;str[pos];pos++)
	{
		ret=str[pos];
		if(str[pos]=='\n')
			putch('\r');
		putch(str[pos]);
	}
	return(ret);
}

void textbackground(int colour)
{
	unsigned char attr;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr&=143;
	attr|=(colour<<4);
	textattr(attr);
}

void textcolor(int colour)
{
	unsigned char attr;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr&=240;
	attr|=colour;
	textattr(attr);
}

void highvideo(void)
{
	int attr;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr |= 8;
	textattr(attr);
}

void lowvideo(void)
{
	int attr;

	if(!initialized)
		initciowrap(3);
	gettextinfo(&cio_textinfo);
	attr=cio_textinfo.attribute;
	attr &= 0xf7;
	textattr(attr);
}

void normvideo(void)
{
	if(!initialized)
		initciowrap(3);
	textattr(0x07);
}

int puttext(int a,int b,int c,int d,char *e)
{
	if(!initialized)
		initciowrap(3);
	return(cio_api.puttext(a,b,c,d,e));
}

int gettext(int a,int b,int c,int d,char *e)
{
	if(!initialized)
		initciowrap(3);
	return(cio_api.gettext(a,b,c,d,e));
}

void textattr(unsigned char a)
{
	if(!initialized)
		initciowrap(3);
	cio_api.textattr(a);
}

void delay(long a)
{
	if(!initialized)
		initciowrap(3);
	cio_api.delay(a);
}

int putch(unsigned char a)
{
	if(!initialized)
		initciowrap(3);
	return(cio_api.putch(a));
}

void _setcursortype(int a)
{
	if(!initialized)
		initciowrap(3);
	cio_api.setcursortype(a);
}
