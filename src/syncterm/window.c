/* Copyright (C), 2007 by Stephen Hurd */

#include <gen_defs.h>
#include <ciolib.h>
#include "uifcinit.h"
#include "term.h"
#include "syncterm.h"

int drawwin(void)
{
	struct	text_info txtinfo;
	char	*winbuf;
	char	*p;
	char	str[32];
	int		x,y,c;

    gettextinfo(&txtinfo);

#if 0
	switch(ciolib_to_screen(txtinfo.currmode)) {
		case SCREEN_MODE_ATARI:
		case SCREEN_MODE_ATARI_XEP80:
			strcpy(str,"3ync4%2- ");
			break;
		case SCREEN_MODE_C64:
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			strcpy(str,"SYNCTERM ");
			break;
		default:
			strcpy(str,"SyncTERM ");
	}
#else
	strcpy(str,"         ");
#endif

	if(txtinfo.screenwidth < 80)
		term.width=40;
	else {
		if(txtinfo.screenwidth <132)
			term.width=80;
		else
			term.width=132;
	}
	term.height=txtinfo.screenheight;
	if(!term.nostatus)
		term.height--;
	if(term.height<24) {
		term.height=24;
		term.nostatus=1;
	}
	term.x=(txtinfo.screenwidth-term.width)/2+2;
	term.y=(txtinfo.screenheight-term.height)/2+2;
	if((winbuf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2))==NULL) {
		uifcmsg("Cannot allocate memory for terminal buffer",	"`Memory error`\n\n"
																"Either your system is dangerously low on resources or your\n"
																"window is farking huge!");
		return(-1);
	}

	c=0;
	for(y=0;y<txtinfo.screenheight;y++) {
		p=str;
		for(x=0;x<y;x++) {
			p++;
			if(!*p)
				p=str;
		}
		for(x=0;x<txtinfo.screenwidth;x++) {
			winbuf[c++]=*(p++);
			if(!*p)
				p=str;
			winbuf[c++]=YELLOW|(BLUE<<4);
		}
	}
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,winbuf);
	return(0);
}
