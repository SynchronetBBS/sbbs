/* Modern implementation of UIFC (user interface) library based on uifc.c */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifdef __unix__
	#include <stdio.h>
	#include <unistd.h>
	#ifdef __QNX__
		#include <strings.h>
	#endif
	#define mswait(x) delay(x)
#elif defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <share.h>
	#include <windows.h>
	#define mswait(x) Sleep(x)
#endif
#include <genwrap.h>    // for alloca()
#include <datewrap.h>   // localtime_r()
#include "xpprintf.h"

#include "ciolib.h"
#include "uifc.h"
#define MAX_GETSTR  5120

#define BLINK   128

static int               cursor;
static char*             helpfile = 0;
static uint              helpline = 0;
static size_t            blk_scrn_len;
static struct vmem_cell *blk_scrn;
static unsigned char     blk_scrn_attr = 0;
static unsigned char     blk_scrn_ch = 0;
static struct vmem_cell *tmp_buffer;
static struct vmem_cell *tmp_buffer2;
static win_t             sav[MAX_BUFS];
static uifcapi_t*        api;

/* Prototypes */
static int   uprintf(int x, int y, unsigned attr, char *fmt, ...);
static void  bottomline(uifc_winmode_t);
static char  *utimestr(time_t *intime);
static void  help(void);
static int   ugetstr(int left, int top, int width, char *outstr, int max, long mode, int *lastkey);
static void  timedisplay(BOOL force);

/* API routines */
static void uifcbail(void);
static int  uscrn(const char *str);
static int  ulist(uifc_winmode_t, int left, int top, int width, int *dflt, int *bar
                  , const char *title, char **option);
static int  uinput(uifc_winmode_t, int left, int top, const char *prompt, char *str
                   , int len, int kmode);
static int  umsg(const char *str);
static int  umsgf(const char *fmt, ...);
static BOOL confirm(const char *fmt, ...);
static BOOL deny(const char *fmt, ...);
static void upop(const char *str, ...);
static void sethelp(int line, char* file);
static void showbuf(uifc_winmode_t, int left, int top, int width, int height, const char *title
                    , const char *hbuf, int *curp, int *barp);
static BOOL restore(void);

/* Dynamic menu support */
static int *last_menu_cur = NULL;
static int *last_menu_bar = NULL;
static int  save_menu_cur = -1;
static int  save_menu_bar = -1;
static int  save_menu_opts = -1;

char*       uifcYesNoOpts[] = {"Yes", "No", NULL};

static void reset_dynamic(void) {
	last_menu_cur = NULL;
	last_menu_bar = NULL;
	save_menu_cur = -1;
	save_menu_bar = -1;
	save_menu_opts = -1;
}

static uifc_graphics_t cp437_chars = {
	.background = 0xb0,
	.help_char = '?',
	.close_char = 0xfe,
	.up_arrow = 30,
	.down_arrow = 31,
	.left_arrow = 17,
	.right_arrow = 16,
	.button_left = '[',
	.button_right = ']',

	.list_top_left = 0xc9,
	.list_top = 0xcd,
	.list_top_right = 0xbb,
	.list_separator_left = 0xcc,
	.list_separator_right = 0xb9,
	.list_horizontal_separator = 0xcd,
	.list_left = 0xba,
	.list_right = 0xba,
	.list_bottom_left = 0xc8,
	.list_bottom_right = 0xbc,
	.list_bottom = 0xcd,
	.list_scrollbar_separator = 0xb3,

	.input_top_left = 0xc9,
	.input_top = 0xcd,
	.input_top_right = 0xbb,
	.input_left = 0xba,
	.input_right = 0xba,
	.input_bottom_left = 0xc8,
	.input_bottom_right = 0xbc,
	.input_bottom = 0xcd,

	.popup_top_left = 0xda,
	.popup_top = 0xc4,
	.popup_top_right = 0xbf,
	.popup_left = 0xb3,
	.popup_right = 0xb3,
	.popup_bottom_left = 0xc0,
	.popup_bottom_right = 0xd9,
	.popup_bottom = 0xc4,

	.help_top_left = 0xda,
	.help_top = 0xc4,
	.help_top_right = 0xbf,
	.help_left = 0xb3,
	.help_right = 0xb3,
	.help_bottom_left = 0xc0,
	.help_bottom_right = 0xd9,
	.help_bottom = 0xc4,
	.help_titlebreak_left = 0xb4,
	.help_titlebreak_right = 0xc3,
	.help_hitanykey_left = 0xb4,
	.help_hitanykey_right = 0xc3,
};

/****************************************************************************/
/* Initialization function, see uifc.h for details.							*/
/* Returns 0 on success.													*/
/****************************************************************************/

void uifc_mouse_enable(void)
{
	ciomouse_setevents(0);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_1_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_2_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
	ciomouse_addevent(CIOLIB_BUTTON_5_PRESS);
	mousepointer(CIOLIB_MOUSEPTR_BAR);
	showmouse();
}

void uifc_mouse_disable(void)
{
	ciomouse_setevents(0);
	hidemouse();
}

int uifc_kbwait(void) {
	return kbwait(100);
}

static int
dyn_kbwait(uifc_winmode_t mode) {
	if (mode & WIN_DYN)
		return kbhit();
	return uifc_kbwait();
}

int inkey(void)
{
	int c;

	c = getch();
	if (!c || c == 0xe0) {
		c |= (getch() << 8);
		if (c == CIO_KEY_LITERAL_E0)
			c = 0xe0;
	}
	return c;
}

static void
fill_blk_scrn(BOOL force)
{
	uchar attr = api->cclr | (api->bclr << 4);
	if (!blk_scrn)
		return;
	if (force || blk_scrn_attr != attr || blk_scrn_ch != api->chars->background) {
		for (int i = 0; i < blk_scrn_len; i++) {
			blk_scrn[i].legacy_attr = attr;
			blk_scrn[i].ch = api->chars->background;
			blk_scrn[i].font = 0;
			attr2palette(blk_scrn[i].legacy_attr, &blk_scrn[i].fg, &blk_scrn[i].bg);
		}
		blk_scrn_ch = api->chars->background;
		blk_scrn_attr = attr;
	}
}

int uifcini32(uifcapi_t* uifcapi)
{
	unsigned          i;
	struct  text_info txtinfo;

	if (uifcapi == NULL || uifcapi->size != sizeof(uifcapi_t))
		return -1;

	api = uifcapi;
	if (api->chars == NULL)
		api->chars = &cp437_chars;

	if (api->yesNoOpts == NULL)
		api->yesNoOpts = uifcYesNoOpts;

	/* install function handlers */
	api->bail = uifcbail;
	api->scrn = uscrn;
	api->msg = umsg;
	api->msgf = umsgf;
	api->confirm = confirm;
	api->deny = deny;
	api->pop = upop;
	api->list = ulist;
	api->restore = restore;
	api->input = uinput;
	api->sethelp = sethelp;
	api->showhelp = help;
	api->showbuf = showbuf;
	api->timedisplay = timedisplay;
	api->bottomline = bottomline;
	api->getstrxy = ugetstr;
	api->printf = uprintf;

	if (api->scrn_len != 0) {
		switch (api->scrn_len) {
			case 14:
				textmode(C80X14);
				break;
			case 21:
				textmode(C80X21);
				break;
			case 25:
				textmode(C80);
				break;
			case 28:
				textmode(C80X28);
				break;
			case 43:
				textmode(C80X43);
				break;
			case 50:
				textmode(C80X50);
				break;
			case 60:
				textmode(C80X60);
				break;
			default:
				textmode(C4350);
				break;
		}
	}

#if 0
	clrscr();
#endif

	gettextinfo(&txtinfo);
	/* unsupported mode? */
	if (txtinfo.screenheight < MIN_LINES
/*        || txtinfo.screenheight>MAX_LINES */
	    || txtinfo.screenwidth < 40) {
		textmode(C80);  /* set mode to 80x25*/
		gettextinfo(&txtinfo);
	}
	window(1, 1, txtinfo.screenwidth, txtinfo.screenheight);

	api->scrn_len = txtinfo.screenheight;
	if (api->scrn_len < MIN_LINES) {
		uifcbail();
		printf("\r\nUIFC: Screen length (%u) must be %d lines or greater\r\n"
		       , api->scrn_len, MIN_LINES);
		return -2;
	}
	api->scrn_len--; /* account for status line */

	if (txtinfo.screenwidth < 40) {
		uifcbail();
		printf("\r\nUIFC: Screen width (%u) must be at least 40 characters\r\n"
		       , txtinfo.screenwidth);
		return -3;
	}
	api->scrn_width = txtinfo.screenwidth;

	if (!(api->mode & UIFC_COLOR)
	    && (api->mode & UIFC_MONO
	        || txtinfo.currmode == MONO || txtinfo.currmode == BW40 || txtinfo.currmode == BW80
	        || txtinfo.currmode == MONO14 || txtinfo.currmode == BW40X14 || txtinfo.currmode == BW80X14
	        || txtinfo.currmode == MONO21 || txtinfo.currmode == BW40X21 || txtinfo.currmode == BW80X21
	        || txtinfo.currmode == MONO28 || txtinfo.currmode == BW40X28 || txtinfo.currmode == BW80X28
	        || txtinfo.currmode == MONO43 || txtinfo.currmode == BW40X43 || txtinfo.currmode == BW80X43
	        || txtinfo.currmode == MONO50 || txtinfo.currmode == BW40X50 || txtinfo.currmode == BW80X50
	        || txtinfo.currmode == MONO60 || txtinfo.currmode == BW40X60 || txtinfo.currmode == BW80X60
	        || txtinfo.currmode == ATARI_40X24 || txtinfo.currmode == ATARI_80X25))
	{
		api->bclr = BLACK;
		api->hclr = WHITE;
		api->lclr = LIGHTGRAY;
		api->cclr = LIGHTGRAY;
		api->lbclr = BLACK | (LIGHTGRAY << 4);  /* lightbar color */
	} else {
		api->bclr = BLUE;
		api->hclr = YELLOW;
		api->lclr = WHITE;
		api->cclr = CYAN;
		api->lbclr = BLUE | (LIGHTGRAY << 4); /* lightbar color */
	}

	blk_scrn_len = api->scrn_width * api->scrn_len;
	if ((blk_scrn = (struct vmem_cell *)malloc(blk_scrn_len * sizeof(*blk_scrn))) == NULL)  {
		cprintf("UIFC line %d: error allocating %u bytes."
		        , __LINE__, blk_scrn_len * sizeof(*blk_scrn));
		return -1;
	}
	if ((tmp_buffer = (struct vmem_cell *)malloc(blk_scrn_len * sizeof(*blk_scrn))) == NULL)  {
		cprintf("UIFC line %d: error allocating %u bytes."
		        , __LINE__, blk_scrn_len * sizeof(*blk_scrn));
		return -1;
	}
	if ((tmp_buffer2 = (struct vmem_cell *)malloc(blk_scrn_len * sizeof(*blk_scrn))) == NULL)  {
		cprintf("UIFC line %d: error allocating %u bytes."
		        , __LINE__, blk_scrn_len * sizeof(*blk_scrn));
		return -1;
	}
	fill_blk_scrn(TRUE);

	cursor = _NOCURSOR;
	_setcursortype(cursor);

	if (cio_api.mouse && !(api->mode & UIFC_NOMOUSE)) {
		api->mode |= UIFC_MOUSE;
		uifc_mouse_enable();
	}

	/* A esc_delay of less than 10 is stupid... silently override */
	if (api->esc_delay < 10)
		api->esc_delay = 25;

	if (cio_api.escdelay)
		*(cio_api.escdelay) = api->esc_delay;

	for (i = 0; i < MAX_BUFS; i++)
		sav[i].buf = NULL;
	api->savnum = 0;
	api->exit_flags = 0;

	api->initialized = TRUE;

	return 0;
}

static BOOL restore(void)
{
	if (api->savnum < 1)
		return FALSE;
	if (sav[api->savnum - 1].buf == NULL)
		return FALSE;
	--api->savnum;
	vmem_puttext(sav[api->savnum].left, sav[api->savnum].top
	             , sav[api->savnum].right, sav[api->savnum].bot
	             , sav[api->savnum].buf);
	FREE_AND_NULL(sav[api->savnum].buf);
	return TRUE;
}

void docopy(void)
{
	int                   key;
	struct mouse_event    mevent;
	struct ciolib_screen *screen;
	struct ciolib_screen *sbuffer;
	int                   sbufsize;
	int                   x, y, startx, starty, endx, endy, lines;
	int                   outpos;
	char *                copybuf;

	screen = ciolib_savescreen();
	sbuffer = ciolib_savescreen();
	freepixels(sbuffer->pixels);
	sbuffer->pixels = NULL;
	sbufsize = screen->text_info.screenwidth * screen->text_info.screenheight * sizeof(screen->vmem[0]);
	while (1) {
		key = getch();
		if (key == 0 || key == 0xe0) {
			key |= getch() << 8;
			if (key == CIO_KEY_LITERAL_E0)
				key = 0xe0;
		}
		switch (key) {
			case CIO_KEY_MOUSE:
				getmouse(&mevent);
				if (mevent.startx < mevent.endx) {
					startx = mevent.startx;
					endx = mevent.endx;
				}
				else {
					startx = mevent.endx;
					endx = mevent.startx;
				}
				if (mevent.starty < mevent.endy) {
					starty = mevent.starty;
					endy = mevent.endy;
				}
				else {
					starty = mevent.endy;
					endy = mevent.starty;
				}
				switch (mevent.event) {
					case CIOLIB_BUTTON_1_DRAG_MOVE:
						memcpy(sbuffer->vmem, screen->vmem, sbufsize);
						for (y = starty - 1; y < endy; y++) {
							for (x = startx - 1; x < endx; x++) {
								int pos = y * api->scrn_width + x;

								if ((sbuffer->vmem[pos].legacy_attr & 0x70) != 0x10) {
									sbuffer->vmem[pos].legacy_attr &= 0x8F;
									sbuffer->vmem[pos].legacy_attr |= 0x10;
								}
								else {
									sbuffer->vmem[pos].legacy_attr &= 0x8F;
									sbuffer->vmem[pos].legacy_attr |= 0x60;
								}
								if ((sbuffer->vmem[pos].legacy_attr & 0x70) == ((sbuffer->vmem[pos].legacy_attr & 0x0F) << 4))
									sbuffer->vmem[pos].legacy_attr |= 0x08;
								attr2palette(sbuffer->vmem[pos].legacy_attr,
								    &sbuffer->vmem[pos].fg,
								    &sbuffer->vmem[pos].bg);
							}
						}
						restorescreen(sbuffer);
						break;
					case CIOLIB_BUTTON_1_DRAG_END:
						lines = abs(mevent.endy - mevent.starty) + 1;
						copybuf = malloc(((endy - starty + 1) * (endx - startx + 1) + 1 + lines * 2) * 4);
						if (copybuf) {
							outpos = 0;
							for (y = starty - 1; y < endy; y++) {
								for (x = startx - 1; x < endx; x++) {
									size_t   outlen;
									uint8_t *utf8str;
									char     ch;

									ch = screen->vmem[(y * api->scrn_width + x)].ch ? screen->vmem[(y * api->scrn_width + x)].ch : ' ';
									utf8str = cp_to_utf8(conio_fontdata[screen->vmem[(y * api->scrn_width + x)].font].cp, &ch, 1, &outlen);
									if (utf8str == NULL)
										continue;
									memcpy(copybuf + outpos, utf8str, outlen);
									outpos += outlen;
								}
								#ifdef _WIN32
								copybuf[outpos++] = '\r';
								#endif
								copybuf[outpos++] = '\n';
							}
							copybuf[outpos] = 0;
							copytext(copybuf, strlen(copybuf));
							free(copybuf);
						}
						restorescreen(screen);
						freescreen(screen);
						freescreen(sbuffer);
						return;
				}
				break;
			default:
				restorescreen(screen);
				freescreen(screen);
				freescreen(sbuffer);
				ungetch(key);
				return;
		}
	}
}

static int uifc_getmouse(struct mouse_event *mevent)
{
	mevent->startx = 0;
	mevent->starty = 0;
	mevent->event = 0;
	if (api->mode & UIFC_MOUSE) {
		getmouse(mevent);
		if (mevent->event == CIOLIB_BUTTON_3_CLICK)
			return ESC;
		if (mevent->event == CIOLIB_BUTTON_1_DRAG_START) {
			docopy();
			return 0;
		}
		if (mevent->starty == api->buttony) {
			if (mevent->startx >= api->exitstart
			    && mevent->startx <= api->exitend
			    && mevent->event == CIOLIB_BUTTON_1_CLICK) {
				return ESC;
			}
			if (mevent->startx >= api->helpstart
			    && mevent->startx <= api->helpend
			    && mevent->event == CIOLIB_BUTTON_1_CLICK) {
				return CIO_KEY_F(1);
			}
		}
		if (mevent->event == CIOLIB_BUTTON_4_PRESS)
			return CIO_KEY_UP;
		if (mevent->event == CIOLIB_BUTTON_5_PRESS)
			return CIO_KEY_DOWN;
		return 0;
	}
	return -1;
}

void uifcbail(void)
{
	int i;

	_setcursortype(_NORMALCURSOR);
	textattr(LIGHTGRAY);
	uifc_mouse_disable();
	suspendciolib();
	FREE_AND_NULL(blk_scrn);
	FREE_AND_NULL(tmp_buffer);
	FREE_AND_NULL(tmp_buffer2);
	api->initialized = FALSE;
	for (i = 0; i < MAX_BUFS; i++)
		FREE_AND_NULL(sav[i].buf);
}

/****************************************************************************/
/* Clear screen, fill with background attribute, display application title.	*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uscrn(const char *str)
{
	textattr(api->bclr | (api->cclr << 4));
	gotoxy(1, 1);
	clreol();
	gotoxy(3, 1);
	cputs(str);
	fill_blk_scrn(FALSE);
	if (!vmem_puttext(1, 2, api->scrn_width, api->scrn_len, blk_scrn))
		return -1;
	gotoxy(1, api->scrn_len + 1);
	clreol();
	reset_dynamic();
	setname(str);
	return 0;
}

/****************************************************************************/
/****************************************************************************/
static void scroll_text(int x1, int y1, int x2, int y2, int down)
{
	vmem_gettext(x1, y1, x2, y2, tmp_buffer2);
	if (down)
		vmem_puttext(x1, y1 + 1, x2, y2, tmp_buffer2);
	else
		vmem_puttext(x1, y1, x2, y2 - 1, tmp_buffer2 + (((x2 - x1) + 1)));
}

/****************************************************************************/
/* Updates time in upper left corner of screen with current time in ASCII/  */
/* Unix format																*/
/****************************************************************************/
static void timedisplay(BOOL force)
{
	static time_t savetime;
	static int    savemin;
	time_t        now;
	struct tm     gm;
	int           old_hold;

	if (api->scrn_width < 80)
		return;
	now = time(NULL);
	localtime_r(&now, &gm);
	if (force || savemin != gm.tm_min || difftime(now, savetime) >= 60) {
		old_hold = hold_update;
		hold_update = FALSE;
		uprintf(api->scrn_width - 25, 1, api->bclr | (api->cclr << 4), utimestr(&now));
		hold_update = old_hold;
		savetime = now;
		savemin = gm.tm_min;
	}
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncspctrl(char *str)
{
	uint c;

	c = strlen(str);
	while (c && (uchar)str[c - 1] <= ' ') c--;
	if (str[c] != 0)   /* don't write to string constants */
		str[c] = 0;
}

static void
inactive_win(struct vmem_cell *buf, int left, int top, int right, int bottom, int y, int hbrdrsize, uchar cclr, uchar lclr, uchar hclr, int btop)
{
	int width = right - left + 1;
	int height = bottom - top + 1;
	int i, j;

	vmem_gettext(left, top, right, bottom, buf);
	for (i = 0; i < (width * height); i++)
		set_vmem_attr(&buf[i], lclr | (cclr << 4));
	j = (((y - btop) * width)) + 3 + ((width - hbrdrsize - 2));
	for (i = (((y - btop) * width)) + 3; i < j; i++)
		set_vmem_attr(&buf[i], hclr | (cclr << 4));

	vmem_puttext(left, top, right, bottom, buf);
}

static inline char* non_null_str(char* str) { return str == NULL ? "" : str; }

/****************************************************************************/
/* General menu function, see uifc.h for details.							*/
/****************************************************************************/
// cur is the currently selected option.
// bar is how many unselected options are displayed up to (ie: The number
//     of unselected options before cur currently displayed on screen)
int ulist(uifc_winmode_t mode, int left, int top, int width, int *cur, int *bar
          , const char *initial_title, char **option)
{
	struct vmem_cell * ptr, *win, shade[MAX_LINES * 2], line[MAX_COLS];
	static char        search[MAX_OPLN] = "";
	int                height, y;
	int                i, j, opts = 0, s = 0; /* s=search index into options */
	int                is_redraw = 0;
	int                s_top = SCRN_TOP;
	int                s_left = SCRN_LEFT;
	int                s_right = SCRN_RIGHT;
	int                s_bottom = api->scrn_len - 3;
	int                hbrdrsize = 2;
	int                lbrdrwidth = 1;
	int                rbrdrwidth = 1;
	int                vbrdrsize = 4;
	int                tbrdrwidth = 3;
	int                bbrdrwidth = 1;
	int                title_len;
	int                tmpcur = 0;
	struct mouse_event mevnt;
	char *             title = NULL;
	int                a, b, c, longopt;
	int                optheight = 0;
	int                gotkey;
	uchar              hclr, lclr, bclr, cclr, lbclr;
	BOOL               shadow = api->scrn_width >= 80;

	if (cur == NULL)
		cur = &tmpcur;
	hclr = api->hclr;
	lclr = api->lclr;
	bclr = api->bclr;
	cclr = api->cclr;
	lbclr = api->lbclr;
	if (mode & WIN_INACT) {
		bclr = api->cclr;
		hclr = api->lclr;
		lclr = api->lclr;
		cclr = api->cclr;
		lbclr = (api->cclr << 4) | api->hclr;
	}
	title = strdup(initial_title == NULL?"":initial_title);

	if (!(api->mode & UIFC_NHM))
		uifc_mouse_disable();

	title_len = (mode & WIN_NOBRDR) ? 0 :strlen(title);

	if (mode & WIN_FAT) {
		s_top = 1;
		s_left = 2;
		s_right = api->scrn_width - 3;  /* Leave space for the shadow */
		s_bottom = api->scrn_len - 1;   /* Leave one for the shadow */
	}
	if (mode & WIN_NOBRDR) {
		hbrdrsize = 0;
		vbrdrsize = 0;
		lbrdrwidth = 0;
		rbrdrwidth = 0;
		tbrdrwidth = 0;
		bbrdrwidth = 0;
		shadow = FALSE;
	}
	/* Count the options */
	while (option != NULL && opts < MAX_OPTS) {
		if (option[opts] == NULL || (!(mode & WIN_BLANKOPTS) && option[opts][0] == 0))
			break;
		else
			opts++;
	}
	if (mode & WIN_XTR && opts < MAX_OPTS)
		opts++;

	/* Sanity-check the savnum */
	if (mode & WIN_SAV && api->savnum >= MAX_BUFS - 1) {
		putch(7);
		mode &= ~WIN_SAV;
	}

	api->help_available = (api->helpbuf != NULL || api->helpixbfile[0] != 0);

	/* Create the status bar/bottom-line */
	uifc_winmode_t bline = mode;
	if (api->bottomline != NULL) {
		if ((mode & (WIN_XTR | WIN_PASTEXTR)) == WIN_XTR && (*cur) == opts - 1)
			api->bottomline(bline & ~WIN_PASTE);
		else
			api->bottomline(bline);
	}
	optheight = opts + vbrdrsize;
	height = optheight;
	if (mode & WIN_FIXEDHEIGHT) {
		height = api->list_height;
	}
	if (mode & WIN_T2B)
		top = 0;    // This is overridden later, so don't use top in height calculation in T2B mode
	if (top + height > s_bottom)
		height = (s_bottom) - top;
	if (optheight > height)
		optheight = height;
	if (!width || width < title_len + hbrdrsize + 2) {
		width = title_len + hbrdrsize + 2;
		for (i = 0; i < opts; i++) {
			if (option[i] != NULL) {
				truncspctrl(option[i]);
				if ((j = strlen(option[i]) + hbrdrsize + 2 + 1) > width)
					width = j;
			}
		}
	}
	/* Determine minimum widths here to accommodate mouse "icons" in border */
	if (!(mode & WIN_NOBRDR) && api->mode & UIFC_MOUSE) {
		if (api->help_available && width < 8)
			width = 8;
		else if (width < 5)
			width = 5;
	}
	if (left + width > (s_right + 1) - s_left) {
		left = 0;
		if (width > (s_right + 1) - s_left) {
			width = (s_right + 1) - s_left;
			if (title_len > (width - hbrdrsize - 2)) {
				*(title + width - hbrdrsize - 2 - 3) = '.';
				*(title + width - hbrdrsize - 2 - 2) = '.';
				*(title + width - hbrdrsize - 2 - 1) = '.';
				*(title + width - hbrdrsize - 2) = 0;
				title_len = strlen(title);
			}
		}
	}
	if (mode & WIN_L2R)
		left = (s_right - s_left - width + 1) / 2;
	else if (mode & WIN_RHT)
		left = s_right - (width + hbrdrsize + 2 + left);
	if (mode & WIN_T2B)
		top = (api->scrn_len - height + 1) / 2 - 2;
	else if (mode & WIN_BOT)
		top = s_bottom - height - top;
	if (left < 0)
		left = 0;
	if (top < 0)
		top = 0;

	/* Dynamic Menus */
	if (mode & WIN_DYN
	    && cur != NULL
	    && bar != NULL
	    && last_menu_cur == cur
	    && last_menu_bar == bar
	    && save_menu_cur == *cur
	    && save_menu_bar == *bar
	    && save_menu_opts == opts) {
		is_redraw = 1;
	}

	if (mode & WIN_DYN && mode & WIN_REDRAW)
		is_redraw = 1;
	if (mode & WIN_DYN && mode & WIN_NODRAW)
		is_redraw = 0;

	if (mode & WIN_ORG && !(mode & WIN_SAV)) {       /* Clear all save buffers on WIN_ORG */
		for (i = 0; i < MAX_BUFS; i++)
			FREE_AND_NULL(sav[i].buf);
		api->savnum = 0;
	}

	if (mode & WIN_SAV) {
		/* Check if this screen (by cur/bar) is already saved */
		for (i = 0; i < MAX_BUFS; i++) {
			if (sav[i].buf != NULL) {
				if (cur == sav[i].cur && bar == sav[i].bar) {
					/* Yes, it is... */
					for (j = api->savnum - 1; j > i; j--) {
						/* Restore old screens */
						vmem_puttext(sav[j].left, sav[j].top, sav[j].right, sav[j].bot
						             , sav[j].buf); /* put original window back */
						FREE_AND_NULL(sav[j].buf);
					}
					api->savnum = i;
				}
			}
		}
		/* savnum not the next one - must be a dynamic window or we popped back up the stack */
		if (sav[api->savnum].buf != NULL) {
			/* Is this even the right window? */
			if (sav[api->savnum].cur == cur
			    && sav[api->savnum].bar == bar) {
				if ((sav[api->savnum].left != s_left + left
				     || sav[api->savnum].top != s_top + top
				     || sav[api->savnum].right != s_left + left + width + (shadow * 2) - 1
				     || sav[api->savnum].bot != s_top + top + height - (!shadow))) { /* dimensions have changed */
					vmem_puttext(sav[api->savnum].left, sav[api->savnum].top, sav[api->savnum].right, sav[api->savnum].bot
					             , sav[api->savnum].buf); /* put original window back */
					FREE_AND_NULL(sav[api->savnum].buf);
					if ((sav[api->savnum].buf = malloc((width + 3) * (height + 2) * sizeof(struct vmem_cell))) == NULL) {
						cprintf("UIFC line %d: error allocating %u bytes."
						        , __LINE__, (width + 3) * (height + 2) * sizeof(struct vmem_cell));
						free(title);
						if (!(api->mode & UIFC_NHM))
							uifc_mouse_enable();
						return -1;
					}
					win_t* s = &sav[api->savnum];
					s->left = s_left + left;
					s->top = s_top + top;
					s->right = s_left + left + width + (shadow * 2) - 1;
					s->bot = s_top + top + height - (!shadow);
					s->cur = cur;
					s->bar = bar;
					vmem_gettext(s->left, s->top, s->right, s->bot, s->buf); /* save again */
				}
			}
			else {
				/* Find something available... */
				while (sav[api->savnum].buf != NULL)
					api->savnum++;
			}
		}
		else {
			if ((sav[api->savnum].buf = malloc((width + 3) * (height + 2) * sizeof(struct vmem_cell))) == NULL) {
				cprintf("UIFC line %d: error allocating %u bytes."
				        , __LINE__, (width + 3) * (height + 2) * sizeof(struct vmem_cell));
				free(title);
				if (!(api->mode & UIFC_NHM))
					uifc_mouse_enable();
				return -1;
			}
			win_t* s = &sav[api->savnum];
			s->left = s_left + left;
			s->top = s_top + top;
			s->right = s_left + left + width + (shadow * 2) - 1;
			s->bot = s_top + top + height - (!shadow);
			s->cur = cur;
			s->bar = bar;
			vmem_gettext(s->left, s->top, s->right, s->bot, s->buf);
		}
	}

	if (!is_redraw) {
		if (mode & WIN_ORG) { /* Clear around menu */
			if (top) {
				fill_blk_scrn(FALSE);
				vmem_puttext(1, 2, api->scrn_width, s_top + top - 1, blk_scrn);
			}
			if ((unsigned)(s_top + height + top) <= api->scrn_len) {
				fill_blk_scrn(FALSE);
				vmem_puttext(1, s_top + height + top, api->scrn_width, api->scrn_len, blk_scrn);
			}
			if (left) {
				fill_blk_scrn(FALSE);
				vmem_puttext(1, s_top + top, s_left + left - 1, s_top + height + top
				             , blk_scrn);
			}
			if (s_left + left + width <= s_right) {
				fill_blk_scrn(FALSE);
				vmem_puttext(s_left + left + width, s_top + top, /* s_right+2 */ api->scrn_width
				             , s_top + height + top, blk_scrn);
			}
		}
		ptr = tmp_buffer;
		if (!(mode & WIN_NOBRDR)) {
			set_vmem(ptr++, api->chars->list_top_left, hclr | (bclr << 4), 0);

			if (api->mode & UIFC_MOUSE) {
				set_vmem(ptr++, api->chars->button_left, hclr | (bclr << 4), 0);
				set_vmem(ptr++, api->chars->close_char, lclr | (bclr << 4), 0);
				set_vmem(ptr++, api->chars->button_right, hclr | (bclr << 4), 0);
				i = 3;
				if (api->help_available) {
					set_vmem(ptr++, api->chars->button_left, hclr | (bclr << 4), 0);
					set_vmem(ptr++, api->chars->help_char, lclr | (bclr << 4), 0);
					set_vmem(ptr++, api->chars->button_right, hclr | (bclr << 4), 0);
					i += 3;
				}
				api->buttony = s_top + top;
				api->exitstart = s_left + left + 1;
				api->exitend = s_left + left + 3;
				api->helpstart = s_left + left + 4;
				api->helpend = s_left + left + 6;
			}
			else
				i = 0;

			if (mode & (WIN_LEFTKEY | WIN_RIGHTKEY)) {
				for (; i < width - 7; i++)
					set_vmem(ptr++, api->chars->list_top, hclr | (bclr << 4), 0);
				set_vmem(ptr++, api->chars->button_left, hclr | (bclr << 4), 0);
				if (mode & WIN_LEFTKEY)
					set_vmem(ptr++, api->chars->left_arrow, lclr | (bclr << 4), 0);
				else
					set_vmem(ptr++, ' ', lclr | (bclr << 4), 0);
				set_vmem(ptr++, ' ', lclr | (bclr << 4), 0);
				if (mode & WIN_RIGHTKEY)
					set_vmem(ptr++, api->chars->right_arrow, lclr | (bclr << 4), 0);
				else
					set_vmem(ptr++, ' ', lclr | (bclr << 4), 0);
				set_vmem(ptr++, api->chars->button_right, hclr | (bclr << 4), 0);
			} else {
				for (; i < width - 2; i++)
					set_vmem(ptr++, api->chars->list_top, hclr | (bclr << 4), 0);
			}
			set_vmem(ptr++, api->chars->list_top_right, hclr | (bclr << 4), 0);
			set_vmem(ptr++, api->chars->list_left, hclr | (bclr << 4), 0);
			a = title_len;
			b = (width - a - 1) / 2;
			for (i = 0; i < b; i++) {
				set_vmem(ptr++, ' ', hclr | (bclr << 4), 0);
			}
			for (i = 0; i < a; i++)
				set_vmem(ptr++, title[i], hclr | (bclr << 4), 0);
			for (i = 0; i < width - (a + b) - 2; i++)
				set_vmem(ptr++, ' ', hclr | (bclr << 4), 0);
			set_vmem(ptr++, api->chars->list_right, hclr | (bclr << 4), 0);
			set_vmem(ptr++, api->chars->list_separator_left, hclr | (bclr << 4), 0);
			for (i = 0; i < width - 2; i++)
				set_vmem(ptr++, api->chars->list_horizontal_separator, hclr | (bclr << 4), 0);
			set_vmem(ptr++, api->chars->list_separator_right, hclr | (bclr << 4), 0);
		}

		// Clamp cur
		if ((*cur) >= opts)
			*cur = opts - 1;            /* returned after scrolled */
		if ((*cur) < 0)
			*cur = 0;

		if (!bar) {
			// If we don't have bar, do not allow scrolling
			if ((*cur) > height - vbrdrsize - 1)
				*cur = height - vbrdrsize - 1;
			if ((*cur) > opts - 1)
				*cur = opts - 1;
			i = 0;
		}
		else {
			// *bar must be strictly <= *cur
			if (*bar > *cur)
				*bar = *cur;

			// *bar needs to fit in window with room for highlight
			if ((*bar) >= height - vbrdrsize)
				*bar = height - vbrdrsize - 1;

			// And finally, *bar can't be negative
			if ((*bar) < 0)
				*bar = 0;

			// Set i to the first option to display on the screen...
			i = (*cur) - (*bar);

			// If there are enough options to fill the screen
			if ((opts > (height - vbrdrsize))) {
				// If the screen is not filled...
				if ((opts - i) < (height - vbrdrsize)) {
					*bar += (height - vbrdrsize) - (opts - i);
					// This shouldn't be possible, but may as well be paranoid
					if (*bar > *cur)
						*bar = *cur;
					// Recalculate i since we adjusted bar
					i = (*cur) - (*bar);
				}
			}
			else {
				// Show whole menu
				*bar = *cur;
				i = 0;
			}
		}

		j = 0;
		longopt = 0;
		while (j < height - vbrdrsize) {
			if (!(mode & WIN_NOBRDR))
				set_vmem(ptr++, api->chars->list_left, hclr | (bclr << 4), 0);
			set_vmem(ptr++, ' ', hclr | (bclr << 4), 0);
			set_vmem(ptr++, api->chars->list_scrollbar_separator, lclr | (bclr << 4), 0);
			if (i == (*cur))
				a = lbclr;
			else
				a = lclr | (bclr << 4);
			if (i < opts && option[i] != NULL) {
				b = strlen(option[i]);
				if (b > longopt)
					longopt = b;
				if (b + hbrdrsize + 2 > width)
					b = width - hbrdrsize - 2;
				for (c = 0; c < b; c++)
					set_vmem(ptr++, option[i][c], a, 0);
			}
			else
				c = 0;
			while (c < width - hbrdrsize - 2) {
				set_vmem(ptr++, ' ', a, 0);
				c++;
			}
			if (!(mode & WIN_NOBRDR))
				set_vmem(ptr++, api->chars->list_right, hclr | (bclr << 4), 0);
			i++;
			j++;
		}
		if (!(mode & WIN_NOBRDR)) {
			set_vmem(ptr++, api->chars->list_bottom_left, hclr | (bclr << 4), 0);
			for (i = 0; i < width - 2; i++)
				set_vmem(ptr++, api->chars->list_bottom, hclr | (bclr << 4), 0);
			set_vmem(ptr, api->chars->list_bottom_right, hclr | (bclr << 4), 0);    /* Not incremented to shut up BCC */
		}
		vmem_puttext(s_left + left, s_top + top, s_left + left + width - 1
		             , s_top + top + height - 1, tmp_buffer);
		if (bar)
			y = top + tbrdrwidth + (*bar);
		else
			y = top + tbrdrwidth + (*cur);
		if (opts + vbrdrsize > height && ((!bar && (*cur) != opts - 1)
		                                  || (bar && ((*cur) - (*bar)) + (height - vbrdrsize) < opts))) {
			gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
			textattr(lclr | (bclr << 4));
			putch(api->chars->down_arrow);     /* put down arrow */
			textattr(hclr | (bclr << 4));
		}

		if (bar && opts > 0 && (*bar) != (*cur)) {
			gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
			textattr(lclr | (bclr << 4));
			putch(api->chars->up_arrow);       /* put the up arrow */
			textattr(hclr | (bclr << 4));
		}

		if (shadow) {
			if (api->bclr == BLUE) {
				vmem_gettext(s_left + left + width, s_top + top + 1, s_left + left + width + 1
				             , s_top + top + height - 1, shade);
				for (i = 0; i < height * 2; i++)
					set_vmem_attr(&shade[i], DARKGRAY);
				vmem_puttext(s_left + left + width, s_top + top + 1, s_left + left + width + 1
				             , s_top + top + height - 1, shade);
				vmem_gettext(s_left + left + 2, s_top + top + height, s_left + left + width + 1
				             , s_top + top + height, shade);
				for (i = 0; i < width; i++)
					set_vmem_attr(&shade[i], DARKGRAY);
				vmem_puttext(s_left + left + 2, s_top + top + height, s_left + left + width + 1
				             , s_top + top + height, shade);
			}
		}
	}
	else {  /* Is a redraw */
		if (bar)
			y = top + tbrdrwidth + (*bar);
		else
			y = top + tbrdrwidth + (*cur);
		i = (*cur) + (top + tbrdrwidth - y);
		j = tbrdrwidth - 1;

		longopt = 0;
		while (j < height - bbrdrwidth - 1) {
			ptr = tmp_buffer;
			if (i == (*cur))
				a = lbclr;
			else
				a = lclr | (bclr << 4);
			if (i < opts && option[i] != NULL) {
				b = strlen(option[i]);
				if (b > longopt)
					longopt = b;
				if (b + hbrdrsize + 2 > width)
					b = width - hbrdrsize - 2;
				for (c = 0; c < b; c++)
					set_vmem(ptr++, option[i][c], a, 0);
			}
			else
				c = 0;
			while (c < width - hbrdrsize - 2) {
				set_vmem(ptr++, ' ', a, 0);
				c++;
			}
			i++;
			j++;
			vmem_puttext(s_left + left + lbrdrwidth + 2, s_top + top + j, s_left + left + width - rbrdrwidth - 1
			             , s_top + top + j, tmp_buffer);
		}
	}
	free(title);

	last_menu_cur = cur;
	last_menu_bar = bar;
	if (!(api->mode & UIFC_NHM))
		uifc_mouse_enable();

	if (mode & WIN_IMM) {
		return -2;
	}

	if (mode & WIN_ORG) {
		if (api->timedisplay != NULL)
			api->timedisplay(/* force? */ TRUE);
	}

	while (1) {
	#if 0                   /* debug */
		struct text_info txtinfo;
		gettextinfo(&txtinfo);
		gotoxy(30, 1);
		cprintf("y=%2d h=%2d c=%2d b=%2d s=%2d o=%2d w=%d/%d h=%d/%d"
		        , y, height, *cur, bar ? *bar :0xff, api->savnum, opts, txtinfo.screenwidth, api->scrn_width, txtinfo.screenheight, api->scrn_len);
	#endif
		if (api->timedisplay != NULL)
			api->timedisplay(/* force? */ FALSE);
		gotkey = 0;
		textattr(((api->lbclr) & 0x0f) | ((api->lbclr >> 4) & 0x0f));
		gotoxy(s_left + lbrdrwidth + 2 + left, s_top + y);
		if (((api->exit_flags & UIFC_XF_QUIT) && !(mode & WIN_ATEXIT)) || dyn_kbwait(mode) || (mode & (WIN_POP | WIN_SEL))) {
			if ((api->exit_flags & UIFC_XF_QUIT) && !(mode & WIN_ATEXIT))
				gotkey = CIO_KEY_QUIT;
			else if (mode & WIN_POP)
				gotkey = ESC;
			else if (mode & WIN_SEL)
				gotkey = CR;
			else
				gotkey = inkey();
			if (gotkey == CIO_KEY_MOUSE) {
				if ((gotkey = uifc_getmouse(&mevnt)) == 0) {
					/* Clicked in menu */
					if (mevnt.startx >= s_left + left + lbrdrwidth + 2
					    && mevnt.startx <= s_left + left + width - rbrdrwidth - 1
					    && mevnt.starty >= s_top + top + tbrdrwidth
					    && mevnt.starty <= (s_top + top + optheight) - bbrdrwidth - 1
					    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {

						(*cur) = ((mevnt.starty) - (s_top + top + tbrdrwidth)) + (*cur + (top + tbrdrwidth - y));
						if (bar)
							(*bar) = (*cur);
						y = top + tbrdrwidth + ((mevnt.starty) - (s_top + top + tbrdrwidth));

						if (!opts)
							continue;

						if (mode & WIN_SAV)
							api->savnum++;
						if (mode & WIN_ACT) {
							if (!(api->mode & UIFC_NHM))
								uifc_mouse_disable();
							if ((win = malloc((width + 3) * (height + 2) * sizeof(*win))) == NULL) {
								cprintf("UIFC line %d: error allocating %u bytes."
								        , __LINE__, (width + 3) * (height + 2) * sizeof(*win));
								if (!(api->mode & UIFC_NHM))
									uifc_mouse_enable();
								return -1;
							}
							inactive_win(win, s_left + left, s_top + top, s_left + left + width - 1, s_top + top + height - 1, y, hbrdrsize, cclr, lclr, hclr, top);
							free(win);
							if (!(api->mode & UIFC_NHM))
								uifc_mouse_enable();
						}
						else if (mode & WIN_SAV) {
							if (!(api->mode & UIFC_NHM))
								uifc_mouse_disable();
							restore();
							if (!(api->mode & UIFC_NHM))
								uifc_mouse_enable();
						}
						if (mode & WIN_XTR && (*cur) == opts - 1)
							return MSK_INS | *cur;
						return *cur;
					}
					/* Clicked Scroll Up */
					else if (mevnt.startx == s_left + left + lbrdrwidth
					         && mevnt.starty == s_top + top + tbrdrwidth
					         && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
						gotkey = CIO_KEY_PPAGE;
					}
					/* Clicked Scroll Down */
					else if (mevnt.startx == s_left + left + lbrdrwidth
					         && mevnt.starty == (s_top + top + height) - bbrdrwidth - 1
					         && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
						gotkey = CIO_KEY_NPAGE;
					}
					/* Clicked Left Arrow */
					else if ((mode & WIN_LEFTKEY)
					         && mevnt.startx == s_left + left + (width - 5)
					         && mevnt.starty == (s_top + top) - (bbrdrwidth - 1)
					         && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
						gotkey = CIO_KEY_LEFT;
					}
					/* Clicked Right Arrow */
					else if ((mode & WIN_RIGHTKEY)
					         && mevnt.startx == s_left + left + (width - 3)
					         && mevnt.starty == (s_top + top) - (bbrdrwidth - 1)
					         && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
						gotkey = CIO_KEY_RIGHT;
					}
					/* Clicked Outside of Window */
					else if ((mevnt.startx < s_left + left
					          || mevnt.startx > s_left + left + width - 1
					          || mevnt.starty < s_top + top
					          || mevnt.starty > s_top + top + height - 1)
					         && (mevnt.event == CIOLIB_BUTTON_1_CLICK
					             || mevnt.event == CIOLIB_BUTTON_3_CLICK)) {
						if (mode & WIN_UNGETMOUSE) {
							ungetmouse(&mevnt);
							gotkey = CIO_KEY_MOUSE;
						}
						else {
							gotkey = ESC;
						}
					}
				}
			}
			/* For compatibility with terminals lacking special keys */
			switch (gotkey) {
				case '\b':
					gotkey = ESC;
					break;
				case '+':
					gotkey = CIO_KEY_IC;  /* insert */
					break;
				case '-':
				case DEL:
					gotkey = CIO_KEY_DC;  /* delete */
					break;
				case CTRL_A:
					if (mode & WIN_TAG)
						return MSK_TAGALL;
					break;
				case CTRL_B:
					if (!(api->mode & UIFC_NOCTRL))
						gotkey = CIO_KEY_HOME;
					break;
				case CTRL_C:
					if (!(api->mode & UIFC_NOCTRL))
						gotkey = CIO_KEY_F(5);  /* copy */
					break;
				case CTRL_D:
					if (!(api->mode & UIFC_NOCTRL))
						gotkey = CIO_KEY_NPAGE;
					break;
				case CTRL_E:
					if (!(api->mode & UIFC_NOCTRL))
						gotkey = CIO_KEY_END;
					break;
				case CTRL_U:
					if (!(api->mode & UIFC_NOCTRL))
						gotkey = CIO_KEY_PPAGE;
					break;
				case CTRL_V:
					if (!(api->mode & UIFC_NOCTRL))
						gotkey = CIO_KEY_F(6);  /* paste */
					break;
				case CTRL_X:
					if (!(api->mode & UIFC_NOCTRL))
						gotkey = CIO_KEY_SHIFT_DC;  /* cut */
					break;
				case '?':
				case CTRL_Z:
					if (!(api->mode & UIFC_NOCTRL))
						gotkey = CIO_KEY_F(1);  /* help */
					break;
				case CIO_KEY_ABORTED:
					gotkey = ESC;
					break;
				case CIO_KEY_QUIT:
					api->exit_flags |= UIFC_XF_QUIT;
					gotkey = ESC;
					break;
			}
			if (gotkey > 255) {
				s = 0;
				switch (gotkey) {
					/* ToDo extended keys */
					case CIO_KEY_HOME:  /* home */
						if (!opts)
							break;
						if (opts + vbrdrsize > optheight) {
							gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
							textattr(lclr | (bclr << 4));
							putch(' ');    /* Delete the up arrow */
							gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
							putch(api->chars->down_arrow);     /* put the down arrow */
							uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth
							        , lbclr
							        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2, option[0]);
							for (i = 1; i < optheight - vbrdrsize; i++)    /* re-display options */
								uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + i
								        , lclr | (bclr << 4)
								        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
								        , non_null_str(option[i]));
							(*cur) = 0;
							if (bar)
								(*bar) = 0;
							y = top + tbrdrwidth;
							break;
						}
						vmem_gettext(s_left + left + lbrdrwidth + 2, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						for (i = 0; i < width; i++)
							set_vmem_attr(&line[i], lclr | (bclr << 4));
						vmem_puttext(s_left + left + lbrdrwidth + 2, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						(*cur) = 0;
						if (bar)
							(*bar) = 0;
						y = top + tbrdrwidth;
						vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						for (i = 0; i < width; i++)
							set_vmem_attr(&line[i], lbclr);
						vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						break;
					case CIO_KEY_UP:    /* up arrow */
						if (!opts)
							break;
						if (!(*cur) && opts + vbrdrsize > optheight) {
							gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth); /* like end */
							textattr(lclr | (bclr << 4));
							putch(api->chars->up_arrow);       /* put the up arrow */
							gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
							putch(' ');    /* delete the down arrow */
							for (i = (opts + vbrdrsize) - optheight, j = 0; i < opts; i++, j++)
								uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + j
								        , i == opts - 1 ? lbclr
								        : lclr | (bclr << 4)
								        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
								        , non_null_str(option[i]));
							(*cur) = opts - 1;
							if (bar)
								(*bar) = optheight - vbrdrsize - 1;
							y = top + optheight - bbrdrwidth - 1;
							break;
						}
						vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						for (i = 0; i < width; i++)
							set_vmem_attr(&line[i], lclr | (bclr << 4));
						vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						if (!(*cur)) {
							y = top + optheight - bbrdrwidth - 1;
							(*cur) = opts - 1;
							if (bar)
								(*bar) = optheight - vbrdrsize - 1;
						}
						else {
							(*cur)--;
							y--;
							if (bar && *bar)
								(*bar)--;
						}
						if (y < top + tbrdrwidth) {  /* scroll */
							if (!(*cur)) {
								gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
								textattr(lclr | (bclr << 4));
								putch(' '); /* delete the up arrow */
							}
							if ((*cur) + optheight - vbrdrsize == opts - 1) {
								gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
								textattr(lclr | (bclr << 4));
								putch(api->chars->down_arrow);  /* put the dn arrow */
							}
							y++;
							scroll_text(s_left + left + lbrdrwidth + 1, s_top + top + tbrdrwidth
							            , s_left + left + width - rbrdrwidth - 1, s_top + top + height - bbrdrwidth - 1, 1);
							uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth
							        , lbclr
							        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
							        , non_null_str(option[*cur]));
						}
						else {
							vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
							             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
							for (i = 0; i < width; i++)
								set_vmem_attr(&line[i], lbclr);
							vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
							             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						}
						break;
					case CIO_KEY_PPAGE: /* PgUp */
						if (!opts)
							break;
						*cur -= (optheight - vbrdrsize - 1);
						if (*cur < 0)
							*cur = 0;
						if (bar)
							*bar = 0;
						y = top + tbrdrwidth;
						gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
						textattr(lclr | (bclr << 4));
						if (*cur)  /* Scroll mode */
							putch(api->chars->up_arrow);       /* put the up arrow */
						else
							putch(' ');    /* delete the up arrow */
						gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
						if (opts >= height - tbrdrwidth && *cur + height - vbrdrsize < opts)
							putch(api->chars->down_arrow);     /* put the down arrow */
						else
							putch(' ');    /* delete the down arrow */
						for (i = *cur, j = 0; i <= *cur - vbrdrsize - 1 + optheight; i++, j++)
							uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + j
							        , i == *cur ? lbclr
							        : lclr | (bclr << 4)
							        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
							        , non_null_str(option[i]));
						break;
					case CIO_KEY_NPAGE: /* PgDn */
						if (!opts)
							break;
						*cur += (height - vbrdrsize - 1);
						if (*cur > opts - 1)
							*cur = opts - 1;
						if (bar)
							*bar = optheight - vbrdrsize - 1;
						y = top + optheight - bbrdrwidth - 1;
						gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
						textattr(lclr | (bclr << 4));
						if (*cur > height - vbrdrsize - 1)  /* Scroll mode */
							putch(api->chars->up_arrow);       /* put the up arrow */
						else
							putch(' ');    /* delete the up arrow */
						gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
						if (*cur < opts - 1)
							putch(api->chars->down_arrow);     /* put the down arrow */
						else
							putch(' ');    /* delete the down arrow */
						for (i = *cur + vbrdrsize + 1 - optheight, j = 0; i <= *cur; i++, j++)
							uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + j
							        , i == *cur ? lbclr
							        : lclr | (bclr << 4)
							        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
							        , non_null_str(option[i]));
						break;
					case CIO_KEY_END:   /* end */
						if (!opts)
							break;
						if (opts + vbrdrsize > height) { /* Scroll mode */
							gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
							textattr(lclr | (bclr << 4));
							putch(api->chars->up_arrow);       /* put the up arrow */
							gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
							putch(' ');    /* delete the down arrow */
							for (i = (opts + vbrdrsize) - height, j = 0; i < opts; i++, j++)
								uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + j
								        , i == opts - 1 ? lbclr
								        : lclr | (bclr << 4)
								        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
								        , non_null_str(option[i]));
							(*cur) = opts - 1;
							y = top + optheight - bbrdrwidth - 1;
							if (bar)
								(*bar) = optheight - vbrdrsize - 1;
							break;
						}
						vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						for (i = 0; i < width; i++)
							set_vmem_attr(&line[i], lclr | (bclr << 4));
						vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						(*cur) = opts - 1;
						y = top + optheight - bbrdrwidth - 1;
						if (bar)
							(*bar) = optheight - vbrdrsize - 1;
						vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						for (i = 0; i < 74; i++)
							set_vmem_attr(&line[i], lbclr);
						vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						break;
					case CIO_KEY_DOWN:  /* dn arrow */
						if (!opts)
							break;
						if ((*cur) == opts - 1 && opts + vbrdrsize > height) { /* like home */
							gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
							textattr(lclr | (bclr << 4));
							putch(' ');    /* Delete the up arrow */
							gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
							putch(api->chars->down_arrow);     /* put the down arrow */
							uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth
							        , lbclr
							        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2, option[0]);
							for (i = 1; i < height - vbrdrsize; i++)    /* re-display options */
								uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + i
								        , lclr | (bclr << 4)
								        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
								        , non_null_str(option[i]));
							(*cur) = 0;
							y = top + tbrdrwidth;
							if (bar)
								(*bar) = 0;
							break;
						}
						vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						for (i = 0; i < width; i++)
							set_vmem_attr(&line[i], lclr | (bclr << 4));
						vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
						             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
						if ((*cur) == opts - 1) {
							(*cur) = 0;
							y = top + tbrdrwidth;
							if (bar) {
								/* gotoxy(1,1); cprintf("bar=%08lX ",bar); */
								(*bar) = 0;
							}
						}
						else {
							(*cur)++;
							y++;
							if (bar && (*bar) < height - vbrdrsize - 1) {
								/* gotoxy(1,1); cprintf("bar=%08lX ",bar); */
								(*bar)++;
							}
						}
						if (y == top + height - bbrdrwidth) {  /* scroll */
							if (*cur == opts - 1) {
								gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
								textattr(lclr | (bclr << 4));
								putch(' '); /* delete the down arrow */
							}
							if ((*cur) + vbrdrsize == height) {
								gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
								textattr(lclr | (bclr << 4));
								putch(api->chars->up_arrow);    /* put the up arrow */
							}
							y--;
							/* gotoxy(1,1); cprintf("\rdebug: %4d ",__LINE__); */
							scroll_text(s_left + left + lbrdrwidth + 1, s_top + top + tbrdrwidth
							            , s_left + left + width - rbrdrwidth - 1, s_top + top + height - bbrdrwidth - 1, 0);
							/* gotoxy(1,1); cprintf("\rdebug: %4d ",__LINE__); */
							uprintf(s_left + left + lbrdrwidth + 2, s_top + top + height - bbrdrwidth - 1
							        , lbclr
							        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
							        , non_null_str(option[*cur]));
						}
						else {
							vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
							             , s_left + left + width - rbrdrwidth - 1, s_top + y
							             , line);
							for (i = 0; i < width; i++)
								set_vmem_attr(&line[i], lbclr);
							vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
							             , s_left + left + width - rbrdrwidth - 1, s_top + y
							             , line);
						}
						break;
					case CIO_KEY_F(1):  /* F1 - Help */
					{
						struct vmem_cell *save = malloc(width * height * sizeof(*save));
						if (save != NULL) {
							vmem_gettext(s_left + left, s_top + top, s_left
							             + left + width - 1, s_top + top + height - 1, save);
							struct vmem_cell *copy = malloc(width * height * sizeof(*copy));
							if (copy != NULL) {
								inactive_win(copy, s_left + left, s_top + top, s_left + left + width - 1, s_top + top + height - 1, y, hbrdrsize, cclr, lclr, hclr, top);
								free(copy);
							}
						}
						api->showhelp();
						if (save != NULL) {
							vmem_puttext(s_left + left, s_top + top, s_left
							             + left + width - 1, s_top + top + height - 1, save);
							free(save);
						}
						break;
					}
					case CIO_KEY_F(2):  /* F2 - Edit */
						if (mode & WIN_XTR && (*cur) == opts - 1)  /* can't edit */
							break;                          /* extra line */
						if (mode & WIN_EDIT) {
							if (mode & WIN_SAV)
								api->savnum++;
							if (mode & WIN_ACT)
								inactive_win(tmp_buffer, s_left + left, s_top + top, s_left + left + width - 1, s_top + top + height - 1, y, hbrdrsize, cclr, lclr, hclr, top);
							else if (mode & WIN_SAV) {
								restore();
							}
							return (*cur) | MSK_EDIT;
						}
						break;
					case CIO_KEY_F(5):      /* F5 - Copy */
					case CIO_KEY_CTRL_IC:   /* Ctrl-Insert */
						if (mode & WIN_COPY && !(mode & WIN_XTR && (*cur) == opts - 1))
							return (*cur) | MSK_COPY;
						break;
					case CIO_KEY_SHIFT_DC:  /* Shift-Del: Cut */
						if (mode & WIN_CUT && !(mode & WIN_XTR && (*cur) == opts - 1))
							return (*cur) | MSK_CUT;
						break;
					case CIO_KEY_SHIFT_IC:  /* Shift-Insert: Paste */
					case CIO_KEY_F(6):      /* F6 - Paste */
						if (mode & WIN_PASTE && (mode & WIN_PASTEXTR || !(mode & WIN_XTR && (*cur) == opts - 1)))
							return (*cur) | MSK_PASTE;
						break;
					case CIO_KEY_IC:    /* insert */
						if (mode & WIN_INS) {
							if (mode & WIN_SAV)
								api->savnum++;
							if (mode & WIN_INSACT)
								inactive_win(tmp_buffer, s_left + left, s_top + top, s_left + left + width - 1, s_top + top + height - 1, y, hbrdrsize, cclr, lclr, hclr, top);
							else if (mode & WIN_SAV) {
								restore();
							}
							if (!opts) {
								return MSK_INS;
							}
							return (*cur) | MSK_INS;
						}
						break;
					case CIO_KEY_DC:    /* delete */
						if (mode & WIN_XTR && (*cur) == opts - 1)  /* can't delete */
							break;                          /* extra line */
						if (mode & WIN_DEL) {
							if (mode & WIN_SAV)
								api->savnum++;
							if (mode & WIN_DELACT)
								inactive_win(tmp_buffer, s_left + left, s_top + top, s_left + left + width - 1, s_top + top + height - 1, y, hbrdrsize, cclr, lclr, hclr, top);
							else if (mode & WIN_SAV) {
								restore();
							}
							return (*cur) | MSK_DEL;
						}
						break;
					default:
						if (mode & WIN_EXTKEYS)
							return -2 - gotkey;
						break;
				}
			}
			else {
				gotkey &= 0xff;
				if (isalnum(gotkey) && opts > 1 && option[0][0]) {
					search[s] = gotkey;
					search[s + 1] = 0;
					for (j = (*cur) + 1, a = b = 0; a < 2; j++) {   /* a = search count */
						if (j == opts) {                   /* j = option count */
							j = -1;                       /* b = letter count */
							continue;
						}
						if (j == (*cur)) {
							b++;
							continue;
						}
						if (b >= longopt) {
							b = 0;
							a++;
						}
						if (a == 1 && !s)
							break;
						if (option[j] != NULL
						    && strlen(option[j]) > (size_t)b
						    && ((!a && s && !strnicmp(option[j] + b, search, s + 1))
						        || ((a || !s) && toupper(option[j][b]) == toupper(gotkey)))) {
							if (a)
								s = 0;
							else
								s++;
							if (y + (j - (*cur)) + 2 > height + top) {
								(*cur) = j;
								gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
								textattr(lclr | (bclr << 4));
								putch(api->chars->up_arrow);       /* put the up arrow */
								if ((*cur) == opts - 1) {
									gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
									putch(' '); /* delete the down arrow */
								}
								for (i = ((*cur) + vbrdrsize + 1) - height, j = 0; i < (*cur) + 1; i++, j++)
									uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + j
									        , i == (*cur) ? lbclr
									        : lclr | (bclr << 4)
									        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
									        , non_null_str(option[i]));
								y = top + height - bbrdrwidth - 1;
								if (bar)
									(*bar) = optheight - vbrdrsize - 1;
								break;
							}
							if (y - ((*cur) - j) < top + tbrdrwidth) {
								(*cur) = j;
								gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
								textattr(lclr | (bclr << 4));
								if (!(*cur))
									putch(' ');    /* Delete the up arrow */
								gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
								putch(api->chars->down_arrow);     /* put the down arrow */
								uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth
								        , lbclr
								        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
								        , non_null_str(option[(*cur)]));
								for (i = 1; i < height - vbrdrsize; i++)     /* re-display options */
									uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + i
									        , lclr | (bclr << 4)
									        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
									        , non_null_str(option[(*cur) + i]));
								y = top + tbrdrwidth;
								if (bar)
									(*bar) = 0;
								break;
							}
							vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
							             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
							for (i = 0; i < width; i++)
								set_vmem_attr(&line[i], lclr | (bclr << 4));
							vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
							             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
							if ((*cur) > j)
								y -= (*cur) - j;
							else
								y += j - (*cur);
							if (bar) {
								if ((*cur) > j)
									(*bar) -= (*cur) - j;
								else
									(*bar) += j - (*cur);
							}
							(*cur) = j;
							vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
							             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
							for (i = 0; i < width; i++)
								set_vmem_attr(&line[i], lbclr);
							vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
							             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
							break;
						}
					}
					if (a == 2)
						s = 0;
				}
				else
					switch (gotkey) {
						case ' ':
							if (mode & WIN_TAG)
								return MSK_TAG | (*cur);
							break;
						case CR:
							if (!opts)
								break;
							if (mode & WIN_SAV)
								api->savnum++;
							if (mode & WIN_ACT)
								inactive_win(tmp_buffer, s_left + left, s_top + top, s_left + left + width - 1, s_top + top + height - 1, y, hbrdrsize, cclr, lclr, hclr, top);
							else if (mode & WIN_SAV) {
								restore();
							}
							if (mode & WIN_XTR && (*cur) == opts - 1)
								return MSK_INS | *cur;
							return *cur;
						case 3:
						case ESC:
							if (mode & WIN_SAV)
								api->savnum++;
							if (mode & WIN_ESC || (mode & WIN_CHE && api->changes)) {
								vmem_gettext(s_left + left, s_top + top, s_left
								             + left + width - 1, s_top + top + height - 1, tmp_buffer);
								for (i = 0; i < (width * height); i++)
									set_vmem_attr(&tmp_buffer[i], lclr | (cclr << 4));
								vmem_puttext(s_left + left, s_top + top, s_left
								             + left + width - 1, s_top + top + height - 1, tmp_buffer);
							}
							else if (mode & WIN_SAV) {
								restore();
							}
							return -1;
						case CTRL_F:            /* find */
						case CTRL_G:
							if (/*!(api->mode&UIFC_NOCTRL)*/ 1) { // No no, *this* control key is fine!
								if (gotkey == CTRL_G || api->input(WIN_MID | WIN_SAV, 0, 0, "Find", search, sizeof(search), K_EDIT | K_FIND) > 0) {
									for (j = (*cur) + 1; j != *cur; j++, j = (j >= opts) ? 0 : j) {
										if (option[j] == NULL || j >= opts)
											continue;
										if (strcasestr(option[j], search) != NULL) {
											// Copy/pasted from search above.
											if (y + (j - (*cur)) + 2 > height + top) {
												(*cur) = j;
												gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
												textattr(lclr | (bclr << 4));
												putch(api->chars->up_arrow);       /* put the up arrow */
												if ((*cur) == opts - 1) {
													gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
													putch(' '); /* delete the down arrow */
												}
												for (i = ((*cur) + vbrdrsize + 1) - height, j = 0; i < (*cur) + 1; i++, j++)
													uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + j
													        , i == (*cur) ? lbclr
													        : lclr | (bclr << 4)
													        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
													        , non_null_str(option[i]));
												y = top + height - bbrdrwidth - 1;
												if (bar)
													(*bar) = optheight - vbrdrsize - 1;
												break;
											}
											if (y - ((*cur) - j) < top + tbrdrwidth) {
												(*cur) = j;
												gotoxy(s_left + left + lbrdrwidth, s_top + top + tbrdrwidth);
												textattr(lclr | (bclr << 4));
												if (!(*cur))
													putch(' ');    /* Delete the up arrow */
												gotoxy(s_left + left + lbrdrwidth, s_top + top + height - bbrdrwidth - 1);
												putch(api->chars->down_arrow);     /* put the down arrow */
												uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth
												        , lbclr
												        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
												        , non_null_str(option[(*cur)]));
												for (i = 1; i < height - vbrdrsize; i++)     /* re-display options */
													uprintf(s_left + left + lbrdrwidth + 2, s_top + top + tbrdrwidth + i
													        , lclr | (bclr << 4)
													        , "%-*.*s", width - hbrdrsize - 2, width - hbrdrsize - 2
													        , non_null_str(option[(*cur) + i]));
												y = top + tbrdrwidth;
												if (bar)
													(*bar) = 0;
												break;
											}
											vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
											             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
											for (i = 0; i < width; i++)
												set_vmem_attr(&line[i], lclr | (bclr << 4));
											vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
											             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
											if ((*cur) > j)
												y -= (*cur) - j;
											else
												y += j - (*cur);
											if (bar) {
												if ((*cur) > j)
													(*bar) -= (*cur) - j;
												else
													(*bar) += j - (*cur);
											}
											(*cur) = j;
											vmem_gettext(s_left + lbrdrwidth + 2 + left, s_top + y
											             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
											for (i = 0; i < width; i++)
												set_vmem_attr(&line[i], lbclr);
											vmem_puttext(s_left + lbrdrwidth + 2 + left, s_top + y
											             , s_left + left + width - rbrdrwidth - 1, s_top + y, line);
											break;
										}
									}
								}
							}
							break;
						default:
							if (mode & WIN_EXTKEYS)
								return -2 - gotkey;
					}
			}
			/* Update the status bar to reflect the Put/Paste option applicability */
			if (bline & WIN_PASTE && api->bottomline != NULL) {
				if ((mode & (WIN_XTR | WIN_PASTEXTR)) == WIN_XTR && (*cur) == opts - 1)
					api->bottomline(bline & ~WIN_PASTE);
				else
					api->bottomline(bline);
			}
		}
		else
			mswait(1);
		if (mode & WIN_DYN) {
			if (cur)
				save_menu_cur = *cur;
			else
				save_menu_cur = -1;
			if (bar)
				save_menu_bar = *bar;
			else
				save_menu_bar = -1;
			save_menu_opts = opts;
			return -2 - gotkey;
		}
	}
}


/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(uifc_winmode_t mode, int left, int top, const char *inprompt, char *str,
           int max, int kmode)
{
	struct vmem_cell shade[MAX_COLS], save_buf[MAX_COLS * 4], in_win[MAX_COLS * 3], save_bottomline[MAX_COLS];
	int              width;
	int              height = 3;
	int              i, plen, slen, j;
	int              offset;
	int              iwidth;
	int              l;
	char *           prompt;
	int              s_top = SCRN_TOP;
	int              s_left = SCRN_LEFT;
	int              s_right = SCRN_RIGHT;
	int              s_bottom = api->scrn_len - 3;
	int              hbrdrsize = 2;
	int              tbrdrwidth = 1;
	BOOL             shadow = api->scrn_width >= 80;

	reset_dynamic();

	kmode |= api->input_mode; // Global keyboard input mode flags (e.g. K_TRIM)

	if (mode & WIN_FAT) {
		s_top = 1;
		s_left = 2;
		s_right = api->scrn_width - 3;  /* Leave space for the shadow */
	}
	if (mode & WIN_NOBRDR) {
		hbrdrsize = 0;
		tbrdrwidth = 0;
		height = 1;
		shadow = FALSE;
	}

	prompt = strdup(inprompt == NULL ? "":inprompt);
	plen = strlen(prompt);
	if (mode & WIN_NOBRDR)
		offset = 3;
	else if (!plen)
		offset = 2;
	else
		offset = 4;
	slen = hbrdrsize + offset;

	width = plen + slen + max;
	if (width > (s_right - s_left + 1))
		width = (s_right - s_left + 1);
	if (mode & WIN_T2B)
		top = (api->scrn_len - height + 1) / 2 - 2;
	else if (mode & WIN_BOT)
		top = s_bottom - height - top;
	if (mode & WIN_L2R)
		left = (s_right - s_left - width + 1) / 2;
	if (left <= -(s_left))
		left = -(s_left) + 1;
	if (top < 0)
		top = 0;
	if (mode & WIN_SAV) {
		vmem_gettext(s_left + left, s_top + top, s_left + left + width + (shadow * 2) - 1
		             , s_top + top + height + (!shadow), save_buf);
		vmem_gettext(1, api->scrn_len + 1, api->scrn_width, api->scrn_len + 1, save_bottomline);
	}
	if (mode & WIN_ORG) { /* Clear around menu */
		if (top) {
			fill_blk_scrn(FALSE);
			vmem_puttext(1, 2, api->scrn_width, s_top + top - 1, blk_scrn);
		}
		if ((unsigned)(s_top + height + top) <= api->scrn_len) {
			fill_blk_scrn(FALSE);
			vmem_puttext(1, s_top + height + top, api->scrn_width, api->scrn_len, blk_scrn);
		}
		if (left) {
			fill_blk_scrn(FALSE);
			vmem_puttext(1, s_top + top, s_left + left - 1, s_top + height + top
			             , blk_scrn);
		}
		if (s_left + left + width <= s_right) {
			fill_blk_scrn(FALSE);
			vmem_puttext(s_left + left + width, s_top + top, /* s_right+2 */ api->scrn_width
			             , s_top + height + top, blk_scrn);
		}
	}

	iwidth = width - plen - slen;
	while (iwidth < 1 && plen > 4) {
		plen = strlen(prompt);
		prompt[plen - 1] = 0;
		prompt[plen - 2] = '.';
		prompt[plen - 3] = '.';
		prompt[plen - 4] = '.';
		plen--;
		iwidth = width - plen - slen;
	}

	i = 0;
	if (!(mode & WIN_NOBRDR)) {
		set_vmem(&in_win[i++], api->chars->input_top_left, api->hclr | (api->bclr << 4), 0);
		for (j = 1; j < width - 1; j++)
			set_vmem(&in_win[i++], api->chars->input_top, api->hclr | (api->bclr << 4), 0);
		if (api->mode & UIFC_MOUSE && width > 6) {
			j = 1;
			set_vmem(&in_win[j++], api->chars->button_left, api->hclr | (api->bclr << 4), 0);
			set_vmem(&in_win[j++], api->chars->close_char, api->lclr | (api->bclr << 4), 0);
			set_vmem(&in_win[j++], api->chars->button_right, api->hclr | (api->bclr << 4), 0);
			l = 3;
			if (api->helpbuf != NULL || api->helpixbfile[0] != 0) {
				set_vmem(&in_win[j++], api->chars->button_left, api->hclr | (api->bclr << 4), 0);
				set_vmem(&in_win[j++], api->chars->help_char, api->lclr | (api->bclr << 4), 0);
				set_vmem(&in_win[j++], api->chars->button_right, api->hclr | (api->bclr << 4), 0);
				l += 3;
			}
			api->buttony = s_top + top;
			api->exitstart = s_left + left + 1;
			api->exitend = s_left + left + 3;
			api->helpstart = s_left + left + 4;
			api->helpend = s_left + left + l;
		}

		set_vmem(&in_win[i++], api->chars->input_top_right, api->hclr | (api->bclr << 4), 0);
		set_vmem(&in_win[i++], api->chars->input_right, api->hclr | (api->bclr << 4), 0);
	}

	if (plen)
		set_vmem(&in_win[i++], ' ', api->lclr | (api->bclr << 4), 0);

	for (j = 0; prompt[j]; j++)
		set_vmem(&in_win[i++], prompt[j], api->lclr | (api->bclr << 4), 0);

	if (plen)
		set_vmem(&in_win[i++], ':', api->lclr | (api->bclr << 4), 0);

	for (j = 0; j < iwidth + 2; j++) {
		set_vmem(&in_win[i++], ' ', api->lclr | (api->bclr << 4), 0);
	}

	if (!(mode & WIN_NOBRDR)) {
		set_vmem(&in_win[i++], api->chars->input_right, api->hclr | (api->bclr << 4), 0);
		set_vmem(&in_win[i++], api->chars->input_bottom_left, api->hclr | (api->bclr << 4), 0);
		for (j = 1; j < width - 1; j++)
			set_vmem(&in_win[i++], api->chars->input_bottom, api->hclr | (api->bclr << 4), 0);
		set_vmem(&in_win[i], api->chars->input_bottom_right, api->hclr | (api->bclr << 4), 0);  /* I is not incremented to shut up BCC */
	}
	vmem_puttext(s_left + left, s_top + top, s_left + left + width - 1
	             , s_top + top + height - 1, in_win);

	if (shadow) {
		if (api->bclr == BLUE) {
			vmem_gettext(s_left + left + width, s_top + top + 1, s_left + left + width + 1
			             , s_top + top + (height - 1), shade);
			for (j = 0; j < 6; j++)
				set_vmem_attr(&shade[j], DARKGRAY);
			vmem_puttext(s_left + left + width, s_top + top + 1, s_left + left + width + 1
			             , s_top + top + (height - 1), shade);
			vmem_gettext(s_left + left + 2, s_top + top + 3, s_left + left + width + 1
			             , s_top + top + height, shade);
			for (j = 0; j < width; j++)
				set_vmem_attr(&shade[j], DARKGRAY);
			vmem_puttext(s_left + left + 2, s_top + top + 3, s_left + left + width + 1
			             , s_top + top + height, shade);
		}
	}

	if (api->bottomline != NULL)
		api->bottomline(WIN_COPY | WIN_CUT | WIN_PASTE);
	textattr(api->lclr | (api->bclr << 4));
	i = ugetstr(s_left + left + plen + offset, s_top + top + tbrdrwidth, iwidth, str, max, kmode, NULL);
	if (mode & WIN_SAV) {
		vmem_puttext(s_left + left, s_top + top, s_left + left + width + (shadow * 2) - 1
		             , s_top + top + height + (!shadow), save_buf);
		vmem_puttext(1, api->scrn_len + 1, api->scrn_width, api->scrn_len + 1, save_bottomline);
	}
	free(prompt);
	return i;
}

/****************************************************************************/
/* Displays the message 'str' and waits for the user to select "OK"         */
/****************************************************************************/
int  umsg(const char *str)
{
	int   i = 0;
	char *ok[2] = {"OK", ""};

	if (api->mode & UIFC_INMSG)    /* non-cursive */
		return -1;
	api->mode |= UIFC_INMSG;
	i = ulist(WIN_SAV | WIN_MID, 0, 0, 0, &i, 0, str, ok);
	api->mode &= ~UIFC_INMSG;
	return i;
}

/* Same as above, using printf-style varargs */
int umsgf(const char* fmt, ...)
{
	int     retval = -1;
	va_list va;
	char*   buf = NULL;

	va_start(va, fmt);
	retval = vasprintf(&buf, fmt, va);
	va_end(va);
	if (buf != NULL) {
		retval = umsg(buf);
		free(buf);
	}
	return retval;
}

static int yesno(int dflt, const char* fmt, va_list va)
{
	int   retval;
	char* buf = NULL;

	if (vasprintf(&buf, fmt, va) < 0)
		return dflt;
	if (buf == NULL)
		return dflt;
	retval = ulist(WIN_SAV | WIN_MID, 0, 0, 0, &dflt, 0, buf, api->yesNoOpts);
	free(buf);
	return retval;
}

static BOOL confirm(const char* fmt, ...)
{
	int     retval;

	va_list va;
	va_start(va, fmt);
	retval = yesno(0, fmt, va);
	va_end(va);
	return retval == 0;
}

static BOOL deny(const char* fmt, ...)
{
	int     retval;

	va_list va;
	va_start(va, fmt);
	retval = yesno(1, fmt, va);
	va_end(va);
	return retval != 0;
}

/***************************************/
/* Private sub - updates a ugetstr box */
/***************************************/
void getstrupd(int left, int top, int width, char *outstr, int cursoffset, int *scrnoffset, int mode)
{
	_setcursortype(_NOCURSOR);
	if (cursoffset < *scrnoffset)
		*scrnoffset = cursoffset;

	if (*scrnoffset + width < cursoffset)
		*scrnoffset = cursoffset - width;

	gotoxy(left, top);
	if (mode & K_PASSWORD)
		// This typecast is to suppress a clang warning "adding 'unsigned long' to a string does not append to the string [-Wstring-plus-int]"
		cprintf("%-*.*s", width, width, ((char *)"********************************************************************************") + (80 - strlen(outstr + *scrnoffset)));
	else
		cprintf("%-*.*s", width, width, outstr + *scrnoffset);
	gotoxy(left + (cursoffset - *scrnoffset), top);
	_setcursortype(cursor);
}

static void set_cursor_type(uifcapi_t* api)
{
	BOOL block_cursor = api->insert_mode;
	if (api->reverse_cursor)
		block_cursor = !block_cursor;
	_setcursortype(cursor = (block_cursor ? _SOLIDCURSOR : _NORMALCURSOR));
}

/****************************************************************************/
/* Gets a string of characters from the user. Turns cursor on. Allows 	    */
/* Different modes - K_* macros. ESC aborts input.                          */
/****************************************************************************/
int ugetstr(int left, int top, int width, char *outstr, int max, long mode, int *lastkey)
{
	char *             str;
	int                ch;
	int                i, j, k, f = 0; /* i=offset, j=length */
	BOOL               gotdecimal = FALSE;
	int                soffset = 0;
	struct mouse_event mevnt;
	char *             pastebuf = NULL;
	unsigned char *    pb = NULL;

	if ((str = alloca(max + 1)) == NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
		        , __LINE__, (max + 1));
		_setcursortype(cursor);
		return -1;
	}
	gotoxy(left, top);
	set_cursor_type(api);
	str[0] = 0;
	if (mode & K_EDIT && outstr[0]) {
		/***
		    truncspctrl(outstr);
		***/
		outstr[max] = 0;
		i = j = strlen(outstr);
		textattr(api->lbclr);
		getstrupd(left, top, width, outstr, i, &soffset, mode);
		textattr(api->lclr | (api->bclr << 4));
		if (strlen(outstr) < (size_t)width) {
			k = wherex();
			f = wherey();
			cprintf("%*s", width - strlen(outstr), "");
			gotoxy(k, f);
		}
		strcpy(str, outstr);
#if 0
		while (uifc_kbwait() == 0) {
			mswait(1);
		}
#endif
		f = inkey();
		if (f == CIO_KEY_QUIT) {
			api->exit_flags |= UIFC_XF_QUIT;
			return -1;
		}

		if (f == CIO_KEY_MOUSE) {
			f = uifc_getmouse(&mevnt);
			if (f == 0 || (f == ESC && mevnt.event == CIOLIB_BUTTON_3_CLICK)) {
				if (mode & K_MOUSEEXIT
				    && (mevnt.starty != top
				        || mevnt.startx > left + width
				        || mevnt.startx < left)
				    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
					if (lastkey)
						*lastkey = CIO_KEY_MOUSE;
					ungetmouse(&mevnt);
					return j;
				}
				if (mevnt.startx >= left
				    && mevnt.startx <= left + width
				    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
					i = mevnt.startx - left + soffset;
					if (i > j)
						i = j;
				}
				if (mevnt.starty == top
				    && mevnt.startx >= left
				    && mevnt.startx <= left + width
				    && (mevnt.event == CIOLIB_BUTTON_2_CLICK
				        || mevnt.event == CIOLIB_BUTTON_3_CLICK)) {
					i = mevnt.startx - left + soffset;
					if (i > j)
						i = j;
					pb = (uint8_t *)getcliptext();
					if (pb) {
						pastebuf = utf8_to_cp(CIOLIB_CP437, pb, '?', strlen((char *)pb), NULL);
						free(pb);
						pb = (unsigned char *)pastebuf;
					}
					else {
						free(pastebuf);
						pastebuf = NULL;
					}
					f = 0;
				}
			}
		}

		if (f == CR
		    || (f >= 0xff && f != CIO_KEY_DC)
		    || (f == 3840 && mode & K_TABEXIT)      // Backtab
		    || (f == '\t' && mode & K_TABEXIT)
		    || (f == '%' && mode & K_SCANNING)
		    || f == CTRL_B
		    || f == CTRL_E
		    || f == CTRL_Z
		    || f == CTRL_X      /* Cut */
		    || f == CTRL_C      /* Copy */
		    || f == 0)
		{
			getstrupd(left, top, width, str, i, &soffset, mode);
		}
		else
		{
			getstrupd(left, top, width, str, i, &soffset, mode);
			i = j = 0;
		}
	}
	else
		i = j = 0;

	ch = 0;
	while (ch != CR)
	{
		if (i > j)
			j = i;
		str[j] = 0;
		getstrupd(left, top, width, str, i, &soffset, mode);
		if (f || pb != NULL || (ch = inkey()) != 0)
		{
			if (f) {
				ch = f;
				f = 0;
			}
			else if (pb != NULL) {
				ch = *(pb++);
				if (!*pb) {
					free(pastebuf);
					pastebuf = NULL;
					pb = NULL;
				}
			}
			if ((mode & (K_SPACE|K_TRIM)) == K_TRIM && i < 1 && IS_WHITESPACE(ch))
				continue;
			if ((mode & (K_SPACE|K_NOSPACE)) == K_NOSPACE && IS_WHITESPACE(ch))
				continue;
			if (ch == CIO_KEY_MOUSE) {
				ch = uifc_getmouse(&mevnt);
				if (ch == 0 || (ch == ESC && mevnt.event == CIOLIB_BUTTON_3_CLICK)) {
					if (mode & K_MOUSEEXIT
					    && (mevnt.starty != top
					        || mevnt.startx > left + width
					        || mevnt.startx < left)
					    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
						if (lastkey)
							*lastkey = CIO_KEY_MOUSE;
						ungetmouse(&mevnt);
						ch = CR;
						continue;
					}
					if (mevnt.starty == top
					    && mevnt.startx >= left
					    && mevnt.startx <= left + width
					    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
						i = mevnt.startx - left + soffset;
						if (i > j)
							i = j;
					}
					if (mevnt.starty == top
					    && mevnt.startx >= left
					    && mevnt.startx <= left + width
					    && (mevnt.event == CIOLIB_BUTTON_2_CLICK
					        || mevnt.event == CIOLIB_BUTTON_3_CLICK)) {
						i = mevnt.startx - left + soffset;
						if (i > j)
							i = j;
						pb = (uint8_t *)getcliptext();
						if (pb) {
							pastebuf = utf8_to_cp(CIOLIB_CP437, pb, '?', strlen((char *)pb), NULL);
							free(pb);
							pb = (unsigned char *)pastebuf;
						}
						else {
							free(pastebuf);
							pastebuf = NULL;
						}
						ch = 0;
					}
				}
			}
			if (lastkey != NULL)
				*lastkey = ch;
			switch (ch)
			{
				case CTRL_Z:
				case CIO_KEY_F(1):  /* F1 Help */
					api->showhelp();
					if (api->exit_flags & UIFC_XF_QUIT)
						f = CIO_KEY_QUIT;
					continue;
				case CIO_KEY_LEFT:  /* left arrow */
					if (i)
					{
						i--;
					}
					continue;
				case CIO_KEY_RIGHT: /* right arrow */
					if (i < j)
					{
						i++;
					}
					continue;
				case CTRL_B:
				case CIO_KEY_HOME:  /* home */
					if (i)
					{
						i = 0;
					}
					continue;
				case CTRL_E:
				case CIO_KEY_END:   /* end */
					if (i < j)
					{
						i = j;
					}
					continue;
				case CTRL_V:
				case CIO_KEY_SHIFT_IC:  /* Shift-Insert: Paste */
					pb = (uint8_t *)getcliptext();
					if (pb) {
						pastebuf = utf8_to_cp(CIOLIB_CP437, pb, '?', strlen((char *)pb), NULL);
						free(pb);
						pb = (unsigned char *)pastebuf;
					}
					else {
						free(pastebuf);
						pastebuf = NULL;
					}
					continue;
				case CIO_KEY_IC:    /* insert */
					api->insert_mode = !api->insert_mode;
					set_cursor_type(api);
					continue;
				case BS:
					if (i)
					{
						if (i == j)
						{
							j--;
							i--;
						}
						else {
							i--;
							j--;
							if (str[i] == '.')
								gotdecimal = FALSE;
							for (k = i; k <= j; k++)
								str[k] = str[k + 1];
						}
						if (soffset > 0)
							soffset--;
						continue;
					}
					break;
				case CIO_KEY_DC:    /* delete */
				case DEL:           /* sdl_getch() is returning 127 when keypad "Del" is hit */
					if (i < j)
					{
						if (str[i] == '.')
							gotdecimal = FALSE;
						for (k = i; k < j; k++)
							str[k] = str[k + 1];
						j--;
					}
					continue;
				case CIO_KEY_QUIT:
					api->exit_flags |= UIFC_XF_QUIT;
				/* Fall-through */
				case CIO_KEY_ABORTED:
				case ESC:
				{
					cursor = _NOCURSOR;
					_setcursortype(cursor);
					if (pastebuf != NULL) {
						free(pastebuf);
						pastebuf = NULL;
					}
					return -1;
				}
				case CR:
					break;
				case 3840:  /* Backtab */
				case '\t':
					if (mode & K_TABEXIT)
						ch = CR;
					break;
				case '%':   /* '%' indicates that a UPC is coming next */
					if (mode & K_SCANNING)
						ch = CR;
					break;
				case CIO_KEY_F(2):
				case CIO_KEY_UP:
				case CIO_KEY_DOWN:
					if (mode & K_DEUCEEXIT) {
						ch = CR;
						break;
					}
					continue;
				case CTRL_C:
				case CIO_KEY_CTRL_IC:   /* Ctrl-Insert */
				{
					size_t   sz;
					uint8_t *utf8 = cp_to_utf8(CIOLIB_CP437, str, j, &sz);
					copytext((char *)utf8, sz);
					free(utf8);
					continue;
				}
				case CTRL_X:
				case CIO_KEY_SHIFT_DC:
					if (j)
					{
						size_t   sz;
						uint8_t *utf8 = cp_to_utf8(CIOLIB_CP437, str, j, &sz);
						copytext((char *)utf8, sz);
						free(utf8);
						i = j = 0;
					}
					continue;
				case CTRL_Y:
					if (i < j)
					{
						j = i;
					}
					continue;
			}
			if (!((mode & K_NEGATIVE) && ch == '-' && i == 0)) {
				if (mode & K_NUMBER && !isdigit(ch))
					continue;
				if (mode & K_DECIMAL && !isdigit(ch)) {
					if (ch != '.')
						continue;
					if (gotdecimal)
						continue;
					gotdecimal = TRUE;
				}
			}
			if (mode & K_ALPHA && !isalpha(ch))
				continue;
			if ((ch >= ' ' || (ch == 1 && mode & K_MSG)) && i < max && (!api->insert_mode || j < max) && ch < 256)
			{
				if (mode & K_UPPER)
					ch = toupper(ch);
				if (api->insert_mode)
				{
					for (k = ++j; k > i; k--)
						str[k] = str[k - 1];
				}
				str[i++] = ch;
			}
		}
	}


	str[j] = 0;
	if (mode & K_EDIT)
	{
		if (strcmp(outstr, str) != 0) {
			if (!(mode & K_FIND))
				api->changes = TRUE;
		}
		else if (mode & K_CHANGED)
			j = -1;
	}
	else
	{
		if (!(mode & K_FIND) && j)
			api->changes = TRUE;
	}
	if ((mode & (K_SPACE|K_TRIM)) == K_TRIM)
		truncspctrl(str);
	strcpy(outstr, str);
	cursor = _NOCURSOR;
	_setcursortype(cursor);
	if (pastebuf != NULL)
		free(pastebuf);
	return j;
}

/****************************************************************************/
/* Performs printf() through puttext() routine								*/
/****************************************************************************/
static int uprintf(int x, int y, unsigned attr, char *fmat, ...)
{
	va_list          argptr;
	char             str[MAX_COLS + 1];
	struct vmem_cell buf[MAX_COLS];
	int              i;

	va_start(argptr, fmat);
	vsprintf(str, fmat, argptr);
	va_end(argptr);
	for (i = 0; str[i]; i++)
		set_vmem(&buf[i], str[i], attr, 0);
	vmem_puttext(x, y, x + (i - 1), y, buf);
	return i;
}


/****************************************************************************/
/* Display bottom line of screen in inverse                                 */
/****************************************************************************/
void bottomline(uifc_winmode_t mode)
{
	int i = 1;

	i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "    ");
	if (api->help_available) {
		i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "F1 ");
		i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "Help  ");
	}
	if (mode & WIN_EDIT) {
		i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "F2 ");
		i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "Edit Item  ");
	}
	if (mode & WIN_TAG) {
		i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "Space ");
		i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "Tag Item  ");
		i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "^A");
		i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "ll  ");
	}
	if (mode & WIN_COPY) {
		if (api->mode & UIFC_NOCTRL) {
			i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "F5 ");
			i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "Copy  ");
		} else {
			i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "^C");
			i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "opy  ");
		}
	}
	if (mode & WIN_CUT) {
		if (api->mode & UIFC_NOCTRL)
			i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "Shift-DEL ");
		else
			i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "^X ");
		i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "Cut  ");
	}
	if (mode & WIN_PASTE) {
		if (api->mode & UIFC_NOCTRL)
			i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "F6 ");
		else
			i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "^V ");
		i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "Paste  ");
	}
	if (mode & WIN_INS) {
#ifdef __DARWIN__
		i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "+/");
#endif
		i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "INS");
		i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "ert Item  ");
	}
	if (mode & WIN_DEL) {
#ifdef __DARWIN__
		i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "fn-");
#endif
		i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "DEL");
		i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "ete Item  ");
	}
	i += uprintf(i, api->scrn_len + 1, api->bclr | (api->cclr << 4), "ESC ");    /* Backspace is no good no way to abort editing */
	i += uprintf(i, api->scrn_len + 1, BLACK | (api->cclr << 4), "Exit");
	gotoxy(i, api->scrn_len + 1);
	if (wherex() == i && wherey() == api->scrn_len + 1) {
		textattr(BLACK | (api->cclr << 4));
		clreol();
	}
}

/*****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer  */
/* Used as a replacement for ctime()                                         */
/*****************************************************************************/
char *utimestr(time_t *intime)
{
	static char str[25];
	const char* wday = "";
	const char* mon = "";
	const char* mer = "";
	int         hour;
	struct tm * gm;

	gm = localtime(intime);
	switch (gm->tm_wday) {
		case 0:
			wday = "Sun";
			break;
		case 1:
			wday = "Mon";
			break;
		case 2:
			wday = "Tue";
			break;
		case 3:
			wday = "Wed";
			break;
		case 4:
			wday = "Thu";
			break;
		case 5:
			wday = "Fri";
			break;
		case 6:
			wday = "Sat";
			break;
	}
	switch (gm->tm_mon) {
		case 0:
			mon = "Jan";
			break;
		case 1:
			mon = "Feb";
			break;
		case 2:
			mon = "Mar";
			break;
		case 3:
			mon = "Apr";
			break;
		case 4:
			mon = "May";
			break;
		case 5:
			mon = "Jun";
			break;
		case 6:
			mon = "Jul";
			break;
		case 7:
			mon = "Aug";
			break;
		case 8:
			mon = "Sep";
			break;
		case 9:
			mon = "Oct";
			break;
		case 10:
			mon = "Nov";
			break;
		case 11:
			mon = "Dec";
			break;
	}
	if (gm->tm_hour >= 12) {
		mer = "pm";
		hour = gm->tm_hour;
		if (hour > 12)
			hour -= 12;
	}
	else {
		if (!gm->tm_hour)
			hour = 12;
		else
			hour = gm->tm_hour;
		mer = "am";
	}
	safe_snprintf(str, sizeof(str), "%s %s %02d %4d %02d:%02d %s", wday, mon, gm->tm_mday, 1900 + gm->tm_year
	              , hour, gm->tm_min, mer);
	return str;
}

/****************************************************************************/
/* Status popup/down function, see uifc.h for details.						*/
/****************************************************************************/
void upop(const char *instr, ...)
{
	char*                   str;
	static struct vmem_cell sav[MAX_COLS * 3], buf[MAX_COLS * 3];
	int                     i, j, k;
	static int              width;

	if (instr == NULL) {
		vmem_puttext((api->scrn_width - width + 1) / 2 + 1, (api->scrn_len - 3 + 1) / 2 + 1
		             , (api->scrn_width + width - 1) / 2 + 1, (api->scrn_len + 3 - 1) / 2 + 1, sav);
		return;
	}

	va_list va;
	va_start(va, instr);
	i = vasprintf(&str, instr, va);
	va_end(va);
	if (i < 0)
		return;

	width = strlen(str);
	if (!width) {
		free(str);
		return;
	}
	width += 7;
	if ((uint)width > api->scrn_width) {
		str[api->scrn_width - 7] = '\0';
		width = api->scrn_width;
	}
	vmem_gettext((api->scrn_width - width + 1) / 2 + 1, (api->scrn_len - 3 + 1) / 2 + 1
	             , (api->scrn_width + width - 1) / 2 + 1, (api->scrn_len + 3 - 1) / 2 + 1, sav);
	for (i = 0; i < width * 3; i++)
		set_vmem(&buf[i], ' ', api->hclr | (api->bclr << 4), 0);
	set_vmem(&buf[0], api->chars->popup_top_left, api->hclr | (api->bclr << 4), 0);
	for (i = 1; i < (width - 1); i++)
		set_vmem(&buf[i], api->chars->popup_top, api->hclr | (api->bclr << 4), 0);
	set_vmem(&buf[i++], api->chars->popup_top_right, api->hclr | (api->bclr << 4), 0);
	set_vmem(&buf[i++], api->chars->popup_left, api->hclr | (api->bclr << 4), 0);
	k = strlen(str);
	i += ((((width - 3) - k) / 2));
	for (j = 0; j < k; j++, i++)
		set_vmem(&buf[i], str[j], api->hclr | (api->bclr << 4) | BLINK, 0);
	i = (((width - 1) * 2) + 1);
	set_vmem(&buf[i++], api->chars->popup_right, api->hclr | (api->bclr << 4), 0);
	set_vmem(&buf[i++], api->chars->popup_bottom_left, api->hclr | (api->bclr << 4), 0);
	for (; i < ((width * 3) - 1); i++)
		set_vmem(&buf[i], api->chars->popup_bottom, api->hclr | (api->bclr << 4), 0);
	set_vmem(&buf[i], api->chars->popup_bottom_right, api->hclr | (api->bclr << 4), 0);

	vmem_puttext((api->scrn_width - width + 1) / 2 + 1, (api->scrn_len - 3 + 1) / 2 + 1
	             , (api->scrn_width + width - 1) / 2 + 1, (api->scrn_len + 3 - 1) / 2 + 1, buf);

	free(str);
}

/****************************************************************************/
/* Sets the current help index by source code file and line number.			*/
/****************************************************************************/
void sethelp(int line, char* file)
{
	helpline = line;
	helpfile = file;
}

/****************************************************************************/
/* Shows a scrollable text buffer - optionally parsing "help markup codes"	*/
/****************************************************************************/
void showbuf(uifc_winmode_t mode, int left, int top, int width, int height, const char *title, const char *hbuf, int *curp, int *barp)
{
	char               inverse = 0, high = 0;
	struct vmem_cell * textbuf;
	struct vmem_cell * p;
	const char *       cpc;
	struct vmem_cell * oldp = NULL;
	int                i, j, k, len;
	int                lines;
	int                line_offset;
	int                pad = 1;
	int                is_redraw = 0;
	uint               title_len = 0;
	struct mouse_event mevnt;

	_setcursortype(_NOCURSOR);

	title_len = strlen(title);
	if (title_len + 13 + left > api->scrn_width)
		title_len = api->scrn_width - (left + 13);

	if ((unsigned)(top + height) >= api->scrn_len)
		height = api->scrn_len - top;
	if (!width || (unsigned)width < title_len + 6)
		width = title_len + 6;
	if ((unsigned)(width + left) > api->scrn_width)
		width = api->scrn_width - left;
	if (mode & WIN_L2R)
		left = (api->scrn_width - width + 2) / 2;
	else if (mode & WIN_RHT)
		left = SCRN_RIGHT - (width + 4 + left);
	if (mode & WIN_T2B)
		top = (api->scrn_len - height + 1) / 2;
	else if (mode & WIN_BOT)
		top = api->scrn_len - height - 3 - top;
	if (left < 0)
		left = 0;
	if (top < 0)
		top = 0;

	if (mode & WIN_PACK)
		pad = 0;

	/* Dynamic Menus */
	if (mode & WIN_DYN
	    && curp != NULL
	    && barp != NULL
	    && last_menu_cur == curp
	    && last_menu_bar == barp
	    && save_menu_cur == *curp
	    && save_menu_bar == *barp)
		is_redraw = 1;
	if (mode & WIN_DYN && mode & WIN_REDRAW)
		is_redraw = 1;
	if (mode & WIN_DYN && mode & WIN_NODRAW)
		is_redraw = 0;

	last_menu_cur = curp;
	last_menu_bar = barp;

	vmem_gettext(1, 1, api->scrn_width, api->scrn_len, tmp_buffer);

	if (!is_redraw) {
		for (i = 0; i < width * height; i++)
			set_vmem(&tmp_buffer2[i], ' ', api->hclr | (api->bclr << 4), 0);
		tmp_buffer2[0].ch = api->chars->help_top_left;
		j = title_len + 4; // Account for title breaks and spaces
		if (j > width - 6) {
			j = width - 6;
		}
		for (i = 1; i < (width - j) / 2; i++)
			tmp_buffer2[i].ch = api->chars->help_top;
		if ((api->mode & UIFC_MOUSE) && (!(mode & WIN_DYN))) {
			set_vmem(&tmp_buffer2[1], api->chars->button_left, api->hclr | (api->bclr << 4), 0);
			set_vmem(&tmp_buffer2[2], api->chars->close_char, api->lclr | (api->bclr << 4), 0);
			set_vmem(&tmp_buffer2[3], api->chars->button_right, api->hclr | (api->bclr << 4), 0);
			/* Buttons are ignored - leave it this way to not confuse stuff from help() */
		}
		tmp_buffer2[i].ch = api->chars->help_titlebreak_left;
		i += 2;
		for (cpc = title; *cpc && cpc < &title[j]; cpc++) {
			tmp_buffer2[i].ch = *cpc;
			i++;
		}
		i++;
		tmp_buffer2[i].ch = api->chars->help_titlebreak_right;
		i++;
		for (; i < ((width - 1)); i++)
			tmp_buffer2[i].ch = api->chars->help_top;
		tmp_buffer2[i].ch = api->chars->help_top_right;
		i++;
		j = i;    /* leave i alone */
		for (k = 0; k < (height - 2); k++) {         /* the sides of the box */
			tmp_buffer2[j].ch = api->chars->help_left;
			j++;
			j += ((width - 2));
			tmp_buffer2[j].ch = api->chars->help_right;
			j++;
		}
		tmp_buffer2[j].ch = api->chars->help_bottom_left;
		j++;
		if (!(mode & WIN_DYN) && (width > 31)) {
			for (k = j; k < j + (((width - 4) / 2 - 13)); k++)
				tmp_buffer2[k].ch = api->chars->help_bottom;
			tmp_buffer2[k].ch = api->chars->help_hitanykey_left; k += 2;
			tmp_buffer2[k].ch = 'H'; k++;
			tmp_buffer2[k].ch = 'i'; k++;
			tmp_buffer2[k].ch = 't'; k += 2;
			tmp_buffer2[k].ch = 'a'; k++;
			tmp_buffer2[k].ch = 'n'; k++;
			tmp_buffer2[k].ch = 'y'; k += 2;
			tmp_buffer2[k].ch = 'k'; k++;
			tmp_buffer2[k].ch = 'e'; k++;
			tmp_buffer2[k].ch = 'y'; k += 2;
			tmp_buffer2[k].ch = 't'; k++;
			tmp_buffer2[k].ch = 'o'; k += 2;
			tmp_buffer2[k].ch = 'c'; k++;
			tmp_buffer2[k].ch = 'o'; k++;
			tmp_buffer2[k].ch = 'n'; k++;
			tmp_buffer2[k].ch = 't'; k++;
			tmp_buffer2[k].ch = 'i'; k++;
			tmp_buffer2[k].ch = 'n'; k++;
			tmp_buffer2[k].ch = 'u'; k++;
			tmp_buffer2[k].ch = 'e'; k += 2;
			tmp_buffer2[k].ch = api->chars->help_hitanykey_right; k++;
			for (j = k; j < k + (((width - 4) / 2 - 12)) + (width & 1); j++)
				tmp_buffer2[j].ch = api->chars->help_bottom;
		}
		else {
			for (k = j; k < j + ((width - 2)); k++)
				tmp_buffer2[k].ch = api->chars->help_bottom;
			j = k;
		}
		tmp_buffer2[j].ch = api->chars->help_bottom_right;
		if (!(mode & WIN_DYN)) {
			tmp_buffer2[j - 1].ch = api->chars->button_right;
			tmp_buffer2[j - 2].ch = ' ';
			tmp_buffer2[j - 3].ch = ' ';
			tmp_buffer2[j - 4].ch = api->chars->button_left;
#define SCROLL_UP_BUTTON_X  left + (width - 4)
#define SCROLL_UP_BUTTON_Y  top + height
#define SCROLL_DN_BUTTON_X  left + (width - 3)
#define SCROLL_DN_BUTTON_Y  top + height
		}
		vmem_puttext(left, top + 1, left + width - 1, top + height, tmp_buffer2);
	}
	len = strlen(hbuf);

	lines = 0;
	k = 0;
	for (j = 0; j < len; j++) {
		if ((mode & WIN_HLP) && (hbuf[j] == 2 || hbuf[j] == '~' || hbuf[j] == 1 || hbuf[j] == '`'))
			continue;
		if (hbuf[j] == CR)
			continue;
		k++;
		if ((hbuf[j] == LF) || (k >= width - 2 - pad - pad && (hbuf[j + 1] != '\n' && hbuf[j + 1] != '\r'))) {
			k = 0;
			lines++;
		}
	}
	if (k)
		lines++;
	if (lines < height - 2 - pad - pad)
		lines = height - 2 - pad - pad;

	if ((textbuf = (struct vmem_cell *)malloc((width - 2 - pad - pad) * lines * sizeof(*textbuf))) == NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
		        , __LINE__, (width - 2 - pad - pad) * lines * sizeof(*textbuf));
		_setcursortype(cursor);
		return;
	}
	for (i = 0; i < (width - 2 - pad - pad) * lines; i++)
		set_vmem(&textbuf[i], ' ', (api->hclr | (api->bclr << 4)), 0);
	line_offset = 0;
	for (j = i = 0; j < len; j++, i++) {
		if (hbuf[j] == LF) {
			i++;
			while (i % ((width - 2 - pad - pad)))
				i++;
			i--;
			line_offset = 0;
		}
		else if (hbuf[j] == '\t') {
			++line_offset;
			while (line_offset % 8) {
				++line_offset;
				++i;
			}
		}
		else if ((mode & WIN_HLP) && (hbuf[j] == 2 || hbuf[j] == '~')) {
			inverse = !inverse;
			i--;
		}
		else if ((mode & WIN_HLP) && (hbuf[j] == 1 || hbuf[j] == '`')) {
			high = !high;
			i--;
		}
		else if (hbuf[j] != CR && hbuf[j] != FF) {
			++line_offset;
			set_vmem(&textbuf[i], hbuf[j], inverse ? (api->bclr | (api->cclr << 4)) : high ? (api->hclr | (api->bclr << 4)) : (api->lclr | (api->bclr << 4)), 0);
			if (((i + 1) % ((width - 2 - pad - pad)) == 0 && (hbuf[j + 1] == LF)) || (hbuf[j + 1] == CR && hbuf[j + 2] == LF))
				i--;
		}
		else
			i--;
	}
	i = 0;
	p = textbuf;
	struct vmem_cell * textend = textbuf + (lines - (height - 2 - pad - pad)) * (width - 2 - pad - pad);
	if (mode & WIN_DYN) {
		vmem_puttext(left + 1 + pad, top + 2 + pad, left + width - 2 - pad, top + height - 1 - pad, p);
	}
	else {
		while (i == 0) {
			if (p != oldp) {
				if (p > textend)
					p = textend;
				if (p < textbuf)
					p = textbuf;
				if (p != oldp) {
					vmem_puttext(left + 1 + pad, top + 2 + pad, left + width - 2 - pad, top + height - 1 - pad, p);
					oldp = p;
				}
				gotoxy(SCROLL_UP_BUTTON_X, SCROLL_UP_BUTTON_Y);
				textattr(api->lclr | (api->bclr << 4));
				putch(p > textbuf ? api->chars->up_arrow : ' ');
				putch(p < textend ? api->chars->down_arrow : ' ');
			}
			if (dyn_kbwait(mode)) {
				j = inkey();
				if (j == CIO_KEY_MOUSE) {
					/* Ignores return value to avoid hitting help/exit hotspots */
					if (uifc_getmouse(&mevnt) >= 0) {
						/* Clicked Scroll Up */
						if (mevnt.startx >= left + pad
						    && mevnt.startx <= left + pad + width - 3
						    && mevnt.starty >= top + pad + 1
						    && mevnt.starty <= top + pad + (height / 2) - 2
						    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
							p -= ((width - 2 - pad - pad) * (height - 5));
							continue;
						}
						if (mevnt.startx == SCROLL_UP_BUTTON_X && mevnt.starty == SCROLL_UP_BUTTON_Y
						    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
							p -= ((width - 2 - pad - pad));
							continue;
						}
						/* Clicked Scroll Down */
						if (mevnt.startx >= left + pad
						    && mevnt.startx <= left + pad + width
						    && mevnt.starty <= top + pad + height - 2
						    && mevnt.starty >= top + pad + height - (height / 2 + 1) - 2
						    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
							p += (width - 2 - pad - pad) * (height - 5);
							continue;
						}
						if (mevnt.startx == SCROLL_DN_BUTTON_X && mevnt.starty == SCROLL_DN_BUTTON_Y
						    && mevnt.event == CIOLIB_BUTTON_1_CLICK) {
							p += ((width - 2 - pad - pad));
							continue;
						}
						/* Scroll up */
						if (mevnt.event == CIOLIB_BUTTON_4_PRESS) {
							p = p - ((width - 2 - pad - pad));
							continue;
						}
						/* Scroll down */
						if (mevnt.event == CIOLIB_BUTTON_5_PRESS) {
							p += ((width - 2 - pad - pad));
							continue;
						}
						/* Non-click events (drag, move, multiclick, etc) */
						if (mevnt.event != CIOLIB_BUTTON_CLICK(CIOLIB_BUTTON_NUMBER(mevnt.event)))
							continue;
						i = 1;
					}
					continue;
				}
				switch (j) {
					case CIO_KEY_HOME:  /* home */
						p = textbuf;
						break;

					case CIO_KEY_UP:    /* up arrow */
						p = p - ((width - 2 - pad - pad));
						break;

					case CIO_KEY_PPAGE: /* PgUp */
						p = p - ((width - 2 - pad - pad) * (height - 5));
						break;

					case CIO_KEY_NPAGE: /* PgDn */
						p += (width - 2 - pad - pad) * (height - 5);
						break;

					case CIO_KEY_END:   /* end */
						p = textend;
						break;

					case CIO_KEY_DOWN:  /* dn arrow */
						p += ((width - 2 - pad - pad));
						break;

					case CIO_KEY_QUIT:
						api->exit_flags |= UIFC_XF_QUIT;
					// Fall-through
					default:
						i = 1;
				}
			}
			mswait(1);
		}

		vmem_puttext(1, 1, api->scrn_width, api->scrn_len, tmp_buffer);
	}
	free(textbuf);
	if (is_redraw)           /* Force redraw of menu also. */
		reset_dynamic();
	_setcursortype(cursor);
}

/************************************************************/
/* Help (F1) key function. Uses helpbuf as the help input.	*/
/************************************************************/
static void help(void)
{
	char           hbuf[HELPBUF_SIZE], str[256];
	char *         p;
	unsigned short line;    /* This must be 16-bits */
	long           l;
	FILE *         fp;

	if (api->helpbuf == NULL && api->helpixbfile[0] == 0)
		return;

	_setcursortype(_NOCURSOR);

	if (!api->helpbuf) {
		if ((fp = fopen(api->helpixbfile, "rb")) == NULL)
			SAFEPRINTF(hbuf, "ERROR: Cannot open help index: %s"
			           , api->helpixbfile);
		else {
			p = strrchr(helpfile, '/');
			if (p == NULL)
				p = strrchr(helpfile, '\\');
			if (p == NULL)
				p = helpfile;
			else
				p++;
			l = -1L;
			while (!feof(fp)) {
				if (fread(str, 12, 1, fp) != 1)
					break;
				str[12] = 0;
				if (fread(&line, 2, 1, fp) != 1)
					break;
				if (stricmp(str, p) || line != helpline) {
					if (fseek(fp, 4, SEEK_CUR) == 0)
						break;
					continue;
				}
				if (fread(&l, 4, 1, fp) != 1)
					l = -1L;
				break;
			}
			fclose(fp);
			if (l == -1L)
				SAFEPRINTF3(hbuf, "ERROR: Cannot locate help key (%s:%u) in: %s"
				            , p, helpline, api->helpixbfile);
			else {
				if ((fp = fopen(api->helpdatfile, "rb")) == NULL)
					SAFEPRINTF(hbuf, "ERROR: Cannot open help file: %s"
					           , api->helpdatfile);
				else {
					if (fseek(fp, l, SEEK_SET) != 0) {
						SAFEPRINTF4(hbuf, "ERROR: Cannot seek to help key (%s:%u) at %ld in: %s"
						            , p, helpline, l, api->helpixbfile);
					}
					else {
						if (fread(hbuf, 1, HELPBUF_SIZE, fp) < 1) {
							SAFEPRINTF4(hbuf, "ERROR: Cannot read help key (%s:%u) at %ld in: %s"
							            , p, helpline, l, api->helpixbfile);
						}
					}
					fclose(fp);
				}
			}
		}
		showbuf(WIN_MID | WIN_HLP, 0, 0, 76, api->scrn_len, "Online Help", hbuf, NULL, NULL);
	}
	else {
		showbuf(WIN_MID | WIN_HLP, 0, 0, 76, api->scrn_len, "Online Help", api->helpbuf, NULL, NULL);
	}
}
