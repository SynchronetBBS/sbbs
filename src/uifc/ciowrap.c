#include <stdarg.h>
#include <stdio.h>

#include "ciowrap.h"
#ifndef NO_X
 #include "x_cio.h"
#endif
#include "curs_cio.h"

cioapi_t	cio_api;

static int ungotch;
static struct text_info cio_textinfo;
static int lastmode=3;
int _wscroll=1;
int directvideo=0;

void initciowrap(int mode)
{
#ifndef NO_X
	if(!console_init()) {
		cio_api.mode=X_MODE;
		cio_api.puttext=x_puttext;
		cio_api.gettext=x_gettext;
		cio_api.textattr=x_textattr;
		cio_api.textbackground=x_textbackground;
		cio_api.textcolor=x_textcolor;
		cio_api.kbhit=x_kbhit;
		cio_api.delay=x_delay;
		cio_api.wherey=x_wherey;
		cio_api.wherex=x_wherex;
		cio_api.putch=x_putch;
		cio_api.c_printf=x_cprintf;
		cio_api.cputs=x_cputs;
		cio_api.gotoxy=x_gotoxy;
		cio_api.gettextinfo=x_gettextinfo;
		cio_api.setcursortype=x_setcursortype;
		cio_api.getch=x_getch;
		cio_api.getche=x_getche;
		cio_api.beep=x_beep;
		cio_api.highvideo=x_highvideo;
		cio_api.lowvideo=x_lowvideo;
		cio_api.normvideo=x_normvideo;
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
		cio_api.textbackground=curs_textbackground;
		cio_api.textcolor=curs_textcolor;
		cio_api.kbhit=curs_kbhit;
		cio_api.delay=curs_delay;
		cio_api.wherey=curs_wherey;
		cio_api.wherex=curs_wherex;
		cio_api.putch=curs_putch;
		cio_api.c_printf=curs_cprintf;
		cio_api.cputs=curs_cputs;
		cio_api.gotoxy=curs_gotoxy;
		cio_api.gettextinfo=curs_gettextinfo;
		cio_api.setcursortype=curs_setcursortype;
		cio_api.getch=curs_getch;
		cio_api.getche=curs_getche;
		cio_api.beep=beep;
		cio_api.highvideo=curs_highvideo;
		cio_api.lowvideo=curs_lowvideo;
		cio_api.normvideo=curs_normvideo;
		cio_api.textmode=curs_textmode;
#ifndef NO_X
	}
#endif
	gettextinfo(&cio_textinfo);
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
	cio_textinfo.normattr=7;
}

int kbhit(void)
{
	if(ungotch)
		return(1);
	return(cio_api.kbhit());
}

int getch(void)
{
	int ch;

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
					beep();
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
					beep();
					break;
				}
				len--;
				break;
			default:
				if(len==8)
					beep();
				else
					pass[len++]=ch;
				break;
		}
	}
}

void gettextinfo(struct text_info *info)
{
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

void wscroll(void)
{
	char *buf;
	int os;

	gettextinfo(&cio_textinfo);
	if(!_wscroll)
		return;
	movetext(cio_textinfo.winleft,cio_textinfo.wintop+1,cio_textinfo.winright,cio_textinfo.winbottom,cio_textinfo.winleft,cio_textinfo.wintop);
	gotoxy(1,cio_textinfo.winbottom-cio_textinfo.winleft+1);
	os=_wscroll;
	_wscroll=0;
	cprintf("%*s",cio_textinfo.winright-cio_textinfo.winleft+1,"");
	_wscroll=os;
	gotoxy(cio_textinfo.curx,cio_textinfo.cury);
}

int wherex(void)
{
	int x;

	x=cio_api.wherex();
	x=x-cio_textinfo.winleft+1;
	return(x);
}

int wherey(void)
{
	int y;

	y=cio_api.wherey();
	y=y-cio_textinfo.wintop+1;
	return(y);
}

void gotoxy(int x, int y)
{
	int nx;
	int ny;

	gettextinfo(&cio_textinfo);
	if(		x<1
			|| x > cio_textinfo.winright-cio_textinfo.winleft+1
			|| y < 1
			|| x > cio_textinfo.winbottom-cio_textinfo.wintop+1)
		return;
	nx=x+cio_textinfo.winleft-1;
	ny=y+cio_textinfo.wintop-1;
	cio_api.gotoxy(nx,ny);
}

void textmode(mode)
{
	if(mode==-1) {
		gettextinfo(&cio_textinfo);
		cio_api.textmode(lastmode);
		lastmode=cio_textinfo.videomode;
	}
	else {
		gettextinfo(&cio_textinfo);
		lastmode=cio_textinfo.videomode;
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

	gettextinfo(&cio_textinfo);
	os=_wscroll;
	_wscroll=0;
	cprintf("%*s",cio_textinfo.winright-cio_textinfo.curx+1,"");
	_wscroll=os;
	gotoxy(cio_textinfo.curx,cio_textinfo.cury);
}

void clrscr(void)
{
	char *buf;
	int i;
	gettextinfo(&cio_textinfo);

	buf=(char *)malloc(cio_textinfo.screenheight*cio_textinfo.screenwidth*2);
	for(i=0;i<cio_textinfo.screenheight*cio_textinfo.screenwidth*2;) {
		buf[i++]=' ';
		buf[i++]=cio_textinfo.attribute;
	}
	puttext(1,1,cio_textinfo.screenwidth,cio_textinfo.screenheight,buf);
	free(buf);
}

void delline(void)
{
	gettextinfo(&cio_textinfo);

	movetext(cio_textinfo.winleft,cio_textinfo.cury+1,cio_textinfo.winright,cio_textinfo.winbottom,cio_textinfo.winleft,cio_textinfo.cury);
	gotoxy(1,cio_textinfo.winbottom-cio_textinfo.wintop+1);
	clreol();
	gotoxy(cio_textinfo.curx,cio_textinfo.cury);
}

void insline(void)
{
	gettextinfo(&cio_textinfo);

	movetext(cio_textinfo.winleft,cio_textinfo.cury,cio_textinfo.winright,cio_textinfo.winbottom,cio_textinfo.winleft,cio_textinfo.cury+1);
	gotoxy(1,cio_textinfo.cury);
	clreol();
	gotoxy(cio_textinfo.curx,cio_textinfo.cury);
}
