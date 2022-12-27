/* Copyright (C), 2007 by Sephen Hurd */

/* $Id: uifcinit.c,v 1.44 2020/05/15 11:05:40 deuce Exp $ */

#include <gen_defs.h>
#include <stdio.h>

#include <ciolib.h>
#include <vidmodes.h>
#include <uifc.h>

#include "uifcinit.h"
#include "syncterm.h"

uifcapi_t uifc; /* User Interface (UIFC) Library API */
static int uifc_initialized=0;

#define UIFC_INIT	(1<<0)
#define WITH_SCRN	(1<<1)
#define WITH_BOT	(1<<2)

static void (*bottomfunc)(uifc_winmode_t);
int orig_vidflags;
int orig_x;
int orig_y;
uint32_t orig_palette[16];

int
init_uifc(bool scrn, bool bottom) {
	int	i;
	struct	text_info txtinfo;
	char	top[80];

	gettextinfo(&txtinfo);
	if(!uifc_initialized) {
		/* Set scrn_len to 0 to prevent textmode() call */
		uifc.scrn_len=0;
		orig_vidflags = getvideoflags();
		orig_x=wherex();
		orig_y=wherey();
		setvideoflags(orig_vidflags&(CIOLIB_VIDEO_NOBLINK|CIOLIB_VIDEO_BGBRIGHT));
		uifc.chars = NULL;
		if((i=uifcini32(&uifc))!=0) {
			fprintf(stderr,"uifc library init returned error %d\n",i);
			return(-1);
		}
		bottomfunc=uifc.bottomline;
		uifc_initialized=UIFC_INIT;
		get_modepalette(orig_palette);
		set_modepalette(palettes[COLOUR_PALETTE]);
		if ((cio_api.options & (CONIO_OPT_EXTENDED_PALETTE | CONIO_OPT_PALETTE_SETTING)) == (CONIO_OPT_EXTENDED_PALETTE | CONIO_OPT_PALETTE_SETTING)) {
			uifc.bclr=BLUE;
			uifc.hclr=YELLOW;
			uifc.lclr=WHITE;
			uifc.cclr=CYAN;
			uifc.lbclr=BLUE|(LIGHTGRAY<<4);	/* lightbar color */
		}
	}

	if(scrn) {
		sprintf(top, "%.40s - %.30s", syncterm_version, output_descrs[cio_api.mode]);
		if(uifc.scrn(top)) {
			printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
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
		set_modepalette(orig_palette);
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
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	init_uifc(false, false);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		uifc.msg(msg);
		check_exit(false);
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
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	init_uifc(false, false);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		uifc.input(WIN_MID|WIN_SAV, 0, 0, title, msg, len, mode);
		check_exit(false);
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
	int		ret=true;
	int		copt=0;

	i=uifc_initialized;
	if(!i)
		savscrn = savescreen();
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	init_uifc(false, false);
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&copt,NULL,msg,options)!=0) {
			check_exit(false);
			ret=false;
		}
	}
	if(!i) {
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
	}
	return(ret);
}
