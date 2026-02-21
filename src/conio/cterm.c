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

/*
 * Coordinate systems in use here... text is 1-based.
 * "pixel"   the ciolib screen in pixels.  sixel commands happen here.
 * "screen"  the ciolib screen... gettext/puttext/movetext and mouse
 *           are in this space.
 * "absterm" position inside the full terminal area.  These coordinates
 *           are used when origin mode is off.
 * "term"    The scrolling region.  These coordinates are used when origin
 *           mode is on.
 * "curr"    Either absterm or term depending on the state of origin mode
 *           This is what's sent to and received from the remote, and
 *           matches the CTerm window.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
 #include <malloc.h>	/* alloca() on Win32 */
 #include <xpprintf.h>	/* asprintf() on Win32 */
#endif

#include <genwrap.h>
#include <xpbeep.h>
#include <link_list.h>
#ifdef __unix__
	#include <xpsem.h>
#endif
#include <threadwrap.h>

#include "ciolib.h"

#include "cterm.h"
#include "vidmodes.h"
#include "base64.h"
#include <crc16.h>

/*
 * Disable warnings for well-defined standard C operations
 * 4244 appears to be for implicit conversions
 * warning C4244: '=': conversion from 'uint64_t' to 'int', possible loss of data
 * 4267 is the same thing, but from size_t.
 * 4018 warns when comparing signed and unsigned if the signed is implicitly
 *      converted to unsigned (so more sane than -Wsign-compare)
 */
#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

// Hack for Visual Studio 2022
#ifndef UINT64_MAX
#define UINT64_MAX UINT64_C(-1)
#endif

#define	BUFSIZE	2048

const int cterm_tabs[]={1,9,17,25,33,41,49,57,65,73,81,89,97,105,113,121,129,137,145};

const char *octave="C#D#EF#G#A#B";

/* Characters allowed in music strings... if one that is NOT in here is
 * found, CTerm leaves music capture mode and tosses the buffer away */
const char *musicchars="aAbBcCdDeEfFgGlLmMnNoOpPsStT0123456789.-+#<> ";
const uint note_frequency[]={	/* Hz*1000 */
/* Octave 0 (Note 1) */
	 65406
	,69296
	,73416
	,77782
	,82407
	,87307
	,92499
	,97999
	,103820
	,110000
	,116540
	,123470
/* Octave 1 */
	,130810
	,138590
	,146830
	,155560
	,164810
	,174610
	,185000
	,196000
	,207650
	,220000
	,233080
	,246940
/* Octave 2 */
	,261620
	,277180
	,293660
	,311130
	,329630
	,349230
	,369990
	,392000
	,415300
	,440000
	,466160
	,493880
/* Octave 3 */
	,523250
	,554370
	,587330
	,622250
	,659260
	,698460
	,739990
	,783990
	,830610
	,880000
	,932330
	,987770
/* Octave 4 */
	,1046500
	,1108700
	,1174700
	,1244500
	,1318500
	,1396900
	,1480000
	,1568000
	,1661200
	,1760000
	,1864600
	,1975500
/* Octave 5 */
	,2093000
	,2217500
	,2349300
	,2489000
	,2637000
	,2793800
	,2959900
	,3136000
	,3322400
	,3520000
	,3729300
	,3951100
/* Octave 6 */
	,4186000
	,4435000
	,4698600
	,4978000
	,5274000
	,5587000
	,5920000
	,6272000
	,6644000
	,7040000
	,7458600
	,7902200
};

struct note_params {
	int notenum;
	int	notelen;
	int	dotted;
	int	tempo;
	int	noteshape;
	int	foreground;
};

static void ctputs(struct cterminal *cterm, char *buf);
static void cterm_reset(struct cterminal *cterm);

enum cterm_coordinates {
	CTERM_COORD_SCREEN,
	CTERM_COORD_ABSTERM,
	CTERM_COORD_TERM,
	CTERM_COORD_CURR,
};

#define SCR_MINX	1
#define ABS_MINX	1
#define TERM_MINX	1
#define CURR_MINX	1

#if 0
static int
scr_maxx(struct cterminal *cterm)
{
	struct text_info ti;

	gettextinfo(&ti);
	return ti.screenwidth;
}
#endif
//#define SCR_MAXX	coord_maxx(cterm, CTERM_COORD_SCREEN)
#define ABS_MAXX	cterm->width
#define TERM_MAXX	(cterm->right_margin - cterm->left_margin + 1)
#define CURR_MAXX	((cterm->extattr & CTERM_EXTATTR_ORIGINMODE) ? TERM_MAXX : ABS_MAXX)

#define SCR_MINY	1
#define ABS_MINY	1
#define TERM_MINY	1
#define CURR_MINY	1


#if 0
static int
scr_maxy(struct cterminal *cterm, enum cterm_coordinates coord)
{
	struct text_info ti;

	gettextinfo(&ti);
	return ti.screenheight;
}
#endif
//#define SCR_MAXY	scr_maxy(cterm)
#define ABS_MAXY	cterm->height
#define TERM_MAXY	(cterm->bottom_margin - cterm->top_margin + 1)
#define CURR_MAXY	((cterm->extattr & CTERM_EXTATTR_ORIGINMODE) ? TERM_MAXY : ABS_MAXY)

static void
coord_conv_xy(struct cterminal *cterm, enum cterm_coordinates from_coord,
    enum cterm_coordinates to_coord, int *x, int *y)
{
	if (from_coord == to_coord)
		return;

	if (x) {
		if (from_coord == CTERM_COORD_CURR) {
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				from_coord = CTERM_COORD_TERM;
			else
				from_coord = CTERM_COORD_ABSTERM;
		}
		switch(from_coord) {
			case CTERM_COORD_SCREEN:
				break;
			case CTERM_COORD_TERM:
				*x += cterm->left_margin - 1;
				// Fall-through
			case CTERM_COORD_ABSTERM:
				*x += cterm->x - 1;
				break;
			default:
				// Silence warnings
				break;
		}
		if (to_coord == CTERM_COORD_CURR) {
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				to_coord = CTERM_COORD_TERM;
			else
				to_coord = CTERM_COORD_ABSTERM;
		}
		switch(to_coord) {
			case CTERM_COORD_SCREEN:
				break;
			case CTERM_COORD_TERM:
				*x -= cterm->left_margin - 1;
				// Fall-through
			case CTERM_COORD_ABSTERM:
				*x -= cterm->x - 1;
				break;
			default:
				// Silence warnings
				break;
		}
	}
	if (y) {
		if (from_coord == CTERM_COORD_CURR) {
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				from_coord = CTERM_COORD_TERM;
			else
				from_coord = CTERM_COORD_ABSTERM;
		}
		switch(from_coord) {
			case CTERM_COORD_SCREEN:
				break;
			case CTERM_COORD_TERM:
				*y += cterm->top_margin - 1;
				// Fall-through
			case CTERM_COORD_ABSTERM:
				*y += cterm->y - 1;
				break;
			default:
				// Silence warnings
				break;
		}
		if (to_coord == CTERM_COORD_CURR) {
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				to_coord = CTERM_COORD_TERM;
			else
				to_coord = CTERM_COORD_ABSTERM;
		}
		switch(to_coord) {
			case CTERM_COORD_SCREEN:
				break;
			case CTERM_COORD_TERM:
				*y -= cterm->top_margin - 1;
				// Fall-through
			case CTERM_COORD_ABSTERM:
				*y -= cterm->y - 1;
				break;
			default:
				// Silence warnings
				break;
		}
	}
}

static void
coord_get_xy(struct cterminal *cterm, enum cterm_coordinates coord, int *x, int *y)
{
	if (x)
		*x = wherex();
	if (y)
		*y = wherey();

	coord_conv_xy(cterm, CTERM_COORD_CURR, coord, x, y);
}
#define SCR_XY(x,y)	coord_get_xy(cterm, CTERM_COORD_SCREEN, x, y)
#define ABS_XY(x,y)	coord_get_xy(cterm, CTERM_COORD_ABSTERM, x, y)
#define TERM_XY(x,y)	coord_get_xy(cterm, CTERM_COORD_TERM, x, y)
#define CURR_XY(x,y)	coord_get_xy(cterm, CTERM_COORD_CURR, x, y)
void
setwindow(struct cterminal *cterm)
{
	int col, row, max_col, max_row;

	col = CURR_MINX;
	row = CURR_MINY;
	coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &col, &row);
	max_col = CURR_MAXX;
	max_row = CURR_MAXY;
	coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &max_col, &max_row);
	window(col, row, max_col, max_row);
}

static void
set_attr(struct cterminal *cterm, unsigned char colour, bool bg)
{
	if (bg) {
		cterm->attr &= 0x8F;
		cterm->attr |= ((colour & 0x07) << 4);
	}
	else {
		cterm->attr &= 0xF8;
		cterm->attr |= (colour & 0x07);
	}
	attr2palette(cterm->attr, bg ? NULL : &cterm->fg_color, bg ? &cterm->bg_color : NULL);
	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB) {
		if (cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT)
			cterm->bg_color |= CIOLIB_BG_DOUBLE_HEIGHT;
		cterm->bg_color |= CIOLIB_BG_PRESTEL;
		if (cterm->emulation == CTERM_EMULATION_PRESTEL)
			cterm->bg_color |= CIOLIB_BG_PRESTEL_TERMINAL;
	}
	if (bg)
		FREE_AND_NULL(cterm->bg_tc_str);
	else
		FREE_AND_NULL(cterm->fg_tc_str);
}

static void
set_fgattr(struct cterminal *cterm, unsigned char colour)
{
	set_attr(cterm, colour, false ^ cterm->negative);
}

static void
set_bgattr(struct cterminal *cterm, unsigned char colour)
{
	set_attr(cterm, colour, true ^ cterm->negative);
}

static void
prestel_new_line(struct cterminal *cterm)
{
	cterm->extattr &= ~(CTERM_EXTATTR_PRESTEL_CONCEAL | CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT | CTERM_EXTATTR_PRESTEL_HOLD | CTERM_EXTATTR_PRESTEL_MOSAIC | CTERM_EXTATTR_PRESTEL_SEPARATED);
	cterm->attr = 7;
	attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
	cterm->bg_color |= CIOLIB_BG_PRESTEL;
	if (cterm->emulation == CTERM_EMULATION_PRESTEL)
		cterm->bg_color |= CIOLIB_BG_PRESTEL_TERMINAL;
	cterm->bg_color &= ~CIOLIB_BG_SEPARATED;
	cterm->prestel_last_mosaic = 0;
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
}

static void
prestel_apply_ctrl_before(struct cterminal *cterm, uint8_t ch)
{
	switch(ch) {
		case 73: // Steady
			cterm->attr &= 0x7f;
			attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
			if (cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT)
				cterm->bg_color |= CIOLIB_BG_DOUBLE_HEIGHT;
			break;
		case 76: // Normal Height
			cterm->extattr &= ~(CTERM_EXTATTR_PRESTEL_HOLD);
			cterm->prestel_last_mosaic = 0;
			cterm->extattr &= ~(CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT);
			cterm->bg_color &= ~CIOLIB_BG_DOUBLE_HEIGHT;
			break;
		case 88: // Conceal Display
			cterm->attr |= 0x08;
			cterm->extattr |= CTERM_EXTATTR_PRESTEL_CONCEAL;
			break;
		case 89: // Contiguous Mosaics
			/*
			 * It makes no difference if this is before or after since 
			 * if there's a held mosaic, it's displayed the same way it
			 * was originally, and if there's no held mosaic, this will
			 * just be a space.
			 */
			if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD))
				cterm->prestel_last_mosaic = 0;
			cterm->extattr &= ~(CTERM_EXTATTR_PRESTEL_SEPARATED);
			cterm->bg_color &= ~CIOLIB_BG_SEPARATED;
			break;
		case 90: // Separated Mosaics
			/*
			 * It makes no difference if this is before or after since 
			 * if there's a held mosaic, it's displayed the same way it
			 * was originally, and if there's no held mosaic, this will
			 * just be a space.
			 */
			if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD))
				cterm->prestel_last_mosaic = 0;
			cterm->extattr |= CTERM_EXTATTR_PRESTEL_SEPARATED;
			cterm->bg_color |= CIOLIB_BG_SEPARATED;
			break;
		case 92: // Black Background
			set_bgattr(cterm, BLACK);
			break;
		case 93: // New Background
			set_bgattr(cterm, cterm->attr & 0x07);
			break;
		case 94: // Hold Mosaics
			cterm->extattr |= CTERM_EXTATTR_PRESTEL_HOLD;
			break;
	}
	cterm->bg_color |= CIOLIB_BG_PRESTEL;
	if (cterm->emulation == CTERM_EMULATION_PRESTEL)
		cterm->bg_color |= CIOLIB_BG_PRESTEL_TERMINAL;
}

static void
prestel_colour(struct cterminal *cterm, bool alpha, int colour)
{
	cterm->attr &= 0xF7;
	set_fgattr(cterm, colour);
	cterm->extattr &= ~(CTERM_EXTATTR_PRESTEL_CONCEAL);
	if (alpha) {
		cterm->extattr &= ~(CTERM_EXTATTR_PRESTEL_HOLD | CTERM_EXTATTR_PRESTEL_MOSAIC);
	}
	else {
		cterm->extattr |= CTERM_EXTATTR_PRESTEL_MOSAIC;
	}
	if (!(cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD))
		cterm->prestel_last_mosaic = 0;
}

static void
prestel_apply_ctrl_after(struct cterminal *cterm, uint8_t ch)
{
	switch(ch) {
		case 65: // Alphanumeric Red
			prestel_colour(cterm, true, RED);
			break;
		case 66: // Alphanumeric Green
			prestel_colour(cterm, true, GREEN);
			break;
		case 67: // Alphanumeric Yellow
			prestel_colour(cterm, true, YELLOW);
			break;
		case 68: // Alphanumeric Blue
			prestel_colour(cterm, true, BLUE);
			break;
		case 69: // Alphanumeric Magenta
			prestel_colour(cterm, true, MAGENTA);
			break;
		case 70: // Alphanumeric Cyan
			prestel_colour(cterm, true, CYAN);
			break;
		case 71: // Alphanumeric White
			prestel_colour(cterm, true, WHITE);
			break;
		case 72: // Flash
			cterm->attr |= 0x80;
			attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
			if (cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT)
				cterm->bg_color |= CIOLIB_BG_DOUBLE_HEIGHT;
			break;
		case 77: // Double Height
			cterm->extattr &= ~(CTERM_EXTATTR_PRESTEL_HOLD);
			cterm->prestel_last_mosaic = 0;
			cterm->extattr |= CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT;
			cterm->bg_color |= CIOLIB_BG_DOUBLE_HEIGHT;
			break;
		case 81: // Mosaic Red
			prestel_colour(cterm, false, RED);
			break;
		case 82: // Mosaic Green
			prestel_colour(cterm, false, GREEN);
			break;
		case 83: // Mosaic Yellow
			prestel_colour(cterm, false, YELLOW);
			break;
		case 84: // Mosaic Blue
			prestel_colour(cterm, false, BLUE);
			break;
		case 85: // Mosaic Magenta
			prestel_colour(cterm, false, MAGENTA);
			break;
		case 86: // Mosaic Cyan
			prestel_colour(cterm, false, CYAN);
			break;
		case 87: // Mosaic White
			prestel_colour(cterm, false, WHITE);
			break;
		case 95: // Release Mosaics
			cterm->prestel_last_mosaic = 0;
			cterm->extattr &= ~(CTERM_EXTATTR_PRESTEL_HOLD);
			break;
	}
	cterm->bg_color |= CIOLIB_BG_PRESTEL;
	if (cterm->emulation == CTERM_EMULATION_PRESTEL)
		cterm->bg_color |= CIOLIB_BG_PRESTEL_TERMINAL;
}

static void
prestel_apply_ctrl(struct cterminal *cterm, uint8_t ch)
{
	prestel_apply_ctrl_before(cterm, ch);
	prestel_apply_ctrl_after(cterm, ch);
}

static void
prestel_get_state(struct cterminal *cterm)
{
	struct vmem_cell *line;
	int sx, sy;
	int tx, ty;

	SCR_XY(&sx, &sy);
	TERM_XY(&tx, &ty);
	line = malloc(sizeof(*line) * tx);
	if (line == NULL) {
		fprintf(stderr, "malloc() in prestel_get_state()\n");
		return;
	}
	prestel_new_line(cterm);
	if (tx > 1) {
		vmem_gettext(cterm->x, sy, cterm->x + tx - 2, sy, line);
		for (int i = 0; i < (tx - 1); i++) {
			uint8_t ch = line[i].ch;
			if (line[i].fg & CIOLIB_FG_PRESTEL_CTRL_MASK) {
				ch = (line[i].fg & CIOLIB_FG_PRESTEL_CTRL_MASK) >> CIOLIB_FG_PRESTEL_CTRL_SHIFT;
				prestel_apply_ctrl(cterm, ch);
			}
			else {
				if (ch >= 160) {
					cterm->prestel_last_mosaic = ch;
					if (cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED)
						cterm->prestel_last_mosaic &= 0x7f;
				}
			}
		}
	}
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
	free(line);
}

void
cterm_gotoxy(struct cterminal *cterm, int x, int y)
{
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &x, &y);
	gotoxy(x, y);
	ABS_XY(&cterm->xpos, &cterm->ypos);
	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)
		prestel_get_state(cterm);
}

static void
insert_tabstop(struct cterminal *cterm, int pos)
{
	int i;
	int *new_stops;

	for (i = 0; i < cterm->tab_count && cterm->tabs[i] < pos; i++);
	if (cterm->tabs[i] == pos)
		return;

	new_stops = realloc(cterm->tabs, (cterm->tab_count + 1) * sizeof(cterm->tabs[0]));
	if (new_stops == NULL)
		return;
	cterm->tabs = new_stops;
	if (i < cterm->tab_count)
		memmove(&cterm->tabs[i + 1], &cterm->tabs[i], (cterm->tab_count - i) * sizeof(cterm->tabs[0]));
	cterm->tabs[i] = pos;
	cterm->tab_count++;
}

static void
delete_tabstop(struct cterminal *cterm, int pos)
{
	int i;

	for (i = 0; i < cterm->tab_count && cterm->tabs[i] <= pos; i++) {
		if (cterm->tabs[i] == pos) {
			memmove(&cterm->tabs[i], &cterm->tabs[i+1], (cterm->tab_count - i - 1) * sizeof(cterm->tabs[0]));
			cterm->tab_count--;
			return;
		}
	}
	return;
}

static void tone_or_beep(double freq, int duration, int device_open)
{
	if(device_open)
		xptone(freq,duration,WAVE_SHAPE_SINE_SAW_HARM);
	else
		xpbeep(freq,duration);
}

static void playnote_thread(void *args)
{
	/* Tempo is quarter notes per minute */
	int duration;
	int pauselen;
	struct note_params *note;
	int	device_open=FALSE;
	struct cterminal *cterm=(struct cterminal *)args;

	SetThreadName("PlayNote");
	while(1) {
		if(device_open) {
			if(!listSemTryWaitBlock(&cterm->notes,5000)) {
				xptone_close();
				listSemWait(&cterm->notes);
			}
		}
		else
			listSemWait(&cterm->notes);
		note=listShiftNode(&cterm->notes);
		if(note==NULL)
			break;
		device_open=xptone_open();
		if(note->dotted)
			duration=360000/note->tempo;
		else
			duration=240000/note->tempo;
		duration/=note->notelen;
		switch(note->noteshape) {
			case CTERM_MUSIC_STACATTO:
				pauselen=duration/4;
				break;
			case CTERM_MUSIC_LEGATO:
				pauselen=0;
				break;
			case CTERM_MUSIC_NORMAL:
			default:
				pauselen=duration/8;
				break;
		}
		duration-=pauselen;
		if(note->notenum < 72 && note->notenum >= 0)
			tone_or_beep(((double)note_frequency[note->notenum])/1000,duration,device_open);
		else
			tone_or_beep(0,duration,device_open);
		if(pauselen)
			tone_or_beep(0,pauselen,device_open);
		if(note->foreground)
			sem_post(&cterm->note_completed_sem);
		free(note);
	}
	if (device_open)
		xptone_close();
	cterm->playnote_thread_running=FALSE;
	sem_post(&cterm->playnote_thread_terminated);
}

static void play_music(struct cterminal *cterm)
{
	int		i;
	char	*p;
	char	*out;
	int		offset;
	char	note;
	int		notelen;
	char	numbuf[10];
	int		dotted;
	int		notenum;
	struct	note_params *np;
	int		fore_count;

	if(cterm->quiet) {
		cterm->music=0;
		cterm->musicbuf[0]=0;
		return;
	}
	p=cterm->musicbuf;
	fore_count=0;
	if(cterm->music==1) {
		switch(toupper(*p)) {
			case 'F':
				cterm->musicfore=TRUE;
				p++;
				break;
			case 'B':
				cterm->musicfore=FALSE;
				p++;
				break;
			case 'N':
				if(!isdigit(*(p+1))) {
					cterm->noteshape=CTERM_MUSIC_NORMAL;
					p++;
				}
				break;
			case 'L':
				cterm->noteshape=CTERM_MUSIC_LEGATO;
				p++;
				break;
			case 'S':
				cterm->noteshape=CTERM_MUSIC_STACATTO;
				p++;
				break;
		}
	}
	for(;*p;p++) {
		notenum=0;
		offset=0;
		switch(toupper(*p)) {
			case 'M':
				p++;
				switch(toupper(*p)) {
					case 'F':
						cterm->musicfore=TRUE;
						break;
					case 'B':
						cterm->musicfore=FALSE;
						break;
					case 'N':
						cterm->noteshape=CTERM_MUSIC_NORMAL;
						break;
					case 'L':
						cterm->noteshape=CTERM_MUSIC_LEGATO;
						break;
					case 'S':
						cterm->noteshape=CTERM_MUSIC_STACATTO;
						break;
					default:
						p--;
				}
				break;
			case 'T':						/* Tempo */
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->tempo=strtoul(numbuf,NULL,10);
				if(cterm->tempo>255)
					cterm->tempo=255;
				if(cterm->tempo<32)
					cterm->tempo=32;
				break;
			case 'O':						/* Octave */
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->octave=strtoul(numbuf,NULL,10);
				if(cterm->octave>6)
					cterm->octave=6;
				break;
			case 'L':
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->notelen=strtoul(numbuf,NULL,10);
				if(cterm->notelen<1)
					cterm->notelen=1;
				if(cterm->notelen>64)
					cterm->notelen=64;
				break;
			case 'N':						/* Note by number */
				if(isdigit(*(p+1))) {
					out=numbuf;
					while(isdigit(*(p+1))) {
						*(out++)=*(p+1);
						p++;
					}
					*out=0;
					notenum=strtoul(numbuf,NULL,10);
				}
				if(notenum==0) {
					notenum=-1;
					offset=1;
				}
				/* Fall-through */
			case 'A':						/* Notes in current octave by letter */
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
			case 'G':
			case 'P':
				note=toupper(*p);
				notelen=cterm->notelen;
				dotted=0;
				i=1;
				while(i) {
					i=0;
					if(*(p+1)=='+' || *(p+1)=='#') {	/* SHARP */
						offset+=1;
						p++;
						i=1;
					}
					if(*(p+1)=='-') {					/* FLAT */
						offset-=1;
						p++;
						i=1;
					}
					if(*(p+1)=='.') {					/* Dotted note (1.5*notelen) */
						dotted=1;
						p++;
						i=1;
					}
					if(isdigit(*(p+1))) {
						out=numbuf;
						while(isdigit(*(p+1))) {
							*(out++)=*(p+1);
							p++;
						}
						*out=0;
						notelen=strtoul(numbuf,NULL,10);
						i=1;
					}
				}
				if(note=='P') {
					notenum=-1;
					offset=0;
				}
				if(notenum==0) {
					out=(char*)strchr(octave,note);
					if(out==NULL) {
						notenum=-1;
						offset=1;
					}
					else {
						notenum=cterm->octave*12+1;
						notenum+=(out-octave);
					}
				}
				notenum+=offset;
				np=(struct note_params *)malloc(sizeof(struct note_params));
				if(np!=NULL) {
					np->notenum=notenum;
					np->notelen=notelen;
					if(np->notelen<1)
						np->notelen=1;
					if(np->notelen>64)
						np->notelen=64;
					np->dotted=dotted;
					np->tempo=cterm->tempo;
					np->noteshape=cterm->noteshape;
					np->foreground=cterm->musicfore;
					listPushNode(&cterm->notes, np);
					if(cterm->musicfore)
						fore_count++;
				}
				break;
			case '<':							/* Down one octave */
				cterm->octave--;
				if(cterm->octave<0)
					cterm->octave=0;
				break;
			case '>':							/* Up one octave */
				cterm->octave++;
				if(cterm->octave>6)
					cterm->octave=6;
				break;
		}
	}
	cterm->music=0;
	cterm->musicbuf[0]=0;
	if (fore_count) {
		while(fore_count) {
			sem_wait(&cterm->note_completed_sem);
			fore_count--;
		}
		xptone_complete();
	}
}

void
cterm_clreol(struct cterminal *cterm)
{
	int x, y;
	int rm;
	struct vmem_cell *buf;
	int i;
	int width;

	CURR_XY(&x, &y);
	rm = CURR_MAXX;

	width = rm - x + 1;
	if (width < 1)
		return;
	buf = malloc(width * sizeof(*buf));
	if (!buf)
		return;
	for (i = 0; i < width; i++) {
		if (i > 0)
			buf[i] = buf[0];
		else {
			buf[i].ch = ' ';
			buf[i].legacy_attr = cterm->attr;
			buf[i].fg = cterm->fg_color;
			buf[i].bg = cterm->bg_color;
			buf[i].font = ciolib_attrfont(cterm->attr);
		}
	}
	coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &x, &y);
	coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &rm, NULL);
	vmem_puttext(x, y, rm, y, buf);
	free(buf);
}

void
cterm_clrblk(struct cterminal *cterm, int sx, int sy, int ex, int ey)
{
	int i;
	struct vmem_cell *buf;
	int chars = (ex - sx + 1) * (ey - sy + 1);

	buf = malloc(chars * sizeof(*buf));
	if (!buf)
		return;
	for (i = 0; i < chars ; i++) {
		if (i > 0)
			buf[i] = buf[0];
		else {
			buf[i].ch = ' ';
			buf[i].legacy_attr = cterm->attr;
			buf[i].fg = cterm->fg_color;
			buf[i].bg = cterm->bg_color;
			buf[i].font = ciolib_attrfont(cterm->attr);
		}
	}
	vmem_puttext(sx, sy, ex, ey, buf);
	free(buf);
}

static void
scrolldown(struct cterminal *cterm)
{
	int minx = TERM_MINX;
	int miny = TERM_MINY;
	int maxx = TERM_MAXX;
	int maxy = TERM_MAXY;
	int x,y;

	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &minx, &miny);
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &maxx, &maxy);
	movetext(minx, miny, maxx, maxy - 1, minx, miny + 1);
	CURR_XY(&x, &y);
	cterm_clrblk(cterm, minx, miny, minx + TERM_MAXX - 1, miny);
	gotoxy(x, y);
}

static void
cterm_line_to_scrollback(struct cterminal *cterm, int row)
{
	if(cterm->scrollback!=NULL) {
		int getw;

		cterm->backfilled++;
		if (cterm->backfilled > cterm->backlines) {
			cterm->backfilled--;
			cterm->backstart++;
			if (cterm->backstart == cterm->backlines)
				cterm->backstart = 0;
		}
		getw = cterm->backwidth;
		if (getw > cterm->width)
			getw = cterm->width;
		if (getw < cterm->backwidth) {
			memset(cterm->scrollback + cterm->backpos * cterm->backwidth, 0, sizeof(*cterm->scrollback) * cterm->backwidth);
		}
		vmem_gettext(cterm->x, row, cterm->x + getw - 1, row, cterm->scrollback + cterm->backpos * cterm->backwidth);

		cterm->backpos++;
		if (cterm->backpos == cterm->backlines)
			cterm->backpos = 0;
	}
}

void
cterm_scrollup(struct cterminal *cterm)
{
	int minx = TERM_MINX;
	int miny = TERM_MINY;
	int maxx = TERM_MAXX;
	int maxy = TERM_MAXY;
	int x,y;

	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &minx, &miny);
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &maxx, &maxy);
	cterm_line_to_scrollback(cterm, miny);
	movetext(minx, miny + 1, maxx, maxy, minx, miny);
	CURR_XY(&x, &y);
	cterm_clrblk(cterm, minx, maxy, minx + TERM_MAXX - 1, maxy);
	gotoxy(x, y);
}

static void
cond_scrollup(struct cterminal *cterm)
{
	int x, y;

	TERM_XY(&x, &y);
	if (x >= TERM_MINX && x <= TERM_MAXX &&
	    y >= TERM_MINY && y <= TERM_MAXY)
		cterm_scrollup(cterm);
}

static void
dellines(struct cterminal * cterm, int lines)
{
	int i;
	int minx = TERM_MINX;
	int miny = TERM_MINY;
	int maxx = TERM_MAXX;
	int maxy = TERM_MAXY;
	int sx,sy;
	int x,y;

	if(lines<1)
		return;
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &minx, &miny);
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &maxx, &maxy);
	TERM_XY(&x, &y);
	// Can't delete lines outside of window
	if (x < TERM_MINX || x > TERM_MAXX || y < TERM_MINY || y > TERM_MAXY)
		return;
	SCR_XY(&sx, &sy);
	if ((sy + lines - 1) > maxy) // Delete all the lines... ie: clear screen...
		lines = maxy - sy + 1;
	if (sy + lines <= maxy)
		movetext(minx, sy + lines, maxx, maxy, minx, sy);
	for(i = TERM_MAXY - lines + 1; i <= TERM_MAXY; i++) {
		cterm_gotoxy(cterm, TERM_MINX, i);
		cterm_clreol(cterm);
	}
	cterm_gotoxy(cterm, x, y);
}

static void
clear2bol(struct cterminal * cterm)
{
	struct vmem_cell *buf;
	int i;
	int x, y;
	int minx = CURR_MINX;

	CURR_XY(&x, &y);
	buf = malloc(x * sizeof(*buf));
	if (buf) {
		for(i = 0; i < x; i++) {
			if (i > 0)
				buf[i] = buf[0];
			else {
				buf[i].ch = ' ';
				buf[i].legacy_attr = cterm->attr;
				buf[i].fg = cterm->fg_color;
				buf[i].bg = cterm->bg_color;
				buf[i].font = ciolib_attrfont(cterm->attr);
			}
		}
		coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &x, &y);
		coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &minx, NULL);
		vmem_puttext(minx, y, x, y, buf);
		free(buf);
	}
	else {
		fprintf(stderr, "malloc() in clear2bol()\n");
	}
}

void
cterm_clearscreen(struct cterminal *cterm, char attr)
{
	if(!cterm->started)
		cterm_start(cterm);

	int minx = TERM_MINX;
	int miny = TERM_MINY;
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &minx, &miny);

	for (int i = 0; i < cterm->height; i++)
		cterm_line_to_scrollback(cterm, miny + i);
	clrscr();
	gotoxy(CURR_MINX, CURR_MINY);
}

/*
 * Parses an ESC sequence and returns it broken down
 */

struct esc_seq {
	char c1_byte;			// Character after the ESC.  If '[', ctrl_func and param_str will be non-NULL.
					// For nF sequences (ESC I...I F), this will be NUL and ctrl_func will be
					// non-NULL
	char final_byte;		// Final Byte (or NUL if c1 function);
	char *ctrl_func;		// Intermediate Bytes and Final Byte as NULL-terminated string.
	char *param_str;		// The parameter bytes
	int param_count;		// The number of parameters, or -1 if parameters were not parsed.
	str_list_t param;		// The parameters as strings
	uint64_t *param_int;	// The parameter bytes parsed as integers UINT64_MAX for default value.
};

struct sub_params {
	int param_count;		// The number of parameters, or -1 if parameters were not parsed.
	uint64_t *param_int;	// The parameter bytes parsed as integers UINT64_MAX for default value.
};

static bool parse_sub_parameters(struct sub_params *sub, struct esc_seq *seq, unsigned param)
{
	int i;
	char *p;

	sub->param_count = 1;
	sub->param_int = NULL;

	if (param >= seq->param_count)
		return false;
	for (p=seq->param[param]; *p; p++) {
		if (*p == ':') {
			if (sub->param_count == INT_MAX)
				return false;
			sub->param_count++;
		}
	}
	if (sub->param_count == 0)
		return true;
	sub->param_int = malloc(sub->param_count * sizeof(sub->param_int[0]));
	if (sub->param_int == NULL)
		return false;
	p = seq->param[param];
	for (i=0; i<sub->param_count; i++) {
		p++;
		sub->param_int[i] = strtoull(p, &p, 10);
		if (*p != ':' && *p != 0) {
			free(seq->param_int);
			return false;
		}
	}
	return true;
}

static void free_sub_parameters(struct sub_params *sub)
{
	FREE_AND_NULL(sub->param_int);
}

static bool parse_parameters(struct esc_seq *seq)
{
	char *p;
	char *dup;
	char *start = NULL;
	bool last_was_sc = false;
	int i;

	if (seq == NULL)
		return false;
	if (seq->param_str == NULL)
		return false;

	dup = strdup(seq->param_str);
	if (dup == NULL)
		return false;
	p = dup;

	// First, skip past any private extension indicator...
	if (*p >= 0x3c && *p <= 0x3f)
		p++;

	seq->param_count = 0;

	seq->param = strListInit();

	for (; *p; p++) {
		/* Ensure it's a legal value */
		if (*p < 0x30 || *p > 0x3b) {
			seq->param_count = -1;
			strListFree(&seq->param);
			seq->param = NULL;
			free(dup);
			return false;
		}
		/* Mark the start of the parameter */
		if (start == NULL)
			start = p;

		if (*p == ';') {
			/* End of parameter, add to string list */
			last_was_sc = true;
			*p = 0;
			while(*start == '0' && start[1])
				start++;
			strListAppend(&seq->param, start, seq->param_count);
			if (seq->param_count == INT_MAX) {
				strListFree(&seq->param);
				seq->param = NULL;
				free(dup);
				return false;
			}
			seq->param_count++;
			start = NULL;
		}
		else
			last_was_sc = false;
	}
	/* If the string ended with a semi-colon, there's a final zero-length parameter */
	if (last_was_sc) {
		strListAppend(&seq->param, "", seq->param_count);
		if (seq->param_count == INT_MAX) {
			strListFree(&seq->param);
			seq->param = NULL;
			free(dup);
			return false;
		}
		seq->param_count++;
	}
	else if (start) {
		/* End of parameter, add to string list */
		*p = 0;
		while(*start == '0' && start[1])
			start++;
		strListAppend(&seq->param, start, seq->param_count);
		if (seq->param_count == INT_MAX) {
			strListFree(&seq->param);
			seq->param = NULL;
			free(dup);
			return false;
		}
		seq->param_count++;
	}

	seq->param_int = malloc(seq->param_count * sizeof(seq->param_int[0]));
	if (seq->param_int == NULL) {
			seq->param_count = -1;
			strListFree(&seq->param);
			seq->param = NULL;
			free(dup);
			return false;
	}

	/* Now, parse all the integer values... */
	for (i=0; i<seq->param_count; i++) {
		if (seq->param[i][0] == 0 || seq->param[i][0] == ':') {
			seq->param_int[i] = UINT64_MAX;
			continue;
		}
		seq->param_int[i] = strtoull(seq->param[i], NULL, 10);
		if (seq->param_int[i] == ULLONG_MAX) {
			seq->param_count = -1;
			strListFree(&seq->param);
			seq->param = NULL;
			FREE_AND_NULL(seq->param_int);
			free(dup);
			return false;
		}
	}

	free(dup);
	return true;
}

static void seq_default(struct esc_seq *seq, int index, uint64_t val)
{
	char	tmpnum[24];

	// Params not parsed
	if (seq->param_count == -1)
		return;

	/* Do we need to add on to get defaults? */
	if (index >= seq->param_count) {
		uint64_t *np;

		np = realloc(seq->param_int, (index + 1) * sizeof(seq->param_int[0]));
		if (np == NULL)
			return;
		seq->param_int = np;
		for (; seq->param_count <= index; seq->param_count++) {
			if (seq->param_count == index) {
				seq->param_int[index] = val;
				sprintf(tmpnum, "%" PRIu64, val);
				strListAppend(&seq->param, tmpnum, seq->param_count);
			}
			else {
				seq->param_int[seq->param_count] = UINT64_MAX;
				strListAppend(&seq->param, "", seq->param_count);
			}
		}
		return;
	}
	if (seq->param_int[index] == UINT64_MAX) {
		seq->param_int[index] = val;
		sprintf(tmpnum, "%" PRIu64, val);
		strListReplace(seq->param, index, tmpnum);
	}
}

static void free_sequence(struct esc_seq * seq)
{
	if (seq == NULL)
		return;
	if (seq->param)
		strListFree(&seq->param);
	FREE_AND_NULL(seq->param_int);
	seq->param_count = -1;
	FREE_AND_NULL(seq->param_str);
	FREE_AND_NULL(seq->ctrl_func);
	free(seq);
}

/*
 * Returns true if the sequence is legal so far
 */

static size_t
range_span(const char *s, char min, char max)
{
	size_t ret = 0;

	while (s[ret] >= min && s[ret] <= max)
		ret++;
	return ret;
}

static size_t
range_span_exclude(char exclude, const char *s, char min, char max)
{
	size_t ret = 0;

	while ((s[ret] >= min && s[ret] <= max) && s[ret] != exclude)
		ret++;
	return ret;
}

enum sequence_state {
	SEQ_BROKEN,
	SEQ_INCOMPLETE,
	SEQ_COMPLETE
};

static enum sequence_state
legal_sequence(const char *seq, size_t max_len)
{
	size_t intermediate_len;

	if (seq == NULL)
		return SEQ_BROKEN;

	if (seq[0] == 0)
		goto incomplete;

	/* Check that it's part of C1 set, part of the Independent control functions, or an nF sequence type (ECMA 35)*/
	intermediate_len = range_span(&seq[0], ' ', '/');
	if (seq[intermediate_len] == 0)
		goto incomplete;
	if (seq[intermediate_len] < 0x30 || seq[intermediate_len] > 0x7e)
		return SEQ_BROKEN;

	/* Check if it's CSI */
	if (seq[0] == '[') {
		size_t parameter_len;

		if (seq[1] >= '<' && seq[1] <= '?')
			parameter_len = range_span(&seq[1], '0', '?');
		else
			parameter_len = range_span(&seq[1], '0', ';');

		if (seq[1+parameter_len] == 0)
			goto incomplete;

		intermediate_len = range_span(&seq[1+parameter_len], ' ', '/');
		if (seq[1+parameter_len+intermediate_len] == 0)
			goto incomplete;

		if (seq[1+parameter_len+intermediate_len] < 0x40 || seq[1+parameter_len+intermediate_len] > 0x7e)
			return SEQ_BROKEN;
	}
	return SEQ_COMPLETE;

incomplete:
	if (strlen(seq) >= max_len)
		return SEQ_BROKEN;
	return SEQ_INCOMPLETE;
}

static enum sequence_state
st_vt_52_legal_sequence(const char *seq, size_t seqsz, size_t max_len)
{
	static const char *all_seconds = "ABCDEFGHIJKLMYZbcdefjklopqvw[\\=>\x1b";

	if (seq[0] == 0)
		return SEQ_INCOMPLETE;
	if (strchr(all_seconds, seq[0]) == NULL)
		return SEQ_BROKEN;
	switch (seq[0]) {
		case 'b':
		case 'c':
			if (seqsz == 2)
				return SEQ_COMPLETE;
			break;
		case 'Y':
			if (seqsz == 3 && seq[1] > 31 && seq[2] > 31)
				return SEQ_COMPLETE;
			if (seqsz == 3)
				return SEQ_BROKEN;
			break;
		default:
			return SEQ_COMPLETE;
	}
	return SEQ_INCOMPLETE;
}

static struct esc_seq *parse_sequence(const char *seq)
{
	struct esc_seq *ret;
	size_t intermediate_len;

	ret = calloc(1, sizeof(struct esc_seq));
	if (ret == NULL)
		return ret;
	ret->param_count = -1;

	/* Check that it's part of C1 set, part of the Independent control functions, or an nF sequence type (ECMA 35)*/
	intermediate_len = range_span(&seq[0], ' ', '/');
	if (seq[intermediate_len] == 0)
		goto fail;

	/* Validate C1 final byte */
	if (seq[intermediate_len] < 0x30 || seq[intermediate_len] > 0x7e)
		goto fail;

	if (intermediate_len == 0)
		ret->c1_byte = seq[intermediate_len];

	/* Check if it's CSI */
	if (seq[0] == '[') {
		size_t parameter_len;

		parameter_len = range_span(&seq[1], '0', '?');
		ret->param_str = malloc(parameter_len + 1);
		if (!ret->param_str)
			goto fail;
		memcpy(ret->param_str, &seq[1], parameter_len);
		ret->param_str[parameter_len] = 0;

		intermediate_len = range_span(&seq[1+parameter_len], ' ', '/');
		if (seq[1+parameter_len+intermediate_len] < 0x40 || seq[1+parameter_len+intermediate_len] > 0x7e)
			goto fail;
		ret->ctrl_func = malloc(intermediate_len + 2);
		if (!ret->ctrl_func)
			goto fail;
		memcpy(ret->ctrl_func, &seq[1+parameter_len], intermediate_len);

		ret->final_byte = ret->ctrl_func[intermediate_len] = seq[1+parameter_len+intermediate_len];
		/* Validate final byte */
		if (ret->final_byte < 0x40 || ret->final_byte > 0x7e)
			goto fail;

		ret->ctrl_func[intermediate_len+1] = 0;

		/*
		 * Is this a private extension?
		 * If so, return now, the caller can parse the parameter sequence itself
		 * if the standard format is used.
		 */
		if (ret->param_str[0] >= 0x3c && ret->param_str[0] <= 0x3f)
			return ret;

		if (!parse_parameters(ret))
			goto fail;
	}
	else {
		ret->ctrl_func = malloc(intermediate_len + 2);
		if (!ret->ctrl_func)
			goto fail;
		memcpy(ret->ctrl_func, seq, intermediate_len + 1);
		ret->ctrl_func[intermediate_len + 1] = 0;
	}
	return ret;

fail:
	FREE_AND_NULL(ret->ctrl_func);
	FREE_AND_NULL(ret->param_str);
	free(ret);
	return NULL;
}

static int
cterm_setpalette(struct cterminal *cterm, uint32_t entry, uint16_t r, uint16_t g, uint16_t b)
{
	const uint32_t palette_offset = cio_api.options & CONIO_OPT_EXTENDED_PALETTE ? 16 : 0;
	if (entry == 7 + palette_offset)
		cterm->default_fg_palette = (uint32_t)r >> 8 << 16 | (uint32_t)g >> 8 << 8 | (uint32_t)b >> 8;
	else if (entry == palette_offset)
		cterm->default_bg_palette = (uint32_t)r >> 8 << 16 | (uint32_t)g >> 8 << 8 | (uint32_t)b >> 8;
	return setpalette(entry, r, g, b);
}

static void parse_sixel_string(struct cterminal *cterm, bool finish)
{
	char *p = cterm->strbuf;
	char *end;
	int i, j, k;
	int vmode;
	int pos;
	struct text_info ti;
 	int palette_offset = 0;

	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		palette_offset = 16;

	if (cterm->strbuflen == 0) {
		if (finish)
			goto all_done;
		return;
	}

	end = p+cterm->strbuflen-1;

	if ((*end < '?' || *end > '~') && !finish)
		return;

	/*
	 * TODO: Sixel interaction with scrolling margins...
	 * This one is interesting because there is no real terminal
	 * which supported left/right margins *and* sixel graphics.
	 */
	while (p <= end) {
		if (*p >= '?' && *p <= '~') {
			unsigned data = *p - '?';

			cterm->sx_pixels_sent = 1;
			gettextinfo(&ti);
			vmode = find_vmode(ti.currmode);
			if (vmode == -1)
				return;
			if (cterm->sx_pixels == NULL) {
				cterm->sx_pixels = malloc(sizeof(struct ciolib_pixels));
				if (cterm->sx_pixels != NULL) {
					cterm->sx_pixels->pixels = malloc(sizeof(cterm->sx_pixels->pixels[0]) * cterm->sx_iv * ti.screenwidth * vparams[vmode].charwidth * 6);
					if (cterm->sx_pixels->pixels != NULL) {
						cterm->sx_pixels->pixelsb = NULL;
						cterm->sx_pixels->width = ti.screenwidth * vparams[vmode].charwidth;
						cterm->sx_pixels->height = cterm->sx_iv * 6;
						cterm->sx_mask = malloc(sizeof(struct ciolib_mask));
						if (cterm->sx_mask != NULL) {
							cterm->sx_mask->width = cterm->sx_pixels->width;
							cterm->sx_mask->height = cterm->sx_pixels->height;
							cterm->sx_mask->bits = malloc((cterm->sx_iv * 6 * ti.screenwidth * vparams[vmode].charwidth * 6 + 7)/8);
							if (cterm->sx_mask->bits != NULL)
								memset(cterm->sx_mask->bits, 0, (cterm->sx_iv * 6 * ti.screenwidth * vparams[vmode].charwidth * 6 + 7)/8);
							else {
								FREE_AND_NULL(cterm->sx_mask);
								free(cterm->sx_pixels->pixels);
								FREE_AND_NULL(cterm->sx_pixels);
							}
						}
						else {
							free(cterm->sx_pixels->pixels);
							FREE_AND_NULL(cterm->sx_pixels);
						}
					}
					else {
						FREE_AND_NULL(cterm->sx_pixels);
					}
				}
			}
			if (cterm->sx_pixels == NULL) {
				fprintf(stderr, "Error allocating memory for sixel data\n");
				return;
			}
			if (cterm->sx_x == cterm->sx_left && cterm->sx_height && cterm->sx_width && cterm->sx_first_pass) {
				/* Fill in the background of the line */
				for (i = 0; i < (cterm->sx_height > 6 ? 6 : cterm->sx_height); i++) {
					for (j = 0; j < cterm->sx_iv; j++) {
						for (k = 0; k < cterm->sx_ih; k++) {
							pos = i * cterm->sx_iv * cterm->sx_pixels->width + j * cterm->sx_pixels->width + cterm->sx_x + k;
							cterm->sx_pixels->pixels[pos] = cterm->sx_bg;
							cterm->sx_mask->bits[pos/8] |= (0x80 >> (pos % 8));
						}
					}
				}
			}
			if (cterm->sx_x < ti.screenwidth * vparams[vmode].charwidth) {
				for (i=0; i<6; i++) {
					if (data & (1<<i)) {
						for (j = 0; j < cterm->sx_iv; j++) {
							for (k = 0; k < cterm->sx_ih; k++) {
								pos = i * cterm->sx_iv * cterm->sx_pixels->width + j * cterm->sx_pixels->width + cterm->sx_x + k;
								cterm->sx_pixels->pixels[pos] = cterm->sx_fg;
								cterm->sx_mask->bits[pos/8] |= (0x80 >> (pos % 8));
							}
						}
					}
					else {
						if (cterm->sx_first_pass && !cterm->sx_trans) {
							for (j = 0; j < cterm->sx_iv; j++) {
								for (k = 0; k < cterm->sx_ih; k++) {
									pos = i * cterm->sx_iv * cterm->sx_pixels->width + j * cterm->sx_pixels->width + cterm->sx_x + k;
									cterm->sx_pixels->pixels[pos] = cterm->sx_bg;
									cterm->sx_mask->bits[pos/8] |= (0x80 >> (pos % 8));
								}
							}
						}
						else {
							for (j = 0; j < cterm->sx_iv; j++) {
								for (k = 0; k < cterm->sx_ih; k++) {
									pos = i * cterm->sx_iv * cterm->sx_pixels->width + j * cterm->sx_pixels->width + cterm->sx_x + k;
									if (cterm->sx_first_pass)
										cterm->sx_mask->bits[pos/8] &= ~(0x80 >> (pos % 8));
								}
							}
						}
					}
				}
				if (cterm->sx_x > cterm->sx_row_max_x)
					cterm->sx_row_max_x = cterm->sx_x;
			}

			cterm->sx_x+=cterm->sx_ih;
			if (cterm->sx_repeat)
				cterm->sx_repeat--;
			if (!cterm->sx_repeat)
				p++;
		}
		else {
			switch(*p) {
				case '"':	// Raster Attributes
					if (!cterm->sx_pixels_sent) {
						p++;
						cterm->sx_iv = strtoul(p, &p, 10);
						cterm->sx_height = cterm->sx_width = 0;
						if (*p == ';') {
							p++;
							cterm->sx_ih = strtoul(p, &p, 10);
						}
						if (*p == ';') {
							p++;
							cterm->sx_width = strtoul(p, &p, 10);
						}
						if (*p == ';') {
							p++;
							cterm->sx_height = strtoul(p, &p, 10);
						}
					}
					else
						p++;
					break;
				case '!':	// Repeat
					p++;
					if (!*p)
						continue;
					cterm->sx_repeat = strtoul(p, &p, 10);
					if (cterm->sx_repeat > 0x7fff)
						cterm->sx_repeat = 0x7fff;
					break;
				case '#':	// Colour Introducer
					p++;
					if (!*p)
						continue;
					cterm->sx_fg = strtoul(p, &p, 10) + TOTAL_DAC_SIZE + palette_offset;
					/* Do we want to redefine it while we're here? */
					if (*p == ';') {
						unsigned long t,r,g,b;

						p++;
						r=g=b=0;
						t = strtoul(p, &p, 10);
						if (*p == ';') {
							p++;
							r = strtoul(p, &p, 10);
						}
						if (*p == ';') {
							p++;
							g = strtoul(p, &p, 10);
						}
						if (*p == ';') {
							p++;
							b = strtoul(p, &p, 10);
						}
						if (t == 2)	// Only support RGB
							cterm_setpalette(cterm, cterm->sx_fg, UINT16_MAX * r / 100, UINT16_MAX * g / 100, UINT16_MAX * b / 100);
					}
					break;
				case '$':	// Graphics Carriage Return
					cterm->sx_x = cterm->sx_left;
					cterm->sx_first_pass = 0;
					p++;
					break;
				case '-':	// Graphics New Line
					{
						int max_row = TERM_MAXY;
						coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &max_row, NULL);
						gettextinfo(&ti);
						vmode = find_vmode(ti.currmode);

						if (vmode == -1)
							return;
						setpixels(cterm->sx_left, cterm->sx_y, cterm->sx_row_max_x, cterm->sx_y + 6 * cterm->sx_iv - 1, cterm->sx_left, 0, cterm->sx_left, 0, cterm->sx_pixels, cterm->sx_mask);
						cterm->sx_row_max_x = 0;

						if ((!(cterm->extattr & CTERM_EXTATTR_SXSCROLL)) && (((cterm->sx_y + 6 * cterm->sx_iv) + 6*cterm->sx_iv - 1) >= (cterm->y + max_row - 1) * vparams[vmode].charheight)) {
							p++;
							break;
						}

						cterm->sx_x = cterm->sx_left;
						cterm->sx_y += 6*cterm->sx_iv;
						if (cterm->sx_height)
							cterm->sx_height -= cterm->sx_height > 6 ? 6 : cterm->sx_height;
						while ((cterm->sx_y + 6 * cterm->sx_iv - 1) >= (cterm->y + max_row - 1) * vparams[vmode].charheight) {
							cond_scrollup(cterm);
							cterm->sx_y -= vparams[vmode].charheight;
						}
						cterm->sx_first_pass = 1;
						p++;
					}
					break;
				default:
					p++;
			}
		}
	}
	cterm->strbuflen = 0;
	if (finish)
		goto all_done;
	return;

all_done:
	gettextinfo(&ti);
	vmode = find_vmode(ti.currmode);

	if (cterm->sx_row_max_x) {
		setpixels(cterm->sx_left, cterm->sx_y, cterm->sx_row_max_x, cterm->sx_y + 6 * cterm->sx_iv - 1, cterm->sx_left, 0, cterm->sx_left, 0, cterm->sx_pixels, cterm->sx_mask);
	}

	hold_update=cterm->sx_hold_update;

	/* Finish off the background */
	cterm->sx_x = cterm->sx_left;
	cterm->sx_y += 6 * cterm->sx_iv;
	if (cterm->sx_height)
		cterm->sx_height -= cterm->sx_height > 6 ? 6 : cterm->sx_height;

	if (cterm->sx_height && cterm->sx_width) {
		struct ciolib_pixels px;

		px.pixels = malloc(sizeof(px.pixels[0])*cterm->sx_width*cterm->sx_height*cterm->sx_iv*cterm->sx_ih);
		if (px.pixels) {
			px.height = cterm->sx_height;
			px.width = cterm->sx_width;
			for (i = 0; i<cterm->sx_height*cterm->sx_iv; i++) {
				for (j = 0; j < cterm->sx_width*cterm->sx_ih; j++)
					px.pixels[i*cterm->sx_width*cterm->sx_ih + j] = cterm->sx_bg;
			}
			setpixels(cterm->sx_x, cterm->sx_y, cterm->sx_x + cterm->sx_width - 1, cterm->sx_y + cterm->sx_height - 1, 0, 0, 0, 0, &px, NULL);
			free(px.pixels);
		}
		else {
			fprintf(stderr, "Error allocating memory for sixel data\n");
		}
	}

	if (cterm->extattr & CTERM_EXTATTR_SXSCROLL) {
		if (vmode != -1) {
			cterm->sx_x = cterm->sx_x / vparams[vmode].charwidth + 1;
			cterm->sx_x -= (cterm->x - 1);

			cterm->sx_y = (cterm->sx_y - 1) / vparams[vmode].charheight + 1;
			cterm->sx_y -= (cterm->y - 1);

			cterm_gotoxy(cterm, cterm->sx_x, cterm->sx_y);
		}
	}
	else {
		cterm_gotoxy(cterm, cterm->sx_start_x, cterm->sx_start_y);
	}
	cterm->cursor = cterm->sx_orig_cursor;
	ciolib_setcursortype(cterm->cursor);
	cterm->sixel = SIXEL_INACTIVE;
	if (cterm->sx_pixels)
		FREE_AND_NULL(cterm->sx_pixels->pixels);
	if (cterm->sx_mask)
		FREE_AND_NULL(cterm->sx_mask->bits);
	FREE_AND_NULL(cterm->sx_pixels);
	FREE_AND_NULL(cterm->sx_mask);
}

static int
is_hex(char ch)
{
	if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))
		return 1;
	return 0;
}

static int
get_hexstrlen(char *str, char *end)
{
	int ret = 0;

	for (;str <= end; str++) {
		if (is_hex(*str)) {
			if (str == end)
				return -1;
			if (!is_hex(*(str+1)))
				return -1;
			ret++;
			str++;
		}
		else
			return ret;
	}
	return ret;
}

static int
nibble_val(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a') + 10;
	if (ch >= 'A' && ch <= 'F')
		return (ch - 'A') + 10;
	return -1;
}

static void
get_hexstr(char *str, char *end, char *out)
{
	for (;str <= end; str++) {
		if (is_hex(*str)) {
			if (str == end)
				return;
			if (!is_hex(*(str+1)))
				return;
			*(out++) = nibble_val(*str) << 4 | nibble_val(*(str + 1));
			str++;
		}
		else
			return;
	}
}

#define MAX_MACRO_LEN 0xfffff
static void
parse_macro_string(struct cterminal *cterm, bool finish)
{
	char *p = cterm->strbuf;
	char *end = NULL;
	int i;

	if (cterm->strbuflen == 0) {
		if (finish)
			goto all_done;
		return;
	}

	end = p+cterm->strbuflen-1;

	if (*end >= '\x08' && *end <= '\x0d') {
		cterm->strbuflen--;
		if (finish)
			goto all_done;
		return;
	}
	if (cterm->macro_encoding == MACRO_ENCODING_ASCII) {
		if ((*end >= ' ' && *end <= '\x7e') || (*end >= '\xa0' && *end <= '\xff')) {
			if (finish)
				goto all_done;
			return;
		}
	}
	if (cterm->macro_encoding == MACRO_ENCODING_HEX &&
	    (is_hex(*end) || (*end == '!') || (*end == ';'))) {
		if (finish)
			goto all_done;
		return;
	}

	cterm->macro = MACRO_INACTIVE;
	return;

all_done:
	if (cterm->macro_del == MACRO_DELETE_ALL) {
		for (i = 0; i < (sizeof(cterm->macros) / sizeof(cterm->macros[0])); i++) {
			FREE_AND_NULL(cterm->macros[i]);
			cterm->macro_lens[i] = 0;
		}
	}
	else {
		FREE_AND_NULL(cterm->macros[cterm->macro_num]);
		cterm->macro_lens[cterm->macro_num] = 0;
	}
	if (cterm->strbuflen == 0)
		return;
	if (cterm->strbuflen > MAX_MACRO_LEN)
		return;
	if (cterm->macro_encoding == MACRO_ENCODING_ASCII) {
		cterm->macros[cterm->macro_num] = malloc(cterm->strbuflen + 1);
		if (cterm->macros[cterm->macro_num]) {
			cterm->macro_lens[cterm->macro_num] = cterm->strbuflen;
			memcpy(cterm->macros[cterm->macro_num], cterm->strbuf, cterm->strbuflen);
			cterm->macros[cterm->macro_num][cterm->strbuflen] = 0;
		}
	}
	else {
		// Hex string...
		int plen;
		unsigned long ul;
		size_t mlen = 0;
		char *out;

		if (end == NULL)
			return;
		// First, calculate the required length...
		for (p = cterm->strbuf; p <= end;) {
			if (*p == '!') {
				if (p == end)
					return;
				p++;
				if (p == end)
					return;
				if (*p == ';')
					ul = 1;
				else {
					if (memchr(p, ';', cterm->strbuflen - (p - cterm->strbuf)) == NULL)
						return;
					ul = strtoul(p, &p, 10);
					if (*p != ';')
						return;
					if (ul > MAX_MACRO_LEN)
						return;
					p++;
				}
				plen = get_hexstrlen(p, end);
				if (plen < 0)
					return;
				if (plen > MAX_MACRO_LEN)
					return;
				p += plen * 2;
				mlen += ul * plen;
				if (mlen > MAX_MACRO_LEN)
					return;
				if (p <= end) {
					if (*p == ';')
						p++;
					else
						return;
				}
			}
			else {
				plen = get_hexstrlen(p, end);
				if (plen < 0)
					return;
				if (plen > MAX_MACRO_LEN)
					return;
				p += plen * 2;
				mlen += plen;
				if (mlen > MAX_MACRO_LEN)
					return;
			}
		}
		cterm->macros[cterm->macro_num] = malloc(mlen + 1);
		if (cterm->macros[cterm->macro_num] == NULL)
			return;
		cterm->macro_lens[cterm->macro_num] = mlen;
		out = cterm->macros[cterm->macro_num];
		for (p = cterm->strbuf; p <= end;) {
			if (*p == '!') {
				p++;
				if (*p == ';')
					ul = 1;
				else {
					ul = strtoul(p, &p, 10);
					if (ul > MAX_MACRO_LEN) {
						FREE_AND_NULL(cterm->macros[cterm->macro_num]);
						return;
					}
					p++;
				}
				plen = get_hexstrlen(p, end);
				if (plen < 0) {
					FREE_AND_NULL(cterm->macros[cterm->macro_num]);
					return;
				}
				if (plen > MAX_MACRO_LEN) {
					FREE_AND_NULL(cterm->macros[cterm->macro_num]);
					return;
				}
				for (i = 0; i < ul; i++) {
					get_hexstr(p, end, out);
					out += plen;
				}
				p += plen * 2;
				if (p <= end && *p == ';')
					p++;
			}
			else {
				plen = get_hexstrlen(p, end);
				if (plen < 0) {
					FREE_AND_NULL(cterm->macros[cterm->macro_num]);
					return;
				}
				if (plen > MAX_MACRO_LEN) {
					FREE_AND_NULL(cterm->macros[cterm->macro_num]);
					return;
				}
				get_hexstr(p, end, out);
				out += plen;
				p += plen * 2;
			}
		}
	}
}

static void save_extended_colour_seq(struct cterminal *cterm, int fg, struct esc_seq *seq, int seqoff, int count)
{
	char **str = fg ? &cterm->fg_tc_str : &cterm->bg_tc_str;

	if (*str)
		FREE_AND_NULL(*str);

	switch (count) {
		case 1:
			if(asprintf(str, "%s", seq->param[seqoff]) < 0)
				*str = NULL;
			break;
		case 2:
			if(asprintf(str, "%s;%s", seq->param[seqoff], seq->param[seqoff+1]) < 0)
				*str = NULL;
			break;
		case 3:
			if(asprintf(str, "%s;%s;%s", seq->param[seqoff], seq->param[seqoff+1], seq->param[seqoff+2]) < 0)
				*str = NULL;
			break;
		case 5:
			if(asprintf(str, "%s;%s;%s;%s;%s", seq->param[seqoff], seq->param[seqoff+1], seq->param[seqoff+2], seq->param[seqoff+3], seq->param[seqoff+4]) < 0)
				*str = NULL;
			break;
	}
}

static void parse_extended_colour(struct esc_seq *seq, int *i, struct cterminal *cterm, bool fg)
{
	struct sub_params sub = {0};
	uint32_t nc;
	uint32_t *co;
 	int palette_offset = 0;

	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		palette_offset = 16;

	if (seq == NULL || cterm == NULL || i == NULL)
		return;
	if (fg)
		FREE_AND_NULL(cterm->fg_tc_str);
	else
		FREE_AND_NULL(cterm->bg_tc_str);
	co = fg ? (&cterm->fg_color) : (&cterm->bg_color);
	if (*i>=seq->param_count)
		return;

	if (seq->param[*i][2] == ':') {
		// CSI 38 : 5 : X m variant
		// CSI 38 : 2 : Z? : R : G : B m variant

		if (parse_sub_parameters(&sub, seq, *i)) {
			if (sub.param_count == 3 && sub.param_int[1] == 5) {
				*co = sub.param_int[2];
				save_extended_colour_seq(cterm, fg, seq, *i, 1);
			}
			else if (sub.param_count >= 5 && sub.param_int[1] == 2) {
				if (sub.param_count == 5) {
					nc = map_rgb(sub.param_int[2]<<8, sub.param_int[3]<<8, sub.param_int[4]<<8);
					if (nc != UINT32_MAX)
						*co = nc;
					save_extended_colour_seq(cterm, fg, seq, *i, 1);
				}
				else if (sub.param_count > 5) {
					nc = map_rgb(sub.param_int[3]<<8, sub.param_int[4]<<8, sub.param_int[5]<<8);
					if (nc != UINT32_MAX)
						*co = nc;
					save_extended_colour_seq(cterm, fg, seq, *i, 1);
				}
			}
		}
	}
	else if ((*i)+1 < seq->param_count && seq->param_int[(*i)+1] == 5 && seq->param[(*i)+1][1] == ':') {
		// CSI 38 ; 5 : X m variant
		if (parse_sub_parameters(&sub, seq, (*i)+1)) {
			if (sub.param_count == 2)
				*co = sub.param_int[1];
			save_extended_colour_seq(cterm, fg, seq, *i, 2);
			(*i)++;
		}
	}
	else if ((*i)+2 < seq->param_count && seq->param_int[(*i)+1] == 5) {
		// CSI 38 ; 5 ; X m variant
		*co = seq->param_int[(*i)+2] + palette_offset;
		save_extended_colour_seq(cterm, fg, seq, *i, 3);
		*i+=2;
	}
	else if ((*i)+1 < seq->param_count && seq->param_int[(*i)+1] == 2 && seq->param[(*i)+1][1] == ':') {
		// CSI 38 ; 2 : Z? : R : G : B m variant
		if (parse_sub_parameters(&sub, seq, (*i)+1)) {
			nc = UINT32_MAX;
			if (sub.param_count > 4)
				nc = map_rgb(sub.param_int[2]<<8, sub.param_int[3]<<8, sub.param_int[4]<<8);
			else if (sub.param_count == 4)
				nc = map_rgb(sub.param_int[1]<<8, sub.param_int[2]<<8, sub.param_int[3]<<8);
			if (nc != UINT32_MAX)
				*co = nc;
			save_extended_colour_seq(cterm, fg, seq, *i, 2);
			*i += 1;
		}
	}
	else if ((*i)+4 < seq->param_count && seq->param_int[(*i)+1] == 2) {
		// CSI 38 ; 2 ; R ; G ; B m variant
		nc = map_rgb(seq->param_int[(*i)+2]<<8, seq->param_int[(*i)+3]<<8, seq->param_int[(*i)+4]<<8);
		if (nc != UINT32_MAX)
			*co = nc;
		save_extended_colour_seq(cterm, fg, seq, *i, 5);
		*i += 4;
	}
	free_sub_parameters(&sub);
}

static void
do_tab(struct cterminal *cterm)
{
	int i;
	int cx, cy, ox;
	int lm, rm, bm;

	rm = TERM_MAXX;
	bm = TERM_MAXY;
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &rm, &bm);
	lm = TERM_MINX;
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &lm, NULL);

	CURR_XY(&cx, &cy);
	ox = cx;
	for (i = 0; i < cterm->tab_count; i++) {
		if (cterm->tabs[i] > cx) {
			cx = cterm->tabs[i];
			break;
		}
	}
	if ((ox > rm && cx > CURR_MAXX) || (ox <= rm && cx > rm) || i == cterm->tab_count) {
		cx = lm;
		if (cy == bm) {
			cond_scrollup(cterm);
			cy = bm;
		}
		else if(cy < CURR_MAXY)
			cy++;
	}
	gotoxy(cx, cy);
}

static void
do_backtab(struct cterminal *cterm)
{
	int i;
	int cx, cy, ox;
	int lm;

	lm = TERM_MINX;
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &lm, NULL);

	CURR_XY(&cx, &cy);
	ox = cx;
	for (i = cterm->tab_count - 1; i >= 0; i--) {
		if (cterm->tabs[i] < cx) {
			cx = cterm->tabs[i];
			break;
		}
	}
	if (ox >= lm && cx < lm)
		cx = lm;
	gotoxy(cx, cy);
}

static void
adjust_currpos(struct cterminal *cterm, int xadj, int yadj, int scroll)
{
	int x, y;
	int tx, ty;

	/*
	 * If we're inside the scrolling margins, we simply move inside
	 * them, and scrolling works.
	 */
	TERM_XY(&tx, &ty);
	if (tx >= TERM_MINX && tx <= TERM_MAXX &&
	    ty >= TERM_MINY && ty <= TERM_MAXY) {
		if (xadj == INT_MIN)
			tx = TERM_MINX;
		else
			tx += xadj;
		if (yadj == INT_MIN)
			ty = TERM_MINX;
		else
			ty += yadj;
		if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB) {
			while (ty > TERM_MAXY) {
				ty -= TERM_MAXY;
			}
			while (ty < TERM_MINY) {
				ty += TERM_MAXY;
			}
		}
		if (scroll) {
			while(ty > TERM_MAXY) {
				cterm_scrollup(cterm);
				ty--;
			}
			while(ty < TERM_MINY) {
				scrolldown(cterm);
				ty++;
			}
		}
		else {
			if (ty > TERM_MAXY)
				ty = TERM_MAXY;
			if (ty < TERM_MINY)
				ty = TERM_MINY;
		}
		if (tx > TERM_MAXX)
			tx = TERM_MAXX;
		if (tx < TERM_MINX)
			tx = TERM_MINX;
		cterm_gotoxy(cterm, tx, ty);
		return;
	}
	/*
	 * Outside of the scrolling margins, we can cross a margin
	 * into the scrolling region, but can't cross going out.
	 */
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &tx, &ty);
	x = tx;
	y = ty;
	if (xadj == INT_MIN)
		tx = TERM_MINX;
	else
		tx += xadj;
	if (yadj == INT_MIN)
		ty = TERM_MINX;
	else
		ty += yadj;
	if (x <= cterm->right_margin && tx > cterm->right_margin)
		tx = cterm->right_margin;
	if (y <= cterm->bottom_margin && ty > cterm->bottom_margin)
		ty = cterm->bottom_margin;
	if (tx < cterm->left_margin && x >= cterm->left_margin)
		tx = cterm->left_margin;
	if (ty < cterm->top_margin && y >= cterm->top_margin)
		ty = cterm->top_margin;
	gotoxy(tx, ty);
	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)
		prestel_get_state(cterm);
}

static int
skypix_color(struct cterminal *cterm, int color)
{
	if (!cterm->skypix)
		return color;
	switch(color) {
		case BLACK:
			return 0;
		case RED:
			return 3;
		case GREEN:
			return 4;
		case BROWN:
			return 6;
		case BLUE:
			return 1;
		case MAGENTA:
			return 7;
		case CYAN:
			return 5;
		case LIGHTGRAY:
			return 2;
		case DARKGRAY:
			return 8;
		case LIGHTRED:
			return 11;
		case LIGHTGREEN:
			return 12;
		case YELLOW:
			return 14;
		case LIGHTBLUE:
			return 9;
		case LIGHTMAGENTA:
			return 15;
		case LIGHTCYAN:
			return 13;
		case WHITE:
			return 10;
	}
	return color;
}

static void
clear_lcf(struct cterminal *cterm)
{
	cterm->last_column_flag &= ~CTERM_LCF_SET;
}

static void
set_negative(struct cterminal *cterm, bool on)
{
	unsigned char bg;
	unsigned char fg;
	uint32_t tmp_colour;

	// TODO: This one is great...
	if (on != cterm->negative) {
		if (cterm->emulation == CTERM_EMULATION_ATARIST_VT52) {
			bg = cterm->attr & 0xF0;
			fg = cterm->attr & 0x0F;
			cterm->attr = (bg >> 4) | (fg << 4);
			tmp_colour = cterm->fg_color;
			cterm->fg_color = cterm->bg_color;
			cterm->bg_color = tmp_colour;
			cterm->negative = on;
		}
		else {
			bg = cterm->attr & 0x70;
			fg = cterm->attr & 0x07;
			cterm->attr &= 0x88;
			cterm->attr |= (bg >> 4);
			cterm->attr |= (fg << 4);
			tmp_colour = cterm->fg_color;
			cterm->fg_color = cterm->bg_color;
			cterm->bg_color = tmp_colour;
			cterm->negative = on;
		}
	}
}

static void
set_bright_fg(struct cterminal *cterm, unsigned char colour, int flags)
{
	if (!(flags & CIOLIB_VIDEO_NOBRIGHT)) {
		set_fgattr(cterm, colour);
		if (!cterm->skypix)
			cterm->attr|=8;
		attr2palette(cterm->attr, cterm->negative ? NULL : &cterm->fg_color, cterm->negative ? &cterm->fg_color : NULL);
		FREE_AND_NULL(cterm->fg_tc_str);
	}
}

static void
set_bright_bg(struct cterminal *cterm, unsigned char colour, int flags)
{
	if (flags & CIOLIB_VIDEO_BGBRIGHT) {
		set_attr(cterm, colour, true ^ cterm->negative);
		if (!cterm->skypix)
			cterm->attr|=128;
		attr2palette(cterm->attr, cterm->negative ? &cterm->bg_color : NULL, cterm->negative ? NULL : &cterm->bg_color);
		FREE_AND_NULL(cterm->bg_tc_str);
	}
}

static void
do_st_vt_52_attr(struct cterminal *cterm, char attr, bool bg)
{
	if (cterm->negative)
		bg = !bg;
	if (bg) {
		cterm->attr = (attr & 0x0F) << 4 | (cterm->attr & 0x0F);
		attr2palette(cterm->attr, NULL, &cterm->bg_color);
	}
	else {
		cterm->attr = (attr & 0x0F) | (cterm->attr & 0xF0);
		attr2palette(cterm->attr, &cterm->fg_color, NULL);
	}
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
}

static void
do_st_vt_52(struct cterminal *cterm, char *retbuf, size_t retsize)
{
	int x, x2, maxx;
	int y, y2, maxy;
	int i;

	switch(cterm->escbuf[0]) {
		// Standard VT-52 commands
		case '\x1b':
			// ESC ESC does nothing.
			break;
		case '=':
			cterm->extattr |= CTERM_EXTATTR_ALTERNATE_KEYPAD;
			break;
		case '>':
			cterm->extattr &= ~CTERM_EXTATTR_ALTERNATE_KEYPAD;
			break;
		case 'A':
			adjust_currpos(cterm, 0, -1, 0);
			break;
		case 'B':
			adjust_currpos(cterm, 0, 1, 0);
			break;
		case 'C':
			adjust_currpos(cterm, 1, 0, 0);
			break;
		case 'D':
			adjust_currpos(cterm, -1, 0, 0);
			break;
		case 'F':
		case 'G':
			// TODO: Graphics mode...
			// Maybe not supported by ST?
			break;
		case 'H':
			gotoxy(CURR_MINX, CURR_MINY);
			break;
		case 'I':
			adjust_currpos(cterm, 0, -1, 1);
			break;
		case 'J':
			clreol();
			CURR_XY(&x, &y);
			for (i = y + 1; i <= TERM_MAXY; i++) {
				cterm_gotoxy(cterm, TERM_MINY, i);
				clreol();
			}
			gotoxy(x, y);
			break;
		case 'K':
			clreol();
			break;
		case 'Y':
			y = cterm->escbuf[1] - 32;
			if (y < 0)
				break;
			if (y > 23)
				CURR_XY(NULL, &y);
			x = cterm->escbuf[2] - 32;
			if (x < 0)
				break;
			if (x > 79)
				x = 79;
			gotoxy(CURR_MINX + x, CURR_MINY + y);
			break;
		//case 'Z': // Ident, not supported by Atari ST
		//	if(retbuf && strlen(retbuf) + 3 < retsize)
		//		strcat(retbuf, "\x1b/K");
		//	break;
		case '[':
		case '\\':
			// TODO: Hold-screen mode
			// Maybe not supported by ST?
			break;
		// GEMDOS/TOS extensions
		case 'E':
			cterm_clearscreen(cterm, (char)cterm->attr);
			gotoxy(CURR_MINX, CURR_MINY);
			break;
		case 'L': // Insert line
			TERM_XY(&x, &y);
			if(y < TERM_MINY || y > TERM_MAXY || x < TERM_MINX || x > TERM_MAXX)
				break;
			x2 = TERM_MINX;
			y2 = y;
			coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &x2, &y2);
			maxx = TERM_MAXX;
			maxy = TERM_MAXY;
			coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &maxx, &maxy);
			movetext(x2, y2, maxx, maxy - 1, x2, y2 + 1);
			cterm_gotoxy(cterm, TERM_MINX, y);
			cterm_clreol(cterm);
			cterm_gotoxy(cterm, x, y);
			break;
		case 'M': // Delete line
			dellines(cterm, 1);
			break;
		case 'b':
			do_st_vt_52_attr(cterm, cterm->escbuf[1], false);
			break;
		case 'c':
			do_st_vt_52_attr(cterm, cterm->escbuf[1], true);
			break;
		case 'd':
			clear2bol(cterm);
			CURR_XY(&x, &y);
			for (i = y - 1; i >= TERM_MINY; i--) {
				cterm_gotoxy(cterm, TERM_MINX, i);
				clreol();
			}
			gotoxy(x, y);
			break;
		case 'e':
			cterm->cursor=_NORMALCURSOR;
			ciolib_setcursortype(cterm->cursor);
			break;
		case 'f':
			cterm->cursor=_NOCURSOR;
			ciolib_setcursortype(cterm->cursor);
			break;
		case 'j':
			CURR_XY(&cterm->save_xpos, &cterm->save_ypos);
			break;
		case 'k':
			if(cterm->save_ypos>0 && cterm->save_ypos<=cterm->height
					&& cterm->save_xpos>0 && cterm->save_xpos<=cterm->width) {
				if(cterm->save_ypos < CURR_MINY || cterm->save_ypos > CURR_MAXY || cterm->save_xpos < CURR_MINX || cterm->save_xpos > CURR_MAXX)
					break;
				gotoxy(cterm->save_xpos, cterm->save_ypos);
			}
			break;
		case 'l':
			CURR_XY(&x, &y);
			gotoxy(CURR_MINX, y);
			clreol();
			gotoxy(x, y);
			break;
		case 'o':
			clear2bol(cterm);
			break;
		case 'p':
			set_negative(cterm, true);
			textattr(cterm->attr);
			setcolour(cterm->fg_color, cterm->bg_color);
			break;
		case 'q':
			set_negative(cterm, false);
			textattr(cterm->attr);
			setcolour(cterm->fg_color, cterm->bg_color);
			break;
		case 'v':
			cterm->extattr |= CTERM_EXTATTR_AUTOWRAP;
			break;
		case 'w':
			cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
			break;
	}
	cterm->escbuf[0]=0;
	cterm->escbufsz = 0;
	cterm->sequence=0;
}

static void do_ansi(struct cterminal *cterm, char *retbuf, size_t retsize, int *speed, char last)
{
	char	*p;
	char	*p2;
	char	tmp[1024];
	int		i,j,k,l;
	int	flags;
	int		row,col;
	int		row2,col2;
	int		max_row;
	int		max_col;
	struct text_info ti;
	struct esc_seq *seq;
	uint32_t oldfg, oldbg;
	bool updfg, updbg;
	struct vmem_cell *vc;
	int palette_offset = 0;

	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		palette_offset = 16;
	seq = parse_sequence(cterm->escbuf);
	if (seq != NULL) {
		switch(seq->c1_byte) {
			case '[':
				/* ANSI stuff */
				p=cterm->escbuf+strlen(cterm->escbuf)-1;
				if(seq->param_str[0]>=60 && seq->param_str[0] <= 63) {	/* Private extensions */
					switch(seq->final_byte) {
						case 'M':
							if(seq->param_str[0] == '=' && parse_parameters(seq)) {	/* ANSI Music setup */
								seq_default(seq, 0, 0);
								switch(seq->param_int[0]) {
									case 0:					/* Disable ANSI Music */
										cterm->music_enable=CTERM_MUSIC_SYNCTERM;
										break;
									case 1:					/* BANSI (ESC[N) music only) */
										cterm->music_enable=CTERM_MUSIC_BANSI;
										break;
									case 2:					/* ESC[M ANSI music */
										cterm->music_enable=CTERM_MUSIC_ENABLED;
										break;
								}
							}
							break;
						case 'S':		// XTerm graphics query
							if (seq->param_str[0] == '?' && parse_parameters(seq)) {
								seq_default(seq, 0, 0);
								seq_default(seq, 1, 0);
								if (seq->param_int[0] == 2 && seq->param_int[1] == 1) {
									struct text_info ti;
									int vmode;

									tmp[0] = 0;
									gettextinfo(&ti);
									vmode = find_vmode(ti.currmode);
									if (vmode != -1)
										sprintf(tmp, "\x1b[?2;0;%u;%uS", vparams[vmode].charwidth*cterm->width, vparams[vmode].charheight*cterm->height);
									if(retbuf && *tmp && strlen(retbuf) + strlen(tmp) < retsize)
										strcat(retbuf, tmp);
								}
							}
							break;
						case 'c':
							/* SyncTERM Device Attributes */
							if (seq->param_str[0] == '<' && parse_parameters(seq)) {
								seq_default(seq, 0, 0);
								tmp[0] = 0;
								switch (seq->param_int[0]) {
									case 0:		/* Advanced features */
										strcpy(tmp, "\x1b[<0");
										if (cio_api.options & CONIO_OPT_LOADABLE_FONTS)
											strcat(tmp, ";1");
										if (cio_api.options & CONIO_OPT_BRIGHT_BACKGROUND)
											strcat(tmp, ";2");
										if (cio_api.options & CONIO_OPT_PALETTE_SETTING)
											strcat(tmp, ";3");
										if (cio_api.options & CONIO_OPT_SET_PIXEL)
											strcat(tmp, ";4");
										if (cio_api.options & CONIO_OPT_FONT_SELECT)
											strcat(tmp, ";5");
										if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
											strcat(tmp, ";6");
										if (cio_api.mouse)
											strcat(tmp, ";7");
										strcat(tmp, "c");
								}
								if(retbuf && *tmp && strlen(retbuf) + strlen(tmp) < retsize)
									strcat(retbuf, tmp);
							}
							break;
						case 'h':
							if (seq->param_str[0] == '?' && parse_parameters(seq)) {
								attr2palette(cterm->attr, &oldfg, &oldbg);
								updfg = (oldfg == cterm->fg_color);
								updbg = (oldbg == cterm->bg_color);
								for (i=0; i<seq->param_count; i++) {
									seq_default(seq, i, 0);
									switch(seq->param_int[i]) {
										case 6:
											clear_lcf(cterm);
											cterm->extattr |= CTERM_EXTATTR_ORIGINMODE;
											setwindow(cterm);
											break;
										case 7:
											cterm->extattr |= CTERM_EXTATTR_AUTOWRAP;
											break;
										case 25:
											cterm->cursor=_NORMALCURSOR;
											ciolib_setcursortype(cterm->cursor);
											break;
										case 31:
											flags = getvideoflags();
											flags |= CIOLIB_VIDEO_ALTCHARS;
											setvideoflags(flags);
											break;
										case 32:
											flags = getvideoflags();
											flags |= CIOLIB_VIDEO_NOBRIGHT;
											setvideoflags(flags);
											break;
										case 33:
											flags = getvideoflags();
											flags |= CIOLIB_VIDEO_BGBRIGHT;
											setvideoflags(flags);
											break;
										case 34:
											flags = getvideoflags();
											flags |= CIOLIB_VIDEO_BLINKALTCHARS;
											setvideoflags(flags);
											break;
										case 35:
											flags = getvideoflags();
											flags |= CIOLIB_VIDEO_NOBLINK;
											setvideoflags(flags);
											break;
										case 67:
											cterm->extattr |= CTERM_EXTATTR_DECBKM;
											break;
										case 69:
											cterm->extattr |= CTERM_EXTATTR_DECLRMM;
											break;
										case 80:
											cterm->extattr |= CTERM_EXTATTR_SXSCROLL;
											break;
										case 9:
										case 1000:
										case 1001:
										case 1002:
										case 1003:
										case 1004:
										case 1005:
										case 1006:
										case 1007:
										case 1015:
											if (cterm->mouse_state_change)
												cterm->mouse_state_change(seq->param_int[i], 1, cterm->mouse_state_change_cbdata);
											break;
										case 2004:
											cterm->extattr |= CTERM_EXTATTR_BRACKETPASTE;
											break;
									}
								}
								if (updfg || updbg) {
									attr2palette(cterm->attr, updfg ? &cterm->fg_color : NULL, updbg ? &cterm->bg_color : NULL);
									if (updfg) {
										if (cterm->negative)
											FREE_AND_NULL(cterm->bg_tc_str);
										else
											FREE_AND_NULL(cterm->fg_tc_str);
									}
									if (updbg) {
										if (cterm->negative)
											FREE_AND_NULL(cterm->fg_tc_str);
										else
											FREE_AND_NULL(cterm->bg_tc_str);
									}
								}
							}
							else if(seq->param_str[0] == '=' && parse_parameters(seq)) {
								for (i=0; i<seq->param_count; i++) {
									seq_default(seq, i, 0);
									switch(seq->param_int[i]) {
										case 4:
											cterm->last_column_flag |= CTERM_LCF_ENABLED;
											cterm->last_column_flag &= ~(CTERM_LCF_SET);
											break;
										case 5:
											cterm->last_column_flag |= CTERM_LCF_FORCED | CTERM_LCF_ENABLED;
											break;
										case 255:
											cterm->doorway_mode=1;
											break;
									}
								}
							}
							break;
						case 'l':
							if (seq->param_str[0] == '?' && parse_parameters(seq)) {
								attr2palette(cterm->attr, &oldfg, &oldbg);
								updfg = (oldfg == cterm->fg_color);
								updbg = (oldbg == cterm->bg_color);
								for (i=0; i<seq->param_count; i++) {
									seq_default(seq, i, 0);
									switch(seq->param_int[i]) {
										case 6:
											clear_lcf(cterm);
											cterm->extattr &= ~CTERM_EXTATTR_ORIGINMODE;
											setwindow(cterm);
											break;
										case 7:
											clear_lcf(cterm);
											cterm->extattr &= ~(CTERM_EXTATTR_AUTOWRAP);
											break;
										case 25:
											cterm->cursor=_NOCURSOR;
											ciolib_setcursortype(cterm->cursor);
											break;
										case 31:
											flags = getvideoflags();
											flags &= ~CIOLIB_VIDEO_ALTCHARS;
											setvideoflags(flags);
											break;
										case 32:
											flags = getvideoflags();
											flags &= ~CIOLIB_VIDEO_NOBRIGHT;
											setvideoflags(flags);
											break;
										case 33:
											flags = getvideoflags();
											flags &= ~CIOLIB_VIDEO_BGBRIGHT;
											setvideoflags(flags);
											break;
										case 34:
											flags = getvideoflags();
											flags &= ~CIOLIB_VIDEO_BLINKALTCHARS;
											setvideoflags(flags);
											break;
										case 35:
											flags = getvideoflags();
											flags &= ~CIOLIB_VIDEO_NOBLINK;
											setvideoflags(flags);
											break;
										case 67:
											cterm->extattr &= ~(CTERM_EXTATTR_DECBKM);
											break;
										case 69:
											cterm->extattr &= ~(CTERM_EXTATTR_DECLRMM);
											break;
										case 80:
											cterm->extattr &= ~CTERM_EXTATTR_SXSCROLL;
											break;
										case 9:
										case 1000:
										case 1001:
										case 1002:
										case 1003:
										case 1004:
										case 1005:
										case 1006:
										case 1007:
										case 1015:
											if (cterm->mouse_state_change)
												cterm->mouse_state_change(seq->param_int[i], 0, cterm->mouse_state_change_cbdata);
											break;
										case 2004:
											cterm->extattr &= ~(CTERM_EXTATTR_BRACKETPASTE);
											break;
									}
								}
								if (updfg || updbg) {
									attr2palette(cterm->attr, updfg ? &cterm->fg_color : NULL, updbg ? &cterm->bg_color : NULL);
									if (updfg) {
										if (cterm->negative)
											FREE_AND_NULL(cterm->bg_tc_str);
										else
											FREE_AND_NULL(cterm->fg_tc_str);
									}
									if (updbg) {
										if (cterm->negative)
											FREE_AND_NULL(cterm->fg_tc_str);
										else
											FREE_AND_NULL(cterm->bg_tc_str);
									}
								}
							}
							else if(seq->param_str[0] == '=' && parse_parameters(seq)) {
								for (i=0; i<seq->param_count; i++) {
									seq_default(seq, i, 0);
									switch(seq->param_int[i]) {
										case 4:
											if ((cterm->last_column_flag & CTERM_LCF_FORCED) == 0)
												cterm->last_column_flag = 0;
											break;
										case 255:
											cterm->doorway_mode=0;
											break;
									}
								}
							}
							break;
						case 'n':	/* Query (extended) state information */
							if (seq->param_str[0] == '=' && parse_parameters(seq)) {
								int vidflags;

								if(retbuf == NULL)
									break;
								tmp[0] = 0;
								if (seq->param_count > 1)
									break;
								seq_default(seq, 0, 1);
								switch(seq->param_int[0]) {
									case 1:
										sprintf(tmp, "\x1b[=1;%u;%u;%u;%u;%u;%un"
											,CONIO_FIRST_FREE_FONT
											,(uint8_t)cterm->setfont_result
											,(uint8_t)cterm->altfont[0]
											,(uint8_t)cterm->altfont[1]
											,(uint8_t)cterm->altfont[2]
											,(uint8_t)cterm->altfont[3]
										);
										break;
									case 2:
										vidflags = getvideoflags();
										strcpy(tmp, "\x1b[=2");
										if(cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
											strcat(tmp, ";6");
										if (cterm->extattr & CTERM_EXTATTR_AUTOWRAP)
											strcat(tmp, ";7");
										if (cterm->mouse_state_query(9, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";9");
										if(cterm->cursor == _NORMALCURSOR)
											strcat(tmp, ";25");
										if(vidflags & CIOLIB_VIDEO_ALTCHARS)
											strcat(tmp, ";31");
										if(vidflags & CIOLIB_VIDEO_NOBRIGHT)
											strcat(tmp, ";32");
										if(vidflags & CIOLIB_VIDEO_BGBRIGHT)
											strcat(tmp, ";33");
										if(vidflags & CIOLIB_VIDEO_BLINKALTCHARS)
											strcat(tmp, ";34");
										if(vidflags & CIOLIB_VIDEO_NOBLINK)
											strcat(tmp, ";35");
										if (cterm->extattr & CTERM_EXTATTR_DECLRMM)
											strcat(tmp, ";69");
										if (cterm->extattr & CTERM_EXTATTR_SXSCROLL)
											strcat(tmp, ";80");
										if (cterm->mouse_state_query(1000, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1000");
										if (cterm->mouse_state_query(1001, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1001");
										if (cterm->mouse_state_query(1002, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1002");
										if (cterm->mouse_state_query(1003, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1003");
										if (cterm->mouse_state_query(1004, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1004");
										if (cterm->mouse_state_query(1005, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1005");
										if (cterm->mouse_state_query(1006, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1006");
										if (cterm->mouse_state_query(1007, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1007");
										if (cterm->mouse_state_query(1015, cterm->mouse_state_query_cbdata))
											strcat(tmp, ";1015");
										if (strlen(tmp) == 4) {	// Nothing set
											strcat(tmp, ";");
										}
										strcat(tmp, "n");
										break;
									case 3:	/* Query font char dimensions */
									{
										struct text_info ti;
										int vmode;

										gettextinfo(&ti);
										vmode = find_vmode(ti.currmode);
										if (vmode != -1)
											sprintf(tmp, "\x1b[=3;%u;%un", vparams[vmode].charheight, vparams[vmode].charwidth);
										break;
									}
								}
								if(*tmp && strlen(retbuf) + strlen(tmp) < retsize)
									strcat(retbuf, tmp);
							}
							else if (seq->param_str[0] == '?' && parse_parameters(seq)) {
								if(retbuf == NULL)
									break;
								seq_default(seq, 0, 1);
								tmp[0] = 0;
								switch(seq->param_int[0]) {
									case 62: /* Query macro space available */
									{
										if (seq->param_count > 1)
											break;
										// Just fake it as int16_max
										strcpy(tmp, "\x1b[32767*{");
										break;
									}
									case 63: /* Quero macro space "checksum" */
									{
										uint16_t crc = 0;
										if (seq->param_count > 2)
											break;
										seq_default(seq, 1, 1);
										for (k = 0; k < (sizeof(cterm->macros) / sizeof(cterm->macros[0])); k++) {
											if (cterm->macros[k]) {
												for (i = 0; i <= cterm->macro_lens[k]; i++)
													crc = ucrc16(cterm->macros[k][i], crc);
											}
											else
												crc = ucrc16(0, crc);
										}
										*tmp = 0;
										snprintf(tmp, sizeof(tmp), "\x1bP%u!~%04X\x1b\\", (unsigned)seq->param_int[1], crc);
										break;
									}
								}
								if(*tmp && strlen(retbuf) + strlen(tmp) < retsize)
									strcat(retbuf, tmp);
							}
							break;
						case 's':
							if (seq->param_str[0] == '?' && parse_parameters(seq)) {
								gettextinfo(&ti);
								flags = getvideoflags();
								if(seq->param_count == 0) {
									/* All the save stuff... */
									cterm->saved_mode_mask |= (CTERM_SAVEMODE_AUTOWRAP|CTERM_SAVEMODE_CURSOR|CTERM_SAVEMODE_ALTCHARS|
									    CTERM_SAVEMODE_NOBRIGHT|CTERM_SAVEMODE_BGBRIGHT|CTERM_SAVEMODE_ORIGIN|CTERM_SAVEMODE_SIXEL_SCROLL|
									    CTERM_SAVEMODE_MOUSE_X10|CTERM_SAVEMODE_MOUSE_NORMAL|CTERM_SAVEMODE_MOUSE_HIGHLIGHT|
									    CTERM_SAVEMODE_MOUSE_BUTTONTRACK|CTERM_SAVEMODE_MOUSE_ANY|CTERM_SAVEMODE_MOUSE_FOCUS|
									    CTERM_SAVEMODE_MOUSE_UTF8|CTERM_SAVEMODE_MOUSE_SGR|CTERM_SAVEMODE_MOUSE_ALTSCROLL|CTERM_SAVEMODE_MOUSE_URXVT|CTERM_SAVEMODE_DECLRMM|CTERM_SAVEMODE_DECBKM);
									cterm->saved_mode &= ~(cterm->saved_mode_mask);
									cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_AUTOWRAP)?CTERM_SAVEMODE_AUTOWRAP:0;
									cterm->saved_mode |= (cterm->cursor==_NORMALCURSOR)?CTERM_SAVEMODE_CURSOR:0;
									cterm->saved_mode |= (flags & CIOLIB_VIDEO_ALTCHARS)?CTERM_SAVEMODE_ALTCHARS:0;
									cterm->saved_mode |= (flags & CIOLIB_VIDEO_NOBRIGHT)?CTERM_SAVEMODE_NOBRIGHT:0;
									cterm->saved_mode |= (flags & CIOLIB_VIDEO_BGBRIGHT)?CTERM_SAVEMODE_BGBRIGHT:0;
									cterm->saved_mode |= (flags & CIOLIB_VIDEO_BLINKALTCHARS)?CTERM_SAVEMODE_BLINKALTCHARS:0;
									cterm->saved_mode |= (flags & CIOLIB_VIDEO_NOBLINK)?CTERM_SAVEMODE_NOBLINK:0;
									cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)?CTERM_SAVEMODE_ORIGIN:0;
									cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_SXSCROLL)?CTERM_SAVEMODE_SIXEL_SCROLL:0;
									cterm->saved_mode |= (cterm->mouse_state_query(9, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_X10 : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1000, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_NORMAL : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1001, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_HIGHLIGHT : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1002, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_BUTTONTRACK : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1003, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_ANY : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1004, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_FOCUS : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1005, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_UTF8 : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1006, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_SGR : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1007, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_ALTSCROLL : 0);
									cterm->saved_mode |= (cterm->mouse_state_query(1015, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_URXVT : 0);
									cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_DECLRMM) ? CTERM_SAVEMODE_DECLRMM : 0;
									cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_DECBKM) ? CTERM_SAVEMODE_DECBKM : 0;
									setwindow(cterm);
									break;
								}
								else {
									for (i=0; i<seq->param_count; i++) {
										seq_default(seq, i, 0);
										switch(seq->param_int[i]) {
											case 6:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_ORIGIN;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_ORIGIN);
												cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)?CTERM_SAVEMODE_ORIGIN:0;
												setwindow(cterm);
												break;
											case 7:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_AUTOWRAP;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_AUTOWRAP);
												cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_AUTOWRAP)?CTERM_SAVEMODE_AUTOWRAP:0;
												break;
											case 9:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_X10;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_X10);
												cterm->saved_mode |= (cterm->mouse_state_query(9, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_X10 : 0);
												break;
											case 25:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_CURSOR;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_CURSOR);
												cterm->saved_mode |= (cterm->cursor==_NORMALCURSOR)?CTERM_SAVEMODE_CURSOR:0;
												break;
											case 31:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_ALTCHARS;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_ALTCHARS);
												cterm->saved_mode |= (flags & CIOLIB_VIDEO_ALTCHARS)?CTERM_SAVEMODE_ALTCHARS:0;
												break;
											case 32:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_NOBRIGHT;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_NOBRIGHT);
												cterm->saved_mode |= (flags & CIOLIB_VIDEO_NOBRIGHT)?CTERM_SAVEMODE_NOBRIGHT:0;
												break;
											case 33:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_BGBRIGHT;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_BGBRIGHT);
												cterm->saved_mode |= (flags & CIOLIB_VIDEO_BGBRIGHT)?CTERM_SAVEMODE_BGBRIGHT:0;
												break;
											case 34:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_BLINKALTCHARS;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_BLINKALTCHARS);
												cterm->saved_mode |= (flags & CIOLIB_VIDEO_BLINKALTCHARS)?CTERM_SAVEMODE_BLINKALTCHARS:0;
												break;
											case 35:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_NOBLINK;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_NOBLINK);
												cterm->saved_mode |= (flags & CIOLIB_VIDEO_NOBLINK)?CTERM_SAVEMODE_NOBLINK:0;
												break;
											case 67:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_DECBKM;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_DECBKM);
												cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_DECBKM) ? CTERM_SAVEMODE_DECBKM : 0;
												break;
											case 69:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_DECLRMM;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_DECLRMM);
												cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_DECLRMM) ? CTERM_SAVEMODE_DECLRMM : 0;
												break;
											case 80:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_SIXEL_SCROLL;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_SIXEL_SCROLL);
												cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_SXSCROLL)?CTERM_SAVEMODE_SIXEL_SCROLL:0;
												break;
											case 1000:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_NORMAL;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_NORMAL);
												cterm->saved_mode |= (cterm->mouse_state_query(1000, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_NORMAL : 0);
												break;
											case 1001:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_HIGHLIGHT;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_HIGHLIGHT);
												cterm->saved_mode |= (cterm->mouse_state_query(1001, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_HIGHLIGHT : 0);
												break;
											case 1002:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_BUTTONTRACK;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_BUTTONTRACK);
												cterm->saved_mode |= (cterm->mouse_state_query(1002, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_BUTTONTRACK : 0);
												break;
											case 1003:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_ANY;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_ANY);
												cterm->saved_mode |= (cterm->mouse_state_query(1003, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_ANY : 0);
												break;
											case 1004:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_FOCUS;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_FOCUS);
												cterm->saved_mode |= (cterm->mouse_state_query(1004, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_FOCUS : 0);
												break;
											case 1005:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_UTF8;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_UTF8);
												cterm->saved_mode |= (cterm->mouse_state_query(1005, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_UTF8 : 0);
												break;
											case 1006:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_SGR;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_SGR);
												cterm->saved_mode |= (cterm->mouse_state_query(1006, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_SGR : 0);
												break;
											case 1007:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_ALTSCROLL;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_ALTSCROLL);
												cterm->saved_mode |= (cterm->mouse_state_query(1007, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_ALTSCROLL : 0);
												break;
											case 1015:
												cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_URXVT;
												cterm->saved_mode &= ~(CTERM_SAVEMODE_MOUSE_URXVT);
												cterm->saved_mode |= (cterm->mouse_state_query(1015, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_URXVT : 0);
												break;
										}
									}
								}
							}
							break;
						case 'u':
							if (seq->param_str[0] == '?' && parse_parameters(seq)) {
								gettextinfo(&ti);
								flags = getvideoflags();
								attr2palette(cterm->attr, &oldfg, &oldbg);
								updfg = (oldfg == cterm->fg_color);
								updbg = (oldbg == cterm->bg_color);
								if(seq->param_count == 0) {
									/* All the save stuff... */
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_ORIGIN) {
										if (cterm->saved_mode & CTERM_SAVEMODE_ORIGIN)
											cterm->extattr |= CTERM_EXTATTR_ORIGINMODE;
										else
											cterm->extattr &= ~CTERM_EXTATTR_ORIGINMODE;
										setwindow(cterm);
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_AUTOWRAP) {
										if (cterm->saved_mode & CTERM_SAVEMODE_AUTOWRAP)
											cterm->extattr |= CTERM_EXTATTR_AUTOWRAP;
										else
											cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_SIXEL_SCROLL) {
										if (cterm->saved_mode & CTERM_SAVEMODE_SIXEL_SCROLL)
											cterm->extattr |= CTERM_EXTATTR_SXSCROLL;
										else
											cterm->extattr &= ~CTERM_EXTATTR_SXSCROLL;
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_CURSOR) {
										cterm->cursor = (cterm->saved_mode & CTERM_SAVEMODE_CURSOR) ? _NORMALCURSOR : _NOCURSOR;
										ciolib_setcursortype(cterm->cursor);
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_ALTCHARS) {
										if(cterm->saved_mode & CTERM_SAVEMODE_ALTCHARS)
											flags |= CIOLIB_VIDEO_ALTCHARS;
										else
											flags &= ~CIOLIB_VIDEO_ALTCHARS;
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_BLINKALTCHARS) {
										if(cterm->saved_mode & CTERM_SAVEMODE_BLINKALTCHARS)
											flags |= CIOLIB_VIDEO_BLINKALTCHARS;
										else
											flags &= ~CIOLIB_VIDEO_BLINKALTCHARS;
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_NOBRIGHT) {
										if(cterm->saved_mode & CTERM_SAVEMODE_NOBRIGHT)
											flags |= CIOLIB_VIDEO_NOBRIGHT;
										else
											flags &= ~CIOLIB_VIDEO_NOBRIGHT;
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_NOBLINK) {
										if(cterm->saved_mode & CTERM_SAVEMODE_NOBLINK)
											flags |= CIOLIB_VIDEO_NOBLINK;
										else
											flags &= ~CIOLIB_VIDEO_NOBLINK;
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_BGBRIGHT) {
										if(cterm->saved_mode & CTERM_SAVEMODE_BGBRIGHT)
											flags |= CIOLIB_VIDEO_BGBRIGHT;
										else
											flags &= ~CIOLIB_VIDEO_BGBRIGHT;
									}
									setvideoflags(flags);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_X10)
										cterm->mouse_state_change(9, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_X10, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_NORMAL)
										cterm->mouse_state_change(1000, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_NORMAL, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_HIGHLIGHT)
										cterm->mouse_state_change(1001, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_HIGHLIGHT, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_BUTTONTRACK)
										cterm->mouse_state_change(1002, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_BUTTONTRACK, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_ANY)
										cterm->mouse_state_change(1003, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_ANY, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_FOCUS)
										cterm->mouse_state_change(1004, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_FOCUS, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_UTF8)
										cterm->mouse_state_change(1005, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_UTF8, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_SGR)
										cterm->mouse_state_change(1006, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_SGR, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_ALTSCROLL)
										cterm->mouse_state_change(1007, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_ALTSCROLL, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_URXVT)
										cterm->mouse_state_change(1015, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_URXVT, cterm->mouse_state_change_cbdata);
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_DECBKM) {
										if (cterm->saved_mode & CTERM_SAVEMODE_DECBKM)
											cterm->extattr |= CTERM_EXTATTR_DECBKM;
										else
											cterm->extattr &= ~CTERM_EXTATTR_DECBKM;
									}
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_DECLRMM) {
										if (cterm->saved_mode & CTERM_SAVEMODE_DECLRMM)
											cterm->extattr |= CTERM_EXTATTR_DECLRMM;
										else
											cterm->extattr &= ~CTERM_EXTATTR_DECLRMM;
									}
									break;
								}
								else {
									for (i=0; i<seq->param_count; i++) {
										seq_default(seq, i, 0);
										switch(seq->param_int[i]) {
											case 6:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_ORIGIN) {
													if (cterm->saved_mode & CTERM_SAVEMODE_ORIGIN)
														cterm->extattr |= CTERM_EXTATTR_ORIGINMODE;
													else
														cterm->extattr &= ~CTERM_EXTATTR_ORIGINMODE;
												}
												setwindow(cterm);
												break;
											case 7:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_AUTOWRAP) {
													if (cterm->saved_mode & CTERM_SAVEMODE_AUTOWRAP)
														cterm->extattr |= CTERM_EXTATTR_AUTOWRAP;
													else
														cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
												}
												break;
											case 9:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_X10)
													cterm->mouse_state_change(9, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_X10, cterm->mouse_state_change_cbdata);
												break;
											case 25:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_CURSOR) {
													cterm->cursor = (cterm->saved_mode & CTERM_SAVEMODE_CURSOR) ? _NORMALCURSOR : _NOCURSOR;
													ciolib_setcursortype(cterm->cursor);
												}
												break;
											case 31:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_ALTCHARS) {
													if(cterm->saved_mode & CTERM_SAVEMODE_ALTCHARS)
														flags |= CIOLIB_VIDEO_ALTCHARS;
													else
														flags &= ~CIOLIB_VIDEO_ALTCHARS;
													setvideoflags(flags);
												}
												break;
											case 32:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_NOBRIGHT) {
													if(cterm->saved_mode & CTERM_SAVEMODE_NOBRIGHT)
														flags |= CIOLIB_VIDEO_NOBRIGHT;
													else
														flags &= ~CIOLIB_VIDEO_NOBRIGHT;
													setvideoflags(flags);
												}
												break;
											case 33:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_BGBRIGHT) {
													if(cterm->saved_mode & CTERM_SAVEMODE_BGBRIGHT)
														flags |= CIOLIB_VIDEO_BGBRIGHT;
													else
														flags &= ~CIOLIB_VIDEO_BGBRIGHT;
													setvideoflags(flags);
												}
												break;
											case 34:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_BLINKALTCHARS) {
													if(cterm->saved_mode & CTERM_SAVEMODE_BLINKALTCHARS)
														flags |= CIOLIB_VIDEO_BLINKALTCHARS;
													else
														flags &= ~CIOLIB_VIDEO_BLINKALTCHARS;
													setvideoflags(flags);
												}
												break;
											case 35:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_NOBLINK) {
													if(cterm->saved_mode & CTERM_SAVEMODE_NOBLINK)
														flags |= CIOLIB_VIDEO_NOBLINK;
													else
														flags &= ~CIOLIB_VIDEO_NOBLINK;
													setvideoflags(flags);
												}
												break;
											case 67:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_DECBKM) {
													if (cterm->saved_mode & CTERM_SAVEMODE_DECBKM)
														cterm->extattr |= CTERM_EXTATTR_DECBKM;
													else
														cterm->extattr &= ~CTERM_EXTATTR_DECBKM;
												}
												break;
											case 69:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_DECLRMM) {
													if (cterm->saved_mode & CTERM_SAVEMODE_DECLRMM)
														cterm->extattr |= CTERM_EXTATTR_DECLRMM;
													else
														cterm->extattr &= ~CTERM_EXTATTR_DECLRMM;
												}
												break;
											case 80:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_SIXEL_SCROLL) {
													if (cterm->saved_mode & CTERM_SAVEMODE_SIXEL_SCROLL)
														cterm->extattr |= CTERM_EXTATTR_SXSCROLL;
													else
														cterm->extattr &= ~CTERM_EXTATTR_SXSCROLL;
												}
												break;
											case 1000:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_NORMAL)
													cterm->mouse_state_change(1000, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_NORMAL, cterm->mouse_state_change_cbdata);
												break;
											case 1001:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_HIGHLIGHT)
													cterm->mouse_state_change(1001, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_HIGHLIGHT, cterm->mouse_state_change_cbdata);
												break;
											case 1002:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_BUTTONTRACK)
													cterm->mouse_state_change(1002, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_BUTTONTRACK, cterm->mouse_state_change_cbdata);
												break;
											case 1003:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_ANY)
													cterm->mouse_state_change(1003, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_ANY, cterm->mouse_state_change_cbdata);
												break;
											case 1004:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_FOCUS)
													cterm->mouse_state_change(1004, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_FOCUS, cterm->mouse_state_change_cbdata);
												break;
											case 1005:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_UTF8)
													cterm->mouse_state_change(1005, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_UTF8, cterm->mouse_state_change_cbdata);
												break;
											case 1006:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_SGR)
													cterm->mouse_state_change(1006, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_SGR, cterm->mouse_state_change_cbdata);
												break;
											case 1007:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_ALTSCROLL)
													cterm->mouse_state_change(1007, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_ALTSCROLL, cterm->mouse_state_change_cbdata);
												break;
											case 1015:
												if(cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_URXVT)
													cterm->mouse_state_change(1015, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_URXVT, cterm->mouse_state_change_cbdata);
												break;
										}
									}
								}
								if (updfg || updbg) {
									attr2palette(cterm->attr, updfg ? &cterm->fg_color : NULL, updbg ? &cterm->bg_color : NULL);
									if (updfg) {
										if (cterm->negative)
											FREE_AND_NULL(cterm->bg_tc_str);
										else
											FREE_AND_NULL(cterm->fg_tc_str);
									}
									if (updbg) {
										if (cterm->negative)
											FREE_AND_NULL(cterm->fg_tc_str);
										else
											FREE_AND_NULL(cterm->bg_tc_str);
									}
								}
							}
							break;
						case '{':
							if(seq->param_str[0] == '=' && parse_parameters(seq)) {	/* Font loading */
								seq_default(seq, 0, 255);
								seq_default(seq, 1, 0);
								if(seq->param_int[0]>255)
									break;
								cterm->font_read=0;
								cterm->font_slot=seq->param_int[0];
								switch(seq->param_int[1]) {
									case 0:
										cterm->font_size=4096;
										break;
									case 1:
										cterm->font_size=3584;
										break;
									case 2:
										cterm->font_size=2048;
										break;
									default:
										cterm->font_size=0;
										break;
								}
							}
							break;
					}
					break;
				}
				else if (seq->ctrl_func[1]) {	// Control Function with Intermediate Character
					// Shift left
					if (strcmp(seq->ctrl_func, " @") == 0) {
						row = TERM_MINY;
						col = TERM_MINX;
						max_row = TERM_MAXY;
						max_col = TERM_MAXX;

						coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col, &row);
						coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, &max_row);
						seq_default(seq, 0, 1);
						i = seq->param_int[0];
						if(i > TERM_MAXX)
							i = TERM_MAXX;
						movetext(col + i, row, max_col, max_row, col, row);
						j = i * TERM_MAXY;
						vc = malloc(j * sizeof(*vc));
						if (vc != NULL) {
							for(k=0; k < j; k++) {
								vc[k].ch=' ';
								vc[k].legacy_attr=cterm->attr;
								vc[k].fg=cterm->fg_color;
								vc[k].bg=cterm->bg_color;
								vc[k].font = ciolib_attrfont(cterm->attr);
							}
							vmem_puttext(max_col - i + 1, row, max_col, max_row, vc);
							free(vc);
						}
					}
					// Shift right
					else if (strcmp(seq->ctrl_func, " A") == 0) {
						row = TERM_MINY;
						col = TERM_MINX;
						max_row = TERM_MAXY;
						max_col = TERM_MAXX;

						coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col, &row);
						coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, &max_row);
						seq_default(seq, 0, 1);
						i = seq->param_int[0];
						if(i > cterm->width)
							i = cterm->width;
						movetext(col, row, max_col - i, max_row, col + i, row);
						j = i * TERM_MAXY;
						vc = malloc(j * sizeof(*vc));
						if (vc != NULL) {
							for(k=0; k < j; k++) {
								vc[k].ch=' ';
								vc[k].legacy_attr=cterm->attr;
								vc[k].fg=cterm->fg_color;
								vc[k].bg=cterm->bg_color;
								vc[k].font = ciolib_attrfont(cterm->attr);
							}
							vmem_puttext(col, row, col + i - 1, max_row, vc);
							free(vc);
						}
					}
					// Font Select
					else if (strcmp(seq->ctrl_func, " D") == 0) {
						seq_default(seq, 0, 0);
						seq_default(seq, 1, 0);
						switch(seq->param_int[0]) {
							case 0:	/* Four fonts are currently supported */
							case 1:
							case 2:
							case 3:
								/* For compatibility with ciolib.c v1.136-v1.164 */
								/* Feature introduced in CTerm v1.160, return value modified later */
								if (setfont(seq->param_int[1],FALSE,seq->param_int[0]+1) == 0)
									cterm->setfont_result = 1;
								else
									cterm->setfont_result = 0;
								if(cterm->setfont_result == 0)
									cterm->altfont[seq->param_int[0]] = seq->param_int[1];
								break;
						}
					}
					else if (strcmp(seq->ctrl_func, " d") == 0) {
						if (seq->param_count > 0) {
							delete_tabstop(cterm, seq->param_int[0]);
						}
					}
					/*
					 * END OF STANDARD CONTROL FUNCTIONS
					 * AFTER THIS IS ALL PRIVATE EXTENSIONS
					 */
					else if (strcmp(seq->ctrl_func, " q") == 0) {
						int s, e, r, b, v;
						seq_default(seq, 0, 1);
						switch (seq->param_int[0]) {
							case 0:	// Blinking block
							case 1:	// Blinking block
							default:
								cterm->cursor = _SOLIDCURSOR;
								_setcursortype(_SOLIDCURSOR);
								if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
									getcustomcursor(&s, &e, &r, &b, &v);
									b = true;
									v = true;
									setcustomcursor(s, e, r, b, v);
								}
								break;
							case 2: // Steady block
								cterm->cursor = _SOLIDCURSOR;
								_setcursortype(_SOLIDCURSOR);
								if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
									getcustomcursor(&s, &e, &r, &b, &v);
									b = false;
									v = true;
									setcustomcursor(s, e, r, b, v);
								}
								break;
							case 3: // Blinking underline
								cterm->cursor = _NORMALCURSOR;
								_setcursortype(_NORMALCURSOR);
								if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
									getcustomcursor(&s, &e, &r, &b, &v);
									b = true;
									v = true;
									setcustomcursor(s, e, r, b, v);
								}
								break;
							case 4: // Steady underline
								cterm->cursor = _NORMALCURSOR;
								_setcursortype(_NORMALCURSOR);
								if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
									getcustomcursor(&s, &e, &r, &b, &v);
									b = false;
									v = true;
									setcustomcursor(s, e, r, b, v);
								}
								break;
							// Note XTerm has 5 and 6 as extensions.
						}
					}
					// Tab report
					else if (strcmp(seq->ctrl_func, "$w") == 0) {
						seq_default(seq, 0, 0);
						if (seq->param_int[0] == 2) {
							strcpy(tmp, "\x1bP2$u");
							p2 = strchr(tmp, 0);
							for (i = 0; i < cterm->tab_count && cterm->tabs[i] <= cterm->width; i++) {
								if (i != 0)
									*(p2++) = '/';
								p2 += sprintf(p2, "%d", cterm->tabs[i]);
							}
							strcat(p2, "\x1b\\");
							if(retbuf && *tmp && strlen(retbuf) + strlen(tmp) < retsize)
								strcat(retbuf, tmp);
						}
					}
					// Communication speed
					else if (strcmp(seq->ctrl_func, "*r") == 0) {
						/*
						 * Ps1 			Comm Line 		Ps2 		Communication Speed
						 * none, 0, 1 	Host Transmit 	none, 0 	Use default speed.
						 * 2		 	Host Receive 	1 			300
						 * 3 			Printer 		2 			600
						 * 4		 	Modem Hi 		3 			1200
						 * 5		 	Modem Lo 		4 			2400
						 * 								5 			4800
						 * 								6 			9600
						 * 								7 			19200
						 * 								8 			38400
						 * 								9 			57600
						 * 								10 			76800
						 * 								11 			115200
						 */
						int newspeed=-1;

						seq_default(seq, 0, 0);
						seq_default(seq, 1, 0);

						if (seq->param_int[0] < 2) {
							switch(seq->param_int[1]) {
								case 0:
									newspeed=0;
									break;
								case 1:
									newspeed=300;
									break;
								case 2:
									newspeed=600;
									break;
								case 3:
									newspeed=1200;
									break;
								case 4:
									newspeed=2400;
									break;
								case 5:
									newspeed=4800;
									break;
								case 6:
									newspeed=9600;
									break;
								case 7:
									newspeed=19200;
									break;
								case 8:
									newspeed=38400;
									break;
								case 9:
									newspeed=57600;
									break;
								case 10:
									newspeed=76800;
									break;
								case 11:
									newspeed=115200;
									break;
							}
						}
						if(newspeed >= 0)
							*speed = newspeed;
					}
					else if (strcmp(seq->ctrl_func, "*y") == 0) {
						if (seq->param_count >= 6) {
							if (seq->param_int[0] != UINT64_MAX &&
							    seq->param_int[0] <= UINT16_MAX &&
							    seq->param_int[1] == 1 &&
							    seq->param_int[2] != UINT64_MAX &&
							    seq->param_int[3] != UINT64_MAX &&
							    seq->param_int[4] != UINT64_MAX &&
							    seq->param_int[5] != UINT64_MAX) {
								struct ciolib_pixels *pix;
								uint16_t crc;
								int good = 0;
								int vmode;
								gettextinfo(&ti);
								vmode = find_vmode(ti.currmode);
								if (vmode != -1 &&
								    (seq->param_int[3] > 0 && seq->param_int[3] < vparams[vmode].charwidth*cterm->width) &&
								    (seq->param_int[2] > 0 && seq->param_int[2] < vparams[vmode].charwidth*cterm->width) &&
								    (seq->param_int[5] > 0 && seq->param_int[5] < vparams[vmode].charwidth*cterm->width) &&
								    (seq->param_int[4] > 0 && seq->param_int[4] < vparams[vmode].charwidth*cterm->width) &&
								    (seq->param_int[2] <= seq->param_int[4]) &&
								    (seq->param_int[3] <= seq->param_int[5]) &&
								    (pix = getpixels(
								      (seq->param_int[3] - 1 + cterm->x - 1)*vparams[vmode].charwidth,
								      (seq->param_int[2] - 1 + cterm->y - 1)*vparams[vmode].charheight,
								      (seq->param_int[5] + cterm->x - 1)*vparams[vmode].charwidth - 1,
								      (seq->param_int[4] + cterm->y - 1)*vparams[vmode].charheight - 1, true)) != NULL) {
									crc = crc16((void *)pix->pixels, sizeof(pix->pixels[0])*pix->width*pix->height);
									good = 1;
									freepixels(pix);
								}
								else {
									size_t sz = sizeof(struct vmem_cell) * (seq->param_int[2] - seq->param_int[4] + 1) * (seq->param_int[3] - seq->param_int[5] + 1);
									struct vmem_cell *vm = malloc(sz);
									if (vm != NULL) {
										vmem_gettext(seq->param_int[3], seq->param_int[2], seq->param_int[5], seq->param_int[4], vm);
										crc = crc16((void *)vm, sz);
										good = 1;
									}
								}
								if (good) {
									*tmp = 0;
									snprintf(tmp, sizeof(tmp), "\x1bP%u!~%04X\x1b\\", (unsigned)seq->param_int[0], crc);
									if(retbuf && *tmp && strlen(retbuf) + strlen(tmp) < retsize)
										strcat(retbuf, tmp);
								}
							}
						}
					}
					else if (strcmp(seq->ctrl_func, "*z") == 0) {
						if (seq->param_count > 0 && seq->param_int[0] <= 63) {
							if (cterm->macros[seq->param_int[0]]) {
								if ((cterm->in_macro & (UINT64_C(1)<<seq->param_int[0])) == 0) {
									cterm->escbuf[0]=0;
									cterm->sequence=0;
									cterm->in_macro |= (UINT64_C(1)<<seq->param_int[0]);
									cterm_write(cterm, cterm->macros[seq->param_int[0]], cterm->macro_lens[seq->param_int[0]], retbuf ? retbuf + strlen(retbuf) : retbuf, retsize - (retbuf ? strlen(retbuf) : 0), speed);
									cterm->in_macro &= ~(UINT64_C(1)<<seq->param_int[0]);
								}
							}
						}
					}
				}
				else {
					switch(seq->final_byte) {
						case '@':	/* Insert Char */
							clear_lcf(cterm);
							TERM_XY(&i, &j);
							if (i < TERM_MINX || i > TERM_MAXX || j < TERM_MINY || j > TERM_MAXY)
								break;
							col = i;
							row = j;
							coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col, &row);
							max_col = TERM_MAXX;
							coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, NULL);
							seq_default(seq, 0, 1);
							if(seq->param_int[0] < 1)
								seq->param_int[0] = 1;
							if(seq->param_int[0] > cterm->width - j)
								seq->param_int[0] = cterm->width - j;
							movetext(col, row, max_col - seq->param_int[0], row, col + seq->param_int[0], row);
							for(l=0; l < seq->param_int[0]; l++)
								putch(' ');
							cterm_gotoxy(cterm, i, j);
							break;
						case 'A':	/* Cursor Up */
							clear_lcf(cterm);
							// Fall-through
						case 'k':	/* Line Position Backward */
							seq_default(seq, 0, 1);
							if (seq->param_int[0] < 1)
								break;
							adjust_currpos(cterm, 0, 0 - seq->param_int[0], 0);
							break;
						case 'B':	/* Cursor Down */
							clear_lcf(cterm);
							// Fall-through
						case 'e':	/* Line Position Forward */
							seq_default(seq, 0, 1);
							if (seq->param_int[0] < 1)
								break;
							adjust_currpos(cterm, 0, seq->param_int[0], 0);
							break;
						case 'a':	/* Character Position Forward */
							clear_lcf(cterm);
							// Fall-through
						case 'C':	/* Cursor Right */
							seq_default(seq, 0, 1);
							if (seq->param_int[0] < 1)
								break;
							adjust_currpos(cterm, seq->param_int[0], 0, 0);
							break;
						case 'j':	/* Character Position Backward */
							clear_lcf(cterm);
							// Fall-through
						case 'D':	/* Cursor Left */
							seq_default(seq, 0, 1);
							if (seq->param_int[0] < 1)
								break;
							adjust_currpos(cterm, 0 - seq->param_int[0], 0, 0);
							break;
						case 'E':	/* Cursor next line */
							seq_default(seq, 0, 1);
							if (seq->param_int[0] < 1)
								break;
							adjust_currpos(cterm, INT_MIN, seq->param_int[0], 0);
							break;
						case 'F':	/* Cursor preceding line */
							seq_default(seq, 0, 1);
							if (seq->param_int[0] < 1)
								break;
							adjust_currpos(cterm, INT_MIN, 0 - seq->param_int[0], 0);
							break;
						case '`':
						case 'G':	/* Cursor Position Absolute */
							seq_default(seq, 0, 1);
							CURR_XY(NULL, &row);
							col = seq->param_int[0];
							if(col >= CURR_MINX && col <= CURR_MAXX) {
								gotoxy(col, row);
							}
							break;
						case 'H':	/* Cursor Position */
						case 'f':	/* Character And Line Position */
							clear_lcf(cterm);
							seq_default(seq, 0, 1);
							seq_default(seq, 1, 1);
							row=seq->param_int[0];
							col=seq->param_int[1];
							if (row < CURR_MINY)
								row = CURR_MINY;
							if(col < CURR_MINX)
								col = CURR_MINX;
							if(row > CURR_MAXY)
								row = CURR_MAXY;
							if(col > CURR_MAXX)
								col = CURR_MAXX;
							gotoxy(col, row);
							break;
						case 'I':	/* Cursor Forward Tabulation */
						case 'Y':	/* Cursor Line Tabulation */
							clear_lcf(cterm);
							seq_default(seq, 0, 1);
							if (seq->param_int[0] < 1)
								break;
							for (i = 0; i < seq->param_int[0]; i++)
								do_tab(cterm);
							break;
						case 'J':	/* Erase In Page */
							clear_lcf(cterm);
							seq_default(seq, 0, 0);
							switch(seq->param_int[0]) {
								case 0:
									clreol();
									CURR_XY(&col, &row);
									for (i = row + 1; i <= TERM_MAXY; i++) {
										cterm_gotoxy(cterm, TERM_MINY, i);
										clreol();
									}
									gotoxy(col, row);
									break;
								case 1:
									clear2bol(cterm);
									CURR_XY(&col, &row);
									for (i = row - 1; i >= TERM_MINY; i--) {
										cterm_gotoxy(cterm, TERM_MINX, i);
										clreol();
									}
									gotoxy(col, row);
									break;
								case 2:
									cterm_clearscreen(cterm, (char)cterm->attr);
									gotoxy(CURR_MINX, CURR_MINY);
									break;
							}
							break;
						case 'K':	/* Erase In Line */
							clear_lcf(cterm);
							seq_default(seq, 0, 0);
							switch(seq->param_int[0]) {
								case 0:
									clreol();
									break;
								case 1:
									clear2bol(cterm);
									break;
								case 2:
									CURR_XY(&col, &row);
									gotoxy(CURR_MINX, row);
									clreol();
									gotoxy(col, row);
									break;
							}
							break;
						case 'L':		/* Insert line */
							TERM_XY(&col, &row);
							if(row < TERM_MINY || row > TERM_MAXY || col < TERM_MINX || col > TERM_MAXX)
								break;
							seq_default(seq, 0, 1);
							i = seq->param_int[0];
							if(i > TERM_MAXY - row)
								i = TERM_MAXY - row;
							col2 = TERM_MINX;
							row2 = row;
							coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col2, &row2);
							max_col = TERM_MAXX;
							max_row = TERM_MAXY;
							coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, &max_row);
							if(i)
								movetext(col2, row2, max_col, max_row - i, col2, row2 + i);
							for (j = 0; j < i; j++) {
								cterm_gotoxy(cterm, TERM_MINX, row+j);
								cterm_clreol(cterm);
							}
							cterm_gotoxy(cterm, col, row);
							break;
						case 'M':	/* Delete Line (also ANSI music) */
							if(cterm->music_enable==CTERM_MUSIC_ENABLED && seq->param_count == 0) {
								cterm->music=1;
							}
							else {
								TERM_XY(&col, &row);
								if(col >= TERM_MINX && col <= TERM_MAXX && row >= TERM_MINY && row <= TERM_MAXY) {
									seq_default(seq, 0, 1);
									i = seq->param_int[0];
									dellines(cterm, i);
								}
							}
							break;
						case 'N':	/* Erase In Field (also ANSI Music) */
							/* BananANSI style... does NOT start with MF or MB */
							/* This still conflicts (ANSI erase field) */
							if(cterm->music_enable >= CTERM_MUSIC_BANSI)
								cterm->music=2;
							break;
						case 'O':	/* TODO? Erase In Area */
							break;
						case 'P':	/* Delete char */
							clear_lcf(cterm);
							seq_default(seq, 0, 1);
							TERM_XY(&col, &row);
							if (col < TERM_MINX || col > TERM_MAXX || row < TERM_MINY || row > TERM_MAXY)
								break;
							i = seq->param_int[0];
							if(i > TERM_MAXX - col + 1)
								i = TERM_MAXX - col + 1;
							max_col = TERM_MAXX;
							coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, NULL);
							col2 = col;
							row2 = row;
							coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col2, &row2);
							movetext(col2 + i, row2, max_col, row2, col2, row2);
							cterm_gotoxy(cterm, TERM_MAXX - i, row);
							cterm_clreol(cterm);
							cterm_gotoxy(cterm, col, row);
							break;
						case 'Q':	/* TODO? Select Editing Extent */
							break;
						case 'R':	/* TODO? Active Position Report */
							break;
						case 'S':	/* Scroll Up */
							seq_default(seq, 0, 1);
							for(j=0; j<seq->param_int[0]; j++)
								cterm_scrollup(cterm);
							break;
						case 'T':	/* Scroll Down */
							seq_default(seq, 0, 1);
							for(j=0; j<seq->param_int[0]; j++)
								scrolldown(cterm);
							break;
						case 'U':	/* TODO? Next Page */
							break;
						case 'V':	/* TODO? Preceding Page */
							break;
						case 'W':	/* TODO? Cursor Tabulation Control */
							break;
						case 'X':	/* Erase Character */
							clear_lcf(cterm);
							seq_default(seq, 0, 1);
							i=seq->param_int[0];
							CURR_XY(&col, &row);
							if(i > CURR_MAXX - col)
								i=CURR_MAXX - col;
							vc=malloc(i * sizeof(*vc));
							for(k=0; k < i; k++) {
								vc[k].ch=' ';
								vc[k].legacy_attr=cterm->attr;
								vc[k].fg=cterm->fg_color;
								vc[k].bg=cterm->bg_color;
								vc[k].font = ciolib_attrfont(cterm->attr);
							}
							col2 = col;
							row2 = row;
							coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &col2, &row2);
							max_col = CURR_MAXX;
							coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &max_col, NULL);
							vmem_puttext(col2, row2, col2 + i - 1, row2, vc);
							free(vc);
							break;
						// for case 'Y': see case 'I':
						case 'Z':	/* Cursor Backward Tabulation */
							seq_default(seq, 0, 1);
							if (seq->param_int[0] < 1)
								break;
							for (i = 0; i < seq->param_int[0]; i++)
								do_backtab(cterm);
							break;
						case '[':	/* TODO? Start Reversed String */
							break;
						case '\\':	/* TODO? Parallel Texts */
							break;
						case ']':	/* TODO? Start Directed String */
							break;
						case '^':	/* TODO? Select Implicit Movement Direction */
							break;
						case '_':	/* NOT DEFIFINED IN STANDARD */
							break;
						// for case '`': see case 'G':
						// for case 'a': see case 'C':
						case 'b':	/* Repeat */
							if (last != 0) {
								seq_default(seq, 0, 1);
								i = seq->param_int[0];
								if (i > 0) {
									p2 = malloc(i+1);
									if (p2) {
										memset(p2, last, i);
										p2[i] = 0;
										ctputs(cterm, p2);
										free(p2);
									}
								}
							}
							break;
						case 'c':	/* Device Attributes */
							seq_default(seq, 0, 0);
							if(!seq->param_int[0]) {
								if(retbuf!=NULL) {
									if(strlen(retbuf) + strlen(cterm->DA)  < retsize)
										strcat(retbuf,cterm->DA);
								}
							}
							break;
						case 'd':	/* Line Position Absolute */
							seq_default(seq, 0, 1);
							CURR_XY(&col, NULL);
							row = seq->param_int[0];
							if (row < CURR_MINY)
								row = CURR_MINY;
							if (row > CURR_MAXY)
								row = CURR_MAXY;
							gotoxy(col, row);
							break;
						// for case 'e': see case 'B':
						// for case 'f': see case 'H':
						case 'g':	/* Tabulation Clear */
							seq_default(seq, 0, 0);
							switch(seq->param_int[0]) {
								case 0:
									delete_tabstop(cterm, wherex());
									break;
								case 3:
								case 5:
									cterm->tab_count = 0;
									break;
							}
							break;
						case 'h':	/* TODO? Set Mode */
							break;
						case 'i':	/* ToDo?  Media Copy (Printing) */
							break;
						// for case 'j': see case 'D':
						// for case 'k': see case 'A':
						case 'l':	/* TODO? Reset Mode */
							break;
						case 'm':	/* Select Graphic Rendition */
							gettextinfo(&ti);
							flags = getvideoflags();
							seq_default(seq, 0, 0);
							for (i=0; i < seq->param_count; i++) {
								seq_default(seq, i, 0);
								switch(seq->param_int[i]) {
									case 0:
										set_negative(cterm, false);
										cterm->attr=ti.normattr;
										attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
										FREE_AND_NULL(cterm->fg_tc_str);
										FREE_AND_NULL(cterm->bg_tc_str);
										break;
									case 1:
										if (!cterm->skypix)
											cterm->attr|=8;
										if (!(flags & CIOLIB_VIDEO_NOBRIGHT)) {
											attr2palette(cterm->attr, cterm->negative ? NULL : &cterm->fg_color, cterm->negative ? &cterm->fg_color : NULL);
											FREE_AND_NULL(cterm->fg_tc_str);
										}
										break;
									case 2:
										if (!cterm->skypix)
											cterm->attr&=247;
										if (!(flags & CIOLIB_VIDEO_NOBRIGHT)) {
											attr2palette(cterm->attr, cterm->negative ? NULL : &cterm->fg_color, cterm->negative ? &cterm->fg_color : NULL);
											FREE_AND_NULL(cterm->fg_tc_str);
										}
										break;
									case 4:	/* Underscore */
										break;
									case 5:
									case 6:
										if (!cterm->skypix)
											cterm->attr|=128;
										if (flags & CIOLIB_VIDEO_BGBRIGHT) {
											attr2palette(cterm->attr, cterm->negative ? &cterm->bg_color : NULL, cterm->negative ? NULL : &cterm->bg_color);
											FREE_AND_NULL(cterm->bg_tc_str);
										}
										break;
									case 7:
										set_negative(cterm, true);
										break;
									case 8:
										j = cterm->attr & 112;
										cterm->attr &= 112;
										cterm->attr |= j>>4;
										// TODO: Negative image mode problem.
										cterm->fg_color = cterm->bg_color;
										FREE_AND_NULL(cterm->fg_tc_str);
										if (cterm->bg_tc_str != NULL)
											cterm->fg_tc_str = strdup(cterm->bg_tc_str);
										break;
									case 22:
										// TODO: Negative image mode problem.
										cterm->attr &= 0xf7;
										if (!(flags & CIOLIB_VIDEO_NOBRIGHT)) {
											attr2palette(cterm->attr, cterm->negative ? NULL : &cterm->fg_color, cterm->negative ? &cterm->fg_color : NULL);
											FREE_AND_NULL(cterm->fg_tc_str);
										}
										break;
									case 25:
										// TODO: Negative image mode problem.
										cterm->attr &= 0x7f;
										if (flags & CIOLIB_VIDEO_BGBRIGHT) {
											attr2palette(cterm->attr, cterm->negative ? &cterm->bg_color : NULL, cterm->negative ? NULL : &cterm->bg_color);
											FREE_AND_NULL(cterm->bg_tc_str);
										}
										break;
									case 27:
										set_negative(cterm, false);
										break;
									case 30:
										set_fgattr(cterm, skypix_color(cterm, BLACK));
										break;
									case 31:
										set_fgattr(cterm, skypix_color(cterm, RED));
										break;
									case 32:
										set_fgattr(cterm, skypix_color(cterm, GREEN));
										break;
									case 33:
										set_fgattr(cterm, skypix_color(cterm, BROWN));
										break;
									case 34:
										set_fgattr(cterm, skypix_color(cterm, BLUE));
										break;
									case 35:
										set_fgattr(cterm, skypix_color(cterm, MAGENTA));
										break;
									case 36:
										set_fgattr(cterm, skypix_color(cterm, CYAN));
										break;
									case 38:
										parse_extended_colour(seq, &i, cterm, true ^ cterm->negative);
										break;
									case 37:
									case 39:
										set_fgattr(cterm, skypix_color(cterm, LIGHTGRAY));
										break;
									case 49:
									case 40:
										set_bgattr(cterm, skypix_color(cterm, BLACK));
										break;
									case 41:
										set_bgattr(cterm, skypix_color(cterm, RED));
										break;
									case 42:
										set_bgattr(cterm, skypix_color(cterm, GREEN));
										break;
									case 43:
										set_bgattr(cterm, skypix_color(cterm, BROWN));
										break;
									case 44:
										set_bgattr(cterm, skypix_color(cterm, BLUE));
										break;
									case 45:
										set_bgattr(cterm, skypix_color(cterm, MAGENTA));
										break;
									case 46:
										set_bgattr(cterm, skypix_color(cterm, CYAN));
										break;
									case 47:
										set_bgattr(cterm, skypix_color(cterm, LIGHTGRAY));
										break;
									case 48:
										parse_extended_colour(seq, &i, cterm, false ^ cterm->negative);
										break;
									case 90:
										set_bright_fg(cterm, BLACK, flags);
										break;
									case 91:
										set_bright_fg(cterm, RED, flags);
										break;
									case 92:
										set_bright_fg(cterm, GREEN, flags);
										break;
									case 93:
										set_bright_fg(cterm, BROWN, flags);
										break;
									case 94:
										set_bright_fg(cterm, BLUE, flags);
										break;
									case 95:
										set_bright_fg(cterm, MAGENTA, flags);
										break;
									case 96:
										set_bright_fg(cterm, CYAN, flags);
										break;
									case 97:
										set_bright_fg(cterm, LIGHTGRAY, flags);
										break;
									case 100:
										set_bright_bg(cterm, BLACK, flags);
										break;
									case 101:
										set_bright_bg(cterm, RED, flags);
										break;
									case 102:
										set_bright_bg(cterm, GREEN, flags);
										break;
									case 103:
										set_bright_bg(cterm, BROWN, flags);
										break;
									case 104:
										set_bright_bg(cterm, BLUE, flags);
										break;
									case 105:
										set_bright_bg(cterm, MAGENTA, flags);
										break;
									case 106:
										set_bright_bg(cterm, CYAN, flags);
										break;
									case 107:
										set_bright_bg(cterm, LIGHTGRAY, flags);
										break;
								}
							}
							textattr(cterm->attr);
							setcolour(cterm->fg_color, cterm->bg_color);
							break;
						case 'n':	/* Device Status Report */
							seq_default(seq, 0, 0);
							switch(seq->param_int[0]) {
								case 5:
									if(retbuf!=NULL) {
										strcpy(tmp,"\x1b[0n");
										if(strlen(retbuf)+strlen(tmp) < retsize)
											strcat(retbuf,tmp);
									}
									break;
								case 6:
									if(retbuf!=NULL) {
										CURR_XY(&col, &row);
										sprintf(tmp,"\x1b[%d;%dR",row,col);
										if(strlen(retbuf)+strlen(tmp) < retsize)
											strcat(retbuf,tmp);
									}
									break;
								case 255:
									if (retbuf != NULL) {
										sprintf(tmp, "\x1b[%d;%dR", CURR_MAXY, CURR_MAXX);
										if (strlen(retbuf) + strlen(tmp) < retsize) {
											strcat(retbuf, tmp);
										}
									}
									break;
							}
							break;
						case 'o': /* ToDo?  Define Area Qualification */
							break;
						/*
						 * END OF STANDARD CONTROL FUNCTIONS
						 * AFTER THIS IS ALL PRIVATE EXTENSIONS
						 */
						case 'p': /* ToDo?  ANSI keyboard reassignment, pointer mode */
							break;
						case 'q': /* ToDo?  VT100 keyboard lights, cursor style, protection */
							break;
						case 'r': /* Scrolling reigon */
							clear_lcf(cterm);
							seq_default(seq, 0, 1);
							seq_default(seq, 1, cterm->height);
							row = seq->param_int[0];
							max_row = seq->param_int[1];
							if(row >= ABS_MINY && max_row > row && max_row <= ABS_MAXY) {
								cterm->top_margin = row;
								cterm->bottom_margin = max_row;
								setwindow(cterm);
								gotoxy(CURR_MINX, CURR_MINY);
							}
							break;
						case 's':
							if (cterm->extattr & CTERM_EXTATTR_DECLRMM) {
								seq_default(seq, 0, ABS_MINX);
								seq_default(seq, 1, ABS_MAXX);
								col = seq->param_int[0];
								if (col == 0)
									col = cterm->left_margin;
								max_col = seq->param_int[1];
								if (max_col == 0)
									max_col = cterm->right_margin;
								if(col >= ABS_MINX && max_col > col && max_col <= ABS_MAXX) {
									cterm->left_margin = col;
									cterm->right_margin = max_col;
									setwindow(cterm);
									gotoxy(CURR_MINX, CURR_MINY);
								}
							}
							else {
								CURR_XY(&cterm->save_xpos, &cterm->save_ypos);
							}
							break;
						case 't':
							if (seq->param_count >= 4) {
								uint32_t *c = NULL;
								uint32_t nc;

								if ((seq->param_int[0] == 0) ^ cterm->negative)
									c = &cterm->bg_color;
								else if ((seq->param_int[0] == 1) ^ cterm->negative)
									c = &cterm->fg_color;
								if (c == NULL)
									break;
								nc = map_rgb(seq->param_int[1]<<8, seq->param_int[2]<<8, seq->param_int[3]<<8);
								if (nc != UINT32_MAX)
									*c = nc;
								setcolour(cterm->fg_color, cterm->bg_color);
							}
							break;
						case 'u':
							if(cterm->save_ypos>0 && cterm->save_ypos<=cterm->height
									&& cterm->save_xpos>0 && cterm->save_xpos<=cterm->width) {
								// TODO: What to do about the window when position is restored...
								//       Absolute position stored?  Relative?
								if(cterm->save_ypos < CURR_MINY || cterm->save_ypos > CURR_MAXY || cterm->save_xpos < CURR_MINX || cterm->save_xpos > CURR_MAXX)
									break;
								gotoxy(cterm->save_xpos, cterm->save_ypos);
							}
							break;
						case 'y':	/* ToDo?  VT100 Tests */
							break;
						case 'z':	/* ToDo?  Reset */
							break;
						case '|':	/* SyncTERM ANSI Music */
							cterm->music=1;
							break;
					}
				}
				break;
			case '7':	// Save cursor position
				CURR_XY(&cterm->save_xpos, &cterm->save_ypos);
				break;
			case '8':	// Restore cursor positi9on
				if(cterm->save_ypos>0 && cterm->save_ypos<=cterm->height
						&& cterm->save_xpos>0 && cterm->save_xpos<=cterm->width) {
					// TODO: What to do about the window when position is restored...
					//       Absolute position stored?  Relative?
					if(cterm->save_ypos < CURR_MINY || cterm->save_ypos > CURR_MAXY || cterm->save_xpos < CURR_MINX || cterm->save_xpos > CURR_MAXX)
						break;
					gotoxy(cterm->save_xpos, cterm->save_ypos);
				}
				break;
			case 'E':	// Next Line
				clear_lcf(cterm);
				adjust_currpos(cterm, INT_MIN, 1, 1);
				break;
			case 'H':
				insert_tabstop(cterm, wherex());
				break;
			case 'M':	// Previous line
				clear_lcf(cterm);
				adjust_currpos(cterm, 0, -1, 1);
				break;
			case 'P':	// Device Control String - DCS
				cterm->string = CTERM_STRING_DCS;
				cterm->sixel = SIXEL_POSSIBLE;
				cterm->macro = MACRO_POSSIBLE;
				FREE_AND_NULL(cterm->strbuf);
				cterm->strbuf = malloc(1024);
				cterm->strbufsize = 1024;
				cterm->strbuflen = 0;
				break;
			case 'X':	// Start Of String - SOS
				cterm->string = CTERM_STRING_SOS;
				FREE_AND_NULL(cterm->strbuf);
				cterm->strbuf = malloc(1024);
				cterm->strbufsize = 1024;
				cterm->strbuflen = 0;
				break;
			case 'c':
				clrscr();
				cterm_reset(cterm);
				cterm_gotoxy(cterm, CURR_MINX, CURR_MINY);
				break;
			case '\\':
				if (cterm->strbuf) {
					if (cterm->strbufsize == cterm->strbuflen-1) {
						p = realloc(cterm->strbuf, cterm->strbufsize+1);
						if (p == NULL) {
							// SO CLOSE!
							cterm->string = 0;
						}
						else {
							cterm->strbuf = p;
							cterm->strbufsize++;
						}
					}
					cterm->strbuf[cterm->strbuflen] = 0;
					switch (cterm->string) {
						case CTERM_STRING_APC:
							if (cterm->apc_handler)
								cterm->apc_handler(cterm->strbuf, cterm->strbuflen, retbuf, retsize, cterm->apc_handler_data);
							break;
						case CTERM_STRING_DCS:
							if (cterm->sixel == SIXEL_STARTED)
								parse_sixel_string(cterm, true);
							else if (cterm->macro == MACRO_STARTED)
								parse_macro_string(cterm, true);
							else {
								if (strncmp(cterm->strbuf, "CTerm:Font:", 11) == 0) {
									cterm->font_slot = strtoul(cterm->strbuf+11, &p, 10);
									if(cterm->font_slot < CONIO_FIRST_FREE_FONT)
										break;
									if (cterm->font_slot > 255)
										break;
									if (p && *p == ':') {
										p++;
										i = b64_decode(cterm->fontbuf, sizeof(cterm->fontbuf), p, 0);
										p2 = malloc(i);
										if (p2) {
											memcpy(p2, cterm->fontbuf, i);
											replace_font(cterm->font_slot, strdup("Remote Defined Font"), p2, i);
										}
									}
								}
								else if (strncmp(cterm->strbuf, "$q", 2) == 0) {
									// DECRQSS - VT-420
									switch (cterm->strbuf[2]) {
										case 'm':
											if (cterm->strbuf[3] == 0) {
												strcpy(tmp, "\x1bP1$r0");
												if (cterm->attr & 8)
													strcat(tmp, ";1");
												if (cterm->attr & 128)
													strcat(tmp, ";5");
												if (cterm->negative)
													strcat(tmp, ";7");
												if (cterm->fg_tc_str == NULL) {
													switch ((cterm->attr >> (cterm->negative ? 4 : 0)) & 7) {
														case 0:
															strcat(tmp, ";30");
															break;
														case 1:
															strcat(tmp, ";34");
															break;
														case 2:
															strcat(tmp, ";32");
															break;
														case 3:
															strcat(tmp, ";36");
															break;
														case 4:
															strcat(tmp, ";31");
															break;
														case 5:
															strcat(tmp, ";35");
															break;
														case 6:
															strcat(tmp, ";33");
															break;
													}
												}
												if (cterm->bg_tc_str == NULL) {
													switch ((cterm->attr >> (cterm->negative ? 0 : 4)) & 7) {
														case 1:
															strcat(tmp, ";44");
															break;
														case 2:
															strcat(tmp, ";42");
															break;
														case 3:
															strcat(tmp, ";46");
															break;
														case 4:
															strcat(tmp, ";41");
															break;
														case 5:
															strcat(tmp, ";45");
															break;
														case 6:
															strcat(tmp, ";43");
															break;
														case 7:
															strcat(tmp, ";47");
															break;
													}
												}
												if (cterm->fg_tc_str) {
													strcat(tmp, ";");
													strcat(tmp, cterm->fg_tc_str);
												}
												if (cterm->bg_tc_str) {
													strcat(tmp, ";");
													strcat(tmp, cterm->bg_tc_str);
												}
												strcat(tmp, "m\x1b\\");
												if(retbuf && strlen(retbuf)+strlen(tmp) < retsize)
													strcat(retbuf, tmp);
											}
											break;
										case 'r':
											if (cterm->strbuf[3] == 0) {
												sprintf(tmp, "\x1bP1$r%d;%dr\x1b\\", cterm->top_margin, cterm->bottom_margin);
												if(retbuf && strlen(retbuf)+strlen(tmp) < retsize)
													strcat(retbuf, tmp);
											}
											break;
										case 's':
											if (cterm->strbuf[3] == 0) {
												sprintf(tmp, "\x1bP1$r%d;%dr\x1b\\", cterm->left_margin, cterm->right_margin);
												if(retbuf && strlen(retbuf)+strlen(tmp) < retsize)
													strcat(retbuf, tmp);
											}
											break;
										case 't':
											if (cterm->strbuf[3] == 0) {
												sprintf(tmp, "\x1bP1$r%dt\x1b\\", cterm->height);
												if(retbuf && strlen(retbuf)+strlen(tmp) < retsize)
													strcat(retbuf, tmp);
											}
											break;
										case '$':
											if (cterm->strbuf[3] == '|' && cterm->strbuf[4] == 0) {
												sprintf(tmp, "\x1bP1$r%d$|\x1b\\", cterm->width);
												if(retbuf && strlen(retbuf)+strlen(tmp) < retsize)
													strcat(retbuf, tmp);
											}
											break;
										case '*':
											if (cterm->strbuf[3] == '|' && cterm->strbuf[4] == 0) {
												sprintf(tmp, "\x1bP1$r%d$|\x1b\\", cterm->height);
												if(retbuf && strlen(retbuf)+strlen(tmp) < retsize)
													strcat(retbuf, tmp);
											}
											break;
										default:
											if(retbuf!=NULL) {
												strcpy(tmp,"\x1b[0n");
												// TODO: If the string is too long, this is likely terrible.
												if (strlen(retbuf)+5 < retsize)
													strcat(retbuf, "\x1bP0$r");
												if (strlen(retbuf)+strlen(&cterm->strbuf[2]) < retsize)
													strcat(retbuf, &cterm->strbuf[2]);
												if (strlen(retbuf)+2 < retsize)
													strcat(retbuf, "\x1b_");
											}
									}
								}
							}
							cterm->sixel = SIXEL_INACTIVE;
							break;
						case CTERM_STRING_OSC:
							/* Is this an xterm Change Color(s)? */
							if (cterm->strbuf[0] == '4' && cterm->strbuf[1] == ';') {
								unsigned long index = ULONG_MAX;
								char *seqlast;

								p2 = &cterm->strbuf[2];
								while ((p = strtok_r(p2, ";", &seqlast)) != NULL) {
									p2=NULL;
									if (index == ULONG_MAX) {
										index = strtoull(p, NULL, 10);
										if (index == ULONG_MAX || index > 13200)
											break;
									}
									else {

										if (strncmp(p, "rgb:", 4))
											break;
										char *p3;
										char *p4;
										char *collast;
										uint16_t rgb[3];
										int ccount = 0;
										bool broken=false;

										p4 = &p[4];
										while (ccount < 3 && (p3 = strtok_r(p4, "/", &collast))!=NULL) {
											p4 = NULL;
											unsigned long v;
											v = strtoul(p3, NULL, 16);
											if (v > UINT16_MAX)
												break;
											switch(strlen(p3)) {
												case 1:	// 4-bit colour
													rgb[ccount] = v | (v<<4) | (v<<8) | (v<<12);
													break;
												case 2:	// 8-bit colour
													rgb[ccount] = v | (v<<8);
													break;
												case 3:	// 12-bit colour
													rgb[ccount] = (v & 0x0f) | (v<<4);
													break;
												case 4:
													rgb[ccount] = v;
													break;
												default:
													broken = true;
													break;
											}
											ccount++;
										}
										if (ccount == 3 && !broken)
											cterm_setpalette(cterm, index + palette_offset, rgb[0], rgb[1], rgb[2]);
										index = ULONG_MAX;
									}
								}
							}
							else if (strncmp("104", cterm->strbuf, 3)==0) {
								if (strlen(cterm->strbuf) == 3) {
									// Reset all colours
									for (i=0; i < sizeof(dac_default)/sizeof(struct dac_colors); i++)
										cterm_setpalette(cterm, i + palette_offset, dac_default[i].red << 8 | dac_default[i].red, dac_default[i].green << 8 | dac_default[i].green, dac_default[i].blue << 8 | dac_default[i].blue);
								}
								else if(cterm->strbuf[3] == ';') {
									char *seqlast;
									unsigned long pi;

									p2 = &cterm->strbuf[4];
									while ((p = strtok_r(p2, ";", &seqlast)) != NULL) {
										p2=NULL;
										pi = strtoull(p, NULL, 10);
										if (pi < sizeof(dac_default)/sizeof(struct dac_colors))
											cterm_setpalette(cterm, pi + palette_offset, dac_default[pi].red << 8 | dac_default[pi].red, dac_default[pi].green << 8 | dac_default[pi].green, dac_default[pi].blue << 8 | dac_default[pi].blue);
									}
								}
							}
							else if (cterm->strbuf[0] == '1'
							    && (cterm->strbuf[1] == '0' || cterm->strbuf[1] == '1')
							    && cterm->strbuf[2] == ';'
							    && cterm->strbuf[3] == '?'
							    && cterm->strbuf[4] == 0) {
								if (retbuf != NULL) {
									uint32_t colour = cterm->strbuf[1] == '0' ? cterm->default_fg_palette : cterm->default_bg_palette;

									snprintf(tmp, sizeof(tmp), "\x1b]1%c;rgb:%02x/%02x/%02x\x1b\\", cterm->strbuf[1], colour >> 16 & 0xff, colour >> 8 & 0xff, colour & 0xff);
									if (strlen(retbuf) + strlen(tmp) < retsize)
										strcat(retbuf, tmp);
								}
							}
							break;
					}
				}
				FREE_AND_NULL(cterm->strbuf);
				cterm->strbufsize = cterm->strbuflen = 0;
				cterm->string = 0;
				break;
			case ']':	// Operating System Command - OSC
				cterm->string = CTERM_STRING_OSC;
				FREE_AND_NULL(cterm->strbuf);
				cterm->strbuf = malloc(1024);
				cterm->strbufsize = 1024;
				cterm->strbuflen = 0;
				break;
			case '^':	// Privacy Message - PM
				cterm->string = CTERM_STRING_PM;
				FREE_AND_NULL(cterm->strbuf);
				cterm->strbuf = malloc(1024);
				cterm->strbufsize = 1024;
				cterm->strbuflen = 0;
				break;
			case '_':	// Application Program Command - APC
				cterm->string = CTERM_STRING_APC;
				FREE_AND_NULL(cterm->strbuf);
				cterm->strbuf = malloc(1024);
				cterm->strbufsize = 1024;
				cterm->strbuflen = 0;
				break;
		}
	}
	free_sequence(seq);
	cterm->escbuf[0]=0;
	cterm->sequence=0;
}

static void
c64_set_reverse(struct cterminal *cterm, bool on)
{
	if (on != cterm->c64reversemode)
		cterm->c64reversemode = on;
}

static uint8_t
c64_get_attr(struct cterminal *cterm)
{
	if (cterm->c64reversemode)
		return (cterm->attr >> 4 | cterm->attr << 4);
	return cterm->attr;
}

static void
cterm_reset(struct cterminal *cterm)
{
	int  i;
	struct text_info ti;

	cterm->altfont[0] = cterm->altfont[1] = cterm->altfont[2] = cterm->altfont[3] = getfont(1);
	cterm->top_margin=1;
	cterm->bottom_margin=cterm->height;
	cterm->left_margin=1;
	cterm->right_margin=cterm->width;
	cterm->save_xpos=0;
	cterm->save_ypos=0;
	cterm->escbuf[0]=0;
	cterm->sequence=0;
	cterm->string = 0;
	FREE_AND_NULL(cterm->strbuf);
	cterm->strbuflen = 0;
	cterm->strbufsize = 0;
	cterm->musicbuf[0] = 0;
	cterm->music_enable=CTERM_MUSIC_BANSI;
	cterm->music=0;
	cterm->tempo=120;
	cterm->octave=4;
	cterm->notelen=4;
	cterm->noteshape=CTERM_MUSIC_NORMAL;
	cterm->musicfore=TRUE;
	cterm->backpos=0;
	cterm->xpos = TERM_MINX;
	cterm->ypos = TERM_MINY;
	cterm->cursor=_NORMALCURSOR;
	cterm->extattr = CTERM_EXTATTR_AUTOWRAP | CTERM_EXTATTR_SXSCROLL | CTERM_EXTATTR_DECBKM;
	if (cterm->emulation == CTERM_EMULATION_ATARIST_VT52)
		cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
	FREE_AND_NULL(cterm->tabs);
	cterm->tabs = malloc(sizeof(cterm_tabs));
	if (cterm->tabs) {
		memcpy(cterm->tabs, cterm_tabs, sizeof(cterm_tabs));
		cterm->tab_count = sizeof(cterm_tabs) / sizeof(cterm_tabs[0]);
	}
	else
		cterm->tab_count = 0;
	cterm->setfont_result = CTERM_NO_SETFONT_REQUESTED;
	cterm->saved_mode = 0;
	cterm->saved_mode_mask = 0;
	cterm->negative = false;
	cterm->c64reversemode = false;
	gettextinfo(&ti);
	switch (ti.currmode) {
		case C64_40X25:
		case C128_40X25:
		case C128_80X25:
			cterm->attr = 15;
			break;
		default:
			cterm->attr = ti.normattr;
			break;
	}
	attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
	if (cterm->last_column_flag & CTERM_LCF_FORCED)
		cterm->last_column_flag &= ~(CTERM_LCF_SET);
	else
		cterm->last_column_flag &= ~(CTERM_LCF_SET | CTERM_LCF_ENABLED);
	cterm->doorway_mode = 0;
	cterm->doorway_char = 0;
	FREE_AND_NULL(cterm->fg_tc_str);
	FREE_AND_NULL(cterm->bg_tc_str);
	cterm->sixel = SIXEL_INACTIVE;
	cterm->sx_iv = 0;
	cterm->sx_ih = 0;
	cterm->sx_trans = 0;
	cterm->sx_repeat = 0;
	cterm->sx_left = 0;
	cterm->sx_x = 0;
	cterm->sx_y = 0;
	cterm->sx_bg = 0;
	cterm->sx_fg = 0;
	cterm->sx_pixels_sent = 0;
	cterm->sx_first_pass = 0;
	cterm->sx_hold_update = 0;
	cterm->sx_start_x = 0;
	cterm->sx_start_y = 0;
	cterm->sx_row_max_x = 0;
	FREE_AND_NULL(cterm->sx_pixels);
	cterm->sx_width = 0;
	cterm->sx_height = 0;
	FREE_AND_NULL(cterm->sx_mask);
	for (i = 0; i < (sizeof(cterm->macros) / sizeof(cterm->macros[0])); i++) {
		FREE_AND_NULL(cterm->macros[i]);
		cterm->macro_lens[i] = 0;
	}
	setwindow(cterm);

	/* Set up tabs for ATASCII */
	if(cterm->emulation == CTERM_EMULATION_ATASCII) {
		for(i=0; i<cterm->tab_count; i++)
			cterm->escbuf[cterm->tabs[i]]=1;
	}

	/* Set up a shadow palette */
	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE) {
		for (i=0; i < sizeof(dac_default)/sizeof(struct dac_colors); i++)
			cterm_setpalette(cterm, i + 16, dac_default[i].red << 8 | dac_default[i].red, dac_default[i].green << 8 | dac_default[i].green, dac_default[i].blue << 8 | dac_default[i].blue);
	}

	/* Reset mouse state */
	if (cterm->mouse_state_change) {
		cterm->mouse_state_change(9, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1000, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1001, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1002, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1003, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1004, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1005, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1006, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1007, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1015, 0, cterm->mouse_state_change_cbdata);
	}
}

struct cterminal* cterm_init(int height, int width, int xpos, int ypos, int backlines, int backcols, struct vmem_cell *scrollback, int emulation)
{
	char	*revision="$Revision: 1.324 $";
	char *in;
	char	*out;
	struct cterminal *cterm;
	int flags;

	if((cterm=calloc(1, sizeof(struct cterminal)))==NULL)
		return cterm;

	cterm->x=xpos;
	cterm->y=ypos;
	cterm->height=height;
	cterm->width=width;
	cterm->backlines=backlines;
	cterm->backwidth = backcols;
	cterm->scrollback=scrollback;
	cterm->log=CTERM_LOG_NONE;
	cterm->logfile=NULL;
	cterm->emulation=emulation;
	cterm->last_column_flag = 0;
	cterm_reset(cterm);
	if(cterm->scrollback!=NULL)
		memset(cterm->scrollback, 0, cterm->backwidth * cterm->backlines * sizeof(*cterm->scrollback));
	strcpy(cterm->DA,"\x1b[=67;84;101;114;109;");
	out=strchr(cterm->DA, 0);
	if(out != NULL) {
		for(in=revision; *in; in++) {
			if(isdigit(*in))
				*(out++)=*in;
			if(*in=='.')
				*(out++)=';';
		}
		*out=0;
	}
	strcat(cterm->DA,"c");
	/* Fire up note playing thread */
	if(!cterm->playnote_thread_running) {
		cterm->playnote_thread_running=TRUE;
		listInit(&cterm->notes, LINK_LIST_SEMAPHORE|LINK_LIST_MUTEX);
		sem_init(&cterm->note_completed_sem,0,0);
		sem_init(&cterm->playnote_thread_terminated,0,0);
		_beginthread(playnote_thread, 0, cterm);
	}
	if (cterm->emulation == CTERM_EMULATION_PRESTEL) {
		cterm->cursor = _NOCURSOR;
		ciolib_setcursortype(cterm->cursor);
		memcpy(cterm->prestel_data[0], "????????????????", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[1], ":2:?4967?89:1???", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[2], ":1632?123?456???", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[3], ";???????????????", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[4], ";???????????????", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[5], ";???????????????", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[6], ";???????????????", PRESTEL_MEM_SLOT_SIZE);
	}
	if (cterm->emulation == CTERM_EMULATION_ATARIST_VT52) {
		flags = getvideoflags();
		flags |= CIOLIB_VIDEO_BGBRIGHT;
		setvideoflags(flags);
	}

	return cterm;
}

void cterm_start(struct cterminal *cterm)
{
	struct text_info ti;

	if (!cterm->started) {
		gettextinfo(&ti);
		switch (ti.currmode) {
			case C64_40X25:
			case C128_40X25:
			case C128_80X25:
				cterm->attr = 15;
				break;
			default:
				cterm->attr = ti.normattr;
				break;
		}
		attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
		FREE_AND_NULL(cterm->fg_tc_str);
		FREE_AND_NULL(cterm->bg_tc_str);
		cterm->fg_color += 16;
		cterm->bg_color += 16;
		textattr(cterm->attr);
		ciolib_setcolour(cterm->fg_color, cterm->bg_color);
		ciolib_setcursortype(cterm->cursor);
		cterm->started=1;
		setwindow(cterm);
		cterm_clearscreen(cterm, cterm->attr);
		cterm_gotoxy(cterm, 1, 1);
	}
}

/*
 * Fixes line after changes... possible changes:
 * - Format effector added or removed
 * - Hold mosaic character changed
 */
static void prestel_fix_line(struct cterminal *cterm, int x, int y, bool restore, bool force)
{
	int sy = y;
	int sx = 1;
	int ex = 1;
	struct vmem_cell *line;
	unsigned int extattr = cterm->extattr;
	uint32_t fg_color = cterm->fg_color;
	uint32_t bg_color = cterm->bg_color;
	unsigned char attr = cterm->attr;
	uint8_t prestel_last_mosaic = cterm->prestel_last_mosaic;
	bool fixed = false;
	bool dblheight = false;

	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &sy, &sx);
	ex = sx + TERM_MAXX - 1;
	line = malloc(sizeof(*line) * (ex - sx + 1));
	if (line == NULL) {
		fprintf(stderr, "malloc() in prestel_get_state()\n");
		return;
	}
	vmem_gettext(sx, sy, ex, sy, line);
	prestel_new_line(cterm);
	for (int i = 0; i < TERM_MAXX; i++) {
		// Don't fixup non-Prestel cells
		if ((line[i].bg & CIOLIB_BG_PRESTEL) == 0)
			continue;
		uint8_t ch;
		// Go through the line applying attributes, held mosaics, etc.
		if (line[i].fg & CIOLIB_FG_PRESTEL_CTRL_MASK) {
			// This is a control character
			if (i == (x - 1)) {
				extattr = cterm->extattr;
				fg_color = cterm->fg_color;
				bg_color = cterm->bg_color;
				attr = cterm->attr;
				prestel_last_mosaic = cterm->prestel_last_mosaic;
			}
			ch = (line[i].fg & CIOLIB_FG_PRESTEL_CTRL_MASK) >> CIOLIB_FG_PRESTEL_CTRL_SHIFT;
			prestel_apply_ctrl_before(cterm, ch);
			if ((cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT) && ((line[i].bg & CIOLIB_BG_DOUBLE_HEIGHT) == 0)) {
				// Should be double-high
				line[i].bg |= CIOLIB_BG_DOUBLE_HEIGHT;
				fixed = true;
				dblheight = true;
			}
			if (((cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT) == 0) && (line[i].bg & CIOLIB_BG_DOUBLE_HEIGHT)) {
				// Should not be double-high
				line[i].bg &= ~CIOLIB_BG_DOUBLE_HEIGHT;
				fixed = true;
				dblheight = true;
			}
			if (line[i].fg != (cterm->fg_color | (ch << 24))
			    || line[i].bg != cterm->bg_color
			    || line[i].legacy_attr != cterm->attr) {
				// Attribute change...
				line[i].fg = cterm->fg_color | (ch << 24);
				line[i].bg = cterm->bg_color;
				line[i].legacy_attr = cterm->attr;
				fixed = true;
			}
			if (line[i].ch < 128 && (cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD)) {
				// Should be held mosaic, but is not mosaic char...
				if (cterm->prestel_last_mosaic != 0) {
					line[i].ch = cterm->prestel_last_mosaic | 0x80;
					if (cterm->prestel_last_mosaic & 0x80)
						line[i].bg &= ~CIOLIB_BG_SEPARATED;
					else
						line[i].bg |= CIOLIB_BG_SEPARATED;
					fixed = true;
				}
				else {
					if (line[i].ch != ch - 64) {
						line[i].ch = ch - 64;
						fixed = true;
					}
				}
			}
			if (line[i].ch >= 160 && ((cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD) == 0)) {
				line[i].ch = ch;
				fixed = true;
			}
			if (line[i].ch >= 160 && (cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD)) {
				// Should separated attribute is incorrect for held mosaic
				if ((!!(cterm->prestel_last_mosaic & 0x80)) == (!!(line[i].bg & CIOLIB_BG_SEPARATED))) {
					if (cterm->prestel_last_mosaic & 0x80)
						line[i].bg &= ~CIOLIB_BG_SEPARATED;
					else
						line[i].bg |= CIOLIB_BG_SEPARATED;
				}
			}
			prestel_apply_ctrl_after(cterm, ch);
			if (cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT)
				dblheight = true;
		}
		else {
			// This is displayable
			if (i == (x - 1)) {
				extattr = cterm->extattr;
				fg_color = cterm->fg_color;
				bg_color = cterm->bg_color;
				attr = cterm->attr;
				prestel_last_mosaic = cterm->prestel_last_mosaic;
			}
			if ((cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT) && ((line[i].bg & CIOLIB_BG_DOUBLE_HEIGHT) == 0)) {
				// Should be double-high
				line[i].bg |= CIOLIB_BG_DOUBLE_HEIGHT;
				fixed = true;
				dblheight = true;
			}
			if (((cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT) == 0) && (line[i].bg & CIOLIB_BG_DOUBLE_HEIGHT)) {
				// Should not be double-high
				line[i].bg &= ~CIOLIB_BG_DOUBLE_HEIGHT;
				fixed = true;
				dblheight = true;
			}
			if (line[i].fg != cterm->fg_color
			    || line[i].bg != cterm->bg_color
			    || line[i].legacy_attr != cterm->attr) {
				// Attribute change...
				line[i].fg = cterm->fg_color;
				line[i].bg = cterm->bg_color;
				line[i].legacy_attr = cterm->attr;
				fixed = true;
			}
			if (line[i].ch >= 160 && ((cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC) == 0)) {
				// Mosaic character, but should be alphanum
				line[i].ch &= 0x7f;
				fixed = true;
			}
			else if ((cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC)
			    && ((line[i].ch >= 32 && line[i].ch < 64)
			     || (line[i].ch >= 96 && line[i].ch < 128))) {
				// Alphanum but should be mosaic
				line[i].ch |= 0x80;
				if (cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED)
					line[i].bg |= CIOLIB_BG_SEPARATED;
				else
					line[i].bg &= ~CIOLIB_BG_SEPARATED;
				fixed = true;
			}
			else if ((cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC)
			    && ((line[i].ch >= 0xc0 && line[i].ch <= 0xdf))) {
				// Mosaic but should be blast-through
				line[i].ch &= 0x7f;
				fixed = true;
			}
			else if((line[i].bg & CIOLIB_BG_SEPARATED) && ((cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED) == 0)) {
				// Should not be separated...
				line[i].bg &= ~CIOLIB_BG_SEPARATED;
				fixed = true;
			}
			else if(((line[i].bg & CIOLIB_BG_SEPARATED) == 0) && (cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED)) {
				// Should be separated...
				line[i].bg |= CIOLIB_BG_SEPARATED;
				fixed = true;
			}
			if (line[i].ch >= 160) {
				cterm->prestel_last_mosaic = line[i].ch;
				if (cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED)
					cterm->prestel_last_mosaic &= 0x7f;
			}
		}
	}
	if (force || fixed)
		vmem_puttext(sx, sy, ex, sy, line);
	free(line);
	if (dblheight) {
		prestel_fix_line(cterm, x, y+1, false, true);
	}
	if (restore) {
		cterm->extattr = extattr;
		cterm->fg_color = fg_color;
		cterm->bg_color = bg_color;
		cterm->attr = attr;
		cterm->prestel_last_mosaic = prestel_last_mosaic;
	}
}

static void
advance_char(struct cterminal *cterm, int *x, int *y, int move)
{
	int lm = cterm->left_margin;
	int rm = cterm->right_margin;
	int bm = cterm->bottom_margin;

	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB) {
		prestel_fix_line(cterm, *x, *y, true, false);
		textattr(cterm->attr);
		setcolour(cterm->fg_color, cterm->bg_color);
	}
	if((*x == rm || *x == CURR_MAXX) && (!(cterm->extattr & CTERM_EXTATTR_AUTOWRAP))) {
		gotoxy(*x, *y);
		return;
	}
	else {
		if((*x == rm || *x == CURR_MAXX) && ((cterm->last_column_flag & (CTERM_LCF_ENABLED | CTERM_LCF_SET)) == CTERM_LCF_ENABLED)) {
			cterm->last_column_flag |= CTERM_LCF_SET;
			gotoxy(*x, *y);
		}
		else if(*y == bm && (*x == rm || *x == CURR_MAXX)) {
			if (cterm->emulation == CTERM_EMULATION_PRESTEL) {
				*y = cterm->top_margin;
				move = 1;
				*x = lm;
				prestel_new_line(cterm);
			}
			else {
				cond_scrollup(cterm);
				move = 1;
				*x = lm;
				if (cterm->emulation == CTERM_EMULATION_BEEB)
					prestel_new_line(cterm);
			}
		}
		else {
			if(*x == rm || *x == CURR_MAXX) {
				if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB) {
					prestel_new_line(cterm);
				}
				*x=lm;
				if (*y < CURR_MAXY)
					(*y)++;
				if (move)
					gotoxy(*x, *y);
			}
			else {
				(*x)++;
			}
		}
	}
	if (move)
		gotoxy(*x, *y);
}

static void
ctputs(struct cterminal *cterm, char *buf)
{
	char *outp;
	char *p;
	int		oldscroll;
	int cx, cy;
	int lm, rm, bm;

	if (cterm->font_render) {
		cterm->font_render(buf);
		return;
	}

	outp = buf;
	oldscroll = _wscroll;
	_wscroll = 0;
	CURR_XY(&cx, &cy);
	if (cterm->log == CTERM_LOG_ASCII && cterm->logfile != NULL)
		fputs(buf, cterm->logfile);
	lm = TERM_MINX;
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &lm, NULL);
	rm = TERM_MAXX;
	bm = TERM_MAXY;
	coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &rm, &bm);
	for (p = buf; *p; p++) {
		switch(*p) {
			case '\r':
				clear_lcf(cterm);
				*p = 0;
				cputs(outp);
				outp = p + 1;
				adjust_currpos(cterm, INT_MIN, 0, 0);
				CURR_XY(&cx, &cy);
				break;
			case '\n':
				clear_lcf(cterm);
				*p = 0;
				cputs(outp);
				outp = p + 1;
				adjust_currpos(cterm, 0, 1, 1);
				CURR_XY(&cx, &cy);
				break;
			case '\b':
				clear_lcf(cterm);
				*p=0;
				cputs(outp);
				outp = p + 1;
				adjust_currpos(cterm, -1, 0, 0);
				CURR_XY(&cx, &cy);
				break;
			case 7:		/* Bell */
				break;
			case '\t':
				clear_lcf(cterm);
				*p=0;
				cputs(outp);
				outp=p+1;
				do_tab(cterm);
				CURR_XY(&cx, &cy);
				break;
			default:
				if ((cterm->last_column_flag & (CTERM_LCF_ENABLED | CTERM_LCF_SET)) == (CTERM_LCF_ENABLED | CTERM_LCF_SET)) {
					if (cx == cterm->right_margin || cx == CURR_MAXX) {
						advance_char(cterm, &cx, &cy, 1);
						clear_lcf(cterm);
					}
				}
				if (cx == cterm->right_margin || cx == CURR_MAXX) {
					char ch;
					ch = *(p + 1);
					*(p + 1) = 0;
					cputs(outp);
					*(p+1) = ch;
					outp = p + 1;
				}
				advance_char(cterm, &cx, &cy, 0);
				break;
		}
	}
	cputs(outp);
	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)
		prestel_fix_line(cterm, cx, cy, true, false);
	_wscroll=oldscroll;
}

static void parse_sixel_intro(struct cterminal *cterm)
{
	size_t i;

	if (cterm->sixel != SIXEL_POSSIBLE)
		return;

	i = range_span_exclude(':', cterm->strbuf, '0', ';');

	if (i >= cterm->strbuflen)
		return;

	if (cterm->strbuf[i] == 'q') {
		int ratio, hgrid;
		int vmode;
		struct text_info ti;
		char *p;

		cterm->sixel = SIXEL_STARTED;
		cterm->sx_repeat = 0;
		cterm->sx_pixels_sent = 0;
		cterm->sx_first_pass = 1;
		cterm->sx_height = 0;
		cterm->sx_width = 0;
		cterm->sx_hold_update = hold_update;
		hold_update = 0;

		gettextinfo(&ti);
		vmode = find_vmode(ti.currmode);
		if (vmode == -1) {
			cterm->sixel = SIXEL_INACTIVE;
			return;
		}
		attr2palette(ti.attribute, &cterm->sx_fg, &cterm->sx_bg);
		if (cterm->extattr & CTERM_EXTATTR_SXSCROLL) {
			TERM_XY(&cterm->sx_start_x, &cterm->sx_start_y);
			cterm->sx_left = cterm->sx_x = (cterm->sx_start_x - 1) * vparams[vmode].charwidth;
			cterm->sx_y = (cterm->sx_start_y - 1) * vparams[vmode].charheight;
		}
		else {
			cterm->sx_x = cterm->sx_left = cterm->sx_y = 0;
			TERM_XY(&cterm->sx_start_x, &cterm->sx_start_y);
		}
		cterm->sx_orig_cursor = cterm->cursor;
		cterm->cursor = _NOCURSOR;
		ciolib_setcursortype(cterm->cursor);
		cterm_gotoxy(cterm, TERM_MINX, TERM_MINY);
		hold_update = 1;
		cterm->sx_trans = hgrid = 0;
		ratio = strtoul(cterm->strbuf, &p, 10);
		if (*p == ';') {
			p++;
			cterm->sx_trans = strtoul(p, &p, 10);
		}
		if (*p == ';') {
			p++;
			hgrid = strtoul(p, &p, 10);
		}
		switch (ratio) {
			default:
			case 0:
			case 1:
				cterm->sx_iv = 2;
				cterm->sx_ih = 1;
				break;
			case 2:
				cterm->sx_iv = 5;
				cterm->sx_ih = 1;
				break;
			case 3:
			case 4:
				cterm->sx_iv = 3;
				cterm->sx_ih = 1;
				break;
			case 5:
			case 6:
				cterm->sx_iv = 2;
				cterm->sx_ih = 1;
				break;
			case 7:
			case 8:
			case 9:
				cterm->sx_iv = 1;
				cterm->sx_ih = 1;
				break;
		}
		cterm->strbuflen = 0;
	}
	else if (cterm->strbuf[i] != 'q')
		cterm->sixel = SIXEL_INACTIVE;
}

static void parse_macro_intro(struct cterminal *cterm)
{
	size_t i;

	if (cterm->macro != MACRO_POSSIBLE)
		return;

	i = range_span_exclude(':', cterm->strbuf, '0', ';');

	if (i >= cterm->strbuflen)
		return;

	if (cterm->strbuf[i] != '!') {
		cterm->macro = MACRO_INACTIVE;
		return;
	}
	i++;
	if (i >= cterm->strbuflen)
		return;

	if (cterm->strbuf[i] == 'z') {
		char *p;
		unsigned long res;

		// Parse parameters...
		cterm->macro_num = -1;
		cterm->macro_del = MACRO_DELETE_OLD;
		cterm->macro_encoding = MACRO_ENCODING_ASCII;
		res = strtoul(cterm->strbuf, &p, 10);
		if (res != ULONG_MAX)
			cterm->macro_num = res;
		if (*p == ';') {
			p++;
			res = strtoul(p, &p, 10);
			if (res != ULONG_MAX)
				cterm->macro_del = res;
			else
				cterm->macro_del = -1;
		}
		if (*p == ';') {
			p++;
			res = strtoul(p, &p, 10);
			if (res != ULONG_MAX)
				cterm->macro_encoding = res;
			else
				cterm->macro_encoding = -1;
		}
		if (cterm->macro_num < 0 || cterm->macro_num > 63)
			cterm->macro = MACRO_INACTIVE;
		else if (cterm->macro_del < 0 || cterm->macro_del > 1)
			cterm->macro = MACRO_INACTIVE;
		else if (cterm->macro_encoding < 0 || cterm->macro_encoding > 1)
			cterm->macro = MACRO_INACTIVE;
		else {
			cterm->macro = MACRO_STARTED;
			cterm->strbuflen = 0;
		}
	}
	else if (cterm->strbuf[i] != 'z')
		cterm->macro = MACRO_INACTIVE;
}

#define ustrlen(s)	strlen((const char *)s)
#define uctputs(c, p)	ctputs(c, (char *)p)
#define ustrcat(b, s)	strcat((char *)b, (const char *)s)

static void
prestel_move(struct cterminal *cterm, int xadj, int yadj)
{
	int tx, ty;

	TERM_XY(&tx, &ty);
	tx += xadj;
	ty += yadj;
	// First, fix up x...
	while (tx < cterm->left_margin) {
		ty--;
		tx += TERM_MAXX;
	}
	while (tx > cterm->right_margin) {
		ty++;
		tx -= TERM_MAXX;
	}
	// Now fix up y
	while (ty < cterm->top_margin) {
		if (cterm->emulation == CTERM_EMULATION_BEEB) {
			scrolldown(cterm);
			ty++;
		}
		else {
			ty += TERM_MAXY;
		}
	}
	while (ty > cterm->bottom_margin) {
		if (cterm->emulation == CTERM_EMULATION_BEEB) {
			cterm_scrollup(cterm);
			ty--;
		}
		else {
			ty -= TERM_MAXY;
		}
	}
	gotoxy(tx, ty);
	prestel_get_state(cterm);
}

static void
prestel_send_memory(struct cterminal *cterm, uint8_t mem, char *retbuf, size_t retsize)
{
	char app[2] = {0,0};

	for (int x = 0; x < PRESTEL_MEM_SLOT_SIZE; x++) {
		if (cterm->prestel_data[mem][x] != '?') {
			app[0] = cterm->prestel_data[mem][x];
			if(retbuf && strlen(retbuf) + 1 < retsize)
				strcat(retbuf, app);
		}
	}
}

static void
prestel_handle_escaped(struct cterminal *cterm, uint8_t ctrl)
{
	struct vmem_cell tmpvc[1];
	int sx, sy, x, y;

	if (ctrl < 0x40 || ctrl > 0x5f) {
		cterm->escbuf[0]=0;
		cterm->sequence=0;
		return;
	}
	prestel_apply_ctrl_before(cterm, ctrl);
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
	cterm->escbuf[0]=0;
	cterm->sequence=0;
	tmpvc[0].fg = cterm->fg_color | (ctrl << 24);
	tmpvc[0].bg = cterm->bg_color;
	if ((cterm->extattr & CTERM_EXTATTR_PRESTEL_HOLD) && (cterm->prestel_last_mosaic != 0)) {
		tmpvc[0].ch = cterm->prestel_last_mosaic;
		if (tmpvc[0].ch & 0x80)
			tmpvc[0].bg &= ~CIOLIB_BG_SEPARATED;
		else
			tmpvc[0].bg |= CIOLIB_BG_SEPARATED;
		tmpvc[0].ch |= 0x80;
	}
	else {
		tmpvc[0].ch = ' ';
	}
	tmpvc[0].legacy_attr=cterm->attr;
	tmpvc[0].font = ciolib_attrfont(cterm->attr);
	SCR_XY(&sx, &sy);
	CURR_XY(&x, &y);
	vmem_puttext(sx, sy, sx, sy, tmpvc);
	prestel_apply_ctrl_after(cterm, ctrl);
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
	advance_char(cterm, &x, &y, 1);
}

CIOLIBEXPORT size_t cterm_write(struct cterminal * cterm, const void *vbuf, int buflen, char *retbuf, size_t retsize, int *speed)
{
	const unsigned char *buf = (unsigned char *)vbuf;
	unsigned char ch[2];
	unsigned char prn[BUFSIZE];
	unsigned char *prnpos;
	const unsigned char *prnlast = &prn[BUFSIZE-sizeof(cterm->escbuf)-1];
	size_t prntmp;
	int i, j, k, x, y;
	int sx, sy, ex, ey;
	struct text_info	ti;
	int	olddmc;
	int oldptnm;
	uint32_t palette[16];
	int mpalette;
	struct vmem_cell tmpvc[1];
	int orig_fonts[4];
	char lastch = 0;
 	int palette_offset = 0;

	if(!cterm->started)
		cterm_start(cterm);

	/* Now rejigger the current modes palette... */
	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		palette_offset = 16;
	mpalette = get_modepalette(palette);
	if (mpalette) {
		for (i=0; i < 16; i++)
			palette[i] += palette_offset;
		set_modepalette(palette);
	}

	/* Deedle up the fonts */
	orig_fonts[0] = getfont(1);
	orig_fonts[1] = getfont(2);
	orig_fonts[2] = getfont(3);
	orig_fonts[3] = getfont(4);
	setfont(cterm->altfont[0], FALSE, 1);
	setfont(cterm->altfont[1], FALSE, 2);
	setfont(cterm->altfont[2], FALSE, 3);
	setfont(cterm->altfont[3], FALSE, 4);

	oldptnm=puttext_can_move;
	puttext_can_move=1;
	olddmc=hold_update;
	hold_update=1;
	if (retbuf)
		retbuf[0]=0;
	gettextinfo(&ti);
	setwindow(cterm);
	x = cterm->xpos;
	y = cterm->ypos;
	coord_conv_xy(cterm, CTERM_COORD_ABSTERM, CTERM_COORD_CURR, &x, &y);
	gotoxy(x, y);
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
	ciolib_setcursortype(cterm->cursor);
	ch[1]=0;
	if(buflen==-1)
		buflen=ustrlen(buf);
	switch(buflen) {
		case 0:
			break;
		default:
			if(cterm->log==CTERM_LOG_RAW && cterm->logfile != NULL)
				fwrite(buf, buflen, 1, cterm->logfile);
			prn[0]=0;
			prnpos = prn;
			if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)
				prestel_get_state(cterm);
			for(j=0;j<buflen;j++) {
				if(prnpos >= prnlast) {
					uctputs(cterm, prn);
					prn[0]=0;
					prnpos = prn;
				}
				ch[0]=buf[j];
				if (cterm->string && !cterm->sequence) {
					switch (cterm->string) {
						case CTERM_STRING_DCS:
							/* 0x08-0x0d, 0x20-0x7e */
						case CTERM_STRING_APC:
							/* 0x08-0x0d, 0x20-0x7e */
						case CTERM_STRING_OSC:
							/* 0x08-0x0d, 0x20-0x7e */
						case CTERM_STRING_PM:
							/* 0x08-0x0d, 0x20-0x7e */
							if (ch[0] < 8 || (ch[0] > 0x0d && ch[0] < 0x20) || ch[0] > 0x7e) {
								if (ch[0] == 27) {
									uctputs(cterm, prn);
									prn[0]=0;
									prnpos = prn;
									cterm->sequence=1;
									break;
								}
								else {
									cterm->string = 0;
									/* Just toss out the string and this char */
									FREE_AND_NULL(cterm->strbuf);
									cterm->strbuflen = cterm->strbufsize = 0;
									cterm->sixel = SIXEL_INACTIVE;
								}
							}
							else {
								if (cterm->strbuf) {
									cterm->strbuf[cterm->strbuflen++] = ch[0];
									if (cterm->strbuflen == cterm->strbufsize) {
										char *p;

										cterm->strbufsize *= 2;
										if (cterm->strbufsize > 1024 * 1024 * 512) {
											FREE_AND_NULL(cterm->strbuf);
											cterm->strbuflen = cterm->strbufsize = 0;
											break;
										}
										else {
											p = realloc(cterm->strbuf, cterm->strbufsize);
											if (p == NULL) {
												FREE_AND_NULL(cterm->strbuf);
												cterm->strbuflen = cterm->strbufsize = 0;
												break;
											}
											else
												cterm->strbuf = p;
										}
									}
									cterm->strbuf[cterm->strbuflen] = 0;
									switch(cterm->sixel) {
										case SIXEL_STARTED:
											parse_sixel_string(cterm, false);
											break;
										case SIXEL_POSSIBLE:
											parse_sixel_intro(cterm);
											break;
									}
									switch(cterm->macro) {
										case MACRO_STARTED:
											parse_macro_string(cterm, false);
											break;
										case MACRO_POSSIBLE:
											parse_macro_intro(cterm);
											break;
									}
								}
							}
							break;
						case CTERM_STRING_SOS:
							/* Anything but SOS or ST (ESC X or ESC \) */
							if ((ch[0] == 'X' || ch[0] == '\\') &&
							    cterm->strbuf && cterm->strbuflen &&
							    cterm->strbuf[cterm->strbuflen-1] == '\x1b') {
								cterm->strbuflen--;
								cterm->string = 0;
								FREE_AND_NULL(cterm->strbuf);
								cterm->strbuflen = cterm->strbufsize = 0;
								if (retbuf) {
									cterm_write(cterm, "\x1b", 1, retbuf+strlen(retbuf), retsize-strlen(retbuf), speed);
									cterm_write(cterm, &ch[0], 1, retbuf+strlen(retbuf), retsize-strlen(retbuf), speed);
								}
							}
							else {
								if (cterm->strbuf == NULL) {
									cterm->string = 0;
									cterm->strbuflen = cterm->strbufsize = 0;
								}
								else {
									cterm->strbuf[cterm->strbuflen++] = ch[0];
									if (cterm->strbuflen == cterm->strbufsize) {
										char *p;

										cterm->strbufsize *= 2;
										if (cterm->strbufsize > 1024 * 1024 * 512) {
											FREE_AND_NULL(cterm->strbuf);
											cterm->string = 0;
											cterm->strbuflen = cterm->strbufsize = 0;
											break;
										}
										else {
											p = realloc(cterm->strbuf, cterm->strbufsize);
											if (p == NULL) {
												cterm->string = 0;
												FREE_AND_NULL(cterm->strbuf);
												cterm->strbuflen = cterm->strbufsize = 0;
											}
											else
												cterm->strbuf = p;
										}
									}
									cterm->strbuf[cterm->strbuflen] = 0;
								}
							}
							break;
					}
				}
				else if(cterm->font_size) {
					cterm->fontbuf[cterm->font_read++]=ch[0];
					if(cterm->font_read == cterm->font_size) {
						char *buf2;

						if((buf2=(char *)malloc(cterm->font_size))!=NULL) {
							memcpy(buf2,cterm->fontbuf,cterm->font_size);
							if(cterm->font_slot >= CONIO_FIRST_FREE_FONT && cterm->font_slot < 256) {
								replace_font(cterm->font_slot, strdup("Remote Defined Font"), buf2, cterm->font_size);
							}
							else
								FREE_AND_NULL(buf2);
						}
						cterm->font_size=0;
					}
				}
				else if(cterm->sequence) {
					if (cterm->emulation == CTERM_EMULATION_PRESTEL) {
						if (cterm->prestel_prog_state == PRESTEL_PROG_NONE) {
							if (ch[0] == '1')
								cterm->prestel_prog_state = PRESTEL_PROG_1;
							else {
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_handle_escaped(cterm, ch[0]);
							}
						}
						else {
							if (ch[0] == 0)
								break;
							switch (cterm->prestel_prog_state) {
								case PRESTEL_PROG_1:
									if (ch[0] == 0x1b)
										cterm->prestel_prog_state = PRESTEL_PROG_1_ESC;
									else {
										cterm->prestel_prog_state = PRESTEL_PROG_NONE;
										cterm->escbuf[0]=0;
										cterm->sequence=0;
									}
									break;
								case PRESTEL_PROG_1_ESC:
									if (ch[0] == '2') {
										cterm->prestel_prog_state = PRESTEL_PROG_2;
										cterm->prestel_mem = 0;
									}
									else {
										cterm->prestel_prog_state = PRESTEL_PROG_NONE;
										cterm->escbuf[0]=0;
										cterm->sequence=0;
									}
									break;
								case PRESTEL_PROG_2:
									if (ch[0] == 5) {
										prestel_send_memory(cterm, cterm->prestel_mem, retbuf, retsize);
										cterm->prestel_prog_state = PRESTEL_PROG_NONE;
										cterm->escbuf[0]=0;
										cterm->sequence=0;
									}
									else if (ch[0] == 0x1b) {
										cterm->prestel_prog_state = PRESTEL_PROG_2_ESC;
									}
									else {
										cterm->prestel_prog_state = PRESTEL_PROG_NONE;
										cterm->escbuf[0]=0;
										cterm->sequence=0;
									}
									break;
								case PRESTEL_PROG_2_ESC:
									if (ch[0] == '3') {
										cterm->prestel_mem++;
										if (cterm->prestel_mem >= PRESTEL_MEM_SLOTS) {
											cterm->prestel_prog_state = PRESTEL_PROG_NONE;
											cterm->escbuf[0]=0;
											cterm->sequence=0;
										}
										else
											cterm->prestel_prog_state = PRESTEL_PROG_2;
									}
									else if (ch[0] == '4') {
										cterm->prestel_prog_state = PRESTEL_PROG_PROGRAM_BLOCK;
									}
									else {
										cterm->prestel_prog_state = PRESTEL_PROG_NONE;
										cterm->escbuf[0]=0;
										cterm->sequence=0;
									}
									break;
								case PRESTEL_PROG_PROGRAM_BLOCK:
									ch[1] = 0;
									switch(ch[0]) {
										case ':':
											ch[0] = '0';
											/* Fall-through */
										case '1':
										case '2':
										case '3':
										case '4':
										case '5':
										case '6':
										case '7':
										case '8':
										case '9':
										case ';':
										case '?':
											ustrcat(cterm->escbuf, ch);
											if (strlen(cterm->escbuf) >= PRESTEL_MEM_SLOT_SIZE) {
												memcpy(cterm->prestel_data[cterm->prestel_mem], cterm->escbuf, PRESTEL_MEM_SLOT_SIZE);
												prestel_send_memory(cterm, cterm->prestel_mem, retbuf, retsize);
												cterm->prestel_prog_state = PRESTEL_PROG_NONE;
												cterm->escbuf[0]=0;
												cterm->sequence=0;
											}
											break;
										default:
											cterm->prestel_prog_state = PRESTEL_PROG_NONE;
											cterm->escbuf[0]=0;
											cterm->sequence=0;
											break;
									}
									break;
								default:
									cterm->prestel_prog_state = PRESTEL_PROG_NONE;
									break;
							}
						}
					}
					else if (cterm->emulation == CTERM_EMULATION_BEEB) {
						cterm->escbuf[cterm->sequence++] = ch[0];
						switch (cterm->escbuf[0]) {
							case 23:
								if (cterm->sequence < 10)
									break;
								// Out of all these bytes, we only look at two. :D
								if (cterm->escbuf[1] == 1 && cterm->escbuf[3] == 0 && cterm->escbuf[4] == 0 && cterm->escbuf[5] == 0 && cterm->escbuf[6] == 0 && cterm->escbuf[7] == 0 && cterm->escbuf[8] == 0 && cterm->escbuf[9] == 0) {
									switch (cterm->escbuf[2]) {
										case 0:
											// VDU 23,1,0;0;0;0;
											cterm->cursor = _NOCURSOR;
											ciolib_setcursortype(cterm->cursor);
											break;
										case 1:
											// VDU 23,1,1;0;0;0;
											cterm->cursor = _NORMALCURSOR;
											ciolib_setcursortype(cterm->cursor);
											break;
									}
								}
								cterm->sequence = 0;
								break;
							case 28:
								if (cterm->sequence < 3)
									break;
								gotoxy((cterm->escbuf[2] - ' ') + 1, (cterm->escbuf[1] - ' ') + 1);
								cterm->sequence = 0;
								break;
							default:
								cterm->sequence = 0;
								break;
						}
					}
					else if (cterm->emulation == CTERM_EMULATION_ATARIST_VT52) {
						cterm->escbuf[cterm->escbufsz++] = ch[0];
						switch(st_vt_52_legal_sequence(cterm->escbuf, cterm->escbufsz, sizeof(cterm->escbuf)-1)) {
							case SEQ_BROKEN:
								/* Broken sequence detected */
								// Just throw it out (see COMMANDO.TXT)
								cterm->escbuf[0]=0;
								cterm->escbufsz = 0;
								cterm->sequence=0;
								break;
							case SEQ_INCOMPLETE:
								break;
							case SEQ_COMPLETE:
								do_st_vt_52(cterm, retbuf, retsize);
								break;						}
					}
					else {
						ustrcat(cterm->escbuf,ch);
						switch(legal_sequence(cterm->escbuf, sizeof(cterm->escbuf)-1)) {
							case SEQ_BROKEN:
								/* Broken sequence detected */
								*prnpos++ = '\033';
								*prnpos = 0;
								prntmp = strlen(cterm->escbuf);
								memcpy(prnpos, cterm->escbuf, prntmp + 1);
								prnpos += prntmp;
								cterm->escbuf[0]=0;
								cterm->sequence=0;
								if(ch[0]=='\033') {	/* Broken sequence followed by a legal one! */
									if(prn[0]) {	/* Don't display the ESC */
										prnpos--;
										*prnpos = 0;
									}
									uctputs(cterm, prn);
									prn[0]=0;
									prnpos = prn;
									cterm->sequence=1;
								}
								break;
							case SEQ_INCOMPLETE:
								break;
							case SEQ_COMPLETE:
								do_ansi(cterm, retbuf, retsize, speed, lastch);
								lastch = 0;
								break;
						}
					}
				}
				else if (cterm->music) {
					if(ch[0]==14) {
						hold_update=0;
						puttext_can_move=0;
						CURR_XY(&x, &y);
						gotoxy(x, y);
						ciolib_setcursortype(cterm->cursor);
						hold_update=1;
						puttext_can_move=1;
						play_music(cterm);
					}
					else {
						if(strchr(musicchars,ch[0])!=NULL)
							ustrcat(cterm->musicbuf,ch);
						else {
							/* Kill non-music strings */
							cterm->music=0;
							cterm->musicbuf[0]=0;
						}
					}
				}
				else {
					if(cterm->emulation == CTERM_EMULATION_ATASCII) {
						if(cterm->attr==7) {
							switch(buf[j]) {
								case 27:	/* ESC */
									cterm->attr=1;
									break;
								case 28:	/* Up (TODO: Wraps??) */
									CURR_XY(&x, &y);
									y--;
									if(y < CURR_MINY)
										y = CURR_MINY;
									gotoxy(x, y);
									break;
								case 29:	/* Down (TODO: Wraps??) */
									CURR_XY(&x, &y);
									y++;
									if(y > CURR_MAXY)
										y = CURR_MAXY;
									gotoxy(x, y);
									break;
								case 30:	/* Left (TODO: Wraps around to same line?) */
									CURR_XY(&x, &y);
									x--;
									if(x < CURR_MINX)
										y = CURR_MINX;
									gotoxy(x, y);
									break;
								case 31:	/* Right (TODO: Wraps around to same line?) */
									CURR_XY(&x, &y);
									x++;
									if(x > CURR_MAXX)
										y = CURR_MAXX;
									gotoxy(x, y);
									break;
								case 125:	/* Clear Screen */
									cterm_clearscreen(cterm, cterm->attr);
									break;
								case 126:	/* Backspace (TODO: Wraps around to previous line?) */
											/* DOES NOT delete char, merely erases */
									CURR_XY(&x, &y);
									x--;
									if (x < CURR_MINX) {
										y--;
										if (y < CURR_MINY)
											break;
										y = CURR_MAXY;
									}
									gotoxy(x, y);
									putch(32);
									gotoxy(x, y);
									break;
								/* We abuse the ESC buffer for tab stops */
								case 127:	/* Tab (Wraps around to next line) */
									CURR_XY(&x, &y);
									for (k = x + 1; k <= CURR_MAXX; k++) {
										if(cterm->escbuf[k]) {
											x = k;
											break;
										}
									}
									if (k > CURR_MAXX) {
										x = CURR_MINX;
										y++;
										if(y > CURR_MAXY) {
											cond_scrollup(cterm);
											y = CURR_MAXY;
										}
									}
									gotoxy(x, y);
									break;
								case 155:	/* Return */
									adjust_currpos(cterm, INT_MIN, +1, 1);
									break;
								case 156:	/* Delete Line */
									dellines(cterm, 1);
									adjust_currpos(cterm, INT_MIN, 0, 0);
									break;
								case 157:	/* Insert Line */
									CURR_XY(&x, &y);
									if (y < CURR_MAXY) {
										sx = CURR_MINX;
										sy = y;
										coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
										ex = CURR_MAXX;
										ey = CURR_MAXY;
										coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, &ey);
										movetext(sx, sy, ex, ey - 1, sx, sy + 1);
									}
									gotoxy(CURR_MINX, y);
									cterm_clreol(cterm);
									break;
								case 158:	/* Clear Tab */
									cterm->escbuf[wherex()]=0;
									break;
								case 159:	/* Set Tab */
									cterm->escbuf[wherex()]=1;
									break;
								case 253:	/* Beep */
									if(!cterm->quiet) {
										#ifdef __unix__
											putch(7);
										#else
											MessageBeep(MB_OK);
										#endif
									}
									break;
								case 254:	/* Delete Char */
									CURR_XY(&x, &y);
									if(x < CURR_MAXX) {
										sx = x;
										sy = y;
										coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
										ex = CURR_MAXX;
										coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, NULL);
										movetext(sx + 1, sy, ex, sy, sx, sy);
									}
									gotoxy(CURR_MAXX, y);
									cterm_clreol(cterm);
									gotoxy(x, y);
									break;
								case 255:	/* Insert Char */
									CURR_XY(&x, &y);
									if(x < CURR_MAXX) {
										sx = x;
										sy = y;
										coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
										ex = CURR_MAXX;
										coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, NULL);
										movetext(sx, sy, ex - 1, sy, sx + 1, sy);
									}
									putch(32);
									gotoxy(x, y);
									break;
								default:
									ch[0] = buf[j];
									ch[1] = cterm->attr;
									SCR_XY(&sx, &sy);
									puttext(sx, sy, sx, sy, ch);
									ch[1]=0;
									CURR_XY(&x, &y);
									advance_char(cterm, &x, &y, 1);
									break;
							}
						}
						else {
							switch(buf[j]) {
								case 155:	/* Return */
									adjust_currpos(cterm, INT_MIN, +1, 1);
									break;
								default:
									/* Translate to screen codes */
									k=buf[j];
									if(k < 32) {
										k +=64;
									}
									else if(k < 96) {
										k -= 32;
									}
									else if(k < 128) {
										/* No translation */
									}
									else if(k < 160) {
										k +=64;
									}
									else if(k < 224) {
										k -= 32;
									}
									else if(k < 256) {
										/* No translation */
									}
									ch[0] = k;
									ch[1] = cterm->attr;
									SCR_XY(&sx, &sy);
									puttext(sx, sy, sx, sy, ch);
									ch[1]=0;
									CURR_XY(&x, &y);
									advance_char(cterm, &x, &y, 1);
									break;
							}
							cterm->attr=7;
						}
					}
					else if(cterm->emulation == CTERM_EMULATION_PETASCII) {
						switch(buf[j]) {
							case 5:		/* White */
							case 28:	/* Red */
							case 30:	/* Green */
							case 31:	/* Blue */
							case 129:	/* Orange */
							case 144:	/* Black */
							case 149:	/* Brown */
							case 150:	/* Light Red */
							case 151:	/* Dark Gray */
							case 152:	/* Grey */
							case 153:	/* Light Green */
							case 154:	/* Light Blue */
							case 155:	/* Light Gray */
							case 156:	/* Purple */
							case 158:	/* Yellow */
							case 159:	/* Cyan */
								cterm->attr &= 0xf0;
								if (ti.currmode == C64_40X25 || ti.currmode == C128_40X25) {
									switch(buf[j]) {
										case 5:		/* White/Bright White */
											cterm->attr |= 1;
											break;
										case 28:	/* Red*/
											cterm->attr |= 2;
											break;
										case 30:	/* Green */
											cterm->attr |= 5;
											break;
										case 31:	/* Blue */
											cterm->attr |= 6;
											break;
										case 129:	/* Orange/Magenta */
											cterm->attr |= 8;
											break;
										case 144:	/* Black */
											cterm->attr |= 0;
											break;
										case 149:	/* Brown */
											cterm->attr |= 9;
											break;
										case 150:	/* Light Red/Bright Red */
											cterm->attr |= 10;
											break;
										case 151:	/* Dark Gray/Cyan */
											cterm->attr |= 11;
											break;
										case 152:	/* Grey/Bright Black */
											cterm->attr |= 12;
											break;
										case 153:	/* Light Green/Bright Green */
											cterm->attr |= 13;
											break;
										case 154:	/* Light Blue/Bright Blue */
											cterm->attr |= 14;
											break;
										case 155:	/* Light Gray/White */
											cterm->attr |= 15;
											break;
										case 156:	/* Purple/Bright Magenta */
											cterm->attr |= 4;
											break;
										case 158:	/* Yellow/Bright Yellow */
											cterm->attr |= 7;
											break;
										case 159:	/* Cyan/Bright Cyan */
											cterm->attr |= 3;
											break;
									}
								}
								else {
									// C128 80-column
									switch(buf[j]) {
										case 5:		/* White/Bright White */
											cterm->attr |= 15;
											break;
										case 28:	/* Red*/
											cterm->attr |= 4;
											break;
										case 30:	/* Green */
											cterm->attr |= 2;
											break;
										case 31:	/* Blue */
											cterm->attr |= 1;
											break;
										case 129:	/* Orange/Magenta */
											cterm->attr |= 5;
											break;
										case 144:	/* Black */
											cterm->attr |= 0;
											break;
										case 149:	/* Brown */
											cterm->attr |= 6;
											break;
										case 150:	/* Light Red/Bright Red */
											cterm->attr |= 12;
											break;
										case 151:	/* Dark Gray/Cyan */
											cterm->attr |= 3;
											break;
										case 152:	/* Grey/Bright Black */
											cterm->attr |= 8;
											break;
										case 153:	/* Light Green/Bright Green */
											cterm->attr |= 10;
											break;
										case 154:	/* Light Blue/Bright Blue */
											cterm->attr |= 9;
											break;
										case 155:	/* Light Gray/White */
											cterm->attr |= 7;
											break;
										case 156:	/* Purple/Bright Magenta */
											cterm->attr |= 13;
											break;
										case 158:	/* Yellow/Bright Yellow */
											cterm->attr |= 14;
											break;
										case 159:	/* Cyan/Bright Cyan */
											cterm->attr |= 11;
											break;
									}
								}
								textattr(cterm->attr);
								attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
								FREE_AND_NULL(cterm->fg_tc_str);
								FREE_AND_NULL(cterm->bg_tc_str);
								break;

							/* Movement */
							case 13:	/* "\r\n" and disabled reverse. */
								c64_set_reverse(cterm, false);
								/* Fall-through */
							case 141:
								adjust_currpos(cterm, INT_MIN, 0, 0);
								/* Fall-through */
							case 17:
								adjust_currpos(cterm, 0, 1, 1);
								break;
							case 147:
								cterm_clearscreen(cterm, cterm->attr);
								/* Fall through */
							case 19:
								adjust_currpos(cterm, INT_MIN, INT_MIN, 0);
								break;
							case 20:	/* Delete (Wrapping backspace) */
								CURR_XY(&x, &y);
								if(x == CURR_MINX) {
									if (y == CURR_MINY)
										break;
									x = CURR_MINX;
									gotoxy(x, y-1);
								}
								else
									gotoxy(--x, y);
								if(x < CURR_MAXX) {
									sx = x;
									sy = y;
									coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
									ex = CURR_MAXX;
									coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, NULL);
									movetext(sx + 1, sy, ex, sy, sx, sy);
								}
								gotoxy(CURR_MAXX, y);
								cterm_clreol(cterm);
								gotoxy(x, y);
								break;
							case 157:	/* Cursor Left (wraps) */
								CURR_XY(&x, &y);
								if (x == CURR_MINX) {
									if(y > CURR_MINY)
										gotoxy(CURR_MAXX, y - 1);
								}
								else
									gotoxy(x - 1, y);
								break;
							case 29:	/* Cursor Right (wraps) */
								CURR_XY(&x, &y);
								if (x == CURR_MAXX) {
									if (y == CURR_MAXY) {
										cond_scrollup(cterm);
										gotoxy(CURR_MINX, y);
									}
									else
										gotoxy(CURR_MINX, y + 1);
								}
								else
									gotoxy(x + 1, y);
								break;
							case 145:	/* Cursor Up (No scroll */
								adjust_currpos(cterm, 0, -1, 0);
								break;
							case 148:	/* Insert TODO verify last column */
										/* CGTerm does nothing there... we */
										/* Erase under cursor. */
								CURR_XY(&x, &y);
								if (x <= CURR_MAXX) {
									sx = x;
									sy = y;
									coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
									ex = CURR_MAXX;
									coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, NULL);
									movetext(sx, sy, ex - 1, sy, sx + 1, sy);
								}
								putch(' ');
								gotoxy(x, y);
								break;

							/* Font change... whee! */
							case 14:	/* Lower case font */
								if(ti.currmode == C64_40X25) {
									setfont(33,FALSE,1);
									cterm->altfont[0] = 33;
								}
								else {	/* Assume C128 */
									setfont(35,FALSE,1);
									cterm->altfont[0] = 35;
								}
								break;
							case 142:	/* Upper case font */
								if(ti.currmode == C64_40X25) {
									setfont(32,FALSE,1);
									cterm->altfont[0] = 32;
								}
								else {	/* Assume C128 */
									setfont(34,FALSE,1);
									cterm->altfont[0] = 34;
								}
								break;
							case 18:	/* Reverse mode on */
								c64_set_reverse(cterm, true);
								break;
							case 146:	/* Reverse mode off */
								c64_set_reverse(cterm, false);
								break;

							/* Extras */
							case 7:			/* Beep */
								if(!cterm->quiet) {
									#ifdef __unix__
										putch(7);
									#else
										MessageBeep(MB_OK);
									#endif
								}
								break;
							// 0x0f - Flashing on (C128 80-column only)
							// 0x8f - Flashing off (C128 80-column only)
							// 0x02 - Flashing on (C128 80-column only)
							// 0x82 - Flashing off (C128 80-column only)
							default:
								k=buf[j];
								if(k<32 || (k > 127 && k < 160)) {
									break;
								}
								ch[0] = k;
								ch[1] = c64_get_attr(cterm);
								SCR_XY(&sx, &sy);
								puttext(sx, sy, sx, sy, ch);
								ch[1]=0;
								CURR_XY(&x, &y);
								advance_char(cterm, &x, &y, 1);
								break;
						}
					}
					else if(cterm->emulation == CTERM_EMULATION_PRESTEL) {
						switch(buf[j]) {
							case 0: // NUL
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								break;
							case 5: // ENQ
								prestel_send_memory(cterm, 0, retbuf, retsize);
								break;
							case 8: // APB (Active Position Backward)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_move(cterm, -1, 0);
								break;
							case 9: // APF (Active Position Forward)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_move(cterm, 1, 0);
								break;
							case 10: // APD (Active Position Down)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_move(cterm, 0, 1);
								break;
							case 11: // APU (Active Position Up)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_move(cterm, 0, -1);
								break;
							case 12: // CS (Clear Screen)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								cio_api.options &= ~(CONIO_OPT_PRESTEL_REVEAL);
								prestel_new_line(cterm);
								cterm_clearscreen(cterm, (char)cterm->attr);
								gotoxy(CURR_MINX, CURR_MINY);
								break;
							case 17: // Cursor on
								cterm->cursor=_NORMALCURSOR;
								ciolib_setcursortype(cterm->cursor);
								break;
							case 20: // Cursor off
								cterm->cursor=_NOCURSOR;
								ciolib_setcursortype(cterm->cursor);
								break;
							case 27: // ESC
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								cterm->sequence=1;
								break;
							case 30: // APH (Active Position Home)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								gotoxy(CURR_MINX, CURR_MINY);
								prestel_new_line(cterm);
								break;
							default:
								// "Normal" ASCII... including CR and LF in here.
								if (buf[j] == 13 || buf[j] == 10 || (buf[j] >= 32 && buf[j] <= 127)) {
									if (cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC) {
										if ((buf[j] < 64 && buf[j] >= 32) || (buf[j] >= 96 && buf[j] < 128)) {
											ch[0] = buf[j] | 0x80;
										}
										else
											ch[0] = buf[j];
										if (ch[0] >= 160) {
											cterm->prestel_last_mosaic = ch[0];
											if (cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED)
												cterm->prestel_last_mosaic &= 0x7f;
										}
									}
									*prnpos++ = ch[0];
									*prnpos = 0;
								}
								else if ((buf[j] >= 0x80) && (buf[j] <= 0x9f)) {
									// "Raw" C1 control
									uctputs(cterm, prn);
									prn[0]=0;
									prnpos = prn;
									prestel_handle_escaped(cterm, ch[0] - 64);
								}
								else if (buf[j] < 32) {
									// Unhandled C0 control
									// Section 2.3.1:
									// "The C0 CHARACTER SET... contain... characters which are not stored or displayed"
									// Section 2.3.2 appears to be specific to the C1 set
									/* Do Nothing */
								}
								else {
									// G1... unicode replacement
									*prnpos++ = ch[0];
									*prnpos = 0;
								}
								break;
						}
					}
					else if(cterm->emulation == CTERM_EMULATION_BEEB) {
						switch(buf[j]) {
							case 0: // NUL
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								break;
							// 6: Enable output (see 21)
							case 7: // Beep
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								if(!cterm->quiet) {
									#ifdef __unix__
										putch(7);
									#else
										MessageBeep(MB_OK);
									#endif
								}
								break;
							case 8: // APB (Active Position Backward)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_move(cterm, -1, 0);
								break;
							case 9: // APF (Active Position Forward)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_move(cterm, 1, 0);
								break;
							case 10: // APD (Active Position Down)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_move(cterm, 0, 1);
								break;
							case 11: // APU (Active Position Up)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								prestel_move(cterm, 0, -1);
								break;
							case 12: // CS (Clear Screen)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								cio_api.options &= ~(CONIO_OPT_PRESTEL_REVEAL);
								prestel_new_line(cterm);
								cterm_clearscreen(cterm, (char)cterm->attr);
								gotoxy(CURR_MINX, CURR_MINY);
								break;
							// 14: Page mode on
							// 15: Page mode off
							// 21: Disable output (see 6)
							// 22: Select screen mode
							case 23: // Reprogram display character (not mode 7) and disable cursor
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								cterm->escbuf[cterm->sequence++] = ch[0];
								break;
							case 28: // APS (Active Position Set) Move to position X,Y
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								cterm->escbuf[cterm->sequence++] = ch[0];
								break;
							case 30: // APH (Active Position Home)
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								gotoxy(CURR_MINX, CURR_MINY);
								prestel_new_line(cterm);
								break;
							case 127: // Destructive backspace
								*prnpos++ = '\b';
								*prnpos++ = ' ';
								*prnpos++ = '\b';
								*prnpos = 0;
								break;
							default:
								// Mode 7 VDU translates to/from ASCII...
								if (ch[0] == '#')
									ch[0] = '_';
								else if (ch[0] == '_')
									ch[0] = '`';
								else if (ch[0] == '`')
									ch[0] = '#';
								// "Normal" ASCII... including CR and LF in here.
								if (buf[j] == 13 || buf[j] == 10 || (buf[j] >= 32 && buf[j] <= 127)) {
									if (cterm->extattr & CTERM_EXTATTR_PRESTEL_MOSAIC) {
										if ((buf[j] < 64 && buf[j] >= 32) || (buf[j] >= 96 && buf[j] < 128)) {
											ch[0] = ch[0] | 0x80;
										}
										if (ch[0] >= 160) {
											cterm->prestel_last_mosaic = ch[0];
											if (cterm->extattr & CTERM_EXTATTR_PRESTEL_SEPARATED)
												cterm->prestel_last_mosaic &= 0x7f;
										}
									}
									*prnpos++ = ch[0];
									*prnpos = 0;
								}
								else if ((buf[j] >= 0x80) && (buf[j] <= 0x9f)) {
									// "Raw" C1 control
									uctputs(cterm, prn);
									prn[0]=0;
									prnpos = prn;
									prestel_handle_escaped(cterm, ch[0] - 64);
								}
								else if (buf[j] < 32) {
									// Unhandled C0 control
									// Section 2.3.1:
									// "The C0 CHARACTER SET... contain... characters which are not stored or displayed"
									// Section 2.3.2 appears to be specific to the C1 set
									/* Do Nothing */
								}
								else {
									// G1... unicode replacement
									*prnpos++ = ch[0];
									*prnpos = 0;
								}
								break;
						}
					}
					else if(cterm->emulation == CTERM_EMULATION_ATARIST_VT52) {
						switch(buf[j]) {
							case 0:
								break;
							case 1:
							case 2:
							case 3:
							case 4:
							case 5:
							case 6:
								// Ignored
								break;
							case 7:			/* Beep */
								lastch = 0;
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								if(cterm->log==CTERM_LOG_ASCII && cterm->logfile != NULL)
									fputs("\x07", cterm->logfile);
								if(!cterm->quiet) {
									#ifdef __unix__
										putch(7);
									#else
										MessageBeep(MB_OK);
									#endif
								}
								break;
							case 11:
							case 12:
								// VT and FF both move down one row and scroll.
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								adjust_currpos(cterm, 0, 1, true);
								break;
							case 14:
							case 15:
							case 16:
							case 17:
							case 18:
							case 19:
							case 20:
							case 21:
							case 22:
							case 23:
							case 24:
							case 25:
							case 26:
								break;
							case 27:		/* ESC */
								uctputs(cterm, prn);
								prn[0]=0;
								prnpos = prn;
								cterm->sequence=1;
								cterm->escbufsz = 0;
								break;
							case 28:
							case 29:
							case 30:
							case 31:
								break;
							default:
								lastch = ch[0];
								*prnpos++ = ch[0];
								*prnpos = 0;
						}
					}
					else {	/* ANSI-BBS */
						if(cterm->doorway_char) {
							uctputs(cterm, prn);
							prn[0] = 0;
							prnpos = prn;
							tmpvc[0].ch = ch[0];
							tmpvc[0].legacy_attr=cterm->attr;
							tmpvc[0].fg = cterm->fg_color;
							tmpvc[0].bg = cterm->bg_color;
							tmpvc[0].font = ciolib_attrfont(cterm->attr);
							SCR_XY(&sx, &sy);
							vmem_puttext(sx, sy, sx, sy, tmpvc);
							ch[1]=0;
							CURR_XY(&x, &y);
							advance_char(cterm, &x, &y, 1);
							cterm->doorway_char=0;
						}
						else {
							switch(buf[j]) {
								case 0:
									lastch = 0;
									if(cterm->doorway_mode)
										cterm->doorway_char=1;
									break;
								case 7:			/* Beep */
									lastch = 0;
									uctputs(cterm, prn);
									prn[0]=0;
									prnpos = prn;
									if(cterm->log==CTERM_LOG_ASCII && cterm->logfile != NULL)
										fputs("\x07", cterm->logfile);
									if(!cterm->quiet) {
										#ifdef __unix__
											putch(7);
										#else
											MessageBeep(MB_OK);
										#endif
									}
									break;
								case 12:		/* ^L - Clear screen */
									lastch = 0;
									uctputs(cterm, prn);
									prn[0]=0;
									prnpos = prn;
									if(cterm->log==CTERM_LOG_ASCII && cterm->logfile != NULL)
										fputs("\x0c", cterm->logfile);
									cterm_clearscreen(cterm, (char)cterm->attr);
									gotoxy(CURR_MINX, CURR_MINY);
									break;
								case 27:		/* ESC */
									uctputs(cterm, prn);
									prn[0]=0;
									prnpos = prn;
									cterm->sequence=1;
									break;
								default:
									lastch = ch[0];
									*prnpos++ = ch[0];
									*prnpos = 0;
							}
						}
					}
				}
			}
			uctputs(cterm, prn);
			prn[0]=0;
			prnpos = prn;
			break;
	}
	ABS_XY(&cterm->xpos, &cterm->ypos);

	hold_update=olddmc;
	puttext_can_move=oldptnm;
	CURR_XY(&x, &y);
	gotoxy(x, y);
	ciolib_setcursortype(cterm->cursor);

	/* Now rejigger the current modes palette... */
	if (mpalette) {
		for (i=0; i < 16; i++)
			palette[i] -= palette_offset;
		set_modepalette(palette);
	}

	/* De-doodle the fonts */
	setfont(orig_fonts[0], FALSE, 1);
	setfont(orig_fonts[1], FALSE, 2);
	setfont(orig_fonts[2], FALSE, 3);
	setfont(orig_fonts[3], FALSE, 4);

	if (retbuf && retbuf[0])
		return strlen(retbuf);
	return 0;
}

int cterm_openlog(struct cterminal *cterm, char *logfile, int logtype)
{
	if(!cterm->started)
		cterm_start(cterm);

	cterm->logfile=fopen(logfile, "ab");
	if(cterm->logfile==NULL)
		return(0);
	cterm->log=logtype;
	return(1);
}

void cterm_closelog(struct cterminal *cterm)
{
	if(!cterm->started)
		cterm_start(cterm);

	if(cterm->logfile != NULL)
		fclose(cterm->logfile);
	cterm->logfile=NULL;
	cterm->log=CTERM_LOG_NONE;
}

FILE *dbg;
void cterm_end(struct cterminal *cterm, int free_fonts)
{
	int i;

	if(cterm) {
		cterm_closelog(cterm);
		if (free_fonts) {
			for(i=CONIO_FIRST_FREE_FONT; i < 256; i++) {
				FREE_AND_NULL(conio_fontdata[i].eight_by_sixteen);
				FREE_AND_NULL(conio_fontdata[i].eight_by_fourteen);
				FREE_AND_NULL(conio_fontdata[i].eight_by_eight);
				FREE_AND_NULL(conio_fontdata[i].twelve_by_twenty);
				FREE_AND_NULL(conio_fontdata[i].desc);
			}
		}
		if(cterm->playnote_thread_running) {
			if(sem_trywait(&cterm->playnote_thread_terminated)==-1) {
				listSemPost(&cterm->notes);
				sem_wait(&cterm->playnote_thread_terminated);
			}
			sem_destroy(&cterm->playnote_thread_terminated);
			sem_destroy(&cterm->note_completed_sem);
			listFree(&cterm->notes);
		}

		FREE_AND_NULL(cterm->strbuf);
		FREE_AND_NULL(cterm->tabs);
		FREE_AND_NULL(cterm->fg_tc_str);
		FREE_AND_NULL(cterm->bg_tc_str);
		FREE_AND_NULL(cterm->sx_pixels);
		FREE_AND_NULL(cterm->sx_mask);
		for (i = 0; i < (sizeof(cterm->macros) / sizeof(cterm->macros[0])); i++) {
			FREE_AND_NULL(cterm->macros[i]);
		}

		free(cterm);
	}
}
