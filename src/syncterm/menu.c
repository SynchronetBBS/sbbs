/* $Id$ */

#include <genwrap.h>
#include <uifc.h>
#include <ciolib.h>
#include <keys.h>

#include "cterm.h"
#include "term.h"
#include "uifcinit.h"
#include "bbslist.h"
#include "conn.h"
#include "window.h"

void viewscroll(void)
{
	int	top;
	int key;
	int i;
	char	*scrollback;
	struct	text_info txtinfo;
	int	x,y;
	struct mouse_event mevent;

	x=wherex();
	y=wherey();
	uifcbail();
    gettextinfo(&txtinfo);
	scrollback=(char *)malloc((term.width*2*backlines)+(txtinfo.screenheight*txtinfo.screenwidth*2));
	memcpy(scrollback,cterm.scrollback,term.width*2*backlines);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,scrollback+(cterm.backpos)*cterm.width*2);
	drawwin();
	top=cterm.backpos;
	gotoxy(1,1);
	for(i=0;!i;) {
		if(top<1)
			top=1;
		if(top>cterm.backpos)
			top=cterm.backpos;
		puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,scrollback+(term.width*2*top));
		key=getch();
		switch(key) {
			case 0xff:
			case 0:
				switch(key|getch()<<8) {
					case CIO_KEY_MOUSE:
						getmouse(&mevent);
						switch(mevent.event) {
							case CIOLIB_BUTTON_1_DRAG_START:
								mousedrag(scrollback);
								break;
						}
						break;
					case CIO_KEY_UP:
						top--;
						break;
					case CIO_KEY_DOWN:
						top++;
						break;
					case CIO_KEY_PPAGE:
						top-=term.height;
						break;
					case CIO_KEY_NPAGE:
						top+=term.height;
						break;
					case CIO_KEY_F(1):
						init_uifc();
						uifc.helpbuf=	"`Scrollback Buffer`\n\n"
										"~ J ~ or ~ Up Arrow ~   Scrolls up one line\n"
										"~ K ~ or ~ Down Arrow ~ Scrolls down one line\n"
										"~ H ~ or ~ Page Up ~    Scrolls up one screen\n"
										"~ L ~ or ~ Page Down ~  Scrolls down one screen\n";
						uifc.showhelp();
						uifcbail();
						drawwin();
						break;
				}
				break;
			case 'j':
			case 'J':
				top--;
				break;
			case 'k':
			case 'K':
				top++;
				break;
			case 'h':
			case 'H':
				top-=term.height;
				break;
			case 'l':
			case 'L':
				top+=term.height;
				break;
			case ESC:
				i=1;
				break;
		}
	}
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,scrollback+(cterm.backpos)*cterm.width*2);
	free(scrollback);
	gotoxy(x,y);
	return;
}

int syncmenu(struct bbslist *bbs)
{
	char	*opts[]={
						 "Scrollback (Alt-B)"
						,"Disconnect (Ctrl-Q)"
						,"Send Login (Alt-L)"
						,"Zmodem Upload (Alt-U)"
						,"Zmodem Download (Alt-D)"
						,"Exit (Alt-X)"
						,""};
	int		opt=0;
	int		i;
	struct	text_info txtinfo;
	char	*buf;
	int		ret;

    gettextinfo(&txtinfo);
	buf=(char *)malloc(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);

	if(cio_api.mode!=CIOLIB_MODE_CURSES
			&& cio_api.mode!=CIOLIB_MODE_CURSES_IBM
			&& cio_api.mode!=CIOLIB_MODE_ANSI) {
		opts[1]="Disconnect (Alt-H)";
	}

	for(ret=0;!ret;) {
		init_uifc();
		i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&opt,NULL,"SyncTERM Online Menu",opts);
		switch(i) {
			case -1:	/* Cancel */
				ret=1;
				break;
			case 0:		/* Scrollback */
				uifcbail();
				puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
				viewscroll();
				break;
			case 1:		/* Disconnect */
				ret=-1;
				break;
			case 2:		/* Login */
				ret=1;
				conn_send(bbs->user,strlen(bbs->user),0);
				conn_send("\r",1,0);
				SLEEP(10);
				conn_send(bbs->password,strlen(bbs->password),0);
				conn_send("\r",1,0);
				break;
			default:
				ret=i;
				uifcbail();
				puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
				free(buf);
				return(ret);
		}
	}

	uifcbail();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	free(buf);
	return(ret);
}
