/* Copyright (C), 2007 by Sephen Hurd */

#include <ciolib.h>
#include <gen_defs.h>
#include <stdio.h>
#include <uifc.h>
#include <vidmodes.h>

#include "syncterm.h"
#include "uifcinit.h"

uifcapi_t  uifc; /* User Interface (UIFC) Library API */
static int uifc_initialized = 0;

#define UIFC_INIT (1 << 0)
#define WITH_SCRN (1 << 1)
#define WITH_BOT (1 << 2)

static void (*bottomfunc) (uifc_winmode_t);
int      orig_vidflags;
int      orig_x;
int      orig_y;
uint32_t orig_palette[16];
bool reveal;

static void
get_title(char *str, size_t sz)
{
	snprintf(str, sz, "%.40s - %.30s", syncterm_version, output_descrs[cio_api.mode]);
}

void
set_uifc_title(void)
{
	char top[80];

	textattr(uifc.bclr | (uifc.cclr<<4));
	get_title(top, sizeof(top));
	gotoxy(1,1);
	clreol();
	gotoxy(3,1);
	cputs(top);
        uifc.timedisplay(true);
}

int
init_uifc(bool scrn, bool bottom)
{
	int               i;
	struct  text_info txtinfo;
	char              top[80];

	gettextinfo(&txtinfo);
	if (!uifc_initialized) {
		reveal = !!(cio_api.options & CONIO_OPT_PRESTEL_REVEAL);
		cio_api.options |= CONIO_OPT_PRESTEL_REVEAL;
                /* Set scrn_len to 0 to prevent textmode() call */
		uifc.scrn_len = 0;
		orig_vidflags = getvideoflags();
		orig_x = wherex();
		orig_y = wherey();
		setvideoflags(orig_vidflags & (CIOLIB_VIDEO_NOBLINK | CIOLIB_VIDEO_BGBRIGHT));
		uifc.chars = NULL;
		get_modepalette(orig_palette);
		set_modepalette(palettes[COLOUR_PALETTE]);
		if ((i = uifcini32(&uifc)) != 0) {
			set_modepalette(orig_palette);
			fprintf(stderr, "uifc library init returned error %d\n", i);
			return -1;
		}
		if ((cio_api.options & (CONIO_OPT_EXTENDED_PALETTE | CONIO_OPT_PALETTE_SETTING))
		    == (CONIO_OPT_EXTENDED_PALETTE | CONIO_OPT_PALETTE_SETTING)) {
			uifc.bclr = BLUE;
			uifc.hclr = YELLOW;
			uifc.lclr = WHITE;
			uifc.cclr = CYAN;
			uifc.lbclr = BLUE | (LIGHTGRAY << 4); /* lightbar color */
		}
		if (settings.uifc_bclr != 8)
			uifc.bclr = settings.uifc_bclr;
		if (settings.uifc_cclr != 8)
			uifc.cclr = settings.uifc_cclr;
		if (settings.uifc_hclr != 16)
			uifc.hclr = settings.uifc_hclr;
		if (settings.uifc_lbclr != 16)
			uifc.lbclr = (uifc.lbclr & 0xf0) | settings.uifc_lbclr;
		if (settings.uifc_lbbclr != 8)
			uifc.lbclr = (uifc.lbclr & 0x0f) | (settings.uifc_lbbclr << 4);
		if (settings.uifc_lclr != 16)
			uifc.lclr = settings.uifc_lclr;
		bottomfunc = uifc.bottomline;
		uifc_initialized = UIFC_INIT;
	}

	if (scrn) {
		get_title(top, sizeof(top));
		if (uifc.scrn(top)) {
			printf(" USCRN (len=%d) failed!\n", uifc.scrn_len + 1);
			uifc.bail();
		}
		uifc_initialized |= (WITH_SCRN | WITH_BOT);
	}
	else {
		uifc.timedisplay = NULL;
		uifc_initialized &= ~WITH_SCRN;
	}

	if (bottom) {
		uifc.bottomline = bottomfunc;
		uifc_initialized |= WITH_BOT;
		gotoxy(1, txtinfo.screenheight);
		textattr(uifc.bclr | (uifc.cclr << 4));
		clreol();
	}
	else {
		uifc.bottomline = NULL;
		uifc_initialized &= ~WITH_BOT;
	}

	return 0;
}

void
uifcbail(void)
{
	if (uifc_initialized) {
		uifc.bail();
		set_modepalette(orig_palette);
		setvideoflags(orig_vidflags);
		loadfont(NULL);
		gotoxy(orig_x, orig_y);
		if (!reveal)
			cio_api.options &= ~CONIO_OPT_PRESTEL_REVEAL;
	}
	uifc_initialized = 0;
}

void
uifcmsg(char *msg, char *helpbuf)
{
	int                   i;
	struct ciolib_screen *savscrn;

	i = uifc_initialized;
	if (!i)
		savscrn = savescreen();
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	init_uifc(false, false);
	if (uifc_initialized) {
		uifc.helpbuf = helpbuf;
		uifc.msg(msg);
		check_exit(false);
	}
	else {
		fprintf(stderr, "%s\n", msg);
	}
	if (!i) {
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
	}
}

void
uifcinput(char *title, int len, char *msg, int mode, char *helpbuf)
{
	int                   i;
	struct ciolib_screen *savscrn;

	i = uifc_initialized;
	if (!i)
		savscrn = savescreen();
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	init_uifc(false, false);
	if (uifc_initialized) {
		uifc.helpbuf = helpbuf;
		uifc.input(WIN_MID | WIN_SAV, 0, 0, title, msg, len, mode);
		check_exit(false);
	}
	else {
		fprintf(stderr, "%s\n", msg);
	}
	if (!i) {
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
	}
}

int
confirm(char *msg, char *helpbuf)
{
	int                   i;
	struct ciolib_screen *savscrn;
	char                 *options[] = {
		"Yes",
		"No",
		""
	};
	int                   ret = true;
	int                   copt = 0;

	i = uifc_initialized;
	if (!i)
		savscrn = savescreen();
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	init_uifc(false, false);
	if (uifc_initialized) {
		uifc.helpbuf = helpbuf;
		if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &copt, NULL, msg, options) != 0) {
			check_exit(false);
			ret = false;
		}
	}
	if (!i) {
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
	}
	return ret;
}
