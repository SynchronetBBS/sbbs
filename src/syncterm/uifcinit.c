/* Copyright (C), 2007 by Stephen Hurd */

/* $Id$ */

#include <gen_defs.h>
#include <stdio.h>

#include <ciolib.h>
#include <uifc.h>

#include "uifcinit.h"
#include "syncterm.h"

uifcapi_t uifc; /* User Interface (UIFC) Library API */
static int uifc_initialized=0;
static int uifc_old_font=0;

#define UIFC_INIT	(1<<0)
#define WITH_SCRN	(1<<1)
#define WITH_BOT	(1<<2)

static void (*bottomfunc)(int);

int	init_uifc(BOOL scrn, BOOL bottom) {
	int	i;
	struct	text_info txtinfo;
	char	top[80];

    gettextinfo(&txtinfo);
	if(!uifc_initialized) {
		/* Get old font... */
		uifc_old_font=getfont();
		if(uifc_old_font >= 32 && uifc_old_font <= 36)
			setfont(0, FALSE);
		/* Set scrn_len to 0 to prevent textmode() call */
		uifc.scrn_len=0;
		if((i=uifcini32(&uifc))!=0) {
			fprintf(stderr,"uifc library init returned error %d\n",i);
			return(-1);
		}
		bottomfunc=uifc.bottomline;
		uifc_initialized=UIFC_INIT;
	}

	if(scrn) {
		sprintf(top, "%.40s - %.30s", syncterm_version, output_descrs[cio_api.mode]);
		if(uifc.scrn(top)) {
			printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
			uifc_initialized=0;
			uifc.bail();
		}
		uifc_initialized |= (WITH_SCRN|WITH_BOT);
	}
	else {
		uifc.timedisplay=NULL;
		uifc_initialized &= ~WITH_SCRN;
	}

	if(bottom) {
		uifc.bottomline=bottomfunc;
		uifc_initialized |= WITH_BOT;
		gotoxy(1, txtinfo.screenheight);
		textattr(uifc.bclr|(uifc.cclr<<4));
		clreol();
	}
	else {
		uifc.bottomline=NULL;
		uifc_initialized &= ~WITH_BOT;
	}

	return(0);
}

void uifcbail(void)
{
	if(uifc_initialized) {
		uifc.bail();
		if(uifc_old_font != getfont())
			setfont(uifc_old_font, FALSE);
	}
	uifc_initialized=0;
}

void uifcmsg(char *msg, char *helpbuf)
{
	int i;
	char	*buf;
	struct	text_info txtinfo;

    gettextinfo(&txtinfo);
	i=uifc_initialized;
	if(!i) {
		buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
		gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	}
	init_uifc(FALSE, FALSE);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		uifc.msg(msg);
	}
	else
		fprintf(stderr,"%s\n",msg);
	if(!i) {
		uifcbail();
		puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	}
}

void uifcinput(char *title, int len, char *msg, int mode, char *helpbuf)
{
	int i;
	char	*buf;
	struct	text_info txtinfo;

    gettextinfo(&txtinfo);
	i=uifc_initialized;
	if(!i) {
		buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
		gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	}
	init_uifc(FALSE, FALSE);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		uifc.input(WIN_MID|WIN_SAV, 0, 0, title, msg, len, mode);
	}
	else
		fprintf(stderr,"%s\n",msg);
	if(!i) {
		uifcbail();
		puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	}
}

int confirm(char *msg, char *helpbuf)
{
	int i;
	char	*buf;
	struct	text_info txtinfo;
	char	*options[] = {
				 "Yes"
				,"No"
				,"" };
	int		ret=TRUE;
	int		copt=0;

    gettextinfo(&txtinfo);
	i=uifc_initialized;
	if(!i) {
		buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
		gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	}
	init_uifc(FALSE, FALSE);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&copt,NULL,msg,options)!=0)
			ret=FALSE;
	}
	if(!i) {
		uifcbail();
		puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	}
	return(ret);
}
