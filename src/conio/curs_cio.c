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

#include <langinfo.h>
#include <locale.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "gen_defs.h"	/* xpdev, for BOOL/TRUE/FALSE */

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "curs_cio.h"
#include "vidmodes.h"
#include "utf8_codepages.h"

static unsigned char curs_nextgetch=0;

static int lastattr=0;
static long mode;
static int vflags=0;
static int suspended = 0;
static int mpress = 0;
void curs_resume(void);

static int default_font=0;
static int current_font[4]={0, 0, 0, 0};

static short curses_color(short color)
{
	switch(color)
	{
		case 0 :
			return(COLOR_BLACK);
		case 1 :
			return(COLOR_BLUE);
		case 2 :
			return(COLOR_GREEN);
		case 3 :
			return(COLOR_CYAN);
		case 4 :
			return(COLOR_RED);
		case 5 :
			return(COLOR_MAGENTA);
		case 6 :
			return(COLOR_YELLOW);
		case 7 :
			return(COLOR_WHITE);
		case 8 :
			return(COLOR_BLACK+8);
		case 9 :
			return(COLOR_BLUE+8);
		case 10 :
			return(COLOR_GREEN+8);
		case 11 :
			return(COLOR_CYAN+8);
		case 12 :
			return(COLOR_RED+8);
		case 13 :
			return(COLOR_MAGENTA+8);
		case 14 :
			return(COLOR_YELLOW+8);
		case 15 :
			return(COLOR_WHITE+8);
	}
	return(0);
}

static int _putch(unsigned char ch, BOOL refresh_now, int cp)
{
	int	ret;
	cchar_t	cha;
	wchar_t wch[2] = {0};
	attr_t	attr = 0;
	short	cpair = 0;

	attr_get(&attr, &cpair, NULL);
	attr &= ~A_COLOR;
	switch (mode) {
		case CIOLIB_MODE_CURSES_ASCII:
			switch(ch)
			{
				case 30:
					wch[0]=ACS_UARROW & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 31:
					wch[0]=ACS_DARROW & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 176:
					wch[0]=ACS_CKBOARD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 177:
					wch[0]=ACS_BOARD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 178:
					wch[0]=ACS_BOARD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 179:
					wch[0]=ACS_SBSB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 180:
					wch[0]=ACS_SBSS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 181:
					wch[0]=ACS_SBSD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 182:
					wch[0]=ACS_DBDS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 183:
					wch[0]=ACS_BBDS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 184:
					wch[0]=ACS_BBSD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 185:
					wch[0]=ACS_DBDD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 186:
					wch[0]=ACS_DBDB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 187:
					wch[0]=ACS_BBDD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 188:
					wch[0]=ACS_DBBD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 189:
					wch[0]=ACS_DBBS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 190:
					wch[0]=ACS_SBBD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 191:
					wch[0]=ACS_BBSS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 192:
					wch[0]=ACS_SSBB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 193:
					wch[0]=ACS_SSBS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 194:
					wch[0]=ACS_BSSS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 195:
					wch[0]=ACS_SSSB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 196:
					wch[0]=ACS_BSBS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 197:
					wch[0]=ACS_SSSS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 198:
					wch[0]=ACS_SDSB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 199:
					wch[0]=ACS_DSDB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 200:
					wch[0]=ACS_DDBB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 201:
					wch[0]=ACS_BDDB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 202:
					wch[0]=ACS_DDBD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 203:
					wch[0]=ACS_BDDD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 204:
					wch[0]=ACS_DDDB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 205:
					wch[0]=ACS_BDBD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 206:
					wch[0]=ACS_DDDD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 207:
					wch[0]=ACS_SDBD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 208:
					wch[0]=ACS_DSBS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 209:
					wch[0]=ACS_BDSD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 210:
					wch[0]=ACS_BSDS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 211:
					wch[0]=ACS_DSBB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 212:
					wch[0]=ACS_SDBB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 213:
					wch[0]=ACS_BDSB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 214:
					wch[0]=ACS_BSDB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 215:
					wch[0]=ACS_DSDS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 216:
					wch[0]=ACS_SDSD & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 217:
					wch[0]=ACS_SBBS & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 218:
					wch[0]=ACS_BSSB & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 219:
					wch[0]=ACS_BLOCK & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				case 254:
					wch[0]=ACS_BULLET & A_CHARTEXT;
					attr |= WA_ALTCHARSET;
					break;
				default:
					wch[0]=ch;
			}
			break;
		case CIOLIB_MODE_CURSES_IBM:
			wch[0]=ch;
			break;
		case CIOLIB_MODE_CURSES:
			wch[0] = cpoint_from_cpchar_ext(cp, ch);
			break;
	}

	if(!wch[0]) {
		wch[0]=' ';
		attr |= WA_BOLD;
	}
	else {
		attr &= ~WA_BOLD;
	}
	curs_resume();
	if (wch[0] < ' ') {
		attr |= WA_REVERSE;
		wch[0] = wch[0] + 'A' - 1;
	}

	setcchar(&cha, wch, attr, cpair, NULL);
	ret = add_wch(&cha);

	if(!hold_update) {
		if(refresh_now)
			refresh();
	}

	return(ret);
}

int curs_vmem_puttext(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int x,y;
	int fillpos=0;
	unsigned char attr;
	unsigned char fill_char;
	unsigned char orig_attr;
	int oldx, oldy;

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > cio_textinfo.screenwidth
			|| sy > cio_textinfo.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > cio_textinfo.screenwidth
			|| ey > cio_textinfo.screenheight
			|| fill==NULL)
		return(0);

	curs_resume();
	getyx(stdscr,oldy,oldx);
	orig_attr=lastattr;
	for(y=sy-1;y<ey;y++)
	{
		for(x=sx-1;x<ex;x++)
		{
			fill_char=fill[fillpos].ch;
			attr=fill[fillpos].legacy_attr;
			textattr(attr);
			move(y, x);
			_putch(fill_char,FALSE,conio_fontdata[fill[fillpos].font].cp);
			fillpos++;
		}
	}
	textattr(orig_attr);
	move(oldy, oldx);
	if(!hold_update)
		refresh();
	return(1);
}

int curs_puttext(int sx, int sy, int ex, int ey, void *fillbuf)
{
	int x,y;
	int fillpos=0;
	unsigned char attr;
	unsigned char fill_char;
	unsigned char orig_attr;
	int oldx, oldy;
	unsigned char *fill;

	fill = fillbuf;

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > cio_textinfo.screenwidth
			|| sy > cio_textinfo.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > cio_textinfo.screenwidth
			|| ey > cio_textinfo.screenheight
			|| fill==NULL)
		return(0);

	curs_resume();
	getyx(stdscr,oldy,oldx);
	orig_attr=lastattr;
	for(y=sy-1;y<ey;y++)
	{
		for(x=sx-1;x<ex;x++)
		{
			fill_char=fill[fillpos++];
			attr=fill[fillpos++];
			textattr(attr);
			move(y, x);
			_putch(fill_char,FALSE,getcodepage());
		}
	}
	textattr(orig_attr);
	move(oldy, oldx);
	if(!hold_update)
		refresh();
	return(1);
}

int curs_vmem_gettext(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int x,y;
	int fillpos=0;
	unsigned char attrib;
	unsigned char colour;
	short cpair;
	int oldx, oldy;
#ifdef CURSES_CCHAR_MAX
	wchar_t allchars[CURSES_CCHAR_MAX];
#else
#ifdef CCHARW_MAX
	wchar_t allchars[CCHARW_MAX];
#endif
#endif
	unsigned char thischar;
	int	ext_char;
	struct text_info	ti;
	attr_t attr;
	cchar_t cchar;
	int fnt = current_font[0];
	int cp;
	fd_set cp_checked;

	curs_resume();
	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	getyx(stdscr,oldy,oldx);
	for(y=sy-1;y<=ey-1;y++)
	{
		for(x=sx-1;x<=ex-1;x++)
		{
			mvin_wch(y, x, &cchar);
			getcchar(&cchar, allchars, &attr, &cpair, NULL);
			thischar = ext_char = allchars[0];
			if(attr&WA_REVERSE) {
				thischar=(thischar)-'A'+1;
			}
			else {
				switch (mode) {
					case CIOLIB_MODE_CURSES_ASCII:
						/* likely ones */
						if (attr & WA_ALTCHARSET) {
							if (ext_char == (ACS_CKBOARD & A_CHARTEXT))
							{
								thischar=176;
							}
							else if (ext_char == (ACS_BOARD & A_CHARTEXT))
							{
								thischar=177;
							}
							else if (ext_char == (ACS_BSSB & A_CHARTEXT))
							{
								thischar=218;
							}
							else if (ext_char == (ACS_SSBB & A_CHARTEXT))
							{
								thischar=192;
							}
							else if (ext_char == (ACS_BBSS & A_CHARTEXT))
							{
								thischar=191;
							}
							else if (ext_char == (ACS_SBBS & A_CHARTEXT))
							{
								thischar=217;
							}
							else if (ext_char == (ACS_SBSS & A_CHARTEXT))
							{
								thischar=180;
							}
							else if (ext_char == (ACS_SSSB & A_CHARTEXT))
							{
								thischar=195;
							}
							else if (ext_char == (ACS_SSBS & A_CHARTEXT))
							{
								thischar=193;
							}
							else if (ext_char == (ACS_BSSS & A_CHARTEXT))
							{
								thischar=194;
							}
							else if (ext_char == (ACS_BSBS & A_CHARTEXT))
							{
								thischar=196;
							}
							else if (ext_char == (ACS_SBSB & A_CHARTEXT))
							{
								thischar=179;
							}
							else if (ext_char == (ACS_SSSS & A_CHARTEXT))
							{
								thischar=197;
							}
							else if (ext_char == (ACS_BLOCK & A_CHARTEXT))
							{
								thischar=219;
							}
							else if (ext_char == (ACS_UARROW & A_CHARTEXT))
							{
								thischar=30;
							}
							else if (ext_char == (ACS_DARROW & A_CHARTEXT))
							{
								thischar=31;
							}

							/* unlikely (Not in ncurses) */
							else if (ext_char == (ACS_SBSD & A_CHARTEXT))
							{
								thischar=181;
							}
							else if (ext_char == (ACS_DBDS & A_CHARTEXT))
							{
								thischar=182;
							}
							else if (ext_char == (ACS_BBDS & A_CHARTEXT))
							{
								thischar=183;
							}
							else if (ext_char == (ACS_BBSD & A_CHARTEXT))
							{
								thischar=184;
							}
							else if (ext_char == (ACS_DBDD & A_CHARTEXT))
							{
								thischar=185;
							}
							else if (ext_char == (ACS_DBDB & A_CHARTEXT))
							{
								thischar=186;
							}
							else if (ext_char == (ACS_BBDD & A_CHARTEXT))
							{
								thischar=187;
							}
							else if (ext_char == (ACS_DBBD & A_CHARTEXT))
							{
								thischar=188;
							}
							else if (ext_char == (ACS_DBBS & A_CHARTEXT))
							{
								thischar=189;
							}
							else if (ext_char == (ACS_SBBD & A_CHARTEXT))
							{
								thischar=190;
							}
							else if (ext_char == (ACS_SDSB & A_CHARTEXT))
							{
								thischar=198;
							}
							else if (ext_char == (ACS_DSDB & A_CHARTEXT))
							{
								thischar=199;
							}
							else if (ext_char == (ACS_DDBB & A_CHARTEXT))
							{
								thischar=200;
							}
							else if (ext_char == (ACS_BDDB & A_CHARTEXT))
							{
								thischar=201;
							}
							else if (ext_char == (ACS_DDBD & A_CHARTEXT))
							{
								thischar=202;
							}
							else if (ext_char == (ACS_BDDD & A_CHARTEXT))
							{
								thischar=203;
							}
							else if (ext_char == (ACS_DDDB & A_CHARTEXT))
							{
								thischar=204;
							}
							else if (ext_char == (ACS_BDBD & A_CHARTEXT))
							{
								thischar=205;
							}
							else if (ext_char == (ACS_DDDD & A_CHARTEXT))
							{
								thischar=206;
							}
							else if (ext_char == (ACS_SDBD & A_CHARTEXT))
							{
								thischar=207;
							}
							else if (ext_char == (ACS_DSBS & A_CHARTEXT))
							{
								thischar=208;
							}
							else if (ext_char == (ACS_BDSD & A_CHARTEXT))
							{
								thischar=209;
							}
							else if (ext_char == (ACS_BSDS & A_CHARTEXT))
							{
								thischar=210;
							}
							else if (ext_char == (ACS_DSBB & A_CHARTEXT))
							{
								thischar=211;
							}
							else if (ext_char == (ACS_SDBB & A_CHARTEXT))
							{
								thischar=212;
							}
							else if (ext_char == (ACS_BDSB & A_CHARTEXT))
							{
								thischar=213;
							}
							else if (ext_char == (ACS_BSDB & A_CHARTEXT))
							{
								thischar=214;
							}
							else if (ext_char == (ACS_DSDS & A_CHARTEXT))
							{
								thischar=215;
							}
							else if (ext_char == (ACS_SDSD & A_CHARTEXT))
							{
								thischar=216;
							}
						}
						fnt = 0;
						break;
					case CIOLIB_MODE_CURSES_IBM:
						if (ext_char == (ACS_UARROW & A_CHARTEXT))
						{
							thischar=30;
						}
						else if (ext_char == (ACS_DARROW & A_CHARTEXT))
						{
							thischar=31;
						}
						fnt = 0;
						break;
					case CIOLIB_MODE_CURSES:
						thischar = cpchar_from_unicode_cpoint(getcodepage(), ext_char, 0);
						if (thischar == 0 && ext_char != 0) {
							FD_ZERO(&cp_checked);
							for (fnt = 0; fnt < 256; fnt++) {
								cp = conio_fontdata[fnt].cp;
								if (!FD_ISSET(cp, &cp_checked)) {
									FD_SET(cp, &cp_checked);
									thischar = cpchar_from_unicode_cpoint(cp, ext_char, 0);
									if (thischar)
										break;
								}
							}
							if (fnt == 256) {
								fnt = current_font[0];
								thischar = '?';
							}
						}
						break;
				}
			}
			fill[fillpos].ch=(unsigned char)(thischar);
			attrib=0;
			if (attr & WA_BOLD) {
				if (thischar != ' ')
					attrib |= 8;
			}
			if (attr & WA_BLINK)
			{
				attrib |= 128;
			}
			colour=PAIR_NUMBER(attr&A_COLOR)-1;
			if (COLORS >= 16)
				colour=colour&0x7f;
			else
				colour=((colour&56)<<1)|(colour&7);
			fill[fillpos].legacy_attr = colour|attrib;
			attr2palette(attrib, &fill[fillpos].fg, &fill[fillpos].bg);
			fill[fillpos].font = fnt;
			fillpos++;
		}
	}
	move(oldy, oldx);
	return(1);
}

void curs_textattr(int attr)
{
	chtype   attrs=WA_NORMAL;
	int	colour;
	int	fg,bg;

	if (lastattr==attr)
		return;

	lastattr=attr;

	fg = attr & 0x0f;
	bg = attr & 0xf0;

	if (vflags & CIOLIB_VIDEO_NOBRIGHT)
		fg &= 0x07;

	if (!(vflags & CIOLIB_VIDEO_BGBRIGHT))
		bg &= 0x70;

	if (attr & 128)
	{
		if (!(vflags & CIOLIB_VIDEO_NOBLINK))
			attrs |= WA_BLINK;
	}

	if (COLORS >= 16) {
		colour = COLOR_PAIR( ((fg|bg)+1) );
	}
	else {
		if (fg & 8)  {
			attrs |= WA_BOLD;
		}
		colour = COLOR_PAIR( ((fg&7)|((bg&0x70)>>1))+1 );
	}
	curs_resume();
#ifdef NCURSES_VERSION_MAJOR
	attrset(attrs);
	color_set(colour, NULL);
#else
	attrset(attrs|colour);
#endif
	/* bkgdset(colour); */
	bkgdset(colour);

	cio_textinfo.attribute=attr;
}

int curs_kbhit(void)
{
	struct timeval timeout;
	fd_set	rfds;

	if(curs_nextgetch)
		return(1);
	if(mpress || mouse_trywait()) {
		mpress = 1;
		return(1);
	}
	timeout.tv_sec=0;
	timeout.tv_usec=0;
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);

	return(select(fileno(stdin)+1,&rfds,NULL,NULL,&timeout) == 1);
}

void curs_gotoxy(int x, int y)
{
	int absx,absy;
	absx=x+cio_textinfo.winleft-1;
	absy=y+cio_textinfo.wintop-1;

	curs_resume();
	move(absy-1,absx-1);
	if(!hold_update)
		refresh();

	cio_textinfo.curx=x;
	cio_textinfo.cury=y;
}

void curs_suspend(void)
{
	if (!suspended) {
		noraw();
		endwin();
	}
	suspended = 1;
}

void curs_resume(void)
{
	wint_t ch;
	if (suspended) {
		raw();
		timeout(10);
		refresh();
		get_wch(&ch);
	}
	suspended = 0;
}

int curs_initciolib(int inmode)
{
	short	fg, bg, pair=0;
	char *p;

	setlocale(LC_ALL, "");
	if (inmode == CIOLIB_MODE_AUTO)
		inmode = CIOLIB_MODE_CURSES;
	if (inmode == CIOLIB_MODE_CURSES) {
		p = nl_langinfo(CODESET);
		if (p == NULL)
			inmode = CIOLIB_MODE_CURSES_ASCII;
		else if (!strcasestr(p, "UTF"))
			if (!strcasestr(p, "UCS2"))
				if (!strcasestr(p, "Unicode"))
					if (!strcasestr(p, "GB18030"))
						inmode = CIOLIB_MODE_CURSES_ASCII;
	}

#ifdef XCURSES
	char	*argv[2]={"ciolib",NULL};

	Xinitscr(1,argv);
#else
	char *term;
	SCREEN *tst;

	cio_api.options = 0;

	term=getenv("TERM");
	if(term==NULL)
		return(0);
	tst=newterm(term,stdout,stdin);
	if(tst==NULL)
		return(0);
	endwin();
	initscr();
#endif
	start_color();
	cbreak();
	noecho();
	nonl();
	keypad(stdscr, TRUE);
	scrollok(stdscr,FALSE);
	halfdelay(1);
	raw();
	timeout(10);
	atexit(curs_suspend);
	suspended = 0;

	/* Set up color pairs */
	if (COLORS >= 16) {
		for(bg=0;bg<16;bg++)  {
			for(fg=0;fg<16;fg++) {
				init_pair(++pair,curses_color(fg),curses_color(bg));
			}
		}
	}
	else {
		for(bg=0;bg<8;bg++)  {
			for(fg=0;fg<8;fg++) {
				init_pair(++pair,curses_color(fg),curses_color(bg));
			}
		}
	}
	mode = inmode;
#ifdef NCURSES_VERSION_MAJOR
	{
		mmask_t msk = BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON2_PRESSED|BUTTON2_RELEASED|BUTTON3_PRESSED|BUTTON3_RELEASED|REPORT_MOUSE_POSITION;
#ifdef BUTTON5_PRESSED
		msk |= BUTTON4_PRESSED|BUTTON5_PRESSED;
#endif
		if (mousemask(msk, NULL) == msk) {
			mouseinterval(0);
			cio_api.mouse=1;
		}
		else {
			mousemask(0,NULL);
		}
	}
#endif

	if (COLORS >= 16)
		cio_api.options = CONIO_OPT_BRIGHT_BACKGROUND;
	if (can_change_color())
		cio_api.options |= CONIO_OPT_PALETTE_SETTING;
	curs_textmode(0);
	return(1);
}

void curs_setcursortype(int type) {
	curs_resume();
	switch(type) {
		case _NOCURSOR:
			curs_set(0);
			break;

		case _SOLIDCURSOR:
			curs_set(2);
			break;

		default:	/* Normal cursor */
			curs_set(1);
			break;

	}
	if(!hold_update)
		refresh();
}

int curs_getch(void)
{
	wint_t ch;
	wint_t newch;
#ifdef NCURSES_VERSION_MAJOR
	MEVENT mevnt;
#endif

	if(curs_nextgetch) {
		ch=curs_nextgetch;
		curs_nextgetch=0;
	}
	else {
		curs_resume();
		while(get_wch(&ch)==ERR) {
			if(mpress || mouse_trywait()) {
				mpress = 0;
				curs_nextgetch=CIO_KEY_MOUSE>>8;
				return(CIO_KEY_MOUSE & 0xff);
			}
		}
		switch(ch) {
			case 0xe0:
				curs_nextgetch = 0xe0;	/* Literal 0xe0 */
				break;
			case KEY_DOWN:            /* Down-arrow */
				curs_nextgetch=0x50;
				ch=0;
				break;

			case KEY_UP:		/* Up-arrow */
				curs_nextgetch=0x48;
				ch=0;
				break;

			case KEY_LEFT:		/* Left-arrow */
				curs_nextgetch=0x4b;
				ch=0;
				break;

			case KEY_RIGHT:            /* Right-arrow */
				curs_nextgetch=0x4d;
				ch=0;
				break;

			case KEY_HOME:            /* Home key (upward+left arrow) */
				curs_nextgetch=0x47;
				ch=0;
				break;

			case KEY_BACKSPACE:            /* Backspace (unreliable) */
				ch=8;
				break;

			case KEY_F(1):			/* Function Key */
				curs_nextgetch=0x3b;
				ch=0;
				break;

			case KEY_F(2):			/* Function Key */
				curs_nextgetch=0x3c;
				ch=0;
				break;

			case KEY_F(3):			/* Function Key */
				curs_nextgetch=0x3d;
				ch=0;
				break;

			case KEY_F(4):			/* Function Key */
				curs_nextgetch=0x3e;
				ch=0;
				break;

			case KEY_F(5):			/* Function Key */
				curs_nextgetch=0x3f;
				ch=0;
				break;

			case KEY_F(6):			/* Function Key */
				curs_nextgetch=0x40;
				ch=0;
				break;

			case KEY_F(7):			/* Function Key */
				curs_nextgetch=0x41;
				ch=0;
				break;

			case KEY_F(8):			/* Function Key */
				curs_nextgetch=0x42;
				ch=0;
				break;

			case KEY_F(9):			/* Function Key */
				curs_nextgetch=0x43;
				ch=0;
				break;

			case KEY_F(10):			/* Function Key */
				curs_nextgetch=0x44;
				ch=0;
				break;

			case KEY_F(11):			/* Function Key */
				curs_nextgetch=0x57;
				ch=0;
				break;

			case KEY_F(12):			/* Function Key */
				curs_nextgetch=0x58;
				ch=0;
				break;

			case KEY_DC:            /* Delete character */
				curs_nextgetch=0x53;
				ch=0;
				break;

			case KEY_IC:            /* Insert char or enter insert mode */
				curs_nextgetch=0x52;
				ch=0;
				break;

			case KEY_EIC:            /* Exit insert char mode */
				curs_nextgetch=0x52;
				ch=0;
				break;

			case KEY_NPAGE:            /* Next page */
				curs_nextgetch=0x51;
				ch=0;
				break;

			case KEY_PPAGE:            /* Previous page */
				curs_nextgetch=0x49;
				ch=0;
				break;

			case KEY_ENTER:            /* Enter or send (unreliable) */
				curs_nextgetch=0x0d;
				ch=0;
				break;

			case KEY_A1:		/* Upper left of keypad */
				curs_nextgetch=0x47;
				ch=0;
				break;

			case KEY_A3:		/* Upper right of keypad */
				curs_nextgetch=0x49;
				ch=0;
				break;

			case KEY_B2:		/* Center of keypad */
				curs_nextgetch=0x4c;
				ch=0;
				break;

			case KEY_C1:		/* Lower left of keypad */
				curs_nextgetch=0x4f;
				ch=0;
				break;

			case KEY_C3:		/* Lower right of keypad */
				curs_nextgetch=0x51;
				ch=0;
				break;

			case KEY_BEG:		/* Beg (beginning) */
				curs_nextgetch=0x47;
				ch=0;
				break;

			case KEY_CANCEL:		/* Cancel */
				curs_nextgetch=0x03;
				ch=0;
				break;

			case KEY_END:		/* End */
				curs_nextgetch=0x4f;
				ch=0;
				break;

			case KEY_SELECT:		/* Select  - Is "End" in X */
				curs_nextgetch=0x4f;
				ch=0;
				break;

#ifdef NCURSES_VERSION_MAJOR
			case KEY_MOUSE:			/* Mouse stuff */
				if(getmouse(&mevnt)==OK) {
					int evnt=0;
					switch(mevnt.bstate) {
						case BUTTON1_PRESSED:
							evnt=CIOLIB_BUTTON_1_PRESS;
							break;
						case BUTTON1_RELEASED:
							evnt=CIOLIB_BUTTON_1_RELEASE;
							break;
						case BUTTON2_PRESSED:
							evnt=CIOLIB_BUTTON_2_PRESS;
							break;
						case BUTTON2_RELEASED:
							evnt=CIOLIB_BUTTON_2_RELEASE;
							break;
						case BUTTON3_PRESSED:
							evnt=CIOLIB_BUTTON_3_PRESS;
							break;
						case BUTTON3_RELEASED:
							evnt=CIOLIB_BUTTON_3_RELEASE;
							break;
#ifdef BUTTON5_PRESSED
						case BUTTON4_PRESSED:
							evnt=CIOLIB_BUTTON_4_PRESS;
							break;
						case BUTTON5_PRESSED:
							evnt=CIOLIB_BUTTON_5_PRESS;
							break;
#endif
						case REPORT_MOUSE_POSITION:
							evnt=CIOLIB_MOUSE_MOVE;
							break;
					}
					ciomouse_gotevent(evnt, mevnt.x+1, mevnt.y+1, -1, -1);
				}
				break;
#endif

			default:
				// Don't translate CTRL-x "keys"...
				newch = cpchar_from_unicode_cpoint(getcodepage(), ch, 0);
				if (newch == 0 && ch < 32)
					newch = ch;
				ch = newch;
				break;
		}
	}
	return(ch);
}

static int
scale_integer_up(int from)
{
	int ret = from * 1000 / 256;

	if (ret < 0)
		ret = 0;
	if (ret > 1000)
		ret = 1000;
	return ret;
}

uint32_t palette[16];
int curs_attr2palette(uint8_t attr, uint32_t *fgp, uint32_t *bgp)
{
	uint32_t fg = attr & 0x0f;
	uint32_t bg = (attr >> 4) & 0x0f;

	if (!(vflags & CIOLIB_VIDEO_BGBRIGHT))
		bg &= 0x07;
	if (vflags & CIOLIB_VIDEO_NOBRIGHT)
		fg &= 0x07;

	if (fgp)
		*fgp = palette[fg];
	if (bgp)
		*bgp = palette[bg];

	return 1;
}

int curs_setpalette(uint32_t entry, uint16_t r, uint16_t g, uint16_t b)
{
	if (!can_change_color())
		return 0;
	init_color(curses_color(entry), scale_integer_up(r>>8), scale_integer_up(g>>8), scale_integer_up(b>>8));
	return 1;
}

int curs_set_modepalette(uint32_t p[16])
{
	int i;

	if (!can_change_color())
		return 0;
	memcpy(palette, p, sizeof(palette));
	for (i = 0; i < 16; i++) {
		struct dac_colors *c;
		c = &dac_default[p[i]];
		curs_setpalette(i, c->red<<8 | c->red, c->green<<8 | c->green, c->blue<<8 | c->blue);
	}
	return 1;
}

int curs_get_modepalette(uint32_t p[16])
{
	if (!can_change_color())
		return 0;
	memcpy(p, palette, sizeof(palette));
	return 1;
}

void curs_textmode(int mode)
{
	int vm;
	int maxx, maxy;

	curs_resume();
	getmaxyx(stdscr, maxy, maxx);
	if (maxy > 255)
		maxy = 255;
	if (maxy < 0)
		maxy = 24;
	if (maxx > 255)
		maxx = 255;
	if (maxx < 0)
		maxx = 80;
	cio_textinfo.screenheight = maxy;
	cio_textinfo.screenwidth = maxx;
	if(has_colors())
		cio_textinfo.currmode=COLOR_MODE;
	else
		cio_textinfo.currmode=MONO;

	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
	cio_textinfo.attribute=7;
	cio_textinfo.normattr=7;
	cio_textinfo.curx=1;
	cio_textinfo.cury=1;

	if(can_change_color() && (vm = find_vmode(cio_textinfo.currmode)) != -1) {
		curs_set_modepalette(palettes[vparams[vm].palette]);
		cio_textinfo.normattr=cio_textinfo.attribute=vparams[vm].default_attr;
		vflags = vparams[vm].flags;
	}

	return;
}

int curs_hidemouse(void)
{
/*
	#ifdef NCURSES_VERSION_MAJOR
		mousemask(0,NULL);
		return(0);
	#else
		return(-1);
	#endif
*/
	return(-1);
}

int curs_showmouse(void)
{
/*
	#ifdef NCURSES_VERSION_MAJOR
		if(mousemask(BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON2_PRESSED|BUTTON2_RELEASED|BUTTON3_PRESSED|BUTTON3_RELEASED|REPORT_MOUSE_POSITION,NULL)==BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON2_PRESSED|BUTTON2_RELEASED|BUTTON3_PRESSED|BUTTON3_RELEASED|REPORT_MOUSE_POSITION)
			return(0);
	#endif
	return(-1);
*/
	return(-1);
}

void curs_beep(void)
{
	curs_resume();
	beep();
}

int curs_getvideoflags(void)
{
	return vflags;
}

void curs_setvideoflags(int flags)
{
	flags &= (CIOLIB_VIDEO_NOBRIGHT|CIOLIB_VIDEO_BGBRIGHT|CIOLIB_VIDEO_NOBLINK);
	if (COLORS < 16)
		flags &= ~CIOLIB_VIDEO_BGBRIGHT;
	vflags = flags;
}

int curs_setfont(int font, int force, int font_num)
{
	if (mode != CIOLIB_MODE_CURSES)
		return 0;

	if(font < 0 || font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2))
		return(0);

	switch(font_num) {
		case 0:
			default_font=font;
			/* Fall-through */
		case 1:
			current_font[0]=font;
			break;
		case 2:
		case 3:
		case 4:
			current_font[font_num-1]=font;
			break;
	}

	return(1);
}

int curs_getfont(int font_num)
{
	int ret;

	if (font_num == 0)
		ret = default_font;
	else if (font_num > 4)
		ret = -1;
	else
		ret = current_font[font_num - 1];

	return ret;
}
