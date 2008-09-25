/* Copyright (C), 2007 by Stephen Hurd */

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
#include "syncterm.h"

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
	/* too large for alloca() */
	scrollback=(char *)malloc((scrollback_buf==NULL?0:(term.width*2*settings.backlines))+(txtinfo.screenheight*txtinfo.screenwidth*2));
	if(cterm.scrollback != NULL)
		memcpy(scrollback,cterm.scrollback,term.width*2*settings.backlines);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,scrollback+(cterm.backpos)*cterm.width*2);
	drawwin();
	top=cterm.backpos;
	gotoxy(1,1);
	textattr(uifc.hclr|(uifc.bclr<<4)|BLINK);
	for(i=0;!i;) {
		if(top<1)
			top=1;
		if(top>cterm.backpos)
			top=cterm.backpos;
		puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,scrollback+(term.width*2*top));
		switch(cterm.emulation) {
		case CTERM_EMULATION_ATASCII:
			cputs("3crollback");
			break;
		case CTERM_EMULATION_PETASCII:
			cputs("SCROLLBACK");
			break;
		default:
			cputs("Scrollback");
		}
		gotoxy(cterm.width-9,1);
		switch(cterm.emulation) {
		case CTERM_EMULATION_ATASCII:
			cputs("3crollback");
			break;
		case CTERM_EMULATION_PETASCII:
			cputs("SCROLLBACK");
			break;
		default:
			cputs("Scrollback");
		}
		gotoxy(1,1);
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
						init_uifc(FALSE, FALSE);
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
	gotoxy(x,y);
	free(scrollback);
	return;
}

int syncmenu(struct bbslist *bbs, int *speed)
{
	char	*opts[]={
						 "Scrollback ("ALT_KEY_NAMEP"-B)"
						,"Disconnect (Ctrl-Q)"
						,"Send Login ("ALT_KEY_NAMEP"-L)"
						,"Upload ("ALT_KEY_NAMEP"-U)"
						,"Download ("ALT_KEY_NAMEP"-D)"
						,"Change Output Rate ("ALT_KEY_NAMEP"-Up/"ALT_KEY_NAMEP"-Down)"
						,"Change Log Level"
						,"Capture Control ("ALT_KEY_NAMEP"-C)"
						,"ANSI Music Control ("ALT_KEY_NAMEP"-M)"
						,"Font Control ("ALT_KEY_NAMEP"-F)"
						,"Toggle Doorway Mode"
#ifndef WITHOUT_OOII
						,"Toggle Operation Overkill ][ Mode"
#endif
						,"Exit ("ALT_KEY_NAMEP"-X)"
						,"Edit Dialing Directory ("ALT_KEY_NAMEP"-E)"
						,""};
	int		opt=0;
	int		i,j;
	struct	text_info txtinfo;
	char	*buf;
	int		ret;

    gettextinfo(&txtinfo);
	buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);

	if(cio_api.mode!=CIOLIB_MODE_CURSES
			&& cio_api.mode!=CIOLIB_MODE_CURSES_IBM
			&& cio_api.mode!=CIOLIB_MODE_ANSI) {
		opts[1]="Disconnect ("ALT_KEY_NAMEP"-H)";
	}

	for(ret=0;!ret;) {
		init_uifc(FALSE, !(bbs->nostatus));
		uifc.helpbuf=	"`Online Menu`\n\n"
						"`Scrollback`     Allows to you to view the scrollback buffer\n"
						"`Disconnect`     Disconnects the current connection\n"
						"`Send Login`     Sends the username and password pair separated by CR\n"
						"`Upload`         Initiates a file upload (ZMODEM or ASCII)\n"
						"`Download`       Initiates a file download (ZMODEM)\n"
						"`Log Level`      Changes the minimum log level for ZMODEM information\n"
						"`Output Rate`    Changes the speed characters are output to the screen\n"
						"`Capture`        Enables/Disables screen capture\n"
						"`ANSI Music`     Enables/Disables ANSI Music\n"
						"`Font`           Changes the current font (when supported)\n"
						"`Doorway Mode`   Toggles the current DoorWay (keyboard input) setting\n"
#ifndef WITHOUT_OOII
						"`Operation Overkill ][ Mode`   Toggles the current Operation Overkill ][ setting\n"
#endif
						"`Exit`           Disconnects and closes Syncterm";
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
				if(bbs->syspass[0]) {
					SLEEP(10);
					conn_send(bbs->syspass,strlen(bbs->syspass),0);
					conn_send("\r",1,0);
				}
				break;
			case 5:		/* Output rate */
				if(bbs->conn_type==CONN_TYPE_MODEM || bbs->conn_type==CONN_TYPE_SERIAL)
					uifcmsg("Not supported for this connection type"
						,"Cannot change the display rate for Modem/Serial connections.");
				else if(speed != NULL) {
					j=get_rate_num(*speed);
					uifc.helpbuf="`Output Rate`\n\n"
							"The output rate is the rate in emulated \"bits per second\" to draw incoming\n"
							"data on the screen.  This rate is a maximum, not guaranteed to be attained\n"
							"In general, you will only use this option for ANSI animations.";
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,NULL,"Output Rate",rate_names);
					if(i>=0)
						*speed = rates[i];
				}
				ret=5;
				break;
			case 6:		/* Change log level (temporarily) */
				j=log_level;
				uifc.helpbuf="`Log Level\n\n"
						"The log level changes the verbosity of messages shown in the transfer\n"
						"window.  For the selected log level, messages of that level and those above\n"
						"it will be displayed.";
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,NULL,"Log Level",log_levels);
				if(i>=0)
					log_level = j;
				ret=6;
				break;
			default:
				ret=i;
				uifcbail();
				puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
				return(ret);
		}
	}

	uifcbail();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	return(ret);
}
