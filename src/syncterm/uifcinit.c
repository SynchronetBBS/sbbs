/* Copyright (C), 2007 by Sephen Hurd */

/* $Id$ */

#include <gen_defs.h>
#include <stdio.h>

#include <ciolib.h>
#include <uifc.h>

#include "uifcinit.h"
#include "syncterm.h"

uifcapi_t uifc; /* User Interface (UIFC) Library API */
static int uifc_initialized=0;

#define UIFC_INIT	(1<<0)
#define WITH_SCRN	(1<<1)
#define WITH_BOT	(1<<2)

static void (*bottomfunc)(int);
int orig_ciolib_xlat;
int orig_vidflags;
int orig_x;
int orig_y;

int	init_uifc(BOOL scrn, BOOL bottom) {
	int	i;
	struct	text_info txtinfo;
	char	top[80];

    gettextinfo(&txtinfo);
	if(!uifc_initialized) {
		/* Set scrn_len to 0 to prevent textmode() call */
		uifc.scrn_len=0;
		orig_ciolib_xlat = ciolib_xlat;
		orig_vidflags = getvideoflags();
		orig_x=wherex();
		orig_y=wherey();
		setvideoflags(orig_vidflags&(CIOLIB_VIDEO_NOBLINK|CIOLIB_VIDEO_BGBRIGHT));
		ciolib_xlat = TRUE;
		uifc.chars = NULL;
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
		ciolib_xlat = orig_ciolib_xlat;
		setvideoflags(orig_vidflags);
		loadfont(NULL);
		gotoxy(orig_x, orig_y);
	}
	uifc_initialized=0;
}

void uifcmsg(char *msg, char *helpbuf)
{
	int i;
	struct ciolib_screen *savscrn;

	i=uifc_initialized;
	if(!i)
		savscrn = savescreen();
	init_uifc(FALSE, FALSE);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		uifc.msg(msg);
		check_exit(FALSE);
	}
	else
		fprintf(stderr,"%s\n",msg);
	if(!i) {
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
	}
}

void uifcinput(char *title, int len, char *msg, int mode, char *helpbuf)
{
	int i;
	struct ciolib_screen *savscrn;

	i=uifc_initialized;
	if(!i)
		savscrn = savescreen();
	init_uifc(FALSE, FALSE);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		uifc.input(WIN_MID|WIN_SAV, 0, 0, title, msg, len, mode);
		check_exit(FALSE);
	}
	else
		fprintf(stderr,"%s\n",msg);
	if(!i) {
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
	}
}

int confirm(char *msg, char *helpbuf)
{
	int i;
	struct ciolib_screen *savscrn;
	char	*options[] = {
				 "Yes"
				,"No"
				,"" };
	int		ret=TRUE;
	int		copt=0;

	i=uifc_initialized;
	if(!i)
		savscrn = savescreen();
	init_uifc(FALSE, FALSE);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&copt,NULL,msg,options)!=0) {
			check_exit(FALSE);
			ret=FALSE;
		}
	}
	if(!i) {
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
	}
	return(ret);
}
