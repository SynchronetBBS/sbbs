#include <stdarg.h>
#include <stdio.h>

#include "ciowrap.h"
#ifndef NO_X
 #include "x_cio.h"
#endif
#include "curs_cio.h"

cioapi_t	cio_api;

static int ungotch;

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
		cio_api.clrscr=x_clrscr;
		cio_api.gettextinfo=x_gettextinfo;
		cio_api.setcursortype=x_setcursortype;
		cio_api.clreol=x_clreol;
		cio_api.getch=x_getch;
		cio_api.getche=x_getche;
		cio_api.beep=x_beep;
		cio_api.highvideo=x_highvideo;
		cio_api.lowvideo=x_lowvideo;
		cio_api.normvideo=x_normvideo;
		cio_api.textmode=x_textmode;
		return;
	}
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
	cio_api.clrscr=curs_clrscr;
	cio_api.gettextinfo=curs_gettextinfo;
	cio_api.setcursortype=curs_setcursortype;
	cio_api.clreol=curs_clreol;
	cio_api.getch=curs_getch;
	cio_api.getche=curs_getche;
	cio_api.beep=beep;
	cio_api.highvideo=curs_highvideo;
	cio_api.lowvideo=curs_lowvideo;
	cio_api.normvideo=curs_normvideo;
	cio_api.textmode=curs_textmode;
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
