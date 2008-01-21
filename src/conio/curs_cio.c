/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

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
#include "keys.h"
#include "mouse.h"
#include "vidmodes.h"

static unsigned char curs_nextgetch=0;

static int lastattr=0;
static long mode;

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
			return(COLOR_BLACK);
		case 9 :
			return(COLOR_BLUE);
		case 10 :
			return(COLOR_GREEN);
		case 11 :
			return(COLOR_CYAN);
		case 12 :
			return(COLOR_RED);
		case 13 :
			return(COLOR_MAGENTA);
		case 14 :
			return(COLOR_YELLOW);
		case 15 :
			return(COLOR_WHITE);
	}
	return(0);
}

static int _putch(unsigned char ch, BOOL refresh_now)
{
	int		ret;
	chtype	cha;

	if(!(mode==CIOLIB_MODE_CURSES_IBM))
	{
		switch(ch)
		{
			case 30:
				cha=ACS_UARROW;
				break;
			case 31:
				cha=ACS_DARROW;
				break;
			case 176:
				cha=ACS_CKBOARD;
				break;
			case 177:
				cha=ACS_BOARD;
				break;
			case 178:
				cha=ACS_BOARD;
				break;
			case 179:
				cha=ACS_SBSB;
				break;
			case 180:
				cha=ACS_SBSS;
				break;
			case 181:
				cha=ACS_SBSD;
				break;
			case 182:
				cha=ACS_DBDS;
				break;
			case 183:
				cha=ACS_BBDS;
				break;
			case 184:
				cha=ACS_BBSD;
				break;
			case 185:
				cha=ACS_DBDD;
				break;
			case 186:
				cha=ACS_DBDB;
				break;
			case 187:
				cha=ACS_BBDD;
				break;
			case 188:
				cha=ACS_DBBD;
				break;
			case 189:
				cha=ACS_DBBS;
				break;
			case 190:
				cha=ACS_SBBD;
				break;
			case 191:
				cha=ACS_BBSS;
				break;
			case 192:
				cha=ACS_SSBB;
				break;
			case 193:
				cha=ACS_SSBS;
				break;
			case 194:
				cha=ACS_BSSS;
				break;
			case 195:
				cha=ACS_SSSB;
				break;
			case 196:
				cha=ACS_BSBS;
				break;
			case 197:
				cha=ACS_SSSS;
				break;
			case 198:
				cha=ACS_SDSB;
				break;
			case 199:
				cha=ACS_DSDB;
				break;
			case 200:
				cha=ACS_DDBB;
				break;
			case 201:
				cha=ACS_BDDB;
				break;
			case 202:
				cha=ACS_DDBD;
				break;
			case 203:
				cha=ACS_BDDD;
				break;
			case 204:
				cha=ACS_DDDB;
				break;
			case 205:
				cha=ACS_BDBD;
				break;
			case 206:
				cha=ACS_DDDD;
				break;
			case 207:
				cha=ACS_SDBD;
				break;
			case 208:
				cha=ACS_DSBS;
				break;
			case 209:
				cha=ACS_BDSD;
				break;
			case 210:
				cha=ACS_BSDS;
				break;
			case 211:
				cha=ACS_DSBB;
				break;
			case 212:
				cha=ACS_SDBB;
				break;
			case 213:
				cha=ACS_BDSB;
				break;
			case 214:
				cha=ACS_BSDB;
				break;
			case 215:
				cha=ACS_DSDS;
				break;
			case 216:
				cha=ACS_SDSD;
				break;
			case 217:
				cha=ACS_SBBS;
				break;
			case 218:
				cha=ACS_BSSB;
				break;
			case 219:
				cha=ACS_BLOCK;
				break;
			default:
				cha=ch;
		}
	}
	else
		cha=ch;

	if(!cha)
		cha=' ';
	if(cha == ' ')
		ret=addch(A_BOLD|' ');
	else if (cha<' ') {
 		attron(A_REVERSE);
		ret=addch(cha+'A'-1);
		attroff(A_REVERSE);
	}
	else
		ret=addch(cha);

	if(!hold_update) {
		if(refresh_now)
			refresh();
	}

	return(ret);
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

	fill=fillbuf;

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
			_putch(fill_char,FALSE);
		}
	}
	textattr(orig_attr);
	move(oldy, oldx);
	if(!hold_update)
		refresh();
	return(1);
}

int curs_gettext(int sx, int sy, int ex, int ey, void *fillbuf)
{
	int x,y;
	int fillpos=0;
	chtype attr;
	unsigned char attrib;
	unsigned char colour;
	int oldx, oldy;
	unsigned char thischar;
	int	ext_char;
	struct text_info	ti;
	unsigned char *fill;

	fill=fillbuf;
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
			attr=mvinch(y, x);
			if(attr&A_REVERSE) {
				thischar=(attr&255)-'A'+1;
			}
			else if(attr&A_ALTCHARSET) {
				ext_char=A_ALTCHARSET|(attr&255);
				if(!(mode==CIOLIB_MODE_CURSES_IBM)){
					/* likely ones */
					if (ext_char == ACS_CKBOARD)
					{
						thischar=176;
					}
					else if (ext_char == ACS_BOARD)
					{
						thischar=177;
					}
					else if (ext_char == ACS_BSSB)
					{
						thischar=218;
					}
					else if (ext_char == ACS_SSBB)
					{
						thischar=192;
					}
					else if (ext_char == ACS_BBSS)
					{
						thischar=191;
					}
					else if (ext_char == ACS_SBBS)
					{
						thischar=217;
					}
					else if (ext_char == ACS_SBSS)
					{
						thischar=180;
					}
					else if (ext_char == ACS_SSSB)
					{
						thischar=195;
					}
					else if (ext_char == ACS_SSBS)
					{
						thischar=193;
					}
					else if (ext_char == ACS_BSSS)
					{
						thischar=194;
					}
					else if (ext_char == ACS_BSBS)
					{
						thischar=196;
					}
					else if (ext_char == ACS_SBSB)
					{
						thischar=179;
					}
					else if (ext_char == ACS_SSSS)
					{
						thischar=197;
					}
					else if (ext_char == ACS_BLOCK)
					{
						thischar=219;
					}
					else if (ext_char == ACS_UARROW)
					{
						thischar=30;
					}
					else if (ext_char == ACS_DARROW)
					{
						thischar=31;
					}

					/* unlikely (Not in ncurses) */
					else if (ext_char == ACS_SBSD)
					{
						thischar=181;
					}
					else if (ext_char == ACS_DBDS)
					{
						thischar=182;
					}
					else if (ext_char == ACS_BBDS)
					{
						thischar=183;
					}
					else if (ext_char == ACS_BBSD)
					{
						thischar=184;
					}
					else if (ext_char == ACS_DBDD)
					{
						thischar=185;
					}
					else if (ext_char == ACS_DBDB)
					{
						thischar=186;
					}
					else if (ext_char == ACS_BBDD)
					{
						thischar=187;
					}
					else if (ext_char == ACS_DBBD)
					{
						thischar=188;
					}
					else if (ext_char == ACS_DBBS)
					{
						thischar=189;
					}
					else if (ext_char == ACS_SBBD)
					{
						thischar=190;
					}
					else if (ext_char == ACS_SDSB)
					{
						thischar=198;
					}
					else if (ext_char == ACS_DSDB)
					{
						thischar=199;
					}
					else if (ext_char == ACS_DDBB)
					{
						thischar=200;
					}
					else if (ext_char == ACS_BDDB)
					{
						thischar=201;
					}
					else if (ext_char == ACS_DDBD)
					{
						thischar=202;
					}
					else if (ext_char == ACS_BDDD)
					{
						thischar=203;
					}
					else if (ext_char == ACS_DDDB)
					{
						thischar=204;
					}
					else if (ext_char == ACS_BDBD)
					{
						thischar=205;
					}
					else if (ext_char == ACS_DDDD)
					{
						thischar=206;
					}
					else if (ext_char == ACS_SDBD)
					{
						thischar=207;
					}
					else if (ext_char == ACS_DSBS)
					{
						thischar=208;
					}
					else if (ext_char == ACS_BDSD)
					{
						thischar=209;
					}
					else if (ext_char == ACS_BSDS)
					{
						thischar=210;
					}
					else if (ext_char == ACS_DSBB)
					{
						thischar=211;
					}
					else if (ext_char == ACS_SDBB)
					{
						thischar=212;
					}
					else if (ext_char == ACS_BDSB)
					{
						thischar=213;
					}
					else if (ext_char == ACS_BSDB)
					{
						thischar=214;
					}
					else if (ext_char == ACS_DSDS)
					{
						thischar=215;
					}
					else if (ext_char == ACS_SDSD)
					{
						thischar=216;
					}
					else
					{
						thischar=attr&255;
					}
				}
				else {
					if (ext_char == ACS_UARROW)
					{
						thischar=30;
					}
					else if (ext_char == ACS_DARROW)
					{
						thischar=31;
					}
					else
					{
						thischar=attr&255;
					}
				}
			}
			else
				thischar=attr;
			fill[fillpos++]=(unsigned char)(thischar);
			attrib=0;
			if (attr & A_BOLD)
			{
				attrib |= 8;
			}
			if (attr & A_BLINK)
			{
				attrib |= 128;
			}
			colour=PAIR_NUMBER(attr&A_COLOR)-1;
			colour=((colour&56)<<1)|(colour&7);
			fill[fillpos++]=colour|attrib;
		}
	}
	move(oldy, oldx);
	return(1);
}

void curs_textattr(int attr)
{
	chtype   attrs=A_NORMAL;
	int	colour;

	if (lastattr==attr)
		return;

	lastattr=attr;
	
	if (attr & 8)  {
		attrs |= A_BOLD;
	}
	if (attr & 128)
	{
		attrs |= A_BLINK;
	}
	colour = COLOR_PAIR( ((attr&7)|((attr>>1)&56))+1 );
#ifdef NCURSES_VERSION_MAJOR
	attrset(attrs);
	color_set(colour,NULL);
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
	if(mouse_pending())
		return(1);
	timeout.tv_sec=0;
	timeout.tv_usec=0;
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);

	return(select(fileno(stdin)+1,&rfds,NULL,NULL,&timeout));
}

void curs_gotoxy(int x, int y)
{
	int absx,absy;
	absx=x+cio_textinfo.winleft-1;
	absy=y+cio_textinfo.wintop-1;

	move(absy-1,absx-1);
	if(!hold_update)
		refresh();

	cio_textinfo.curx=x;
	cio_textinfo.cury=y;
}

void curs_suspend(void)
{
	noraw();
	endwin();
}

void curs_resume(void)
{
	raw();
	timeout(10);
	refresh();
}

int curs_initciolib(long inmode)
{
	short	fg, bg, pair=0;

#ifdef XCURSES
	char	*argv[2]={"ciolib",NULL};

	Xinitscr(1,argv);
#else
	char *term;
	SCREEN *tst;

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

	/* Set up color pairs */
	for(bg=0;bg<8;bg++)  {
		for(fg=0;fg<8;fg++) {
			init_pair(++pair,curses_color(fg),curses_color(bg));
		}
	}
	mode = inmode;
#ifdef NCURSES_VERSION_MAJOR
		if(mousemask(BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON2_PRESSED|BUTTON2_RELEASED|BUTTON3_PRESSED|BUTTON3_RELEASED|REPORT_MOUSE_POSITION,NULL)==(BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON2_PRESSED|BUTTON2_RELEASED|BUTTON3_PRESSED|BUTTON3_RELEASED|REPORT_MOUSE_POSITION)) {
			mouseinterval(0);
			cio_api.mouse=1;
		}
		else
			mousemask(0,NULL);
#endif

	curs_textmode(0);
	return(1);
}

void curs_setcursortype(int type) {
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
	int ch;
#ifdef NCURSES_VERSION_MAJOR
	MEVENT mevnt;
#endif

	if(curs_nextgetch) {
		ch=curs_nextgetch;
		curs_nextgetch=0;
	}
	else {
		while((ch=getch())==ERR) {
			if(mouse_trywait()) {
				curs_nextgetch=CIO_KEY_MOUSE>>8;
				return(CIO_KEY_MOUSE & 0xff);
			}
		}
		if(ch > 255) {
			switch(ch) {
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
							case REPORT_MOUSE_POSITION:
								evnt=CIOLIB_MOUSE_MOVE;
								break;
						}
						ciomouse_gotevent(evnt, mevnt.x+1, mevnt.y+1);
					}
					break;
#endif

				default:
					curs_nextgetch=0xff;
					ch=0;
					break;
			}
		}
	}
	return(ch);
}

void curs_textmode(int mode)
{
	getmaxyx(stdscr, cio_textinfo.screenheight, cio_textinfo.screenwidth);
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

int curs_beep(void)
{
	return(beep());
}
