/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: term.c,v 1.387 2020/06/27 00:04:50 deuce Exp $ */

#include <assert.h>
#include <ciolib.h>
#include <cterm.h>
#include <genwrap.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <vidmodes.h>

#include "conn.h"
#include "dirwrap.h"
#include "filepick.h"
#include "filewrap.h"
#include "gen_defs.h"
#include "menu.h"
#include "saucedefs.h"
#include "sexyz.h"
#include "strwrap.h"
#include "syncterm.h"
#include "telnet_io.h"
#include "term.h"
#include "threadwrap.h"
#include "uifcinit.h"
#include "window.h"
#include "xmodem.h"
#include "xpbeep.h"
#include "xpendian.h"
#include "xpprintf.h"
#include "zmodem.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

#ifndef WITHOUT_OOII
 #include "ooii.h"
#endif
#include "base64.h"
#include "md5.h"
#include "ripper.h"

#ifdef WITH_JPEG_XL
#include "libjxl.h"
#include "xpmap.h"
#endif

#define ANSI_REPLY_BUFSIZE 2048
static char ansi_replybuf[2048];

#define DUMP

#ifndef MIN
 #define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

struct terminal   term;
struct cterminal *cterm;

#define TRANSFER_WIN_WIDTH 66
#define TRANSFER_WIN_HEIGHT 18
static struct vmem_cell winbuf[(TRANSFER_WIN_WIDTH + 2) * (TRANSFER_WIN_HEIGHT + 1) * 2]; /* Save buffer for transfer
                                                                                           * window */
static struct text_info trans_ti;
static struct text_info log_ti;

static struct ciolib_pixels *pixmap_buffer[2];
static struct ciolib_mask *mask_buffer;
static uint8_t pnm_gamma[256] = {
	0,  13, 22, 28, 34, 38, 42, 46, 50, 53, 56, 59, 61, 64, 66, 69,
	71, 73, 75, 77, 79, 81, 83, 85, 86, 88, 90, 92, 93, 95, 96, 98,
	99, 101, 102, 104, 105, 106, 108, 109, 110, 112, 113, 114, 115,
	117, 118, 119, 120, 121, 122, 124, 125, 126, 127, 128, 129, 130,
	131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 147, 148, 148, 149, 150, 151, 152, 153, 154, 155,
	155, 156, 157, 158, 159, 159, 160, 161, 162, 163, 163, 164, 165,
	166, 167, 167, 168, 169, 170, 170, 171, 172, 173, 173, 174, 175,
	175, 176, 177, 178, 178, 179, 180, 180, 181, 182, 182, 183, 184,
	185, 185, 186, 187, 187, 188, 189, 189, 190, 190, 191, 192, 192,
	193, 194, 194, 195, 196, 196, 197, 197, 198, 199, 199, 200, 200,
	201, 202, 202, 203, 203, 204, 205, 205, 206, 206, 207, 208, 208,
	209, 209, 210, 210, 211, 212, 212, 213, 213, 214, 214, 215, 215,
	216, 216, 217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222,
	223, 223, 224, 224, 225, 226, 226, 227, 227, 228, 228, 229, 229,
	230, 230, 231, 231, 232, 232, 233, 233, 234, 234, 235, 235, 236,
	236, 237, 237, 238, 238, 238, 239, 239, 240, 240, 241, 241, 242,
	242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 247, 247, 248,
	248, 249, 249, 250, 250, 251, 251, 251, 252, 252, 253, 253, 254,
	254, 255, 255
};
static uint8_t pnm_gamma_max = 255;

void
get_cterm_size(int *cols, int *rows, int ns)
{
	*cols = 80;
	*rows = 24;
	if (cterm != NULL) {
		*cols = cterm->width;
		*rows = cterm->height;
	}
	else {
		get_term_win_size(cols, rows, NULL, NULL, &ns);
	}
}

void
setup_mouse_events(struct mouse_state *ms)
{
	ciomouse_setevents(0);
	if (ms && ((ms->flags & MS_FLAGS_DISABLED) == 0)) {
		switch (ms->mode) {
			case MM_RIP:
				ciomouse_addevent(CIOLIB_BUTTON_1_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_1_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_2_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_2_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_3_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_3_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
				mousepointer(CIOLIB_MOUSEPTR_ARROW);
				return;
			case MM_X10:
				ciomouse_addevent(CIOLIB_BUTTON_1_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_1_CLICK);
				ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
				ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
				ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
				ciomouse_addevent(CIOLIB_BUTTON_2_CLICK);
				ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);
				ciomouse_addevent(CIOLIB_BUTTON_4_PRESS); // For scrollback...
				mousepointer(CIOLIB_MOUSEPTR_ARROW);
				return;
			case MM_NORMAL_TRACKING:
				ciomouse_addevent(CIOLIB_BUTTON_1_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_1_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_2_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_2_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_3_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_3_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_5_PRESS);
				mousepointer(CIOLIB_MOUSEPTR_ARROW);
				return;
			case MM_BUTTON_EVENT_TRACKING:
				ciomouse_addevent(CIOLIB_BUTTON_1_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_1_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_2_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_2_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_3_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_3_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_5_PRESS);
				ciomouse_addevent(CIOLIB_MOUSE_MOVE);
				mousepointer(CIOLIB_MOUSEPTR_ARROW);
				return;
			case MM_ANY_EVENT_TRACKING:
				ciomouse_addevent(CIOLIB_BUTTON_1_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_1_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_2_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_2_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_3_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_3_RELEASE);
				ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
				ciomouse_addevent(CIOLIB_BUTTON_5_PRESS);
				ciomouse_addevent(CIOLIB_MOUSE_MOVE);
				mousepointer(CIOLIB_MOUSEPTR_ARROW);
				return;
			default:
				break;
		}
	}
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_2_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_4_PRESS);
	mousepointer(CIOLIB_MOUSEPTR_BAR);
}

#if defined(__BORLANDC__)
 #pragma argsused
#endif

void
mousedrag(struct vmem_cell *scrollback)
{
	int                   key;
	struct mouse_event    mevent;
	struct vmem_cell     *screen;
	unsigned char        *tscreen;
	struct vmem_cell     *sbuffer;
	int                   sbufsize;
	int                   pos, startpos, endpos, lines;
	int                   outpos;
	char                 *copybuf = NULL;
	char                 *newcopybuf;
	int                   lastchar;
	struct ciolib_screen *savscrn;

	sbufsize = term.width * sizeof(*screen) * term.height;
	screen = malloc(sbufsize);
	sbuffer = malloc(sbufsize);
	tscreen = malloc(term.width * 2 * term.height);
	vmem_gettext(term.x - 1, term.y - 1, term.x + term.width - 2, term.y + term.height - 2, screen);
	gettext(term.x - 1, term.y - 1, term.x + term.width - 2, term.y + term.height - 2, tscreen);
	savscrn = savescreen();
	set_modepalette(palettes[COLOUR_PALETTE]);
	while (1) {
		key = getch();
		if ((key == 0) || (key == 0xe0)) {
			key |= getch() << 8;
			if (key == CIO_KEY_LITERAL_E0)
				key = 0xe0;
		}
		switch (key) {
			case CIO_KEY_MOUSE:
				getmouse(&mevent);
				startpos = ((mevent.starty - 1) * term.width) + (mevent.startx - 1);
				endpos = ((mevent.endy - 1) * term.width) + (mevent.endx - 1);
				if (startpos >= term.width * term.height)
					startpos = term.width * term.height - 1;
				if (endpos >= term.width * term.height)
					endpos = term.width * term.height - 1;
				if (endpos < startpos) {
					pos = endpos;
					endpos = startpos;
					startpos = pos;
				}
				switch (mevent.event) {
					case CIOLIB_BUTTON_1_DRAG_MOVE:
						memcpy(sbuffer, screen, sbufsize);
						for (pos = startpos; pos <= endpos; pos++) {
							if ((sbuffer[pos].legacy_attr & 0x70) != 0x10)
								sbuffer[pos].legacy_attr =
								    (sbuffer[pos].legacy_attr & 0x8F) | 0x10;
							else
								sbuffer[pos].legacy_attr =
								    (sbuffer[pos].legacy_attr & 0x8F) | 0x60;
							if (((sbuffer[pos].legacy_attr & 0x70) >> 4)
							    == (sbuffer[pos].legacy_attr & 0x0F))
								sbuffer[pos].legacy_attr |= 0x08;
							attr2palette(sbuffer[pos].legacy_attr,
							    &sbuffer[pos].fg,
							    &sbuffer[pos].bg);
						}
						vmem_puttext(term.x - 1,
						    term.y - 1,
						    term.x + term.width - 2,
						    term.y + term.height - 2,
						    sbuffer);
						break;
					default:
						lines = abs(mevent.endy - mevent.starty) + 1;
						newcopybuf = realloc(copybuf, (endpos - startpos + 4 + lines * 2) * 4);
						if (newcopybuf)
							copybuf = newcopybuf;
						else
							goto cleanup;
						outpos = 0;
						lastchar = 0;
						for (pos = startpos; pos <= endpos; pos++) {
							size_t   outlen;
							uint8_t *utf8str;
							int cp = conio_fontdata[screen[pos].font].cp;

							if (cp == CIOLIB_PRESTEL && (screen[pos].bg & 0x20000000))
								cp = CIOLIB_PRESTEL_SEP;
							utf8str =
							    cp_to_utf8(cp, (char *)&screen[pos].ch, 1, &outlen);
							if (utf8str == NULL)
								continue;
							memcpy(copybuf + outpos, utf8str, outlen);
							outpos += outlen;
							if ((screen[pos].ch != ' ') && screen[pos].ch)
								lastchar = outpos;
							if ((pos + 1) % term.width == 0) {
								outpos = lastchar;
#ifdef _WIN32
								copybuf[outpos++] = '\r';
#endif
								copybuf[outpos++] = '\n';
								lastchar = outpos;
							}
						}
						copybuf[outpos] = 0;
						copytext(copybuf, strlen(copybuf));
						vmem_puttext(term.x - 1,
						    term.y - 1,
						    term.x + term.width - 2,
						    term.y + term.height - 2,
						    screen);
						goto cleanup;
				}
				break;
			default:
				vmem_puttext(term.x - 1,
				    term.y - 1,
				    term.x + term.width - 2,
				    term.y + term.height - 2,
				    screen);
				ungetch(key);
				goto cleanup;
		}
	}

cleanup:
	free(screen);
	free(sbuffer);
	free(tscreen);
	if (copybuf)
		free(copybuf);
	restorescreen(savscrn);
	freescreen(savscrn);
	return;
}

static struct vmem_cell *status_bar;
size_t status_bar_sz;
bool force_status_update = false;

struct ciolib_screen *
cp437_savescrn(void)
{
	struct ciolib_screen *ret = savescreen();
	set_modepalette(palettes[COLOUR_PALETTE]);
	setfont(0, false, 1);
	setfont(0, false, 2);
	setfont(0, false, 3);
	setfont(0, false, 4);
	setvideoflags(getvideoflags() & (CIOLIB_VIDEO_NOBLINK | CIOLIB_VIDEO_BGBRIGHT));

	return ret;
}

void
update_status(struct bbslist *bbs, int speed, int ooii_mode, bool ata_inv)
{
	size_t i;
	int avail = 30;
	int timeon;
	int rc;
	char nbuf[31]; /*
                        * Room for "Name (Logging) (115300) (DrWy) (OOTerm2) (INV)" and terminator
                        * SAFE and Logging should be possible.
                        */
	char sbuf[10];
	char tobuf[9];
	char fullbuf[81];
	int64_t now = xp_fast_timer64();
	struct mouse_state *ms = cterm->mouse_state_change_cbdata;
	static int64_t lastupd = 0;
	static int oldspeed = 0;
	static uint32_t oldbits = UINT32_MAX;
	uint32_t newbits;

	if (term.nostatus)
		return;

	if (status_bar == NULL || status_bar_sz != term.width) {
		free(status_bar);
		status_bar_sz = term.width;
		status_bar = calloc(status_bar_sz, sizeof(status_bar[0]));
		if (status_bar == NULL) {
			status_bar_sz = 0;
			return;
		}
		for (i = 0; i < status_bar_sz; i++) {
			status_bar[i].fg = 0x80ffff54;
			status_bar[i].bg = 0x800000a8;
			status_bar[i].ch = ' ';
			status_bar[i].font = 0;
			status_bar[i].legacy_attr = 0x1e;
		}
	}

	if (term.width < 80)
		lastupd = now;
	if (ata_inv)
		newbits = 1;
	else
		newbits = 0;
	if (safe_mode)
		newbits |= 0x02;
	if (cterm->log)
		newbits |= 0x04;
	if (cterm->doorway_mode)
		newbits |= 0x08;
	if (ooii_mode >= 1 && ooii_mode <= 3)
		newbits |= (ooii_mode << 4);
	if (force_status_update) {
		newbits |= 0x40;
		force_status_update = false;
	}
	if (rip_did_reinit)
		rip_did_reinit = false;
	else {
		if ((now == lastupd) && (speed == oldspeed) && (oldbits == newbits))
			return;
	}
	lastupd = now;
	oldspeed = speed;
	oldbits = newbits;

	switch(cio_api.mode) {
		case CIOLIB_MODE_CURSES:
		case CIOLIB_MODE_CURSES_IBM:
		case CIOLIB_MODE_ANSI:
			if (term.width >= 80)
				avail = 29;
	}

	if (speed)
		snprintf(sbuf, sizeof(sbuf), " (%d)", speed);
	else
		sbuf[0] = 0;
	rc = snprintf(nbuf, avail + 1, "%s%s%s%s%s%s%s", bbs->name, safe_mode ? " (SAFE)" : ""
	    , cterm->log ? " (Logging)" : "", sbuf, cterm->doorway_mode ? " (DrWy)" : ""
	    , ooii_mode == 1 ? " (OOTerm)" : ooii_mode == 2 ? " (OOTerm1)" : ooii_mode == 3 ? " (OOTerm2)" : ""
	    , ata_inv ? " (INV)" : "");
	if (rc > avail) {
		nbuf[avail - 1] = '.';
		nbuf[avail - 2] = '.';
		nbuf[avail - 3] = '.';
	}
	if (term.width >= 80) {
		timeon = now - bbs->fast_connected;
		if (timeon > 360000)
			timeon = 360000;
		else if (timeon < 0)
			timeon = 0;
		if (timeon > 359999)
			strlcpy(tobuf, "Too Long", sizeof(tobuf));
		else
			snprintf(tobuf, sizeof(tobuf), "%02d:%02d:%02d", timeon / 3600, (timeon / 60) % 60
			    , timeon % 60);
		snprintf(fullbuf, sizeof(fullbuf), " %-*.*s %c %-6.6s %c Connected: %s %c %s", avail, avail, nbuf, 0xb3
		    , conn_types[bbs->conn_type], 0xb3, tobuf, 0xb3
		    , avail == 29 ? "CTRL-S for menu" : ALT_KEY_NAME3CH "-Z for menu ");
	}
	else {
		snprintf(fullbuf, sizeof(fullbuf), " %-*.*s %c %-6.6s ", avail, avail, nbuf, 0xb3
		    , conn_types[bbs->conn_type]);
	}
	if (ms->mode == 0) {
		status_bar[30].ch = ' ';
	}
	for (i = 1; fullbuf[i]; i++) {
		status_bar[i].ch = fullbuf[i];
	}
	if (ms->mode != 0) {
		// TODO: Clear before M?
		//status_bar[29].ch = ' ';
		status_bar[30].ch = 'M';
		if (ms->flags & MS_FLAGS_DISABLED) {
			status_bar[30].fg = 0x80545454;
			status_bar[30].legacy_attr = 0x18;
		}
		else {
			status_bar[30].fg = 0x80ffff54;
			status_bar[30].legacy_attr = 0x1e;
		}
	}
	vmem_puttext(term.x - 1, term.y + term.height - 1, term.x + term.width - 2, term.y + term.height - 1
	    , status_bar);
}

#if defined(_WIN32) && defined(_DEBUG) && defined(DUMP)

void
dump(BYTE *buf, int len)
{
	char   str[128];
	int    i, j;
	size_t slen = 0;

	slen = sprintf(str, "RX: ");
	for (i = 0; i < len; i += j) {
		for (j = 0; i + j < len && j < 32; j++)
			slen += sprintf(str + slen, "%02X ", buf[i + j]);
		OutputDebugString(str);
		slen = sprintf(str, "RX: ");
	}
}

#endif /* if defined(_WIN32) && defined(_DEBUG) && defined(DUMP) */

/* Zmodem Stuff */
int log_level = LOG_INFO;

struct zmodem_cbdata {
	zmodem_t       *zm;
	struct bbslist *bbs;
};

enum {
	ZMODEM_MODE_SEND,
	ZMODEM_MODE_RECV
} zmodem_mode;

static BOOL
zmodem_check_abort(void *vp)
{
	struct zmodem_cbdata *zcb = (struct zmodem_cbdata *)vp;
	zmodem_t             *zm = zcb->zm;
	static time_t         last_check = 0;
	int64_t               now = xp_fast_timer64();
	int                   key;

	if (zm == NULL)
		return true;
	if (quitting) {
		zm->cancelled = true;
		zm->local_abort = true;
		return true;
	}
	if (last_check != now) {
		last_check = now;
		while (kbhit()) {
			switch ((key = getch())) {
				case ESC:
				case CTRL_C:
				case CTRL_X:
					zm->cancelled = true;
					zm->local_abort = true;
					break;
				case 0:
				case 0xe0:
					key |= (getch() << 8);
					if (key == CIO_KEY_MOUSE)
						getmouse(NULL);
					if (key == CIO_KEY_QUIT) {
						if (check_exit(false)) {
							zm->cancelled = true;
							zm->local_abort = true;
						}
					}
					break;
			}
		}
	}
	return zm->cancelled;
}

extern FILE *log_fp;
extern char *log_levels[];

#if defined(__BORLANDC__)
 #pragma argsused
#endif

static int
lputs(void *cbdata, int level, const char *str)
{
	char msg[512];
	int  chars;
	int  oldhold = hold_update;

#if defined(_WIN32) && defined(_DEBUG) && false
	sprintf(msg, "SyncTerm: %s\n", str);
	OutputDebugString(msg);
#endif

	if ((log_fp != NULL) && (level <= log_level)) {
		time_t t = time(NULL);
		fprintf(log_fp, "%.15s %s\n", ctime(&t) + 4, str);
	}

	if (level > LOG_INFO)
		return 0;

        /* Assumes the receive window has been drawn! */
	window(log_ti.winleft, log_ti.wintop, log_ti.winright, log_ti.winbottom);
	gotoxy(log_ti.curx, log_ti.cury);
	textbackground(BLUE);
	switch (level) {
#if 0 // Not possible because of above level > LOG_INFO check
		case LOG_DEBUG:
			textcolor(LIGHTCYAN);
			SAFEPRINTF(msg, "%s\r\n", str);
			break;
#endif
		case LOG_INFO:
			textcolor(WHITE);
			SAFEPRINTF(msg, "%s\r\n", str);
			break;
		case LOG_NOTICE:
			textcolor(YELLOW);
			SAFEPRINTF(msg, "%s\r\n", str);
			break;
		case LOG_WARNING:
			textcolor(LIGHTMAGENTA);
			SAFEPRINTF(msg, "Warning: %s\r\n", str);
			break;
		default:
			textcolor(LIGHTRED);
			SAFEPRINTF(msg, "!ERROR: %s\r\n", str);
			break;
	}
	hold_update = false;
	chars = cputs(msg);
	hold_update = oldhold;
	gettextinfo(&log_ti);

	return chars;
}

static int
lprintf(int level, const char *fmt, ...)
{
	char    sbuf[1024];
	va_list argptr;

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	return lputs(NULL, level, sbuf);
}

#if defined(__BORLANDC__)
 #pragma argsused
#endif

void
zmodem_progress(void *cbdata, int64_t current_pos)
{
	char                  orig[128];
	unsigned              cps;
	int                   l;
	time_t                t;
	time_t                now;
	static time_t         last_progress = 0;
	int                   old_hold = hold_update;
	struct zmodem_cbdata *zcb = (struct zmodem_cbdata *)cbdata;
	zmodem_t             *zm = zcb->zm;
	bool                  growing = false;

	now = time(NULL);
	if (current_pos > zm->current_file_size)
		growing = true;
	if ((now != last_progress) || ((current_pos >= zm->current_file_size) && (growing == false))) {
		zmodem_check_abort(cbdata);
		hold_update = true;
		window(((trans_ti.screenwidth - TRANSFER_WIN_WIDTH) / 2) + 2,
		    ((trans_ti.screenheight - TRANSFER_WIN_HEIGHT) / 2) + 1,
		    ((trans_ti.screenwidth - TRANSFER_WIN_WIDTH) / 2) + TRANSFER_WIN_WIDTH - 2,
		    ((trans_ti.screenheight - TRANSFER_WIN_HEIGHT) / 2) + 5);
		gotoxy(1, 1);
		textattr(LIGHTCYAN | (BLUE << 4));
		t = now - zm->transfer_start_time;
		if (t <= 0)
			t = 1;
		if (zm->transfer_start_pos > current_pos)
			zm->transfer_start_pos = 0;
		if ((cps = (unsigned)((current_pos - zm->transfer_start_pos) / t)) == 0)
			cps = 1;                                 /* cps so far */
		l = (zm->current_file_size - current_pos) / cps; /* remaining transfer est time */
		if (l < 0)
			l = 0;
		cprintf("File (%u of %u): %-.*s",
		    zm->current_file_num, zm->total_files, TRANSFER_WIN_WIDTH - 20, zm->current_file_name);
		clreol();
		cputs("\r\n");
		if (zm->transfer_start_pos)
			sprintf(orig, "From: %" PRId64 "  ", zm->transfer_start_pos);
		else
			orig[0] = 0;
		cprintf("%sByte: %" PRId64 " of %" PRId64 " (%" PRId64 " KB)",
		    orig, current_pos, zm->current_file_size, zm->current_file_size / 1024);
		clreol();
		cputs("\r\n");
		cprintf("Time: %lu:%02lu  ETA: %lu:%02lu  Block: %u/CRC-%u  %u cps"
		    ,
		    (unsigned long)(t / 60L)
		    ,
		    (unsigned long)(t % 60L)
		    ,
		    (unsigned long)(l / 60L)
		    ,
		    (unsigned long)(l % 60L)
		    ,
		    zm->block_size
		    ,
		    zmodem_mode == ZMODEM_MODE_RECV ? (zm->receive_32bit_data ? 32 : 16)
		                                                              : (zm->can_fcs_32 && !zm->want_fcs_16) ? 32 : 16
		    ,
		    cps);
		clreol();
		cputs("\r\n");
		if (zm->current_file_size == 0) {
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH / 2 - 5, "", 100);
			l = 60;
		}
		else {
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH / 2 - 5, "",
			    (long)(((float)current_pos / (float)zm->current_file_size) * 100.0));
			l = (long)(60 * ((float)current_pos / (float)zm->current_file_size));
		}
		cprintf("[%*.*s%*s]", l, l,
		    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
		    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
		    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
		    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
		    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
		    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1",
		    (int)(60 - l), "");
		last_progress = now;
		hold_update = false;
		gotoxy(wherex(), wherey());
		hold_update = old_hold;
	}
}

#if defined(__BORLANDC__)
 #pragma argsused
#endif

unsigned char transfer_buffer[BUFFER_SIZE / 2];
unsigned      transfer_buf_len = 0;

static void
flush_send(void *unused)
{
	int sent;

	sent = conn_send(transfer_buffer, transfer_buf_len, 120 * 1000);
	if (sent < transfer_buf_len) {
		memmove(transfer_buffer, transfer_buffer + sent, transfer_buf_len - sent);
		transfer_buf_len -= sent;
	}
	else {
		transfer_buf_len = 0;
	}
}

static int
send_byte(void *unused, uchar ch, unsigned timeout /* seconds */)
{
	transfer_buffer[transfer_buf_len++] = ch;
	if (transfer_buf_len == sizeof(transfer_buffer))
		flush_send(unused);
	return !(transfer_buf_len < sizeof(transfer_buffer));
}

#if defined(__BORLANDC__)
 #pragma argsused
#endif
BYTE     recv_byte_buffer[BUFFER_SIZE];
unsigned recv_byte_buffer_len = 0;
unsigned recv_byte_buffer_pos = 0;

static void
recv_bytes(unsigned timeout /* Milliseconds */)
{
	if (recv_byte_buffer_len == 0) {
		recv_byte_buffer_len = parse_rip(recv_byte_buffer, 0, sizeof(recv_byte_buffer));
		if (recv_byte_buffer_len == 0) {
			recv_byte_buffer_len = conn_recv_upto(recv_byte_buffer, sizeof(recv_byte_buffer) - 3, timeout);
			if (recv_byte_buffer_len)
				recv_byte_buffer_len =
				    parse_rip(recv_byte_buffer, recv_byte_buffer_len, sizeof(recv_byte_buffer));
		}
	}
}

static int
recv_byte(void *unused, unsigned timeout /* seconds */)
{
	BYTE ch;

	recv_bytes(timeout * 1000);

	if (recv_byte_buffer_len > 0) {
		ch = recv_byte_buffer[recv_byte_buffer_pos++];
		if (recv_byte_buffer_pos == recv_byte_buffer_len)
			recv_byte_buffer_len = recv_byte_buffer_pos = 0;
		return ch;
	}

	return -1;
}

#if defined(__BORLANDC__)
 #pragma argsused
#endif

BOOL
data_waiting(void *unused, unsigned timeout /* seconds */)
{
	bool ret;

	if (recv_byte_buffer_len)
		return true;
	assert_pthread_mutex_lock(&(conn_inbuf.mutex));
	ret = conn_buf_wait_bytes(&conn_inbuf, 1, timeout * 1000) != 0;
	assert_pthread_mutex_unlock(&(conn_inbuf.mutex));
	return ret;
}

size_t
count_data_waiting(void)
{
	recv_bytes(0);
	return recv_byte_buffer_len;
}

void
draw_transfer_window(char *title)
{
	char outline[TRANSFER_WIN_WIDTH * 2];
	char shadow[TRANSFER_WIN_WIDTH * 2]; /* Assumes that width*2 > height * 2 */
	int  i, top, left, old_hold;

	old_hold = hold_update;
	hold_update = true;
	gettextinfo(&trans_ti);
	top = (trans_ti.screenheight - TRANSFER_WIN_HEIGHT) / 2;
	left = (trans_ti.screenwidth - TRANSFER_WIN_WIDTH) / 2;
	window(1, 1, trans_ti.screenwidth, trans_ti.screenheight);

	vmem_gettext(left, top, left + TRANSFER_WIN_WIDTH + 1, top + TRANSFER_WIN_HEIGHT, winbuf);
	memset(outline, YELLOW | (BLUE << 4), sizeof(outline));
	for (i = 2; i < sizeof(outline) - 2; i += 2) {
		outline[i] = (char)0xcd; /* Double horizontal line */
	}
	outline[0] = (char)0xc9;
	outline[sizeof(outline) - 2] = (char)0xbb;
	puttext(left, top, left + TRANSFER_WIN_WIDTH - 1, top, outline);

        /* Title */
	gotoxy(left + 4, top);
	textattr(YELLOW | (BLUE << 4));
	cprintf("\xb5 %*s \xc6", strlen(title), "");
	gotoxy(left + 6, top);
	textattr(WHITE | (BLUE << 4));
	cprintf("%s", title);

	for (i = 2; i < sizeof(outline) - 2; i += 2) {
		outline[i] = (char)0xc4;           /* Single horizontal line */
	}
	outline[0] = (char)0xc7;                   /* 0xcc */
	outline[sizeof(outline) - 2] = (char)0xb6; /* 0xb6 */
	puttext(left, top + 6, left + TRANSFER_WIN_WIDTH - 1, top + 6, outline);

	for (i = 2; i < sizeof(outline) - 2; i += 2) {
		outline[i] = (char)0xcd; /* Double horizontal line */
	}
	outline[0] = (char)0xc8;
	outline[sizeof(outline) - 2] = (char)0xbc;
	puttext(left,
	    top + TRANSFER_WIN_HEIGHT - 1,
	    left + TRANSFER_WIN_WIDTH - 1,
	    top + TRANSFER_WIN_HEIGHT - 1,
	    outline);
	outline[0] = (char)0xba;
	outline[sizeof(outline) - 2] = (char)0xba;
	for (i = 2; i < sizeof(outline) - 2; i += 2)
		outline[i] = ' ';
	for (i = 1; i < 6; i++)
		puttext(left, top + i, left + TRANSFER_WIN_WIDTH - 1, top + i, outline);

/*
 *      for(i=3;i < sizeof(outline) - 2; i+=2) {
 *              outline[i] = LIGHTGRAY | (BLACK << 8);
 *      }
 */
	for (i = 7; i < TRANSFER_WIN_HEIGHT - 1; i++)
		puttext(left, top + i, left + TRANSFER_WIN_WIDTH - 1, top + i, outline);

        /* Title */
	gotoxy(left + TRANSFER_WIN_WIDTH - 20, top + i);
	textattr(YELLOW | (BLUE << 4));
	cprintf("\xb5              \xc6");
	textattr(WHITE | (BLUE << 4));
	gotoxy(left + TRANSFER_WIN_WIDTH - 18, top + i);
	cprintf("ESC to Abort");

        /* Shadow */
	if (uifc.bclr == BLUE) {
		gettext(left + TRANSFER_WIN_WIDTH,
		    top + 1,
		    left + TRANSFER_WIN_WIDTH + 1,
		    top + (TRANSFER_WIN_HEIGHT - 1),
		    shadow);
		for (i = 1; i < sizeof(shadow); i += 2)
			shadow[i] = DARKGRAY;
		puttext(left + TRANSFER_WIN_WIDTH,
		    top + 1,
		    left + TRANSFER_WIN_WIDTH + 1,
		    top + (TRANSFER_WIN_HEIGHT - 1),
		    shadow);
		gettext(left + 2,
		    top + TRANSFER_WIN_HEIGHT,
		    left + TRANSFER_WIN_WIDTH + 1,
		    top + TRANSFER_WIN_HEIGHT,
		    shadow);
		for (i = 1; i < sizeof(shadow); i += 2)
			shadow[i] = DARKGRAY;
		puttext(left + 2,
		    top + TRANSFER_WIN_HEIGHT,
		    left + TRANSFER_WIN_WIDTH + 1,
		    top + TRANSFER_WIN_HEIGHT,
		    shadow);
	}

	window(left + 2, top + 7, left + TRANSFER_WIN_WIDTH - 3, top + TRANSFER_WIN_HEIGHT - 2);
	hold_update = false;
	gotoxy(1, 1);
	hold_update = old_hold;
	gettextinfo(&log_ti);
	_setcursortype(_NOCURSOR);
}

void
erase_transfer_window(void)
{
	vmem_puttext(((trans_ti.screenwidth - TRANSFER_WIN_WIDTH) / 2),
	    ((trans_ti.screenheight - TRANSFER_WIN_HEIGHT) / 2),
	    ((trans_ti.screenwidth - TRANSFER_WIN_WIDTH) / 2) + TRANSFER_WIN_WIDTH + 1,
	    ((trans_ti.screenheight - TRANSFER_WIN_HEIGHT) / 2) + TRANSFER_WIN_HEIGHT,
	    winbuf);
	window(trans_ti.winleft, trans_ti.wintop, trans_ti.winright, trans_ti.winbottom);
	gotoxy(trans_ti.curx, trans_ti.cury);
	textattr(trans_ti.attribute);
	_setcursortype(_NORMALCURSOR);
}
void ascii_upload(FILE *fp);
void raw_upload(FILE *fp);

void
begin_upload(struct bbslist *bbs, bool autozm, int lastch)
{
	char                  str[MAX_PATH * 2 + 1];
	char                  path[MAX_PATH + 1];
	int                   result;
	int                   i;
	FILE                 *fp;
	struct file_pick      fpick;
	char                 *opts[7] = {
		"ZMODEM",
		"YMODEM",
		"XMODEM-1K",
		"XMODEM-128",
		"ASCII",
		"Raw",
		""
	};
	struct  text_info     txtinfo;
	struct ciolib_screen *savscrn;
	struct ciolib_screen *savscrn2;

	if (safe_mode)
		return;

	gettextinfo(&txtinfo);
	suspend_rip(true);
	savscrn = cp437_savescrn();
	savscrn2 = savescreen();

	init_uifc(false, false);
	if (!isdir(bbs->uldir)) {
		SAFEPRINTF(str, "Invalid upload directory: %s", bbs->uldir);
		uifcmsg(str, "An invalid `UploadPath` was specified in the `syncterm.lst` file");
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
		freescreen(savscrn2);
		gotoxy(txtinfo.curx, txtinfo.cury);
		return;
	}
	result = filepick(&uifc, "Upload", &fpick, bbs->uldir, NULL, UIFC_FP_ALLOWENTRY);

	if ((result == -1) || (fpick.files < 1)) {
		check_exit(false);
		filepick_free(&fpick);
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
		freescreen(savscrn2);
		gotoxy(txtinfo.curx, txtinfo.cury);
		return;
	}
	SAFECOPY(path, fpick.selected[0]);
	filepick_free(&fpick);
	restorescreen(savscrn2);
	freescreen(savscrn2);

	if ((fp = fopen(path, "rb")) == NULL) {
		SAFEPRINTF2(str, "Error %d opening %s for read", errno, path);
		uifcmsg("Error opening file", str);
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
		gotoxy(txtinfo.curx, txtinfo.cury);
		return;
	}
	setvbuf(fp, NULL, _IOFBF, 0x10000);

	if (autozm) {
		zmodem_upload(bbs, fp, path);
	}
	else {
		i = 0;
		uifc.helpbuf = "Select Protocol";
		switch (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Protocol", opts)) {
			case 0:
				zmodem_upload(bbs, fp, path);
				break;
			case 1:
				xmodem_upload(bbs, fp, path, YMODEM | SEND, lastch);
				break;
			case 2:
				xmodem_upload(bbs, fp, path, XMODEM | SEND, lastch);
				break;
			case 3:
				xmodem_upload(bbs, fp, path, XMODEM | SEND | XMODEM_128B, lastch);
				break;
			case 4:
				ascii_upload(fp);
				break;
			case 5:
				raw_upload(fp);
				break;
		}
	}
	fclose(fp);
	suspend_rip(false);
	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
	gotoxy(txtinfo.curx, txtinfo.cury);
}

void
begin_download(struct bbslist *bbs)
{
	char                  path[MAX_PATH + 1];
	int                   i;
	char                 *opts[6] = {
		"ZMODEM",
		"YMODEM-g",
		"YMODEM",
		"XMODEM-CRC",
		"XMODEM-CHKSUM",
		""
	};
	struct  text_info     txtinfo;
	int                   old_hold = hold_update;
	struct ciolib_screen *savscrn;

	if (safe_mode)
		return;

	gettextinfo(&txtinfo);
	savscrn = cp437_savescrn();

	init_uifc(false, false);

	i = 0;
	uifc.helpbuf = "Select Protocol";
	hold_update = false;
	suspend_rip(true);
	switch (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Protocol", opts)) {
		case -1:
			check_exit(false);
			break;
		case 0:
			zmodem_download(bbs);
			break;
		case 1:
			xmodem_download(bbs, YMODEM | CRC | GMODE | RECV, NULL);
			break;
		case 2:
			xmodem_download(bbs, YMODEM | CRC | RECV, NULL);
			break;
		case 3:
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Filename", path, sizeof(path), 0) != -1)
				xmodem_download(bbs, XMODEM | CRC | RECV, path);
			break;
		case 4:
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Filename", path, sizeof(path), 0) != -1)
				xmodem_download(bbs, XMODEM | RECV, path);
			break;
	}
	suspend_rip(false);
	hold_update = old_hold;
	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
	gotoxy(txtinfo.curx, txtinfo.cury);
}

#if defined(__BORLANDC__)
 #pragma argsused
#endif

static BOOL
is_connected(void *unused)
{
	if (recv_byte_buffer_len)
		return true;
	return conn_connected();
}

void
raw_upload(FILE *fp)
{
	char buf[1024];
	int  r;
	int  inch;
	char ch[2];

	ch[1] = 0;
	for (;;) {
		r = fread(buf, 1, sizeof(buf), fp);
		if (r)
			conn_send(buf, r, 0);

                /* Note, during RAW uploads, do NOT send ANSI responses and don't
                 * allow speed changes. */
		while ((inch = recv_byte(NULL, 0)) >= 0) {
			ch[0] = inch;
			cterm_write(cterm, ch, 1, NULL, 0, NULL);
		}
		if (r == 0)
			break;
	}
}

void
ascii_upload(FILE *fp)
{
	char  linebuf[1024 + 2]; /* One extra for terminator, one extra for added CR */
	char *p;
	char  ch[2];
	int   inch;
	bool  lastwascr = false;

	ch[1] = 0;
	while (!feof(fp)) {
		if (fgets(linebuf, 1025, fp) != NULL) {
			if ((p = strrchr(linebuf, '\n')) != NULL) {
				if (((p == linebuf) && !lastwascr) || ((p > linebuf) && (*(p - 1) != '\n'))) {
					*p = '\r';
					p++;
					*p = '\n';
					p++;
					*p = 0;
				}
			}
			lastwascr = false;
			if (p != NULL)
				p = strchr(p, 0);
			if ((p != NULL) && (p > linebuf)) {
				if (*(p - 1) == '\r')
					lastwascr = true;
			}
			conn_send(linebuf, strlen(linebuf), 0);
		}

                /* Note, during ASCII uploads, do NOT send ANSI responses and don't
                 * allow speed changes. */
		while ((inch = recv_byte(NULL, 0)) >= 0) {
			ch[0] = inch;
			cterm_write(cterm, ch, 1, NULL, 0, NULL);
		}
	}
}

static void
transfer_complete(bool success, bool was_binary)
{
	int timeout = success ? settings.xfer_success_keypress_timeout : settings.xfer_failure_keypress_timeout;

	if (!was_binary)
		conn_binary_mode_off();
	if (log_fp != NULL)
		fflush(log_fp);

        /* TODO: Make this pretty (countdown timer) and don't delay a second between keyboard polls */
	lprintf(LOG_NOTICE, "Hit any key or wait %u seconds to continue...", timeout);
	while (timeout > 0) {
		if (kbhit()) {
			/* coverity[cond_const:SUPPRESS] */
			if (getch() == (CIO_KEY_QUIT & 0xff)) {
				if ((getch() << 8) == (CIO_KEY_QUIT & 0xff00))
					check_exit(false);
			}
			break;
		}
		timeout--;
		SLEEP(1000);
	}

	erase_transfer_window();
}

void
zmodem_upload(struct bbslist *bbs, FILE *fp, char *path)
{
	bool                 success;
	zmodem_t             zm;
	int64_t              fsize;
	struct zmodem_cbdata cbdata;
	bool                 was_binary = conn_api.binary_mode;

	draw_transfer_window("ZMODEM Upload");

	zmodem_mode = ZMODEM_MODE_SEND;

	cbdata.zm = &zm;
	cbdata.bbs = bbs;
	if (!was_binary)
		conn_binary_mode_on();
	transfer_buf_len = 0;
	zmodem_init(&zm,

            /* cbdata */ &cbdata,
	    lputs, zmodem_progress,
	    send_byte, recv_byte,
	    is_connected,
	    zmodem_check_abort,
	    data_waiting,
	    flush_send);
	zm.log_level = &log_level;

	zm.current_file_num = zm.total_files = 1; /* ToDo: support multi-file/batch uploads */

	fsize = filelength(fileno(fp));

	lprintf(LOG_INFO, "Sending %s (%" PRId64 " KB) via ZMODEM",
	    path, fsize / 1024);

	if ((success = zmodem_send_file(&zm, path, fp,

            /* ZRQINIT? */ true, /* start_time */ NULL, /* sent_bytes */ NULL)) == true)
		zmodem_get_zfin(&zm);

	transfer_complete(success, was_binary);
}

BOOL
zmodem_duplicate_callback(void *cbdata, void *zm_void)
{
	struct  text_info     txtinfo;
	struct ciolib_screen *savscrn;
	bool                  ret = false;
	int                   i;
	char                 *opts[4] = {
		"Overwrite",
		"Choose New Name",
		"Cancel Download",
		NULL
	};
	struct zmodem_cbdata *cb = (struct zmodem_cbdata *)cbdata;
	zmodem_t             *zm = (zmodem_t *)zm_void;
	char                  fpath[MAX_PATH * 2 + 2];
	bool                  loop = true;
	int                   old_hold = hold_update;

	gettextinfo(&txtinfo);
	savscrn = cp437_savescrn();
	window(1, 1, txtinfo.screenwidth, txtinfo.screenheight);
	init_uifc(false, false);
	hold_update = false;

	while (loop) {
		loop = false;
		i = 0;
		uifc.helpbuf = "Duplicate file... choose action\n";
		switch (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Duplicate File Name", opts)) {
			case -1:
				if (check_exit(false)) {
					ret = false;
					break;
				}
				loop = true;
				break;
			case 0: /* Overwrite */
				SAFEPRINTF2(fpath, "%s/%s", cb->bbs->dldir, zm->current_file_name);
				unlink(fpath);
				ret = true;
				break;
			case 1: /* Choose new name */
				uifc.changes = 0;
				uifc.helpbuf = "Duplicate Filename... enter new name";
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "New Filename: ", zm->current_file_name,
				    sizeof(zm->current_file_name) - 1, K_EDIT) == -1) {
					loop = true;
				}
				else {
					if (uifc.changes)
						ret = true;
					else
						loop = true;
				}
				break;
		}
	}

	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
	gotoxy(txtinfo.curx, txtinfo.cury);
	hold_update = old_hold;
	return ret;
}

void
zmodem_download(struct bbslist *bbs)
{
	zmodem_t             zm;
	int                  files_received;
	uint64_t             bytes_received;
	struct zmodem_cbdata cbdata;
	bool                 was_binary = conn_api.binary_mode;

	if (safe_mode)
		return;
	draw_transfer_window("ZMODEM Download");

	zmodem_mode = ZMODEM_MODE_RECV;

	if (!was_binary)
		conn_binary_mode_on();
	cbdata.zm = &zm;
	cbdata.bbs = bbs;
	transfer_buf_len = 0;
	zmodem_init(&zm,

            /* cbdata */ &cbdata,
	    lputs, zmodem_progress,
	    send_byte, recv_byte,
	    is_connected,
	    zmodem_check_abort,
	    data_waiting,
	    flush_send);
	zm.log_level = &log_level;

	zm.duplicate_filename = zmodem_duplicate_callback;

	files_received = zmodem_recv_files(&zm, bbs->dldir, &bytes_received);

	if (files_received > 1)
		lprintf(LOG_INFO, "Received %u files (%" PRId64 " bytes) successfully", files_received, bytes_received);

	transfer_complete(files_received, was_binary);
}

/* End of Zmodem Stuff */

/* X/Y-MODEM stuff */

uchar block[1024]; /* Block buffer                                      */
ulong block_num;   /* Block number                                      */

static BOOL
xmodem_check_abort(void *vp)
{
	xmodem_t     *xm = (xmodem_t *)vp;
	static uint64_t last_check = 0;
	uint64_t      now = xp_fast_timer64();
	int           key;

	if (xm == NULL)
		return false;

	if (quitting) {
		xm->cancelled = true;
		return true;
	}

	if (last_check != now) {
		last_check = now;
		while (kbhit()) {
			switch ((key = getch())) {
				case ESC:
				case CTRL_C:
				case CTRL_X:
					xm->cancelled = true;
					break;
				case 0:
				case 0xe0:
					key |= (getch() << 8);
					if (key == CIO_KEY_MOUSE)
						getmouse(NULL);
					if (key == CIO_KEY_QUIT) {
						if (check_exit(false))
							xm->cancelled = true;
					}
					break;
			}
		}
	}
	return xm->cancelled;
}

/****************************************************************************/

/* Returns the number of blocks required to send len bytes                                      */

/****************************************************************************/
uint64_t
num_blocks(unsigned curr_block, uint64_t offset, uint64_t len, unsigned block_size)
{
	uint64_t blocks;

	len -= offset;
	blocks = len / block_size;
	if (len % block_size)
		blocks++;
	return curr_block + blocks;
}

#if defined(__BORLANDC__)
 #pragma argsused
#endif

void
xmodem_progress(void *cbdata, unsigned block_num, int64_t offset, int64_t fsize, time_t start)
{
	uint64_t      total_blocks;
	unsigned      cps;
	int           i;
	uint64_t      l;
	time_t        t;
	time_t        now;
	static time_t last_progress;
	int           old_hold = hold_update;
	xmodem_t     *xm = (xmodem_t *)cbdata;

	now = time(NULL);
	if ((now - last_progress > 0) || (offset >= fsize)) {
		xmodem_check_abort(cbdata);

		hold_update = true;
		window(((trans_ti.screenwidth - TRANSFER_WIN_WIDTH) / 2) + 2,
		    ((trans_ti.screenheight - TRANSFER_WIN_HEIGHT) / 2) + 1,
		    ((trans_ti.screenwidth - TRANSFER_WIN_WIDTH) / 2) + TRANSFER_WIN_WIDTH - 2,
		    ((trans_ti.screenheight - TRANSFER_WIN_HEIGHT) / 2) + 5);
		gotoxy(1, 1);
		textattr(LIGHTCYAN | (BLUE << 4));
		t = now - start;
		if (t <= 0)
			t = 1;
		if ((cps = (unsigned)(offset / t)) == 0)
			cps = 1;           /* cps so far */
		l = (time_t)(fsize / cps); /* total transfer est time */
		if (t >= l)
			l = 0;
		else
			l -= t;                    /* now, it's est time left */
		if ((*(xm->mode)) & SEND) {
			total_blocks = num_blocks(block_num, offset, fsize, xm->block_size);
			cprintf("Block (%lu%s): %u/%" PRId64 "  Byte: %" PRId64,
			    xm->block_size % 1024L ? xm->block_size : xm->block_size / 1024L,
			    xm->block_size % 1024L ? "" : "K",
			    block_num,
			    total_blocks,
			    offset);
			clreol();
			cputs("\r\n");
			cprintf("Time: %lu:%02lu/%" PRIu64 ":%02" PRIu64 "  %u cps",
			    (ulong)(t / 60L),
			    (ulong)(t % 60L),
			    (ulong)(l / 60L),
			    (ulong)(l % 60L),
			    cps);
			clreol();
			cputs("\r\n");
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH / 2 - 5, "",
			    fsize ? (long)(((float)offset / (float)fsize) * 100.0) : 100);
			i = fsize ? (((float)offset / (float)fsize) * 60.0) : 60;
			if (i < 0)
				i = 0;
			else if (i > 60)
				i = 60;
			cprintf("[%*.*s%*s]", i, i,
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1",
			    60 - i, "");
		}
		else if ((*(xm->mode)) & YMODEM) {
			cprintf("Block (%lu%s): %lu  Byte: %" PRId64,
			    xm->block_size % 1024L ? xm->block_size : xm->block_size / 1024L,
			    xm->block_size % 1024L ? "" : "K",
			    block_num,
			    offset);
			clreol();
			cputs("\r\n");
			cprintf("Time: %lu:%02lu/%lu:%02lu  %u cps",
			    (ulong)(t / 60L),
			    (ulong)(t % 60L),
			    (ulong)(l / 60L),
			    (ulong)(l % 60L),
			    cps);
			clreol();
			cputs("\r\n");
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH / 2 - 5, "",
			    fsize ? (long)(((float)offset / (float)fsize) * 100.0) : 100);
			i = fsize ? (long)(((float)offset / (float)fsize) * 60.0) : 60;
			if (i < 0)
				i = 0;
			else if (i > 60)
				i = 60;
			cprintf("[%*.*s%*s]", i, i,
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
			    "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1",
			    60 - i, "");
		}
		else { /* XModem receive */
			cprintf("Block (%lu%s): %lu  Byte: %" PRId64,
			    xm->block_size % 1024L ? xm->block_size : xm->block_size / 1024L,
			    xm->block_size % 1024L ? "" : "K",
			    block_num,
			    offset);
			clreol();
			cputs("\r\n");
			cprintf("Time: %lu:%02lu  %u cps",
			    (ulong)(t / 60L),
			    (ulong)(t % 60L),
			    cps);
			clreol();
		}
		last_progress = now;
		hold_update = false;
		gotoxy(wherex(), wherey());
		hold_update = old_hold;
	}
}

static int
recv_g(void *cbdata, unsigned timeout)
{
	xmodem_t *xm = (xmodem_t *)cbdata;

	xm->recv_byte = recv_byte;
	return 'G';
}

static int
recv_c(void *cbdata, unsigned timeout)
{
	xmodem_t *xm = (xmodem_t *)cbdata;

	xm->recv_byte = recv_byte;
	return 'C';
}

static int
recv_nak(void *cbdata, unsigned timeout)
{
	xmodem_t *xm = (xmodem_t *)cbdata;

	xm->recv_byte = recv_byte;
	return NAK;
}

void
xmodem_upload(struct bbslist *bbs, FILE *fp, char *path, long mode, int lastch)
{
	bool     success;
	xmodem_t xm;
	int64_t  fsize;
	bool     was_binary = conn_api.binary_mode;

	if (!was_binary)
		conn_binary_mode_on();

	xmodem_init(&xm,

            /* cbdata */ &xm,
	    &mode,
	    lputs,
	    xmodem_progress,
	    send_byte,
	    recv_byte,
	    is_connected,
	    xmodem_check_abort,
	    flush_send);
	xm.log_level = &log_level;
	if (!data_waiting(&xm, 0)) {
		switch (lastch) {
			case 'G':
				xm.recv_byte = recv_g;
				break;
			case 'C':
				xm.recv_byte = recv_c;
				break;
			case NAK:
				xm.recv_byte = recv_nak;
				break;
		}
	}

	if (mode & XMODEM_128B)
		xm.block_size = 128;

	xm.total_files = 1; /* ToDo: support multi-file/batch uploads */

	fsize = filelength(fileno(fp));

	if (mode & XMODEM) {
		if (mode & GMODE)
			draw_transfer_window("XMODEM-g Upload");
		else
			draw_transfer_window("XMODEM Upload");
		lprintf(LOG_INFO, "Sending %s (%" PRId64 " KB) via XMODEM%s",
		    path, fsize / 1024, (mode & GMODE) ? "-g" : "");
	}
	else if (mode & YMODEM) {
		if (mode & GMODE)
			draw_transfer_window("YMODEM-g Upload");
		else
			draw_transfer_window("YMODEM Upload");
		lprintf(LOG_INFO, "Sending %s (%" PRId64 " KB) via YMODEM%s",
		    path, fsize / 1024, (mode & GMODE) ? "-g" : "");
	}
	else {
		if (!was_binary)
			conn_binary_mode_off();
		return;
	}

	if ((success = xmodem_send_file(&xm, path, fp,

            /* start_time */ NULL, /* sent_bytes */ NULL)) == true) {
		if (mode & YMODEM) {
			if (xmodem_get_mode(&xm)) {
				lprintf(LOG_INFO, "Sending YMODEM termination block");

				memset(block, 0, 128); /* send short block for terminator */
				xmodem_put_block(&xm, block, 128 /* block_size */, 0 /* block_num */);
				if (xmodem_get_ack(&xm, /* tries: */ 6, /* block_num: */ 0) != ACK)
					lprintf(LOG_WARNING, "Failed to receive ACK after terminating block");
			}
		}
	}

	transfer_complete(success, was_binary);
}

bool
xmodem_duplicate(xmodem_t *xm, struct bbslist *bbs, char *path, size_t pathsize, char *fname)
{
	struct  text_info     txtinfo;
	struct ciolib_screen *savscrn;
	bool                  ret = false;
	int                   i;
	char                 *opts[4] = {
		"Overwrite",
		"Choose New Name",
		"Cancel Download",
		NULL
	};
	char                  newfname[MAX_PATH + 1];
	bool                  loop = true;
	int                   old_hold = hold_update;

	gettextinfo(&txtinfo);
	savscrn = cp437_savescrn();
	window(1, 1, txtinfo.screenwidth, txtinfo.screenheight);

	init_uifc(false, false);

	hold_update = false;
	while (loop) {
		loop = false;
		i = 0;
		uifc.helpbuf = "Duplicate file... choose action\n";
		switch (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Duplicate File Name", opts)) {
			case -1:
				if (check_exit(false)) {
					ret = false;
					break;
				}
				loop = true;
				break;
			case 0: /* Overwrite */
				unlink(path);
				ret = true;
				break;
			case 1: /* Choose new name */
				uifc.changes = 0;
				uifc.helpbuf = "Duplicate Filename... enter new name";
				SAFECOPY(newfname, getfname(fname));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "New Filename: ", newfname,
				    sizeof(newfname) - 1, K_EDIT) == -1) {
					loop = true;
				}
				else {
					if (uifc.changes) {
						sprintf(path, "%s/%s", bbs->dldir, newfname);
						ret = true;
					}
					else {
						loop = true;
					}
				}
				break;
		}
	}

	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
	hold_update = old_hold;
	return ret;
}

void
xmodem_download(struct bbslist *bbs, long mode, char *path)
{
	xmodem_t xm;

        /* The better to -Wunused you with my dear! */
	char     str[MAX_PATH * 2 + 2];
	char     fname[MAX_PATH + 1];
	int      i = 0;
	uint     errors;
	uint     total_files = 0;
	uint     cps;
	uint     wr;
	bool     success = false;
	long     fmode;
	long     serial_num = -1;
	ulong    tmpftime;
	int64_t  file_bytes = 0, file_bytes_left = 0;
	int64_t  total_bytes = 0;
	FILE    *fp = NULL;
	time_t   t, startfile, ftime = 0;
	int      old_hold = hold_update;
	bool     extra_pass = false;
	bool     was_binary = conn_api.binary_mode;

	if (safe_mode)
		return;

	if (mode & XMODEM) {
		if (mode & GMODE)
			draw_transfer_window("XMODEM-g Download");
		else
			draw_transfer_window("XMODEM Download");
	}
	else if (mode & YMODEM) {
		if (mode & GMODE)
			draw_transfer_window("YMODEM-g Download");
		else
			draw_transfer_window("YMODEM Download");
	}
	else {
		return;
	}

	if (!was_binary)
		conn_binary_mode_on();
	xmodem_init(&xm,

            /* cbdata */ &xm,
	    &mode,
	    lputs,
	    xmodem_progress,
	    send_byte,
	    recv_byte,
	    is_connected,
	    xmodem_check_abort,
	    flush_send);
	xm.log_level = &log_level;
	while (is_connected(NULL)) {
		if (mode & XMODEM) {
			if (isfullpath(path))
				SAFECOPY(str, path);
			else
				SAFEPRINTF2(str, "%s/%s", bbs->dldir, path);
			file_bytes = file_bytes_left = 0x7fffffff;
		}
		else {
			lprintf(LOG_INFO, "Fetching YMODEM header block");
			for (errors = 0; errors <= xm.max_errors && !xm.cancelled; errors++) {
				xmodem_put_nak(&xm, /* expected_block: */ 0);
				i = xmodem_get_block(&xm, block, /* expected_block: */ 0);
				if (i == SUCCESS) {
					if (!(mode & GMODE)) {
						send_byte(&xm, ACK, 10);
						flush_send(&xm);
					}
					break;
				}
				if (extra_pass) {
                                        // This is a hack for sz  0.12.21rc
					lprintf(LOG_INFO,
					    "No YMODEM header block after transfer, assuming end of batch");
					goto end;
				}
				if ((i == NOINP) && (mode & GMODE)) { /* Timeout */
					mode &= ~GMODE;
					lprintf(LOG_WARNING, "Falling back to %s",
					    (mode & CRC) ? "CRC-16" : "Checksum");
				}
				if (i == NOT_YMODEM) {
					lprintf(LOG_WARNING, "Falling back to XMODEM%s", (mode & GMODE) ? "-g" : "");
					mode &= ~(YMODEM);
					mode |= XMODEM | CRC;
					erase_transfer_window();
					hold_update = 0;
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "XMODEM Filename", fname, sizeof(fname),
					    0) == -1) {
						xmodem_cancel(&xm);
						goto end;
					}
					hold_update = old_hold;
					if (mode & GMODE)
						draw_transfer_window("XMODEM Download");
					else
						draw_transfer_window("XMODEM-g Download");
					lprintf(LOG_WARNING, "Falling back to XMODEM%s", (mode & GMODE) ? "-g" : "");
					if (isfullpath(fname))
						SAFECOPY(str, fname);
					else
						SAFEPRINTF2(str, "%s/%s", bbs->dldir, fname);
					file_bytes = file_bytes_left = 0x7fffffff;
					break;
				}
				if ((errors + 1 > xm.max_errors / 3) && mode & CRC && !(mode & GMODE)) {
					lprintf(LOG_NOTICE, "Falling back to 8-bit Checksum mode");
					mode &= ~CRC;
				}
			}
			if ((errors > xm.max_errors) || xm.cancelled) {
				xmodem_cancel(&xm);
				goto end;
			}
			if (i != NOT_YMODEM) {
				if (!block[0]) {
					lprintf(LOG_INFO, "Received YMODEM termination block");
					goto end;
				}
				extra_pass = false;
				file_bytes = total_bytes = 0;
				total_files = 0;
				i = sscanf(((char *)block) + strlen((char *)block) + 1,
				        "%" PRId64 " %lo %lo %lo %d %" PRId64
				        ,
				        &file_bytes  /* file size (decimal) */
				        ,
				        &tmpftime    /* file time (octal unix format) */
				        ,
				        &fmode       /* file mode (not used) */
				        ,
				        &serial_num  /* program serial number */
				        ,
				        &total_files /* remaining files to be sent */
				        ,
				        &total_bytes /* remaining bytes to be sent */
				        );
				ftime = tmpftime;
				lprintf(LOG_DEBUG, "YMODEM header (%u fields): %s", i,
				    block + strlen((char *)block) + 1);
				SAFECOPY(fname, ((char *)block));

				if (!file_bytes)
					file_bytes = 0x7fffffff;
				file_bytes_left = file_bytes;
				if (!total_files)
					total_files = 1;
				if (total_bytes < file_bytes)
					total_bytes = file_bytes;

				lprintf(LOG_DEBUG, "Incoming filename: %.64s ", getfname(fname));

				SAFEPRINTF2(str, "%s/%s", bbs->dldir, getfname(fname));
				lprintf(LOG_INFO, "File size: %" PRId64 " bytes", file_bytes);
				if (total_files > 1) {
					lprintf(LOG_INFO,
					    "Remaining: %" PRId64 " bytes in %u files",
					    total_bytes,
					    total_files);
				}
			}
		}

		lprintf(LOG_DEBUG, "Receiving: %.64s ", str);

		while (fexistcase(str) && !(mode & OVERWRITE)) {
			lprintf(LOG_WARNING, "%s already exists", str);
			if (!xmodem_duplicate(&xm, bbs, str, sizeof(str), getfname(fname))) {
				xmodem_cancel(&xm);
				goto end;
			}
		}
		if ((fp = fopen(str, "wb")) == NULL) {
			lprintf(LOG_ERR, "Error %d creating %s", errno, str);
			xmodem_cancel(&xm);
			goto end;
		}

		if (mode & XMODEM) {
			lprintf(LOG_INFO, "Receiving %s via %s %s",
			    str,
			    mode & GMODE ? "XMODEM-g" : "XMODEM",
			    mode & CRC ? "CRC-16" : "Checksum");
		}
		else {
			lprintf(LOG_INFO, "Receiving %s (%" PRId64 " KB) via %s %s",
			    str,
			    file_bytes / 1024,
			    mode & GMODE ? "YMODEM-g" : "YMODEM",
			    mode & CRC ? "CRC-16" : "Checksum");
		}

		startfile = time(NULL);
		success = false;

		errors = 0;
		block_num = 1;
		if (i != NOT_YMODEM)
			xmodem_put_nak(&xm, block_num);
		while (is_connected(NULL)) {
			xmodem_progress(&xm, block_num, ftello(fp), file_bytes, startfile);
			if (xm.is_cancelled(&xm)) {
				lprintf(LOG_WARNING, "Cancelled locally");
				xmodem_cancel(&xm);
				goto end;
			}
			if (i == NOT_YMODEM)
				i = SUCCESS;
			else
				i = xmodem_get_block(&xm, block, block_num);

			if (i != SUCCESS) {
				if (i == EOT) { /* end of transfer */
					success = true;
					xmodem_put_ack(&xm);
					break;
				}
				if (i == CAN) { /* Cancel */
					xm.cancelled = true;
					break;
				}
				if (mode & GMODE) {
					lprintf(LOG_ERR, "Too many errors (%u)", ++errors);
					goto end;
				}

				if (++errors > xm.max_errors) {
					lprintf(LOG_ERR, "Too many errors (%u)", errors);
					xmodem_cancel(&xm);
					break;
				}
				if ((i != NOT_XMODEM)
				    && (block_num == 1) && (errors > (xm.max_errors / 3)) && mode & CRC
				    && !(mode & GMODE)) {
					lprintf(LOG_NOTICE, "Falling back to 8-bit Checksum mode (error=%d)", i);
					mode &= ~CRC;
				}
				xmodem_put_nak(&xm, block_num);
				continue;
			}
			if (!(mode & GMODE)) {
				send_byte(&xm, ACK, 10);
				flush_send(&xm);
			}
			if (file_bytes_left <= 0L) { /* No more bytes to receive */
				lprintf(LOG_WARNING,
				    "Sender attempted to send more bytes than were specified in header");
				break;
			}
			wr = xm.block_size;
			if (wr > (uint)file_bytes_left)
				wr = (uint)file_bytes_left;
			if (fwrite(block, 1, wr, fp) != wr) {
				lprintf(LOG_ERR, "Error writing %u bytes to file at offset %" PRId64,
				    wr, (int64_t)ftello(fp));
				xmodem_cancel(&xm);
				goto end;
			}
			file_bytes_left -= wr;
			block_num++;
		}

                /* Use correct file size */
		fflush(fp);

		lprintf(LOG_DEBUG, "file_bytes=%" PRId64, file_bytes);
		lprintf(LOG_DEBUG, "file_bytes_left=%" PRId64, file_bytes_left);
		lprintf(LOG_DEBUG, "filelength=%" PRIuOFF, filelength(fileno(fp)));

		if (file_bytes < (ulong)filelength(fileno(fp))) {
			lprintf(LOG_INFO, "Truncating file to %lu bytes", (ulong)file_bytes);
			if(chsize(fileno(fp), (ulong)file_bytes) != 0) /* 4GB limit! */
				lprintf(LOG_ERR, "Error %d (%s) truncating file", errno, strerror(errno));
		}
		else {
			file_bytes = filelength(fileno(fp));
		}
		fclose(fp);
		fp = NULL;

		t = time(NULL) - startfile;
		if (!t)
			t = 1;
		if (success)
			lprintf(LOG_INFO, "Successful - Time: %lu:%02lu  CPS: %lu",
			    (ulong)(t / 60), (ulong)(t % 60), (ulong)(file_bytes / t));
		else
			lprintf(LOG_ERR, "File Transfer %s", xm.cancelled ? "Cancelled" : "Failure");

		if (!(mode & XMODEM) && ftime)
			setfdate(str, ftime);

		if (!success && (file_bytes == 0)) { /* remove 0-byte files */
			if (remove(str) == -1)
				lprintf(LOG_ERR, "Unable to remove empty file %s\n", str);
		}

		if (mode & XMODEM) /* maximum of one file */
			break;
		if ((cps = (unsigned)(file_bytes / t)) == 0)
			cps = 1;
		if (total_files == 0 || --total_files == 0)
			extra_pass = true;
		if (file_bytes < total_bytes)
			total_bytes -= file_bytes;
		else
			total_bytes = 0;
		if ((total_files > 1) && total_bytes) {
			lprintf(LOG_INFO, "Remaining - Time: %lu:%02lu  Files: %u  KBytes: %" PRId64,
			    (total_bytes / cps) / 60,
			    (total_bytes / cps) % 60,
			    total_files,
			    total_bytes / 1024);
		}
	}

end:
	if (fp)
		fclose(fp);
	transfer_complete(success, was_binary);
}

/* End of X/Y-MODEM stuff */

void
music_control(struct bbslist *bbs)
{
	struct  text_info     txtinfo;
	struct ciolib_screen *savscrn;
	int                   i;

	gettextinfo(&txtinfo);
	savscrn = cp437_savescrn();
	init_uifc(false, false);

	i = cterm->music_enable;
	uifc.helpbuf = music_helpbuf;
	if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "ANSI Music Setup", music_names) != -1)
		cterm->music_enable = i;
	else
		check_exit(false);
	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
}

void
font_control(struct bbslist *bbs, struct cterminal *cterm)
{
	struct ciolib_screen *savscrn;
	struct  text_info     txtinfo;
	int                   i, j, k;

	if (safe_mode)
		return;
	gettextinfo(&txtinfo);
	savscrn = cp437_savescrn();
	init_uifc(false, false);

	switch (cio_api.mode) {
		case CIOLIB_MODE_CONIO:
		case CIOLIB_MODE_CONIO_FULLSCREEN:
		case CIOLIB_MODE_CURSES_ASCII:
		case CIOLIB_MODE_CURSES_IBM:
		case CIOLIB_MODE_ANSI:
			uifcmsg("Not supported in this video output mode.",
			    "Font cannot be changed in the current video output mode");
			check_exit(false);
			break;
		default:
			i = j = cterm->altfont[0];
			uifc.helpbuf = "`Font Setup`\n\n"
			    "Change the current font.  Font must support the current video mode:\n\n"
			    "`8x8`  Used for screen modes with 35 or more lines and all C64/C128 modes\n"
			    "`8x14` Used for screen modes with 28 and 34 lines\n"
			    "`8x16` Used for screen modes with 30 lines or fewer than 28 lines.";
			k = uifc.list(WIN_MID | WIN_SAV | WIN_INS, 0, 0, 0, &i, &j, "Font Setup", font_names);
			if (k != -1) {
				if (k & MSK_INS) {
					struct file_pick fpick;

					j = filepick(&uifc, "Load Font From File", &fpick, ".", NULL, 0);
					check_exit(false);

					if ((j != -1) && (fpick.files >= 1))
						loadfont(fpick.selected[0]);
					filepick_free(&fpick);
				}
				else {
					setfont(i, false, 1);
					cterm->altfont[0] = i;
				}
			}
			else {
				check_exit(false);
			}
			break;
	}
	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
}

void
capture_control(struct bbslist *bbs)
{
	struct ciolib_screen *savscrn;
	char                 *cap;
	struct  text_info     txtinfo;
	int                   i, j;

	if (safe_mode)
		return;
	gettextinfo(&txtinfo);
	savscrn = cp437_savescrn();
	cap = (char *)alloca(cterm->height * cterm->width * 2);
	gettext(cterm->x, cterm->y, cterm->x + cterm->width - 1, cterm->y + cterm->height - 1, cap);

	init_uifc(false, false);

	if (!cterm->log) {
		struct file_pick fpick;
		char            *opts[] = {
			"ASCII",
			"Raw",
			"Binary",
			"Binary with SAUCE",
			""
		};

		i = 0;
		uifc.helpbuf = "~ Capture Type ~\n\n"
		    "`ASCII`              ASCII only (no ANSI escape sequences)\n"
		    "`Raw`                Preserves ANSI sequences\n"
		    "`Binary`             Saves current screen in IBM-CGA/BinaryText format\n"
		    "`Binary with SAUCE`  Saves current screen in BinaryText format with SAUCE\n"
		    "\n"
		    "Raw is useful for stealing ANSI screens from other systems.\n"
		    "Don't do that though.  :-)";
		if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Capture Type", opts) != -1) {
			j = filepick(&uifc, "Capture File", &fpick, bbs->dldir, i >= 2 ? "*.bin" : NULL,
			        UIFC_FP_ALLOWENTRY | UIFC_FP_OVERPROMPT);
			check_exit(false);

			if ((j != -1) && (fpick.files >= 1)) {
				if (i >= 2) {
					FILE *fp = fopen(fpick.selected[0], "wb");

					if (fp == NULL) {
						char err[256];

						sprintf(err, "Error %u opening file '%s'", errno, fpick.selected[0]);
						uifc.msg(err);
					}
					else {
						char msg[256];

						uifc.pop("Writing to file");
						fwrite(cap, sizeof(uint8_t), cterm->width * cterm->height * 2, fp);
						if (i > 2) {
							time_t       t = time(NULL);
							struct tm   *tm;
							struct sauce sauce;

							memset(&sauce, 0, sizeof(sauce));
							memcpy(sauce.id, SAUCE_ID, sizeof(sauce.id));
							memcpy(sauce.ver, SAUCE_VERSION, sizeof(sauce.ver));
							memset(sauce.title, ' ', sizeof(sauce.title));
							memset(sauce.author, ' ', sizeof(sauce.author));
							memset(sauce.group, ' ', sizeof(sauce.group));
							if (bbs != NULL) {
								memcpy(sauce.title, bbs->name,
								    MIN(strlen(bbs->name), sizeof(sauce.title)));
								memcpy(sauce.author, bbs->user,
								    MIN(strlen(bbs->user), sizeof(sauce.author)));
							}
							if ((tm = localtime(&t)) != NULL) {
								char tmpstr[SAUCE_LEN_DATE + 1] = {0};
								if (snprintf(tmpstr, sizeof(tmpstr), "%04u%02u%02u",
								    1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday) >= 0) {
									memcpy(sauce.date, tmpstr, SAUCE_LEN_DATE);
								}
							}
							sauce.filesize = LE_INT32(ftell(fp)); // LE
							sauce.datatype = sauce_datatype_bin;
							sauce.filetype = cterm->width / 2;
							if (ciolib_getvideoflags()
							    & (CIOLIB_VIDEO_BGBRIGHT | CIOLIB_VIDEO_NOBLINK))
								sauce.tflags |= sauce_ansiflag_nonblink;

							fputc(SAUCE_SEPARATOR, fp);

                                                        /* No comment block (no comments) */
							fwrite(&sauce.id, sizeof(sauce.id), 1, fp);
							fwrite(&sauce.ver, sizeof(sauce.ver), 1, fp);
							fwrite(&sauce.title, sizeof(sauce.title), 1, fp);
							fwrite(&sauce.author, sizeof(sauce.author), 1, fp);
							fwrite(&sauce.group, sizeof(sauce.group), 1, fp);
							fwrite(&sauce.date, sizeof(sauce.date), 1, fp);
							fwrite(&sauce.filesize, sizeof(sauce.filesize), 1, fp);
							fwrite(&sauce.datatype, sizeof(sauce.datatype), 1, fp);
							fwrite(&sauce.filetype, sizeof(sauce.filetype), 1, fp);
							fwrite(&sauce.tinfo1, sizeof(sauce.tinfo1), 1, fp);
							fwrite(&sauce.tinfo2, sizeof(sauce.tinfo2), 1, fp);
							fwrite(&sauce.tinfo3, sizeof(sauce.tinfo3), 1, fp);
							fwrite(&sauce.tinfo4, sizeof(sauce.tinfo4), 1, fp);
							fwrite(&sauce.comments, sizeof(sauce.comments), 1, fp);
							fwrite(&sauce.tflags, sizeof(sauce.tflags), 1, fp);
							fwrite(&sauce.tinfos, sizeof(sauce.tinfos), 1, fp);
						}
						fclose(fp);
						uifc.pop(NULL);
						sprintf(msg, "Screen saved to '%s'", getfname(fpick.selected[0]));
						uifc.msg(msg);
					}
				}
				else {
					cterm_openlog(cterm, fpick.selected[0], i ? CTERM_LOG_RAW : CTERM_LOG_ASCII);
				}
			}
			filepick_free(&fpick);
		}
		else {
			check_exit(false);
		}
	}
	else {
		if (cterm->log & CTERM_LOG_PAUSED) {
			char *opts[3] = {
				"Unpause",
				"Close"
			};

			i = 0;
			uifc.helpbuf = "`Capture Control`\n\n"
			    "~ Unpause ~ Continues logging\n"
			    "~ Close ~   Closes the log\n\n";
			if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Capture Control", opts) != -1) {
				switch (i) {
					case -1:
						check_exit(false);
						break;
					case 0:
						cterm->log = cterm->log & CTERM_LOG_MASK;
						break;
					case 1:
						cterm_closelog(cterm);
						break;
				}
			}
		}
		else {
			char *opts[3] = {
				"Pause",
				"Close"
			};

			i = 0;
			uifc.helpbuf = "`Capture Control`\n\n"
			    "~ Pause ~ Suspends logging\n"
			    "~ Close ~ Closes the log\n\n";
			if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL, "Capture Control", opts) != -1) {
				switch (i) {
					case -1:
						check_exit(false);
						break;
					case 0:
						cterm->log |= CTERM_LOG_PAUSED;
						break;
					case 1:
						cterm_closelog(cterm);
						break;
				}
			}
		}
	}
	uifcbail();
	restorescreen(savscrn);
	freescreen(savscrn);
}

#define OUTBUF_SIZE 2048

#define WRITE_OUTBUF() \
	if (outbuf_size > 0) { \
		size_t retbuf_len = cterm_write(cterm, outbuf, outbuf_size, (char *)ansi_replybuf, sizeof(ansi_replybuf), &speed); \
		outbuf_size = 0; \
		if (retbuf_len) \
			conn_send(ansi_replybuf, retbuf_len, 0); \
		updated = true; \
	}

int
get_cache_fn_base(struct bbslist *bbs, char *fn, size_t fnsz)
{
	get_syncterm_filename(fn, fnsz, SYNCTERM_PATH_CACHE, false);
	backslash(fn);
	strcat(fn, bbs->name);
	backslash(fn);
	if (!isdir(fn)) {
		if (mkpath(fn))
			return 0;
	}
	if (!isdir(fn))
		return 0;
	return 1;
}

int
get_cache_fn_subdir(struct bbslist *bbs, char *fn, size_t fnsz, const char *subdir)
{
	int ret;

	ret = get_cache_fn_base(bbs, fn, fnsz);
	if (ret == 0)
		return ret;
	strcat(fn, subdir);
	backslash(fn);
	if (!isdir(fn)) {
		if (mkpath(fn))
			return 0;
	}
	if (!isdir(fn))
		return 0;
	return 1;
}

static int
clean_path(char *fn, size_t fnsz)
{
	char *fp;

#ifdef _WIN32
	// Convert all backslashes to slashes...
	for (char *fpc = fn; *fpc; fpc++) {
		if (*fpc == '\\')
			*fpc = '/';
	}
#endif
	fp = _fullpath(NULL, fn, fnsz);
#ifdef _WIN32
	// Convert all backslashes to slashes...
	for (char *fpc = fp; *fpc; fpc++) {
		if (*fpc == '\\')
			*fpc = '/';
	}
#endif
	if ((fp == NULL) || strcmp(fp, fn)) {
		FREE_AND_NULL(fp);
		return 0;
	}
	FREE_AND_NULL(fp);
	return 1;
}

// ============ This section taken from pnmgamma.c ============
/* pnmgamma.c - perform gamma correction on a PNM image
**
** Copyright (C) 1991 by Bill Davidson and Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

static void
buildBt709ToSrgbGamma(const uint8_t maxval) {
    uint8_t const newMaxval = 255;
    double const gammaSrgb = 2.4;
/*----------------------------------------------------------------------------
   Build a gamma table of size maxval+1 for the combination of the
   inverse of ITU Rec BT.709 and the forward SRGB gamma transfer
   functions.  I.e. this converts from Rec 709 to SRGB.

   'gammaSrgb' must be 2.4 for true SRGB.
-----------------------------------------------------------------------------*/
    double const oneOverGamma709  = 0.45;
    double const gamma709         = 1.0 / oneOverGamma709;
    double const oneOverGammaSrgb = 1.0 / gammaSrgb;
    double const normalizer       = 1.0 / maxval;

    if (pnm_gamma_max == maxval)
	return;
    /* This transfer function is linear for sample values 0
       .. maxval*.018 and an exponential for larger sample values.
       The exponential is slightly stretched and translated, though,
       unlike the popular pure exponential gamma transfer function.
    */

    uint8_t const linearCutoff709 = (uint8_t) (maxval * 0.018 + 0.5);
    double const linearCompression709 =
        0.018 / (1.099 * pow(0.018, oneOverGamma709) - 0.099);

    double const linearCutoffSrgb = 0.0031308;
    double const linearExpansionSrgb =
        (1.055 * pow(0.0031308, oneOverGammaSrgb) - 0.055) / 0.0031308;

    int i;

    for (i = 0; i <= maxval; ++i) {
        double const normalized = i * normalizer;
            /* Xel sample value normalized to 0..1 */
        double radiance;
        double srgb;

        if (i < linearCutoff709 / linearCompression709)
            radiance = normalized * linearCompression709;
        else
            radiance = pow((normalized + 0.099) / 1.099, gamma709);

        assert(radiance <= 1.0);

        if (radiance < linearCutoffSrgb * normalizer)
            srgb = radiance * linearExpansionSrgb;
        else
            srgb = 1.055 * pow(normalized, oneOverGammaSrgb) - 0.055;

        assert(srgb <= 1.0);

        pnm_gamma[i] = srgb * newMaxval + 0.5;
    }
    pnm_gamma_max = maxval;
}


// ====================== End of section ======================

bool
is_pbm_whitespace(char c)
{
	switch(c) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			return true;
	}
	return false;
}

bool
read_pbm_char(FILE *f, off_t *lastpos, char *ch)
{
	if (lastpos != NULL) {
		*lastpos = ftello(f);
		if (*lastpos == -1)
			return false;
	}
	if (fread(ch, 1, 1, f) != 1)
		return false;
	return true;
}

bool
skip_pbm_whitespace(FILE *f)
{
	char ch;
	off_t lastpos;
	bool start = true;

	for (;;) {
		if (!read_pbm_char(f, &lastpos, &ch)) {
			return false;
		}
		if (start) {
			if (!is_pbm_whitespace(ch)) {
				return false;
			}
			start = false;
		}
		if (ch == '#') {
			do {
				if (!read_pbm_char(f, &lastpos, &ch)) {
					return false;
				}
			} while (ch != '\r' && ch != '\n');
		}
		if (!is_pbm_whitespace(ch)) {
			if (fseeko(f, lastpos, SEEK_SET) != 0)
				return false;
			return true;
		}
	}
}

uintmax_t
read_pbm_number(FILE *f)
{
	char value[256]; // Should be big enough ;)
	char *endptr;
	int i;
	off_t lastpos;

	for (i = 0; i < sizeof(value) - 1; i++) {
		if (!read_pbm_char(f, &lastpos, &value[i]))
			break;
		if (value[i] < '0' || value[i] > '9') {
			if (i == 0)
				return UINTMAX_MAX;
			value[i] = 0;
			if (fseeko(f, lastpos, SEEK_SET) != 0)
				return UINTMAX_MAX;
			return strtoumax(value, &endptr, 10);
		}
	}
	return UINTMAX_MAX;
}

static bool
read_pbm_text_raster(struct ciolib_mask *ret, size_t sz, FILE *f)
{
	uintmax_t num;
	size_t    i;
	size_t    byte = 0;
	uint8_t   bit = 7;

	memset(ret->bits, 0, (sz + 7) / 8);
	for (i = 0; i < sz; i++) {
		num = read_pbm_number(f);
		if (num > 1)
			return false;
		ret->bits[byte] |= num << bit;
		if (bit == 0)
			bit = 7;
		else
			bit--;
	}
	return true;
}

static bool
read_ppm_any_raster(struct ciolib_pixels *p, size_t sz, uint8_t max, FILE *f, uintmax_t(*readnum)(FILE *))
{
	uintmax_t num;
	size_t    i;
	uint32_t  pdata;

	buildBt709ToSrgbGamma(max);
	for (i = 0; i < sz; i++) {
		pdata = 0x80000000;	// RGB value (anything less is palette)

		// Red
		num = readnum(f);
		if (num > 255)
			return false;
		pdata |= (pnm_gamma[num] << 16);

		// Green
		num = readnum(f);
		if (num > 255)
			return false;
		pdata |= (pnm_gamma[num] << 8);

		// Blue
		num = readnum(f);
		if (num > 255)
			return false;
		pdata |= (pnm_gamma[num] << 0);
		p->pixels[i] = pdata;
	}
	return true;
}

static bool
read_ppm_text_raster(struct ciolib_pixels *p, size_t sz, uint8_t max, FILE *f)
{
	return read_ppm_any_raster(p, sz, max, f, read_pbm_number);
}

static uintmax_t
read_pbm_byte(FILE *f)
{
	uint8_t b;

	if (fread(&b, 1, 1, f) != 1)
		return UINTMAX_MAX;
	return b;
}

static bool
read_ppm_raw_raster(struct ciolib_pixels *p, size_t sz, uint8_t max, FILE *f)
{
	return read_ppm_any_raster(p, sz, max, f, read_pbm_byte);
}

static struct ciolib_pixels *
alloc_ciolib_pixels(uint32_t w, uint32_t h)
{
	struct ciolib_pixels *ret;
	size_t pszo;
	size_t psz;

	pszo = w * h;
	if (h != 0 && pszo / h != w)
		return NULL;
	psz = pszo * sizeof(uint32_t);
	if (psz / sizeof(uint32_t) != pszo)
		return NULL;
	ret = malloc(sizeof(*ret));
	if (ret == NULL)
		return ret;
	ret->width = w;
	ret->height = h;
	ret->pixelsb = NULL;
	if (psz > 0) {
		ret->pixels = malloc(psz);
		if (ret->pixels == NULL) {
			free(ret);
			return NULL;
		}
	}
	else {
		ret->pixels = NULL;
	}
	return ret;
}

static struct ciolib_mask *
alloc_ciolib_mask(uint32_t w, uint32_t h)
{
	struct ciolib_mask *ret;
	size_t psz;

	psz = w * h;
	if (h != 0 && psz / h != w)
		return NULL;
	ret = malloc(sizeof(*ret));
	if (ret == NULL)
		return ret;
	ret->width = w;
	ret->height = h;
	if (psz > 0) {
		ret->bits = malloc((psz + 7) / 8);
		if (ret->bits == NULL) {
			free(ret);
			return NULL;
		}
	}
	else {
		ret->bits = NULL;
	}
	return ret;
}

static void *
read_pbm(const char *fn, bool bitmap)
{
	uintmax_t             width;
	uintmax_t             height;
	uintmax_t             maxval = 0;
	uintmax_t             overflow;
	FILE                 *f = fopen(fn, "rb");
	struct ciolib_mask   *mret = NULL;
	struct ciolib_pixels *pret = NULL;
	size_t                raster_size;
	size_t                raster_bit_size;
	char                  magic[2];
	bool                  b;

	if (f == NULL)
		goto fail;
	if (fread(magic, sizeof(magic), 1, f) != 1)
		goto fail;
	if (magic[0] != 'P')
		goto fail;
	switch (magic[1]) {
		case '1':
		case '4':
			if (!bitmap)
				goto fail;
			break;
		case '3':
		case '6':
			if (bitmap)
				goto fail;
			break;
		default:
			goto fail;
	}

	if (!skip_pbm_whitespace(f))
		goto fail;

	assert(UINTMAX_MAX > UINT32_MAX);
	width = read_pbm_number(f);
	if (width > UINT32_MAX)
		goto fail;

	if (!skip_pbm_whitespace(f))
		goto fail;

	height = read_pbm_number(f);
	if (height > UINT32_MAX)
		goto fail;

	// Check for multiplcation overflow
	overflow = width * height;
	if (width != 0 && overflow / height != width)
		goto fail;
	// Check for type truncation
	raster_size = overflow;
	if (raster_size != overflow)
		goto fail;

	if (magic[1] == '3' || magic[1] == '6') {
		if (!skip_pbm_whitespace(f))
			goto fail;

		maxval = read_pbm_number(f);
		if (maxval == UINTMAX_MAX)
			goto fail;

		if (maxval > 255)
			goto fail;
	}

	if (!skip_pbm_whitespace(f))
		goto fail;

	switch (magic[1]) {
		case '1':
		case '4':
			raster_bit_size = (raster_size + 7) / 8;
			mret = alloc_ciolib_mask(width, height);
			if (mret == NULL)
				goto fail;
			if (magic[1] == '1')
				b = read_pbm_text_raster(mret, raster_size, f);
			else
				b = fread(mret->bits, raster_bit_size, 1, f) == 1;
			if (!b)
				goto fail;
			fclose(f);
			return mret;
		case '3':
		case '6':
			pret = alloc_ciolib_pixels(width, height);
			if (pret == NULL)
				goto fail;
			if (magic[1] == '3')
				b = read_ppm_text_raster(pret, raster_size, maxval, f);
			else
				b = read_ppm_raw_raster(pret, raster_size, maxval, f);
			if (!b)
				goto fail;
			fclose(f);
			return pret;
		default:
			goto fail;
	}

fail:
	freemask(mret);
	freepixels(pret);
	if (f)
		fclose(f);
	return NULL;
}

static void *
b64_decode_alloc(const char *strbuf, size_t slen, size_t *outlen)
{
	void  *ret;
	int    ol;
	size_t sz;

	sz = slen * 3 + 3 / 4 + 1;
	ret = malloc(sz);
	if (!ret)
		return NULL;
	ol = b64_decode(ret, sz, strbuf, slen);
	if (ol == -1) {
		free(ret);
		return NULL;
	}
	if (outlen != NULL)
		*outlen = ol;
	return ret;
}

static void
draw_ppm_str_handler(char *str, size_t slen, char *fn, void *apcd)
{
	struct ciolib_mask   *ctmask = NULL;
	char                 *p;
	char                 *p2;
	void                 *mask = NULL;
	char                 *maskfn = NULL;
	char                 *ppmfn = NULL;
	struct ciolib_pixels *ppmp = NULL;
	unsigned long        *val;
	unsigned long         sx = 0; // Source X to start at
	unsigned long         sy = 0; // Source Y to start at
	unsigned long         sw = 0; // Source width to show
	unsigned long         sh = 0; // Source height to show
	unsigned long         dx = 0; // Destination X to start at
	unsigned long         dy = 0; // Destination Y to start at
	unsigned long         mx = 0; // Mask X to start at
	unsigned long         my = 0; // Mask Y to start at
	unsigned long         mw = 0; // Width of the mask
	unsigned long         mh = 0; // Height of the mask
	size_t                mlen = 0;
	bool                  mbuf = false;

	for (p = str + 18; p && *p == ';'; p = strchr(p + 1, ';')) {
		val = NULL;
		switch (p[1]) {
			case 'S':
				switch (p[2]) {
					case 'X':
						val = &sx;
						break;
					case 'Y':
						val = &sy;
						break;
					case 'W':
						val = &sw;
						break;
					case 'H':
						val = &sh;
						break;
				}
				break;
			case 'D':
				switch (p[2]) {
					case 'X':
						val = &dx;
						break;
					case 'Y':
						val = &dy;
						break;
				}
				break;
			case 'M':
				if (p[2] == 'X') {
					val = &mx;
					break;
				}
				if (p[2] == 'Y') {
					val = &my;
					break;
				}
				if (p[2] == 'W') {
					val = &mw;
					break;
				}
				if (p[2] == 'H') {
					val = &mh;
					break;
				}
				if (strncmp(p + 2, "FILE=", 5) == 0) {
					p2 = strchr(p + 7, ';');
					if (p2 == NULL)
						goto done;
					if (!mbuf)
						freemask(ctmask);
					mbuf = false;
					ctmask = NULL;
					free(mask);
					mask = strndup(p + 7, p2 - p - 7);
					continue; // Avoid val check
				}
				else if (strncmp(p + 2, "ASK=", 4) == 0) {
					p2 = strchr(p + 6, ';');
					if (p2 == NULL)
						goto done;
					FREE_AND_NULL(mask);
					if (!mbuf)
						freemask(ctmask);
					mbuf = false;
					ctmask = alloc_ciolib_mask(0, 0);
					ctmask->bits = b64_decode_alloc(p + 6, p2 - p + 5, &mlen);
					if (ctmask->bits == NULL)
						goto done;
					continue; // Avoid val check
				}
				else if (strncmp(p + 2, "BUF", 3) == 0) {
					freemask(ctmask);
					ctmask = NULL;
					mbuf = true;
					continue; // Avoid val check
				}
				break;
		}
		if (val == NULL || p[3] != '=')
			break;
		*val = strtoul(p + 4, NULL, 10);
	}

	if (asprintf(&ppmfn, "%s%s", fn, p + 1) == -1)
		goto done;
	ppmp = read_pbm(ppmfn, false);
	if (ppmp == NULL)
		goto done;

	if (sw == 0)
		sw = ppmp->width - sx;
	if (sh == 0)
		sh = ppmp->height - sy;

	if (ctmask != NULL) {
		if (mlen < (sw * sh + 7) / 8)
			goto done;
		if (mw == 0)
			mw = sw;
		if (mh == 0)
			mh = sh;
		if (mlen < (mw * mh + 7) / 8)
			goto done;
		ctmask->width = mw;
		ctmask->height = mh;
	}

	if (mask != NULL) {
		if (asprintf(&maskfn, "%s%s", fn, (char*)mask) < 0)
			goto done;
	}

	if (maskfn != NULL) {
		freemask(ctmask);
		ctmask = read_pbm(maskfn, true);
		if (ctmask == NULL)
			goto done;
		if (ctmask->width < sw || ctmask->height < sh)
			goto done;
	}

	if (mbuf)
		ctmask = mask_buffer;

	if (ppmp != NULL)
		setpixels(dx, dy, dx + sw - 1, dy + sh - 1, sx, sy, mx, my, ppmp, ctmask);
done:
	free(mask);
	free(maskfn);
	if (!mbuf)
		freemask(ctmask);
	free(ppmfn);
	freepixels(ppmp);
}

static void
load_ppm_str_handler(char *str, size_t slen, char *fn, void *apcd)
{
	char                 *p;
	char                 *ppmfn = NULL;
	struct ciolib_pixels *ppmp = NULL;
	unsigned long         bufnum = 0;
	unsigned long        *val;

	for (p = str + 18; p && *p == ';'; p = strchr(p + 1, ';')) {
		val = NULL;
		switch (p[1]) {
			case 'B':
				val = &bufnum;
				break;
		}
		if (val == NULL || p[2] != '=')
			break;
		*val = strtoul(p + 3, NULL, 10);
	}

	if (bufnum >= sizeof(pixmap_buffer) / sizeof(pixmap_buffer[0]))
		goto done;

	freepixels(pixmap_buffer[bufnum]);
	pixmap_buffer[bufnum] = NULL;

	if (asprintf(&ppmfn, "%s%s", fn, p + 1) == -1)
		goto done;
	ppmp = read_pbm(ppmfn, false);
	if (ppmp == NULL)
		goto done;
	pixmap_buffer[bufnum] = ppmp;
	free(ppmfn);
	return;

done:
	free(ppmfn);
	freepixels(ppmp);
}

static void
load_pbm_str_handler(char *str, size_t slen, char *fn, void *apcd)
{
	char               *p;
	char               *maskfn = NULL;

	p = str + 18;
	if (asprintf(&maskfn, "%s%s", fn, p + 1) == -1)
		goto done;
	freemask(mask_buffer);
	mask_buffer = read_pbm(maskfn, true);

done:
	free(maskfn);
}

#ifdef WITH_JPEG_XL
static void *
read_jxl(const char *fn)
{
	struct xpmapping *map = xpmap(fn, XPMAP_READ);
	struct ciolib_pixels *pret = NULL;
	uint8_t         *pbuf = NULL;
	uintmax_t        width = 0;
	uintmax_t        height = 0;

	if (map == NULL)
		return map;

	JxlDecoderStatus st;
	JxlBasicInfo info;
	size_t sz = 0;
	JxlPixelFormat format = {
		.num_channels = 3,
		.data_type = JXL_TYPE_UINT8,
		.endianness = JXL_NATIVE_ENDIAN,
		.align = 1
	};
	JxlDecoder *dec = Jxl.DecoderCreate(NULL);
	if (dec == NULL) {
		xpunmap(map);
		return NULL;
	}
#ifdef WITH_JPEG_XL_THREADS
	void *rpr = NULL;
	if (Jxl.status == JXL_STATUS_OK) {
		rpr = Jxl.ResizableParallelRunnerCreate(NULL);
		if (rpr) {
			if (Jxl.DecoderSetParallelRunner(dec, Jxl.ResizableParallelRunner, rpr) != JXL_DEC_SUCCESS) {
				Jxl.ResizableParallelRunnerDestroy(rpr);
				rpr = NULL;
			}
		}
	}
#endif
	if (Jxl.DecoderSetInput(dec, map->addr, map->size) != JXL_DEC_SUCCESS) {
		xpunmap(map);
		Jxl.DecoderDestroy(dec);
		return NULL;
	}
	Jxl.DecoderCloseInput(dec);
	if (Jxl.DecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS) {
		xpunmap(map);
		Jxl.DecoderDestroy(dec);
		return NULL;
	}
	for (bool done = false; !done;) {
		st = Jxl.DecoderProcessInput(dec);
		switch(st) {
			case JXL_DEC_ERROR:
				done = true;
				break;
			case JXL_DEC_BASIC_INFO:
				if (Jxl.DecoderGetBasicInfo(dec, &info) != JXL_DEC_SUCCESS) {
					done = true;
					break;
				}
				width = info.xsize;
				height = info.ysize;
#ifdef WITH_JPEG_XL_THREADS
				if (Jxl.status == JXL_STATUS_OK && rpr) {
					Jxl.ResizableParallelRunnerSetThreads(rpr, Jxl.ResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
				}
#endif
				break;
			case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
				if (width == 0 || height == 0 || width >= 0x40000000 || height >= 0x40000000) {
					done = true;
					break;
				}
				if (Jxl.DecoderImageOutBufferSize(dec, &format, &sz) != JXL_DEC_SUCCESS) {
					done = true;
					break;
				}
				// This may break things, but Coverity wants it.
				free(pbuf);
				pbuf = malloc(sz);
				if (pbuf == NULL) {
					done = true;
					break;
				}
				freepixels(pret);
				pret = alloc_ciolib_pixels(width, height);
				if (pret == NULL) {
					done = true;
					break;
				}
				if (Jxl.DecoderSetImageOutBuffer(dec, &format, pbuf, sz) != JXL_DEC_SUCCESS) {
					done = true;
					break;
				}
				break;
			case JXL_DEC_FULL_IMAGE:
				// Got a single frame... this is not necessarily the whole image though.
				break;
			case JXL_DEC_SUCCESS:
				done = true;
				break;
			default:
				// Everything else is fail.
				done = true;
				break;
		}
	}
	if (st != JXL_DEC_SUCCESS) {
		freepixels(pret);
		pret = NULL;
	}
	else {
		for (size_t i = 0; i < sz; i += 3) {
			size_t j = i / 3;
			pret->pixels[j] = 0x80000000;
			pret->pixels[j] |= pbuf[i] << 16;
			pret->pixels[j] |= pbuf[i+1] << 8;
			pret->pixels[j] |= pbuf[i+2];
		}
	}
	free(pbuf);
#ifdef WITH_JPEG_XL_THREADS
	if (rpr)
		Jxl.ResizableParallelRunnerDestroy(rpr);
#endif
	Jxl.DecoderReleaseInput(dec);
	xpunmap(map);
	Jxl.DecoderDestroy(dec);
	return pret;
}

static void
draw_jxl_str_handler(char *str, size_t slen, char *fn, void *apcd)
{
	struct ciolib_mask   *ctmask = NULL;
	char                 *p;
	char                 *p2;
	void                 *mask = NULL;
	char                 *maskfn = NULL;
	char                 *jxlfn = NULL;
	struct ciolib_pixels *jxlp = NULL;
	unsigned long        *val;
	unsigned long         sx = 0; // Source X to start at
	unsigned long         sy = 0; // Source Y to start at
	unsigned long         sw = 0; // Source width to show
	unsigned long         sh = 0; // Source height to show
	unsigned long         dx = 0; // Destination X to start at
	unsigned long         dy = 0; // Destination Y to start at
	unsigned long         mx = 0; // Mask X to start at
	unsigned long         my = 0; // Mask Y to start at
	unsigned long         mw = 0; // Width of the mask
	unsigned long         mh = 0; // Height of the mask
	size_t                mlen = 0;
	bool                  mbuf = false;

	for (p = str + 18; p && *p == ';'; p = strchr(p + 1, ';')) {
		val = NULL;
		switch (p[1]) {
			case 'S':
				switch (p[2]) {
					case 'X':
						val = &sx;
						break;
					case 'Y':
						val = &sy;
						break;
					case 'W':
						val = &sw;
						break;
					case 'H':
						val = &sh;
						break;
				}
				break;
			case 'D':
				switch (p[2]) {
					case 'X':
						val = &dx;
						break;
					case 'Y':
						val = &dy;
						break;
				}
				break;
			case 'M':
				if (p[2] == 'X') {
					val = &mx;
					break;
				}
				if (p[2] == 'Y') {
					val = &my;
					break;
				}
				if (p[2] == 'W') {
					val = &mw;
					break;
				}
				if (p[2] == 'H') {
					val = &mh;
					break;
				}
				if (strncmp(p + 2, "FILE=", 5) == 0) {
					p2 = strchr(p + 7, ';');
					if (p2 == NULL)
						goto done;
					if (!mbuf)
						freemask(ctmask);
					mbuf = false;
					ctmask = NULL;
					free(mask);
					mask = strndup(p + 7, p2 - p - 7);
					continue; // Avoid val check
				}
				else if (strncmp(p + 2, "ASK=", 4) == 0) {
					p2 = strchr(p + 6, ';');
					if (p2 == NULL)
						goto done;
					FREE_AND_NULL(mask);
					if (!mbuf)
						freemask(ctmask);
					mbuf = false;
					ctmask = alloc_ciolib_mask(0, 0);
					ctmask->bits = b64_decode_alloc(p + 6, p2 - p + 5, &mlen);
					if (ctmask->bits == NULL)
						goto done;
					continue; // Avoid val check
				}
				else if (strncmp(p + 2, "BUF", 3) == 0) {
					freemask(ctmask);
					ctmask = NULL;
					mbuf = true;
					continue; // Avoid val check
				}
				break;
		}
		if (val == NULL || p[3] != '=')
			break;
		*val = strtoul(p + 4, NULL, 10);
	}

	if (asprintf(&jxlfn, "%s%s", fn, p + 1) == -1)
		goto done;
	jxlp = read_jxl(jxlfn);
	if (jxlp == NULL)
		goto done;

	if (sw == 0)
		sw = jxlp->width - sx;
	if (sh == 0)
		sh = jxlp->height - sy;

	if (ctmask != NULL) {
		if (mlen < (sw * sh + 7) / 8)
			goto done;
		if (mw == 0)
			mw = sw;
		if (mh == 0)
			mh = sh;
		if (mlen < (mw * mh + 7) / 8)
			goto done;
		ctmask->width = mw;
		ctmask->height = mh;
	}

	if (mask != NULL) {
		if (asprintf(&maskfn, "%s%s", fn, (char*)mask) < 0)
			goto done;
	}

	if (maskfn != NULL) {
		freemask(ctmask);
		ctmask = read_pbm(maskfn, true);
		if (ctmask == NULL)
			goto done;
		if (ctmask->width < sw || ctmask->height < sh)
			goto done;
	}

	if (mbuf)
		ctmask = mask_buffer;

	if (jxlp != NULL)
		setpixels(dx, dy, dx + sw - 1, dy + sh - 1, sx, sy, mx, my, jxlp, ctmask);
done:
	free(mask);
	free(maskfn);
	if (!mbuf)
		freemask(ctmask);
	free(jxlfn);
	freepixels(jxlp);
}

static void
load_jxl_str_handler(char *str, size_t slen, char *fn, void *apcd)
{
	char                 *p;
	char                 *jxlfn = NULL;
	struct ciolib_pixels *jxlp = NULL;
	unsigned long         bufnum = 0;
	unsigned long        *val;

	for (p = str + 18; p && *p == ';'; p = strchr(p + 1, ';')) {
		val = NULL;
		switch (p[1]) {
			case 'B':
				val = &bufnum;
				break;
		}
		if (val == NULL || p[2] != '=')
			break;
		*val = strtoul(p + 3, NULL, 10);
	}

	if (bufnum >= sizeof(pixmap_buffer) / sizeof(pixmap_buffer[0]))
		goto done;

	freepixels(pixmap_buffer[bufnum]);
	pixmap_buffer[bufnum] = NULL;

	if (asprintf(&jxlfn, "%s%s", fn, p + 1) == -1)
		goto done;
	jxlp = read_jxl(jxlfn);
	if (jxlp == NULL)
		goto done;
	pixmap_buffer[bufnum] = jxlp;
	free(jxlfn);
	return;

done:
	free(jxlfn);
	freepixels(jxlp);
}
#endif

static void
copy_pixmap(char *str, size_t slen, char *fn, void *apcd)
{
	unsigned long  x = 0; // Source X
	unsigned long  y = 0; // Source Y
	unsigned long  w = 0; // Source width
	unsigned long  h = 0; // Source height
	unsigned long *val;
	char          *p;
	unsigned long  bufnum = 0;

	for (p = str + 15; p && *p == ';'; p = strchr(p + 1, ';')) {
		val = NULL;
		switch (p[1]) {
			case 'X':
				val = &x;
				break;
			case 'Y':
				val = &y;
				break;
			case 'W':
				val = &w;
				break;
			case 'H':
				val = &h;
				break;
			case 'B':
				val = &bufnum;
				break;
		}
		if (val == NULL || p[2] != '=')
			return;
		*val = strtol(p + 3, NULL, 10);
	}

	if (bufnum >= sizeof(pixmap_buffer) / sizeof(pixmap_buffer[0]))
		return;

	freepixels(pixmap_buffer[bufnum]);
	pixmap_buffer[bufnum] = NULL;
	if (w == 0 || h == 0) {
		struct text_info ti;
		int vmode;
		gettextinfo(&ti);
		vmode = find_vmode(ti.currmode);
		if (vmode == -1)
			return;
		if (w == 0)
			w = vparams[vmode].xres - x;
		if (h == 0)
			h = vparams[vmode].yres - y;
	}
	pixmap_buffer[bufnum] = getpixels(x, y, x + w - 1, y + h - 1, false);
}

static void
paste_pixmap(char *str, size_t slen, char *fn, void *apcd)
{
	unsigned long       sx = 0; // Source X
	unsigned long       sy = 0; // Source Y
	unsigned long       sw = 0; // Source width
	unsigned long       sh = 0; // Source height
	unsigned long       dx = 0; // Destination X
	unsigned long       dy = 0; // Destination Y
	unsigned long       mx = 0; // Destination X
	unsigned long       my = 0; // Destination Y
	unsigned long       mw = 0; // Width of the mask
	unsigned long       mh = 0; // Height of the mask
	unsigned long       bufnum = 0;
	unsigned long      *val;
	void               *mask = NULL;
	struct ciolib_mask *ctmask = NULL;
	char               *maskfn = NULL;
	char               *p;
	char               *p2;
	size_t              mlen = 0;
	bool                mbuf = false;
	size_t              poff;

	for (p = str + 16; p && *p == ';'; p = strchr(p + 1, ';')) {
		val = NULL;
		poff = 3;
		switch (p[1]) {
			case 'B':
				val = &bufnum;
				poff = 2;
				break;
			case 'S':
				switch (p[2]) {
					case 'X':
						val = &sx;
						break;
					case 'Y':
						val = &sy;
						break;
					case 'W':
						val = &sw;
						break;
					case 'H':
						val = &sh;
						break;
				}
				break;
			case 'D':
				switch (p[2]) {
					case 'X':
						val = &dx;
						break;
					case 'Y':
						val = &dy;
						break;
				}
				break;
			case 'M':
				if (p[2] == 'X') {
					val = &mx;
					break;
				}
				if (p[2] == 'Y') {
					val = &my;
					break;
				}
				if (p[2] == 'W') {
					val = &mw;
					break;
				}
				if (p[2] == 'H') {
					val = &mh;
					break;
				}
				if (strncmp(p + 2, "FILE=", 5) == 0) {
					p2 = strchr(p + 7, ';');
					if (p2 == NULL)
						p2 = strchr(p + 6, 0);
					if (p2 == NULL)
						goto done;
					if (!mbuf)
						freemask(ctmask);
					ctmask = NULL;
					free(mask);
					mask = strndup(p + 7, p2 - p - 7);
					continue; // Avoid val check
				}
				else if (strncmp(p + 2, "ASK=", 4) == 0) {
					p2 = strchr(p + 6, ';');
					if (p2 == NULL)
						p2 = strchr(p + 6, 0);
					if (p2 == NULL)
						goto done;
					FREE_AND_NULL(mask);
					if (!mbuf)
						freemask(ctmask);
					ctmask = alloc_ciolib_mask(0, 0);
					if (ctmask == NULL)
						goto done;
					ctmask->bits = b64_decode_alloc(p + 6, p2 - p + 5, &mlen);
					if (ctmask->bits == NULL)
						goto done;
					continue; // Avoid val check
				}
				else if (strncmp(p + 2, "BUF", 3) == 0) {
					freemask(ctmask);
					ctmask = NULL;
					mbuf = true;
					continue; // Avoid val check
				}
				break;
		}
		if (val == NULL || p[poff] != '=')
			goto done;
		*val = strtoul(p + poff + 1, NULL, 10);
	}

	if (bufnum >= sizeof(pixmap_buffer) / sizeof(pixmap_buffer[0]))
		goto done;

	if (pixmap_buffer[bufnum] == NULL)
		goto done;

	if (sw == 0)
		sw = pixmap_buffer[bufnum]->width - sx;
	if (sh == 0)
		sh = pixmap_buffer[bufnum]->height - sy;

	if (ctmask != NULL) {
		if (mlen < (sw * sh + 7) / 8)
			goto done;
		if (mw == 0)
			mw = sw;
		if (mh == 0)
			mh = sh;
		if (mlen < (mw * mh + 7) / 8)
			goto done;
		ctmask->width = mw;
		ctmask->height = mh;
	}

	if (mask != NULL) {
		if (asprintf(&maskfn, "%s%s", fn, (char*)mask) < 0)
			goto done;
	}

	if (maskfn != NULL) {
		freemask(ctmask);
		ctmask = read_pbm(maskfn, true);
		free(maskfn);
		if (ctmask == NULL)
			goto done;
	}

	if (mbuf)
		ctmask = mask_buffer;

	setpixels(dx, dy, dx + sw - 1, dy + sh - 1, sx, sy, mx, my, pixmap_buffer[bufnum], ctmask);
done:
	free(mask);
	if (!mbuf)
		freemask(ctmask);
}

static void
apc_handler(char *strbuf, size_t slen, char *retbuf, size_t retsize, void *apcd)
{
	char            fn[MAX_PATH + 1];
	char            fn_root[MAX_PATH + 1];
	FILE           *f;
	size_t          rc;
	off_t           off;
	char           *p;
	char           *buf;
	struct bbslist *bbs = apcd;
	glob_t          gl;
	int             i;
	MD5             ctx;
	BYTE            digest[MD5_DIGEST_SIZE];
	unsigned long   slot;

	if (ansi_replybuf[0])
		conn_send(ansi_replybuf, strlen((char *)ansi_replybuf), 0);
	ansi_replybuf[0] = 0;
	if (get_cache_fn_base(bbs, fn_root, sizeof(fn_root)) == 0)
		return;
	strcpy(fn, fn_root);

	if (strncmp(strbuf, "SyncTERM:C;S;", 13) == 0) {
                // Request to save b64 encoded data into the cache directory.
		p = strchr(strbuf + 13, ';');
		if (p == NULL)
			return;
		strncat(fn, strbuf + 13, p - strbuf - 13);
		if (!clean_path(fn, sizeof(fn)))
			return;
		p++;
		buf = b64_decode_alloc(p, slen - (p - strbuf), &rc);
		if (buf == NULL)
			return;
		p = strrchr(fn, '/');
		if (p) {
			*p = 0;
			if (mkpath(fn)) {
				free(buf);
				return;
			}
			*p = '/';
		}
		f = fopen(fn, "wb");
		if (f == NULL) {
			free(buf);
			return;
		}
		fwrite(buf, rc, 1, f);
		free(buf);
		fclose(f);
	}
	else if (strncmp(strbuf, "SyncTERM:C;LoadPPM", 18) == 0) {
                // Load PPM into memory buffer
		load_ppm_str_handler(strbuf, slen, fn, apcd);
	}
	else if (strncmp(strbuf, "SyncTERM:C;LoadPBM", 18) == 0) {
                // Load PPM into memory buffer
		load_pbm_str_handler(strbuf, slen, fn, apcd);
	}
#ifdef WITH_JPEG_XL
	else if (strncmp(strbuf, "SyncTERM:C;LoadJXL", 18) == 0) {
                // Load JPEG XL into memory buffer
		load_jxl_str_handler(strbuf, slen, fn, apcd);
	}
#endif
	else if (strncmp(strbuf, "SyncTERM:C;L", 12) == 0) {
                // Cache list
		if ((strbuf[12] != 0) && (strbuf[12] != ';'))
			return;
		if (!clean_path(fn, sizeof(fn))) {
			conn_send("\x1b_SyncTERM:C;L\n\x1b\\", 17, 0);
			return;
		}
		if (!isdir(fn)) {
			conn_send("\x1b_SyncTERM:C;L\n\x1b\\", 17, 0);
			return;
		}
		if (slen == 12)
			p = "*";
		else
			p = strbuf + 13;
		strcat(fn, p);
		conn_send("\x1b_SyncTERM:C;L\n", 15, 0);
		rc = glob(fn, GLOB_MARK, NULL, &gl);
		if (rc != 0) {
			conn_send("\x1b\\", 2, 0);
			return;
		}
		buf = malloc(1024 * 32);
		if (buf == NULL)
			return;
		for (i = 0; i < gl.gl_pathc; i++) {
                        /* Skip . and .. along with any fuckery */
			if (!clean_path(gl.gl_pathv[i], MAX_PATH))
				continue;
			p = getfname(gl.gl_pathv[i]);
			conn_send(p, strlen(p), 0);
			conn_send("\t", 1, 0);
			f = fopen(gl.gl_pathv[i], "rb");
			if (f) {
				MD5_open(&ctx);
				while (!feof(f)) {
					rc = fread(buf, 1, 1024 * 32, f);
					if (rc > 0)
						MD5_digest(&ctx, buf, rc);
				}
				MD5_close(&ctx, digest);
				fclose(f);
				MD5_hex(buf, digest);
				conn_send(buf, strlen(buf), 0);
			}
			conn_send("\n", 1, 0);
		}
		free(buf);
		conn_send("\x1b\\", 2, 0);
		globfree(&gl);
	}
	else if (strncmp(strbuf, "SyncTERM:C;SetFont;", 19) == 0) {
		slot = strtoul(strbuf + 19, &p, 10);
		if (slot < CONIO_FIRST_FREE_FONT)
			return;
		if (slot > 255)
			return;
		if (*p != ';')
			return;
		p++;
		strcat(fn, p);
		if (!clean_path(fn, sizeof(fn)))
			return;
		if (!fexist(fn))
			return;
		off = flength(fn);
		switch (off) {
			case 10240:
			case 4096:
			case 3584:
			case 2048:
				// Only supported values.
				break;
			default:
				return;
		}
		f = fopen(fn, "rb");
		if (f) {
			buf = malloc(off);
			if (buf == NULL) {
				fclose(f);
				return;
			}
			if (fread(buf, off, 1, f) != 1) {
				fclose(f);
				free(buf);
				return;
			}
			switch (off) {
				case 10240:
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].twelve_by_twenty);
					conio_fontdata[cterm->font_slot].twelve_by_twenty = buf;
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
					conio_fontdata[cterm->font_slot].desc = strdup("Cached Font");
					break;
				case 4096:
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_sixteen);
					conio_fontdata[cterm->font_slot].eight_by_sixteen = buf;
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
					conio_fontdata[cterm->font_slot].desc = strdup("Cached Font");
					break;
				case 3584:
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_fourteen);
					conio_fontdata[cterm->font_slot].eight_by_fourteen = buf;
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
					conio_fontdata[cterm->font_slot].desc = strdup("Cached Font");
					break;
				case 2048:
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_eight);
					conio_fontdata[cterm->font_slot].eight_by_eight = buf;
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
					conio_fontdata[cterm->font_slot].desc = strdup("Cached Font");
					break;
				default:
					free(buf);
			}
			fclose(f);
		}
	}
	else if (strncmp(strbuf, "SyncTERM:C;DrawPPM", 18) == 0) {
                // Request to draw a 255 max PPM file from cache
		//SyncTERM:C;DrawPPM;SX=x;SY=y;SW=w;SH=hDX=x;Dy=y;
		draw_ppm_str_handler(strbuf, slen, fn, apcd);
	}
#ifdef WITH_JPEG_XL
	else if (strncmp(strbuf, "SyncTERM:C;DrawJXL", 18) == 0) {
                // Request to draw a JPEG XL file from cache
		//SyncTERM:C;DrawJXL;SX=x;SY=y;SW=w;SH=hDX=x;Dy=y;
		draw_jxl_str_handler(strbuf, slen, fn, apcd);
	}
#endif
	else if (strncmp(strbuf, "SyncTERM:P;Copy", 15) == 0) {
		copy_pixmap(strbuf, slen, fn, apcd);
	}
	else if (strncmp(strbuf, "SyncTERM:P;Paste", 16) == 0) {
		paste_pixmap(strbuf, slen, fn, apcd);
	}
	else if (strncmp(strbuf, "SyncTERM:Q;JXL", 14) == 0) {
		size_t rlen = strlen(retbuf);

		if (rlen + 9 < retsize) {
#ifdef WITH_JPEG_XL
			if (cio_api.options & CONIO_OPT_SET_PIXEL) {
				switch(Jxl.status) {
					case JXL_STATUS_OK:
					case JXL_STATUS_NOTHREADS:
						memcpy(&retbuf[rlen], "\x1b[=1;1-n", 8);
						break;
					default:
						memcpy(&retbuf[rlen], "\x1b[=1;0-n", 8);
						break;
				}
			}
			else
				memcpy(&retbuf[rlen], "\x1b[=1;0-n", 8);
#else
			memcpy(&retbuf[rlen], "\x1b[=1;0-n", 8);
#endif
		}
	}

	// TODO: Copy PBM mask to memory
}

void
mouse_state_change(int type, int action, void *pms)
{
	struct mouse_state *ms = (struct mouse_state *)pms;

	if (ms->mode == MM_RIP)
		return;
	if (!action) {
		if (type == ms->mode) {
			ms->mode = MM_OFF;
			setup_mouse_events(ms);
		}
		if (type == MS_SGR_SET)
			ms->flags &= ~MS_FLAGS_SGR;
	}
	else {
		switch (type) {
			case MM_X10:
			case MM_NORMAL_TRACKING:
			case MM_BUTTON_EVENT_TRACKING:
			case MM_ANY_EVENT_TRACKING:
				ms->mode = type;
				setup_mouse_events(ms);
				break;
			case MS_SGR_SET:
				ms->flags |= MS_FLAGS_SGR;
		}
	}
}

int
mouse_state_query(int type, void *pms)
{
	struct mouse_state *ms = (struct mouse_state *)pms;

	if (type == MS_SGR_SET)
		return ms->flags & MS_FLAGS_SGR;
	return type == ms->mode;
}

/* Win32 doesn't have ffs()... just use this everywhere. */
static int
my_ffs(int mask)
{
	int bit;

	if (mask == 0)
		return 0;
	for (bit = 1; !(mask & 1); bit++)
		mask = (unsigned int)mask >> 1;
	return bit;
}

static int
fill_mevent(char *buf, size_t bufsz, struct mouse_event *me, struct mouse_state *ms)
{
	int  button;
	int  x = me->startx - cterm->x + 1;
	int  y = me->starty - cterm->y + 1;
	int  bit;
	int  ret;
	bool release;

        // TODO: Get modifier keys too...
	if (me->event == CIOLIB_MOUSE_MOVE) {
		if ((me->kbsm & me->bstate) == 0) {
			if (ms->mode == MM_BUTTON_EVENT_TRACKING)
				return 0;
		}
		bit = my_ffs(me->kbsm & me->bstate);
		if (bit == 0)
			bit = 4;
		button = bit - 1;
		release = false;
	}
	else {
		button = CIOLIB_BUTTON_NUMBER(me->event);
		release = (me->event == CIOLIB_BUTTON_RELEASE(button));
		button--;
	}
	if (button < 0)
		return 0;
	if (button >= 11)
		return 0;
	if (button >= 7)
		button += 121;
	else if (button >= 3)
		button += 61;
	if (me->event == CIOLIB_MOUSE_MOVE)
		button += 32;
	if ((ms->flags & MS_FLAGS_SGR) == 0) {
		if (bufsz < 6)
			return 0;
		if (release)
			button = 3;
		x--;
		y--;
		if (x < 0)
			x = 0;
		if (x > 222)
			x = 222;
		if (y < 0)
			y = 0;
		if (y > 222)
			y = 222;
		buf[0] = '\x1b';
		buf[1] = '[';
		buf[2] = 'M';
		buf[3] = ' ' + button;
		buf[4] = '!' + x;
		buf[5] = '!' + y;
		return 6;
	}
	else {
		ret = snprintf(buf, bufsz, "\x1b[<%d;%d;%d%c", button, x, y, release ? 'm' : 'M');
		if (ret > bufsz)
			return 0;
		return ret;
	}
}

void
do_paste(void)
{
	uint8_t *p;
	uint8_t *p2;
	int      oldfont;

	p = (unsigned char *)getcliptext();
	if (p != NULL) {
		p2 = p;
		oldfont = getfont(1);
		setfont(cterm->altfont[0], false, 1);
		p = (unsigned char *)utf8_to_cp(getcodepage(), p, '\x00', strlen((char *)p), NULL);
		setfont(oldfont, false, 1);
		free(p2);
		if (p != NULL) {
			if (cterm->extattr & CTERM_EXTATTR_BRACKETPASTE)
				conn_send("\x1b[200~", 6, 0);
			for (p2 = p; *p2; p2++) {
				if (*p2 == '\n') {
                                        /* If previous char was not \r, send a \r */
					if ((p2 == p) || (*(p2 - 1) != '\r'))
						conn_send("\r", 1, 0);
				}
				else if (*p2 == '\x1b') {
					// If we're using bracked paste, strip paste end sequence
					// TODO: Do we generally want to disable all ESC chars?
					if (strcmp((char*)p2, "\x1b[201~") == 0)
						p2 += 5;
					else
						conn_send(p2, 1, 0);
				}
				else {
					conn_send(p2, 1, 0);
				}
			}
			if (cterm->extattr & CTERM_EXTATTR_BRACKETPASTE)
				conn_send("\x1b[201~", 6, 0);
			free(p);
		}
	}
}

void
send_login(struct bbslist *bbs) {
	const size_t userlen = strlen(bbs->user);
	const size_t passlen = strlen(bbs->password);
	const size_t syspasslen = strlen(bbs->syspass);
	const size_t derbufsz = userlen + passlen + syspasslen + 3 + 1;
	size_t derbufpos = 0;
	char *derbuf = malloc(derbufsz);
	const char enter = (cterm->emulation == CTERM_EMULATION_ATASCII ? '\x9b' : '\r');

	if ((bbs->conn_type != CONN_TYPE_RLOGIN)
	    && (bbs->conn_type != CONN_TYPE_RLOGIN_REVERSED)
	    && (bbs->conn_type != CONN_TYPE_SSH)) {
		if (bbs->conn_type != CONN_TYPE_SSHNA) {
			if (bbs->user[0]) {
				memcpy(&derbuf[derbufpos], bbs->user, userlen);
				derbufpos += userlen;
				derbuf[derbufpos++] = enter;
				derbuf[derbufpos] = 0;
			}
		}
		if (bbs->password[0]) {
			memcpy(&derbuf[derbufpos], bbs->password, passlen);
			derbufpos += passlen;
			derbuf[derbufpos++] = enter;
			derbuf[derbufpos] = 0;
		}
	}
	if (bbs->syspass[0]) {
		memcpy(&derbuf[derbufpos], bbs->syspass, syspasslen);
		derbufpos += syspasslen;
		derbuf[derbufpos++] = enter;
		derbuf[derbufpos] = 0;
	}
	if (derbufpos)
		conn_send(derbuf, derbufpos, 0);
	free(derbuf);
}

static void
normalize_entry(struct bbslist *bbs)
{
	cterm_emulation_t emu = get_emulation(bbs);

	switch (emu) {
		case CTERM_EMULATION_ANSI_BBS:
			break;
		default:
			bbs->rip = RIP_VERSION_NONE;
			bbs->force_lcf = false;
			bbs->yellow_is_yellow = false;
			break;
	}
}

static void
finish_scrollback(void)
{
	scrollback_buf = cterm->scrollback;
	scrollback_cols = cterm->backwidth;
	// TODO: Set scrollback_mode here?
	if (cterm->scrollback != NULL) {
		cterm_clearscreen(cterm, cterm->attr); /* Clear screen into
							* scrollback */

		// Now make the scrollback a linear buffer instead of a ring buffer
		if (cterm->backstart) {
			struct vmem_cell     *bottom;
			int topsz = cterm->backlines - cterm->backstart;

			bottom = malloc(cterm->backwidth * cterm->backstart * sizeof(*bottom));
			if (bottom) {
				memcpy(bottom, cterm->scrollback, cterm->backwidth * cterm->backstart * sizeof(*bottom));
				memmove(cterm->scrollback, cterm->scrollback + cterm->backwidth * cterm->backstart, cterm->backwidth * topsz * sizeof(*cterm->scrollback));
				memcpy(cterm->scrollback + cterm->backwidth * topsz, bottom, cterm->backwidth * cterm->backstart * sizeof(*bottom));
				free(bottom);
				scrollback_lines = cterm->backlines;
			}
			else {
				memmove(cterm->scrollback, cterm->scrollback + cterm->backwidth * cterm->backstart, cterm->backwidth * topsz * sizeof(*cterm->scrollback));
				scrollback_lines = topsz;
			}
		}
		else {
			scrollback_lines = cterm->backpos;
		}
	}
	else {
		scrollback_lines = 0;
	}
}

bool
doterm(struct bbslist *bbs)
{
	unsigned char     ch[2];
	char              mouse_buf[64];
	unsigned char     outbuf[OUTBUF_SIZE];
	size_t            outbuf_size = 0;
	int               key;
	int               i, j;
	struct vmem_cell *vc;
	BYTE              zrqinit[] = {ZDLE, ZHEX, '0', '0', 0};  /* for Zmodem auto-downloads */
	BYTE              zrinit[] = {ZDLE, ZHEX, '0', '1', 0};   /* for Zmodem auto-uploads */
	BYTE              zrqbuf[sizeof(zrqinit)];
	size_t zrqlen;
	int               inch = NOINP;
	long double       nextchar = 0;
	long double       lastchar = 0;
	long double       thischar = 0;
	int               speed;
	int               oldmc;
	int               updated = false;
	bool              sleep;
	size_t            remain;
	struct text_info  txtinfo;

#ifndef WITHOUT_OOII
	BYTE              ooii_buf[256];
	size_t ooii_buf_len;
	BYTE              ooii_init1[] =
	    "\xdb\b \xdb\b \xdb\b[\xdb\b[\xdb\b \xdb\bM\xdb\ba\xdb\bi\xdb\bn\xdb\bt\xdb\be\xdb\bn\xdb\ba\xdb\bn\xdb\bc\xdb\be\xdb\b \xdb\bC\xdb\bo\xdb\bm\xdb\bp\xdb\bl\xdb\be\xdb\bt\xdb\be\xdb\b \xdb\b]\xdb\b]\xdb\b \b\r\n\r\n\r\n\x1b[0;0;36mDo you have the Overkill Ansiterm installed? (y/N)  \xe9 ";            /*
                                                                                                                                                                                                                                                                                                                          *
                                                                                                                                                                                                                                                                                                                          * for
                                                                                                                                                                                                                                                                                                                          *
                                                                                                                                                                                                                                                                                                                          * OOII
                                                                                                                                                                                                                                                                                                                          *
                                                                                                                                                                                                                                                                                                                          * auto-enable
                                                                                                                                                                                                                                                                                                                          */
	BYTE ooii_init2[] =
	    "\xdb\b \xdb\b \xdb\b[\xdb\b[\xdb\b \xdb\bM\xdb\ba\xdb\bi\xdb\bn\xdb\bt\xdb\be\xdb\bn\xdb\ba\xdb\bn\xdb\bc\xdb\be\xdb\b \xdb\bC\xdb\bo\xdb\bm\xdb\bp\xdb\bl\xdb\be\xdb\bt\xdb\be\xdb\b \xdb\b]\xdb\b]\xdb\b \b\r\n\r\n\x1b[0m\x1b[2J\r\n\r\n\x1b[0;1;30mHX Force retinal scan in progress ... \x1b[0;0;30m"; /*
                                                                                                                                                                                                                                                                                                                          *
                                                                                                                                                                                                                                                                                                                          * for
                                                                                                                                                                                                                                                                                                                          *
                                                                                                                                                                                                                                                                                                                          * OOII
                                                                                                                                                                                                                                                                                                                          *
                                                                                                                                                                                                                                                                                                                          * auto-enable
                                                                                                                                                                                                                                                                                                                          */
#endif
	int                ooii_mode = 0;
	recv_byte_buffer_len = recv_byte_buffer_pos = 0;
	struct mouse_state ms = {0};
	int                speedwatch = 0;
	bool atascii_inverse = false;

	normalize_entry(bbs);
	freepixels(pixmap_buffer[0]);
	freepixels(pixmap_buffer[1]);
	pixmap_buffer[0] = NULL;
	pixmap_buffer[1] = NULL;
	freemask(mask_buffer);
	mask_buffer = NULL;
	gettextinfo(&txtinfo);
	if ((bbs->conn_type == CONN_TYPE_SERIAL) || (bbs->conn_type == CONN_TYPE_SERIAL_NORTS))
		speed = 0;
	else
		speed = bbs->bpsrate;
	log_level = bbs->xfer_loglevel;
	conn_api.log_level = bbs->telnet_loglevel;
	vc = realloc(scrollback_buf, term.width * sizeof(*vc) * settings.backlines);
	if (vc != NULL) {
		scrollback_buf = vc;
		memset(scrollback_buf, 0, term.width * sizeof(*vc) * settings.backlines);
	}
	else {
		FREE_AND_NULL(scrollback_buf);
	}
	scrollback_mode = txtinfo.currmode;
	cterm = cterm_init(term.height,
	        term.width,
	        term.x - 1,
	        term.y - 1,
	        settings.backlines,
	        term.width,
	        scrollback_buf,
	        get_emulation(bbs));
	if (!cterm)
		return false;
	if (bbs->palette_size > 0 && (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)) {
		uint32_t np[16];
		int vm = find_vmode(screen_to_ciolib(bbs->screen_mode));
		if (vm != -1) {
			switch (vparams[vm].palette) {
				case MONO_PALETTE:
				case ATARI_PALETTE:
					np[0] = bbs->palette[0];
					for (i = 1; i < 16; i++)
						np[i] = bbs->palette[1];
					break;
				case GREYSCALE_PALETTE:
					np[0] = bbs->palette[0];
					for (i = 1; i < 8; i++)
						np[i] = bbs->palette[1];
					np[8] = bbs->palette[2];
					for (i = 9; i < 16; i++)
						np[i] = bbs->palette[3];
					break;
				case PRESTEL_PALETTE:
					for (i = 0; i < 8; i++) {
						np[i] = bbs->palette[i];
						np[i+8] = bbs->palette[i];
					}
					break;
				case ATARI_PALETTE_4:
					for (i = 0; i < 4; i++) {
						np[i] = bbs->palette[i];
						np[i+4] = bbs->palette[i];
						np[i+8] = bbs->palette[i];
						np[i+12] = bbs->palette[i];
					}
					break;
				case ATARI_PALETTE_2:
					for (i = 0; i < 2; i++) {
						np[i] = bbs->palette[i];
						np[i+2] = bbs->palette[i];
						np[i+4] = bbs->palette[i];
						np[i+6] = bbs->palette[i];
						np[i+8] = bbs->palette[i];
						np[i+10] = bbs->palette[i];
						np[i+12] = bbs->palette[i];
						np[i+14] = bbs->palette[i];
					}
					break;
			}
			/*
			 * TODO: Doing it this way won't last through a cterm_reset(), which
			 *       can be triggered from ANSI.
			 */
			for (i = 0; i < 16; i++) {
				uint32_t op[16];
				get_modepalette(op);
				setpalette(op[i] + 16,
					((np[i] & 0x00FF0000) >> 8) | ((np[i] & 0x00FF0000) >> 16),
					((np[i] & 0x0000FF00)) | ((np[i] & 0x0000FF00) >> 8),
					((np[i] & 0x000000FF) << 8) | ((np[i] & 0x000000FF)));
			}
		}
	}
	if (bbs->force_lcf)
		cterm->last_column_flag = (CTERM_LCF_FORCED | CTERM_LCF_ENABLED);
	cterm->apc_handler = apc_handler;
	cterm->apc_handler_data = bbs;
	cterm->mouse_state_change = mouse_state_change;
	cterm->mouse_state_change_cbdata = &ms;
	cterm->mouse_state_query = mouse_state_query;
	cterm->mouse_state_query_cbdata = &ms;
	cterm->music_enable = bbs->music;
	ch[1] = 0;
	zrqbuf[0] = 0;
	zrqlen = 0;
#ifndef WITHOUT_OOII
	ooii_buf[0] = 0;
	ooii_buf_len = 0;
#endif
#ifdef WITH_JPEG_XL
	if (cio_api.options & CONIO_OPT_SET_PIXEL)
		load_jxl_funcs();
#endif

        /* Main input loop */
	oldmc = hold_update;
	showmouse();
	init_rip(bbs);
	if (bbs->rip)
		ms.mode = MM_RIP;
	setup_mouse_events(&ms);
	force_status_update = true;
	for (; !quitting;) {
		hold_update = true;
		sleep = true;
		if (!term.nostatus) {
			update_status(bbs,
			    (bbs->conn_type == CONN_TYPE_SERIAL || bbs->conn_type == CONN_TYPE_SERIAL_NORTS) ? bbs->bpsrate : speed,
			    ooii_mode, atascii_inverse);
		}
		for (remain = count_data_waiting() /* Hack for connection check */ + (!is_connected(NULL)); remain;
		    remain--) {
			if (speed)
				thischar = xp_timer();

			if ((!speed) || (thischar < lastchar) /* Wrapped */ || (thischar >= nextchar)) {
                                /* Get remote input */
				inch = recv_byte(NULL, 0);

				switch (inch) {
					case -1:
						if (!is_connected(NULL)) {
							WRITE_OUTBUF();
							hold_update = oldmc;
							if (!bbs->hidepopups)
								uifcmsg("Disconnected",
								    "`Disconnected`\n\nRemote host dropped connection");
							check_exit(false);
							finish_scrollback();
							cterm_end(cterm, 0);
							cterm = NULL;
							// TODO: Do this before the popup to avoid being rude...
							conn_close();
							hidemouse();
							return false;
						}
						break;
					default:
						sleep = false;
						if (speed) {
							lastchar = xp_timer();
							nextchar = lastchar + 1 / (long double)(speed / 10);
						}

						switch (speedwatch) {
							case 0:
								if (inch == '\x1b')
									speedwatch = 1;
								break;
							case 1:
								if (inch == '[')
									speedwatch = 2;
								else
									speedwatch = 0;
								break;
							case 2:
								if ((inch == '0') || (inch == '1'))
									speedwatch = 3;
								else
									speedwatch = 0;
								break;
							case 3:
								if (inch == ';')
									speedwatch = 4;
								else
									speedwatch = 0;
								break;
							case 4:
								if ((inch >= '0') && (inch <= '9'))
									break;
								if (inch == '*')
									speedwatch = 5;
								else
									speedwatch = 0;
								break;
							case 5:
								if (inch == 'r')
									remain = 1;
								speedwatch = 0;
								break;
						}
						if ((inch == zrqinit[zrqlen]) || (inch == zrinit[zrqlen])) {
							zrqbuf[zrqlen] = inch;
							zrqbuf[++zrqlen] = 0;
							if (zrqlen == sizeof(zrqinit) - 1) {
                                                                /* Have full sequence (Assumes
                                                                 * zrinit and zrqinit are same
                                                                 * length */
								WRITE_OUTBUF();
								suspend_rip(true);
								if (!strcmp((char *)zrqbuf, (char *)zrqinit)) {
									struct ciolib_screen *savscrn = cp437_savescrn();
									zmodem_download(bbs);
									restorescreen(savscrn);
									freescreen(savscrn);
								}
								else
									begin_upload(bbs, true, inch);
								setup_mouse_events(&ms);
								suspend_rip(false);
								zrqbuf[0] = 0;
								zrqlen = 0;
								remain = 1;
							}
						}
						else {
							zrqbuf[0] = 0;
							zrqlen = 0;
						}
#ifndef WITHOUT_OOII
						if (ooii_mode) {
							if (ooii_buf[0] == 0) {
								if (inch == 0xab) {
									ooii_buf[ooii_buf_len++] = inch;
									ooii_buf[ooii_buf_len] = 0;
									continue;
								}
							}
							else { /* Already have the start of the sequence */
								if (ooii_buf_len + 1 >= sizeof(ooii_buf))
									ooii_buf_len--;
								ooii_buf[ooii_buf_len++] = inch;
								ooii_buf[ooii_buf_len] = 0;
								if (inch == '|') {
									WRITE_OUTBUF();
									if (handle_ooii_code(ooii_buf, &ooii_mode,
									    (unsigned char *)ansi_replybuf,
									    sizeof(ansi_replybuf))) {
										ooii_mode = 0;
										xptone_close();
									}
									if (ansi_replybuf[0])
										conn_send(ansi_replybuf,
										    strlen((char *)ansi_replybuf), 0);
									ooii_buf[0] = 0;
									ooii_buf_len = 0;
								}
								continue;
							}
						}
						else {
							if (inch == ooii_init1[ooii_buf_len]) {
								ooii_buf[ooii_buf_len++] = inch;
								ooii_buf[ooii_buf_len] = 0;
								if (ooii_init1[ooii_buf_len] == 0) {
									if (strcmp((char *)ooii_buf,
									    (char *)ooii_init1) == 0) {
										ooii_mode = 1;
										xptone_open();
									}
									ooii_buf[0] = 0;
									ooii_buf_len = 0;
								}
							}
							else if (inch == ooii_init2[ooii_buf_len]) {
								ooii_buf[ooii_buf_len++] = inch;
								ooii_buf[ooii_buf_len] = 0;
								if (ooii_init2[ooii_buf_len] == 0) {
									if (strcmp((char *)ooii_buf,
									    (char *)ooii_init2) == 0) {
										ooii_mode = 2;
										xptone_open();
									}
									ooii_buf[0] = 0;
									ooii_buf_len = 0;
								}
							}
							else {
								ooii_buf[0] = 0;
								ooii_buf_len = 0;
							}
						}
#endif /* ifndef WITHOUT_OOII */
						if (outbuf_size >= sizeof(outbuf))
							WRITE_OUTBUF();
						outbuf[outbuf_size++] = inch;
						continue;
				}
			}
			else {
				if (speed)
					sleep = false;
				break;
			}
		}
		WRITE_OUTBUF();
		if (updated) {
			hold_update = false;
			gotoxy(wherex(), wherey());
		}
		hold_update = oldmc;

                /* Get local input */
		while (quitting || rip_kbhit()) {
			sleep = false;
			struct mouse_event mevent;

			updated = true;
			gotoxy(wherex(), wherey());
			if (quitting) {
				key = CIO_KEY_QUIT;
			}
			else {
				key = rip_getch();
				if (key == -1)
					continue;
				if (key > 0xff) {
					if (cterm->doorway_mode && ((key & 0xff) == 0) && (key != 0x2c00) /* ALT-Z */) {
						ch[0] = 0;
						ch[1] = key >> 8;
						conn_send(ch, 2, 0);
						continue;
					}
				}
			}

                        /*
                         * These keys are SyncTERM control keys
                         * key is set to zero if consumed
                         */
			switch (key) {
				case CIO_KEY_MOUSE:
					getmouse(&mevent);
					switch (mevent.event) {
						case CIOLIB_BUTTON_1_PRESS:
						case CIOLIB_BUTTON_1_RELEASE:
						case CIOLIB_BUTTON_2_PRESS:
						case CIOLIB_BUTTON_2_RELEASE:
						case CIOLIB_BUTTON_3_PRESS:
						case CIOLIB_BUTTON_3_RELEASE:
						case CIOLIB_MOUSE_MOVE:
						case CIOLIB_BUTTON_1_CLICK:
							conn_send(mouse_buf,
							    fill_mevent(mouse_buf, sizeof(mouse_buf), &mevent, &ms), 0);
							break;
						case CIOLIB_BUTTON_4_PRESS:
						case CIOLIB_BUTTON_5_PRESS:
							if ((ms.mode != 9) && (ms.mode != 0)) {
								conn_send(mouse_buf,
								    fill_mevent(mouse_buf, sizeof(mouse_buf), &mevent,
								    &ms), 0);
								break;
							}
							viewscroll();
							setup_mouse_events(&ms);
							break;
						case CIOLIB_BUTTON_1_DRAG_START:
							mousedrag(scrollback_buf);
							break;
						case CIOLIB_BUTTON_2_CLICK:
						case CIOLIB_BUTTON_3_CLICK:
							if (ms.mode == 9) {
								conn_send(mouse_buf,
								    fill_mevent(mouse_buf, sizeof(mouse_buf), &mevent,
								    &ms), 0);
							}
							else {
								do_paste();
							}
							break;
					}

					key = 0;
					break;
				case CIO_KEY_SHIFT_IC: /* Shift-Insert - Paste */
					do_paste();
					key = 0;
					break;
				case 0x3000: /* ALT-B - Scrollback */
					setup_mouse_events(NULL);
					viewscroll();
					setup_mouse_events(&ms);
					showmouse();
					key = 0;
					break;
				case 0x2e00: /* ALT-C - Capture */
					capture_control(bbs);
					setup_mouse_events(&ms);
					showmouse();
					key = 0;
					break;
				case 0x2000: /* ALT-D - Download */
					begin_download(bbs);
					setup_mouse_events(&ms);
					showmouse();
					key = 0;
					break;
				case 0x1200: /* ALT-E */
				{
					char                  title[LIST_NAME_MAX + 13];
					struct ciolib_screen *savscrn;
					savscrn = cp437_savescrn();
					show_bbslist(bbs->name, true);
					sprintf(title, "SyncTERM - %s\n", bbs->name);
					settitle(title);
					uifcbail();
					setup_mouse_events(&ms);
					restorescreen(savscrn);
					freescreen(savscrn);
					if ((cterm->scrollback != scrollback_buf)
					    || (cterm->backlines != settings.backlines)) {
						cterm->scrollback = scrollback_buf;
						cterm->backlines = settings.backlines;
						if (cterm->backpos > cterm->backlines)
							cterm->backpos = cterm->backlines;
					}
					showmouse();
					_setcursortype(_NORMALCURSOR);
					key = 0;
				}
				break;
				case 0x2100: /* ALT-F */
					font_control(bbs, cterm);
					setup_mouse_events(&ms);
					showmouse();
					key = 0;
					break;
				case 0x2600: /* ALT-L */
					send_login(bbs);
					key = 0;
					break;
				case 0x3200: /* ALT-M */
					music_control(bbs);
					setup_mouse_events(&ms);
					showmouse();
					key = 0;
					break;
				case 0x1800: /* ALT-O */
					ms.flags ^= MS_FLAGS_DISABLED;
					setup_mouse_events(&ms);
					showmouse();
					key = 0;
					sleep = false;
					break;
				case 0x1600: /* ALT-U - Upload */
					begin_upload(bbs, false, inch);
					setup_mouse_events(&ms);
					showmouse();
					key = 0;
					break;
				case 17: /* CTRL-Q */
					if ((cio_api.mode != CIOLIB_MODE_CURSES)
					    && (cio_api.mode != CIOLIB_MODE_CURSES_ASCII)
					    && (cio_api.mode != CIOLIB_MODE_CURSES_IBM)
					    && (cio_api.mode != CIOLIB_MODE_ANSI))
						break;
					if ((cio_api.mode != CIOLIB_MODE_CURSES)
					    && (cio_api.mode != CIOLIB_MODE_CURSES_ASCII)
					    && (cio_api.mode != CIOLIB_MODE_CURSES_IBM)
					    && (cio_api.mode != CIOLIB_MODE_ANSI)) {
                                                        /* FALLTHROUGH for curses/ansi modes */
				case 0x2d00: /* Alt-X - Exit */
				case CIO_KEY_QUIT:
								if (!check_exit(true))
									break;
					}

                                // Fallthrough
				case 0x2300: /* Alt-H - Hangup */
				{
					struct ciolib_screen *savscrn;
					savscrn = cp437_savescrn();
					if (quitting
					    || confirm("Disconnect... Are you sure?",
					    "Selecting Yes closes the connection\n")) {
						freescreen(savscrn);
						setup_mouse_events(&ms);
						finish_scrollback();
						cterm_end(cterm, 0);
						cterm = NULL;
						conn_close();
						hidemouse();
						hold_update = oldmc;
						return key == 0x2d00 /* Alt-X? */ || key == CIO_KEY_QUIT;
					}
					restorescreen(savscrn);
					freescreen(savscrn);
					setup_mouse_events(&ms);
					showmouse();
				}
					key = 0;
					break;
				case 19: /* CTRL-S */
					if ((cio_api.mode != CIOLIB_MODE_CURSES)
					    && (cio_api.mode != CIOLIB_MODE_CURSES_IBM)
					    && (cio_api.mode != CIOLIB_MODE_ANSI))
						break;

                                /* FALLTHROUGH for curses/ansi modes */
				case 0x2c00: /* ALT-Z */
					if (bbs->hidepopups)
						break;
					i = wherex();
					j = wherey();
					switch (syncmenu(bbs, &speed)) {
						case -1:
							finish_scrollback();
							cterm_end(cterm, 0);
							cterm = NULL;
							conn_close();
							hidemouse();
							hold_update = oldmc;
							return false;
						case 3:
							begin_upload(bbs, false, inch);
							break;
						case 4:
							begin_download(bbs);
							break;
						case 7:
							capture_control(bbs);
							break;
						case 8:
							music_control(bbs);
							break;
						case 9:
							font_control(bbs, cterm);
							break;
						case 10:
							cterm->doorway_mode = !cterm->doorway_mode;
							break;
						case 11:
							ms.flags ^= MS_FLAGS_DISABLED;
							setup_mouse_events(&ms);
							showmouse();
							break;

#ifdef WITHOUT_OOII
						case 12:
#else
						case 12:
							ooii_mode++;
							if (ooii_mode > MAX_OOII_MODE) {
								xptone_close();
								ooii_mode = 0;
							}
							else {
								xptone_open();
							}
							break;
						case 13:
#endif
							finish_scrollback();
							cterm_end(cterm, 0);
							cterm = NULL;
							conn_close();
							hidemouse();
							hold_update = oldmc;
							return true;
#ifdef WITHOUT_OOII
						case 13:
#else
						case 14:
#endif
						{
							struct ciolib_screen *savscrn;
							char                  title[LIST_NAME_MAX + 13];

							savscrn = cp437_savescrn();
							show_bbslist(bbs->name, true);
							sprintf(title, "SyncTERM - %s\n", bbs->name);
							settitle(title);
							uifcbail();
							restorescreen(savscrn);
							freescreen(savscrn);
						}
						break;
					}
					setup_mouse_events(&ms);
					showmouse();
					gotoxy(i, j);
					key = 0;
					break;
				case 0x9800: /* ALT-Up */
					if ((bbs->conn_type != CONN_TYPE_SERIAL)
					    && (bbs->conn_type != CONN_TYPE_SERIAL_NORTS)) {
						if (speed)
							speed = rates[get_rate_num(speed) + 1];
						else
							speed = rates[0];
						key = 0;
					}
					break;
				case 0xa000: /* ALT-Down */
					if ((bbs->conn_type != CONN_TYPE_SERIAL)
					    && (bbs->conn_type != CONN_TYPE_SERIAL_NORTS)) {
						i = get_rate_num(speed);
						if (i == 0)
							speed = 0;
						else
							speed = rates[i - 1];
						key = 0;
					}
					break;
			}
			if (key && (cterm->emulation == CTERM_EMULATION_ATASCII)) {
                                /* Translate keys to ATASCII */
				switch (key) {
					case 253: // Undo Unicode: Atascii beep -> ^G
						ch[0] = 7;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_DOWN:
						ch[0] = 29;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_DC: /* "Delete" key */
						ch[0] = 126;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_RIGHT:
						ch[0] = 31;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_UP:
						ch[0] = 28;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_LEFT:
						ch[0] = 30;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_IC:
						ch[0] = 255;
						conn_send(ch, 1, 0);
						break;
					case 96: /* Backtick toggles inverse */
						atascii_inverse = !atascii_inverse;
						break;
					default:
						if (key < 256) {
							ch[0] = key;
							conn_send(ch, 1, 0);
						}
						break;
				}
			}
			else if (key && (cterm->emulation == CTERM_EMULATION_PETASCII)) {
                                /* Translate keys to PETSCII */
				switch (key) {
					case '\r':
					case '\n':
						ch[0] = 13;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_DOWN:
						ch[0] = 17;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_HOME:
						ch[0] = 19;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_END:
						ch[0] = 147; /* Clear / Shift-Home */
						conn_send(ch, 1, 0);
						break;
					case 8:
					case CIO_KEY_DC: /* "Delete" key */
						ch[0] = 20;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_RIGHT:
						ch[0] = 29;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_F(1):
						ch[0] = 133;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_F(3):
						ch[0] = 134;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_F(5):
						ch[0] = 135;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_F(7):
						ch[0] = 136;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_F(2):
						ch[0] = 137;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_F(4):
						ch[0] = 138;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_F(6):
						ch[0] = 139;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_F(8):
						ch[0] = 140;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_UP:
						ch[0] = 145;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_IC:
						ch[0] = 148;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_LEFT:
						ch[0] = 157;
						conn_send(ch, 1, 0);
						break;
					default:
						if (key < 256) {
							ch[0] = key;
							conn_send(ch, 1, 0);
						}
						break;
				}
			}
			else if (key && (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)) {
				switch (key) {
					case '_':
						if (cterm->emulation == CTERM_EMULATION_PRESTEL)
							ch[0] = '`';
						else
							ch[0] = '_';
						conn_send(ch, 1, 0);
						break;
					case '#':
						if (cterm->emulation == CTERM_EMULATION_PRESTEL)
							ch[0] = '_';
						else
							ch[0] = '#';
						conn_send(ch, 1, 0);
						break;
					case '`':
						if (cterm->emulation == CTERM_EMULATION_PRESTEL)
							ch[0] = '#';
						else
							ch[0] = '`';
						conn_send(ch, 1, 0);
						break;
					case 8:
					case CIO_KEY_DC:
						ch[0] = 127;
						conn_send(ch, 1, 0);
						break;
					case 9:
						ch[0] = 9;
						conn_send(ch, 1, 0);
						break;
					case 10:
						if (cterm->emulation == CTERM_EMULATION_PRESTEL)
							ch[0] = '\r';
						else
							ch[0] = 10;
						conn_send(ch, 1, 0);
						break;
					case 13:
						if (cterm->emulation == CTERM_EMULATION_PRESTEL)
							ch[0] = '_';
						else
							ch[0] = 13;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_HOME:
						ch[0] = 0x1f;
						conn_send(ch, 1, 0);
						break;
					// These mappings are from Commstar...
					case CIO_KEY_F(7):	// Another ESC... ESC returns to Commstar menu
						ch[0] = 27;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_LEFT:
						ch[0] = 140;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_RIGHT:
						ch[0] = 141;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_DOWN:
						ch[0] = 142;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_UP:
						ch[0] = 143;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(10):	// F0
						ch[0] = 144;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(1):
						ch[0] = 145;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(2):
						ch[0] = 146;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(3):
						ch[0] = 147;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(4):
						ch[0] = 148;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(5):
						ch[0] = 149;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(6):
						ch[0] = 150;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(7):
						ch[0] = 151;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(8):
						ch[0] = 152;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_F(9):
						ch[0] = 153;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_END:	// Copy
						ch[0] = 155;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_LEFT:
						ch[0] = 156;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_RIGHT:
						ch[0] = 157;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_DOWN:
						ch[0] = 158;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_SHIFT_UP:
						ch[0] = 159;
						conn_send(ch, 1, 0);
						break;

					case CIO_KEY_CTRL_F(10):	// F0
						ch[0] = 160;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(1):
						ch[0] = 161;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(2):
						ch[0] = 162;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(3):
						ch[0] = 163;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(4):
						ch[0] = 164;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(5):
						ch[0] = 165;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(6):
						ch[0] = 166;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(7):
						ch[0] = 167;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(8):
						ch[0] = 168;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_F(9):
						ch[0] = 169;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_END:	// Copy
						ch[0] = 171;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_LEFT:
						ch[0] = 172;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_RIGHT:
						ch[0] = 173;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_DOWN:
						ch[0] = 174;
						conn_send(ch, 1, 0);
						break;
					case CIO_KEY_CTRL_UP:
						ch[0] = 175;
						conn_send(ch, 1, 0);
						break;

					case CIO_KEY_F(3):
					case CIO_KEY_NPAGE:
					{
						cio_api.options ^= CONIO_OPT_PRESTEL_REVEAL;
						struct ciolib_screen *savscrn = savescreen();
						ciolib_vmem_puttext(1, 1, savscrn->text_info.screenwidth, savscrn->text_info.screenheight, savscrn->vmem);
						freescreen(savscrn);
						break;
					}
					// TODO: Add clear screen key?
					default:
						if (cterm->emulation == CTERM_EMULATION_PRESTEL) {
							if (key == 13 || (key < 128 && key > 31)) {
								ch[0] = key;
								conn_send(ch, 1, 0);
							}
						}
						else {
							if (key < 128) {
								ch[0] = key;
								conn_send(ch, 1, 0);
							}
						}
						break;
				}
			}
			else if (key && (cterm->emulation == CTERM_EMULATION_ATARIST_VT52)) {
				switch (key) {
					case CIO_KEY_F(1):
						conn_send("\033P", 2, 0);
						break;
					case CIO_KEY_F(2):
						conn_send("\033Q", 2, 0);
						break;
					case CIO_KEY_F(3):
						conn_send("\033R", 2, 0);
						break;
					case CIO_KEY_LEFT:
						conn_send("\033D", 2, 0);
						break;
					case CIO_KEY_RIGHT:
						conn_send("\033C", 2, 0);
						break;
					case CIO_KEY_UP:
						conn_send("\033A", 2, 0);
						break;
					case CIO_KEY_DOWN:
						conn_send("\033B", 2, 0);
						break;
					case CIO_KEY_DC:    /* "Delete" key, send ASCII 127 (DEL) */
						if (cterm->extattr & CTERM_EXTATTR_DECBKM)
							conn_send("\x7f", 1, 0);
						else
							conn_send("\x1b[3~", 1, 0);
						break;
					case '\b':
						if (cterm->extattr & CTERM_EXTATTR_DECBKM)
							key = '\b';
						else
							key = '\x7f';

                                        /* FALLTHROUGH to default */
					default:
						if ((key < 256) && (key >= 0)) {
							ch[0] = key;
							conn_send(ch, 1, 0);
						}
				}
			}
			else if (key) {
				switch (key) {
					case CIO_KEY_LEFT:
						conn_send("\033[D", 3, 0);
						break;
					case CIO_KEY_RIGHT:
						conn_send("\033[C", 3, 0);
						break;
					case CIO_KEY_UP:
						conn_send("\033[A", 3, 0);
						break;
					case CIO_KEY_DOWN:
						conn_send("\033[B", 3, 0);
						break;
					case CIO_KEY_HOME:
						conn_send("\033[H", 3, 0);
						break;
					case CIO_KEY_END:
#ifdef CIO_KEY_SELECT
					case CIO_KEY_SELECT: /* Some terminfo/termcap entries use KEY_SELECT as the END
                                                              * key! */
#endif
						conn_send("\033[K", 3, 0);
						break;
					case CIO_KEY_DC:    /* "Delete" key, send ASCII 127 (DEL) */
						if (cterm->extattr & CTERM_EXTATTR_DECBKM)
							conn_send("\x7f", 1, 0);
						else
							conn_send("\x1b[3~", 1, 0);
						break;
					case CIO_KEY_NPAGE: /* Page down */
						conn_send("\033[U", 3, 0);
						break;
					case CIO_KEY_PPAGE: /* Page up */
						conn_send("\033[V", 3, 0);
						break;
					case CIO_KEY_F(1):
						conn_send("\033[11~", 3, 0);
						break;
					case CIO_KEY_F(2):
						conn_send("\033[12~", 3, 0);
						break;
					case CIO_KEY_F(3):
						conn_send("\033[13~", 3, 0);
						break;
					case CIO_KEY_F(4):
						conn_send("\033[14~", 3, 0);
						break;
					case CIO_KEY_F(5):
						conn_send("\033[15~", 3, 0);
						break;
					case CIO_KEY_F(6):
						conn_send("\033[17~", 5, 0);
						break;
					case CIO_KEY_F(7):
						conn_send("\033[18~", 5, 0);
						break;
					case CIO_KEY_F(8):
						conn_send("\033[19~", 5, 0);
						break;
					case CIO_KEY_F(9):
						conn_send("\033[20~", 5, 0);
						break;
					case CIO_KEY_F(10):
						conn_send("\033[21~", 5, 0);
						break;
					case CIO_KEY_F(11):
						conn_send("\033[23~", 5, 0);
						break;
					case CIO_KEY_F(12):
						conn_send("\033[24~", 5, 0);
						break;
					case CIO_KEY_SHIFT_F(1):
						conn_send("\033[11;2~", 3, 0);
						break;
					case CIO_KEY_SHIFT_F(2):
						conn_send("\033[12;2~", 3, 0);
						break;
					case CIO_KEY_SHIFT_F(3):
						conn_send("\033[13;2~", 3, 0);
						break;
					case CIO_KEY_SHIFT_F(4):
						conn_send("\033[14;2~", 3, 0);
						break;
					case CIO_KEY_SHIFT_F(5):
						conn_send("\033[15;2~", 3, 0);
						break;
					case CIO_KEY_SHIFT_F(6):
						conn_send("\033[17;2~", 5, 0);
						break;
					case CIO_KEY_SHIFT_F(7):
						conn_send("\033[18;2~", 5, 0);
						break;
					case CIO_KEY_SHIFT_F(8):
						conn_send("\033[19;2~", 5, 0);
						break;
					case CIO_KEY_SHIFT_F(9):
						conn_send("\033[20;2~", 5, 0);
						break;
					case CIO_KEY_SHIFT_F(10):
						conn_send("\033[21;2~", 5, 0);
						break;
					case CIO_KEY_SHIFT_F(11):
						conn_send("\033[23;2~", 5, 0);
						break;
					case CIO_KEY_SHIFT_F(12):
						conn_send("\033[24;2~", 5, 0);
						break;
					case CIO_KEY_CTRL_F(1):
						conn_send("\033[11;5~", 3, 0);
						break;
					case CIO_KEY_CTRL_F(2):
						conn_send("\033[12;5~", 3, 0);
						break;
					case CIO_KEY_CTRL_F(3):
						conn_send("\033[13;5~", 3, 0);
						break;
					case CIO_KEY_CTRL_F(4):
						conn_send("\033[14;5~", 3, 0);
						break;
					case CIO_KEY_CTRL_F(5):
						conn_send("\033[15;5~", 3, 0);
						break;
					case CIO_KEY_CTRL_F(6):
						conn_send("\033[17;5~", 5, 0);
						break;
					case CIO_KEY_CTRL_F(7):
						conn_send("\033[18;5~", 5, 0);
						break;
					case CIO_KEY_CTRL_F(8):
						conn_send("\033[19;5~", 5, 0);
						break;
					case CIO_KEY_CTRL_F(9):
						conn_send("\033[20;5~", 5, 0);
						break;
					case CIO_KEY_CTRL_F(10):
						conn_send("\033[21;5~", 5, 0);
						break;
					case CIO_KEY_CTRL_F(11):
						conn_send("\033[23;5~", 5, 0);
						break;
					case CIO_KEY_CTRL_F(12):
						conn_send("\033[24;5~", 5, 0);
						break;
					case CIO_KEY_ALT_F(1):
						conn_send("\033[11;3~", 3, 0);
						break;
					case CIO_KEY_ALT_F(2):
						conn_send("\033[12;3~", 3, 0);
						break;
					case CIO_KEY_ALT_F(3):
						conn_send("\033[13;3~", 3, 0);
						break;
					case CIO_KEY_ALT_F(4):
						conn_send("\033[14;3~", 3, 0);
						break;
					case CIO_KEY_ALT_F(5):
						conn_send("\033[15;3~", 3, 0);
						break;
					case CIO_KEY_ALT_F(6):
						conn_send("\033[17;3~", 5, 0);
						break;
					case CIO_KEY_ALT_F(7):
						conn_send("\033[18;3~", 5, 0);
						break;
					case CIO_KEY_ALT_F(8):
						conn_send("\033[19;3~", 5, 0);
						break;
					case CIO_KEY_ALT_F(9):
						conn_send("\033[20;3~", 5, 0);
						break;
					case CIO_KEY_ALT_F(10):
						conn_send("\033[21;3~", 5, 0);
						break;
					case CIO_KEY_ALT_F(11):
						conn_send("\033[23;3~", 5, 0);
						break;
					case CIO_KEY_ALT_F(12):
						conn_send("\033[24;3~", 5, 0);
						break;
					case CIO_KEY_IC:
						conn_send("\033[@", 3, 0);
						break;
					case CIO_KEY_BACKTAB:
						conn_send("\033[Z", 3, 0);
						break;
					case '\b':
						if (cterm->extattr & CTERM_EXTATTR_DECBKM)
							key = '\b';
						else
							key = '\x7f';

                                        /* FALLTHROUGH to default */
					default:
						if ((key < 256) && (key >= 0)) {
							ch[0] = key;
							conn_send(ch, 1, 0);
						}
				}
			}
		}
		if (sleep)
			SLEEP(1);
		else
			MAYBE_YIELD();
	}

/*
 *       hidemouse();
 *       hold_update=oldmc;
 */
	finish_scrollback();
	return false;
}
