/* Copyright (C), 2007 by Sephen Hurd */

#include <ciolib.h>
#include <gen_defs.h>
#include <stdio.h>
#include <uifc.h>
#include <vidmodes.h>

#include "filepick.h"
#include "host_ui.h"
#include "syncterm.h"
#include "uifcinit.h"
#include "wren_host.h"

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
uint64_t orig_mouse_events;
static uint64_t uifc_mouse_events;
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
		orig_mouse_events = ciomouse_getevents();
		set_modepalette(palettes[COLOUR_PALETTE]);
		if ((i = uifcini32(&uifc)) != 0) {
			set_modepalette(orig_palette);
			fprintf(stderr, "uifc library init returned error %d\n", i);
			return -1;
		}
		uifc_mouse_events = ciomouse_getevents();
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
	else {
		orig_mouse_events = ciomouse_getevents();
		ciomouse_setevents(uifc_mouse_events);
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
		ciomouse_setevents(orig_mouse_events);
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
	(void)helpbuf;
	host_ui_alert("SyncTERM", msg);
}

int
uifcinput(char *title, int len, char *msg, int mode, char *helpbuf)
{
	return host_ui_prompt(title, helpbuf, msg, (size_t)len + 1,
	    (size_t)len, (mode & K_PASSWORD) != 0);
}

/* Shared init/bail wrapper for filepick + filepick_multi.  Mirrors
 * uifcmsg() / uifcinput() / confirm() so the picker can be invoked
 * from a connected session — without this dance, the UIFC layer's
 * per-session state isn't valid and api->list crashes inside the
 * pane-redraw path. */
static int
uifcfilepick_common(char *title, struct file_pick *fp,
    const char *initial_dir, const char *default_mask, int opts,
    bool multi)
{
	int                   i = uifc_initialized;
	struct ciolib_screen *savscrn = NULL;
	int                   ret = -1;

	if (!i)
		savscrn = savescreen();
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	init_uifc(false, false);
	if (uifc_initialized) {
		wren_host_input_barrier();
		if (multi)
			ret = filepick_multi(&uifc, title, fp, initial_dir,
			    default_mask, opts);
		else
			ret = filepick(&uifc, title, fp, initial_dir,
			    default_mask, opts);
		check_exit(false);
		wren_host_input_barrier();
	}
	if (!i) {
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
	}
	return ret;
}

int
uifcfilepick(char *title, struct file_pick *fp,
    const char *initial_dir, const char *default_mask, int opts)
{
	return uifcfilepick_common(title, fp, initial_dir, default_mask,
	    opts, false);
}

int
uifcfilepick_multi(char *title, struct file_pick *fp,
    const char *initial_dir, const char *default_mask, int opts)
{
	return uifcfilepick_common(title, fp, initial_dir, default_mask,
	    opts, true);
}

int
confirm(char *msg, char *helpbuf)
{
	(void)helpbuf;
	return host_ui_confirm("Confirm", msg);
}
