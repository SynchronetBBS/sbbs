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

#include <windows.h>	/* INPUT_RECORD, etc. */
#include <genwrap.h>
#include <stdio.h>		/* stdin */
#include "ciolib.h"
#include "keys.h"
#include "win32cio.h"

#define VID_MODES	7

const int 	cio_tabs[10]={9,17,25,33,41,49,57,65,73,80};

struct vid_mode {
	int	mode;
	int	xsize;
	int	ysize;
	int	colour;
};

const struct vid_mode vid_modes[VID_MODES]={
	 {BW40,40,25,0}
	,{C40,40,25,1}
	,{BW80,80,25,0}
	,{C80,80,25,1}
	,{MONO,80,25,1}
	,{C4350,80,50,1}
	,{C80X50,80,50,1}
};

static int lastch=0;
static int domouse=1;
static DWORD last_state=0;
static int LastX=-1, LastY=-1;
static int xpos=1;
static int ypos=1;

static int currattr=7;
static int modeidx=3;

WORD DOStoWinAttr(int newattr)
{
	WORD ret=0;

	if(newattr&0x01)
		ret|=FOREGROUND_BLUE;
	if(newattr&0x02)
		ret|=FOREGROUND_GREEN;
	if(newattr&0x04)
		ret|=FOREGROUND_RED;
	if(newattr&0x08)
		ret|=FOREGROUND_INTENSITY;
	if(newattr&0x10)
		ret|=BACKGROUND_BLUE;
	if(newattr&0x20)
		ret|=BACKGROUND_GREEN;
	if(newattr&0x40)
		ret|=BACKGROUND_RED;
	if(newattr&0x80)
		ret|=BACKGROUND_INTENSITY;
	return(ret);
}

unsigned char WintoDOSAttr(WORD newattr)
{
	unsigned char ret=0;

	if(newattr&FOREGROUND_BLUE)
		ret|=0x01;
	if(newattr&FOREGROUND_GREEN)
		ret|=0x02;
	if(newattr&FOREGROUND_RED)
		ret|=0x04;
	if(newattr&FOREGROUND_INTENSITY)
		ret|=0x08;
	if(newattr&BACKGROUND_BLUE)
		ret|=0x10;
	if(newattr&BACKGROUND_GREEN)
		ret|=0x20;
	if(newattr&BACKGROUND_RED)
		ret|=0x40;
	if(newattr&BACKGROUND_INTENSITY)
		ret|=0x80;
	return(ret);
}

int win32_keyboardio(int isgetch)
{
	INPUT_RECORD input;
	DWORD num=0;

	while(1) {
		if(lastch) {
			if(isgetch) {
				int ch;
				ch=lastch&0xff;
				lastch>>=8;
				return(ch);
			}
			else
				return(TRUE);
		}

		while(1) {
			GetNumberOfConsoleInputEvents(GetStdHandle(STD_INPUT_HANDLE), &num);
			if(num || mouse_pending())
				break;
			if(isgetch)
				SLEEP(1);
			else
				return(FALSE);
		}

		if(mouse_pending()) {
			lastch=CIO_KEY_MOUSE;
			continue;
		}

		if(!ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &input, 1, &num)
				|| !num || (input.EventType!=KEY_EVENT && input.EventType!=MOUSE_EVENT))
			continue;

		switch(input.EventType) {
			case KEY_EVENT:
				if(!input.Event.KeyEvent.bKeyDown)
					continue;
#if 0
				sprintf(str,"keydown=%d\n",input.Event.KeyEvent.bKeyDown);
				OutputDebugString(str);
				sprintf(str,"repeat=%d\n",input.Event.KeyEvent.wRepeatCount);
				OutputDebugString(str);
				sprintf(str,"keycode=%x\n",input.Event.KeyEvent.wVirtualKeyCode);
				OutputDebugString(str);
				sprintf(str,"scancode=%x\n",input.Event.KeyEvent.wVirtualScanCode);
				OutputDebugString(str);
				sprintf(str,"ascii=%d\n",input.Event.KeyEvent.uChar.AsciiChar);
				OutputDebugString(str);
				sprintf(str,"dwControlKeyState=%lx\n",input.Event.KeyEvent.dwControlKeyState);
				OutputDebugString(str);
#endif

				if(input.Event.KeyEvent.wVirtualScanCode==0x38 /* ALT */
						|| input.Event.KeyEvent.wVirtualScanCode==0x36 /* SHIFT */
						|| input.Event.KeyEvent.wVirtualScanCode==0x1D /* CTRL */ )
					break;

				if(input.Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED)) {
					if(input.Event.KeyEvent.wVirtualScanCode >= CIO_KEY_F(1)
							&& input.Event.KeyEvent.wVirtualScanCode <= CIO_KEY_F(10)) {
						/* Magic number to convert from Fx to ALT-Fx */
						lastch=(input.Event.KeyEvent.wVirtualScanCode+45)<<8;
						break;
					}
					if(input.Event.KeyEvent.wVirtualScanCode == CIO_KEY_F(11)
							&& input.Event.KeyEvent.wVirtualScanCode == CIO_KEY_F(12)) {
						/* Magic number to convert from F(x>10) to ALT-Fx */
						lastch=(input.Event.KeyEvent.wVirtualScanCode+6i)<<8;
						break;
					}

					lastch=input.Event.KeyEvent.wVirtualScanCode<<8;
					break;
				}

				if(input.Event.KeyEvent.uChar.AsciiChar)
					lastch=input.Event.KeyEvent.uChar.AsciiChar;
				else
					lastch=input.Event.KeyEvent.wVirtualScanCode<<8;
				break;
			case MOUSE_EVENT:
				if(domouse) {
					if(input.Event.MouseEvent.dwMousePosition.X+1 != LastX || input.Event.MouseEvent.dwMousePosition.Y+1 != LastY) {
						LastX=input.Event.MouseEvent.dwMousePosition.X+1;
						LastY=input.Event.MouseEvent.dwMousePosition.Y+1;
						ciomouse_gotevent(CIOLIB_MOUSE_MOVE,LastX,LastY);
					}
					if(last_state != input.Event.MouseEvent.dwButtonState) {
						switch(input.Event.MouseEvent.dwButtonState ^ last_state) {
							case FROM_LEFT_1ST_BUTTON_PRESSED:
								if(input.Event.MouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
									ciomouse_gotevent(CIOLIB_BUTTON_1_PRESS,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1);
								else
									ciomouse_gotevent(CIOLIB_BUTTON_1_RELEASE,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1);
								break;
							case FROM_LEFT_2ND_BUTTON_PRESSED:
								if(input.Event.MouseEvent.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED)
									ciomouse_gotevent(CIOLIB_BUTTON_2_PRESS,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1);
								else
									ciomouse_gotevent(CIOLIB_BUTTON_2_RELEASE,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1);
								break;
							case RIGHTMOST_BUTTON_PRESSED:
								if(input.Event.MouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
									ciomouse_gotevent(CIOLIB_BUTTON_3_PRESS,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1);
								else
									ciomouse_gotevent(CIOLIB_BUTTON_3_RELEASE,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1);
								break;
						}
						last_state=input.Event.MouseEvent.dwButtonState;
					}
				}
		}
	}
}

int win32_kbhit(void)
{
	return(win32_keyboardio(FALSE));
}

int win32_getch(void)
{
	return(win32_keyboardio(TRUE));
}

int win32_getche(void)
{
	int ch;

	ch=win32_getch();
	if(ch)
		putch(ch);
	return(ch);
}

#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_INSERT_MODE		0x0020
#define ENABLE_QUICK_EDIT_MODE	0x0040
#define ENABLE_EXTENDED_FLAGS	0x0080
#define ENABLE_AUTO_POSITION	0x0100
#endif

int win32_initciolib(long inmode)
{
	DWORD conmode;
	int	i,j;

	if(!isatty(fileno(stdin)))
		return(0);
	if(!GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &conmode))
		return(0);
	conmode&=~(ENABLE_PROCESSED_INPUT|ENABLE_QUICK_EDIT_MODE);
	conmode|=ENABLE_MOUSE_INPUT;
	if(!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), conmode))
		return(0);

	if(!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &conmode))
		return(0);
	conmode&=~ENABLE_PROCESSED_OUTPUT;
	conmode&=~ENABLE_WRAP_AT_EOL_OUTPUT;
	if(!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), conmode))
		return(0);

	win32_textmode(inmode);
	cio_api.mouse=1;
	j=vid_modes[modeidx].ysize*vid_modes[modeidx].xsize*2;
	return(1);
}

int win32_hidemouse(void)
{
	/* domouse=0; */
	return(0);
}

int win32_showmouse(void)
{
	/* domouse=1; */
	return(0);
}

void win32_textmode(int mode)
{
	int i;
	COORD	sz;
	SMALL_RECT	rc;

	for(i=0;i<VID_MODES;i++) {
		if(vid_modes[i].mode==mode)
			modeidx=i;
	}
	sz.X=vid_modes[modeidx].xsize;
	sz.Y=vid_modes[modeidx].ysize;
	rc.Left=0;
	rc.Right=vid_modes[modeidx].xsize-1;
	rc.Top=0;
	rc.Bottom=vid_modes[modeidx].ysize-1;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),sz);
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE),TRUE,&rc);
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),sz);
}

int win32_gettext(int left, int top, int right, int bottom, void* buf)
{
	CHAR_INFO *ci;
	int	x;
	int	y;
	COORD	bs;
	COORD	bc;
	SMALL_RECT	reg;
	unsigned char	*bu;

	bu=buf;
	bs.X=right-left+1;
	bs.Y=bottom-top+1;
	bc.X=0;
	bc.Y=0;
	reg.Left=left-1;
	reg.Right=right-1;
	reg.Top=top-1;
	reg.Bottom=bottom-1;
	ci=(CHAR_INFO *)malloc(sizeof(CHAR_INFO)*(bs.X*bs.Y));
	ReadConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE),ci,bs,bc,&reg);
	for(y=0;y<=(bottom-top);y++) {
		for(x=0;x<=(right-left);x++) {
			bu[((y*bs.X)+x)*2]=ci[(y*bs.X)+x].Char.AsciiChar;
			bu[(((y*bs.X)+x)*2)+1]=WintoDOSAttr(ci[(y*bs.X)+x].Attributes);
		}
	}
	free(ci);
	return 1;
}

void win32_gettextinfo(struct text_info* info)
{
	info->currmode=vid_modes[modeidx].mode;
	info->curx=xpos;
	info->cury=ypos;
	info->attribute=currattr;
	info->screenheight=vid_modes[modeidx].ysize;
	info->screenwidth=vid_modes[modeidx].xsize;
}

void win32_gotoxy(int x, int y)
{
	COORD	cp;

	xpos=x;
	ypos=y;
	cp.X=x-1;
	cp.Y=y-1;
	if(!dont_move_cursor)
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE),cp);
}

void win32_highvideo(void)
{
	win32_textattr(currattr|0x08);
}


void win32_lowvideo(void)
{
	win32_textattr(currattr&0xf8);
}


void win32_normvideo(void)
{
	win32_textattr(7);
}

int win32_puttext(int left, int top, int right, int bottom, void* buf)
{
	CHAR_INFO *ci;
	int	x;
	int	y;
	COORD	bs;
	COORD	bc;
	SMALL_RECT	reg;
	unsigned char	*bu;

	bu=buf;
	bs.X=right-left+1;
	bs.Y=bottom-top+1;
	bc.X=0;
	bc.Y=0;
	reg.Left=left-1;
	reg.Right=right-1;
	reg.Top=top-1;
	reg.Bottom=bottom-1;
	ci=(CHAR_INFO *)malloc(sizeof(CHAR_INFO)*(bs.X*bs.Y));
	for(y=0;y<bs.Y;y++) {
		for(x=0;x<bs.X;x++) {
			ci[(y*bs.X)+x].Char.AsciiChar=bu[((y*bs.X)+x)*2];
			ci[(y*bs.X)+x].Attributes=DOStoWinAttr(bu[(((y*bs.X)+x)*2)+1]);
		}
	}
	WriteConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE),ci,bs,bc,&reg);
	free(ci);
	return 1;
}

void win32_textattr(int newattr)
{
	/* SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),DOStoWinAttr(newattr)); */
	currattr=newattr;
}


void win32_textbackground(int newcolor)
{
	win32_textattr((currattr&0x0f)|((newcolor&0xf0)<<4));
}


void win32_textcolor(int newcolor)
{
	win32_textattr((currattr&0xf0)|((newcolor&0x0f)<<4));
}

void win32_setcursortype(int type)
{
	CONSOLE_CURSOR_INFO	ci;

	switch(type) {
		case _NOCURSOR:
			ci.bVisible=FALSE;
			break;
		
		case _SOLIDCURSOR:
			ci.bVisible=TRUE;
			ci.dwSize=100;
			break;
		
		default:	/* Normal cursor */
			ci.bVisible=TRUE;
			ci.dwSize=13;
			break;
	}
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),&ci);
}

int win32_wherex(void)
{
	return(xpos);
}

int win32_wherey(void)
{
	return(ypos);
}

int win32_putch(int ch)
{
	struct text_info ti;
	unsigned char buf[2];

	buf[0]=ch;
	buf[1]=currattr;

	gettextinfo(&ti);
	switch(ch) {
		case '\r':
			win32_gotoxy(1,ypos);
			break;
		case '\n':
			if(ypos==ti.winbottom-ti.wintop+1)
				wscroll();
			else
				win32_gotoxy(xpos,ypos+1);
			break;
		case '\b':
			if(ti.curx>ti.winleft) {
				buf[0]=' ';
				win32_gotoxy(xpos-1,ypos);
				puttext(xpos,ypos,xpos,ypos,buf);
			}
			break;
		case 7:		/* Bell */
			MessageBeep(MB_OK);
			break;
		default:
			if(ypos==ti.winbottom-ti.wintop+1
					&& xpos==ti.winright-ti.winleft+1) {
				puttext(xpos,ypos,xpos,ypos,buf);
				win32_gotoxy(1,ypos);
				wscroll();
			}
			else {
				if(xpos==ti.winright-ti.winleft+1) {
					puttext(xpos,ypos,xpos,ypos,buf);
					win32_gotoxy(1,ypos+1);
				}
				else {
					puttext(xpos,ypos,xpos,ypos,buf);
					win32_gotoxy(xpos+1,ypos);
				}
			}
			break;
	}
	return(ch);
}

void win32_settitle(const char *title)
{
	SetConsoleTitle(title);
}
