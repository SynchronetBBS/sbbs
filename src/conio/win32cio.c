/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#if defined(_WIN32)
 #include <malloc.h>	/* alloca() on Win32 */
#endif

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "keys.h"
#include "vidmodes.h"
#include "win32cio.h"

struct keyvals {
	int	VirtualKeyCode
		,Key
		,Shift
		,CTRL
		,ALT;
};

const struct keyvals keyval[] =
{
	{VK_BACK, 0x08, 0x08, 0x7f, 0x0e00},
	{VK_TAB, 0x09, 0x0f00, 0x9400, 0xa500},
	{VK_RETURN, 0x0d, 0x0d, 0x0a, 0xa600},
	{VK_ESCAPE, 0x1b, 0x1b, 0x1b, 0x0100},
	{VK_SPACE, 0x20, 0x20, 0x0300, 0x20,},
	{'0', '0', ')', 0, 0x8100},
	{'1', '1', '!', 0, 0x7800},
	{'2', '2', '@', 0x0300, 0x7900},
	{'3', '3', '#', 0, 0x7a00},
	{'4', '4', '$', 0, 0x7b00},
	{'5', '5', '%', 0, 0x7c00},
	{'6', '6', '^', 0x1e, 0x7d00},
	{'7', '7', '&', 0, 0x7e00},
	{'8', '8', '*', 0, 0x7f00},
	{'9', '9', '(', 0, 0x8000},
	{'A', 'a', 'A', 0x01, 0x1e00},
	{'B', 'b', 'B', 0x02, 0x3000},
	{'C', 'c', 'C', 0x03, 0x2e00},
	{'D', 'd', 'D', 0x04, 0x2000},
	{'E', 'e', 'E', 0x05, 0x1200},
	{'F', 'f', 'F', 0x06, 0x2100},
	{'G', 'g', 'G', 0x07, 0x2200},
	{'H', 'h', 'H', 0x08, 0x2300},
	{'I', 'i', 'I', 0x09, 0x1700},
	{'J', 'j', 'J', 0x0a, 0x2400},
	{'K', 'k', 'K', 0x0b, 0x2500},
	{'L', 'l', 'L', 0x0c, 0x2600},
	{'M', 'm', 'M', 0x0d, 0x3200},
	{'N', 'n', 'N', 0x0e, 0x3100},
	{'O', 'o', 'O', 0x0f, 0x1800},
	{'P', 'p', 'P', 0x10, 0x1900},
	{'Q', 'q', 'Q', 0x11, 0x1000},
	{'R', 'r', 'R', 0x12, 0x1300},
	{'S', 's', 'S', 0x13, 0x1f00},
	{'T', 't', 'T', 0x14, 0x1400},
	{'U', 'u', 'U', 0x15, 0x1600},
	{'V', 'v', 'V', 0x16, 0x2f00},
	{'W', 'w', 'W', 0x17, 0x1100},
	{'X', 'x', 'X', 0x18, 0x2d00},
	{'Y', 'y', 'Y', 0x19, 0x1500},
	{'Z', 'z', 'Z', 0x1a, 0x2c00},
	{VK_PRIOR, 0x4900, 0x4900, 0x8400, 0x9900},
	{VK_NEXT, 0x5100, 0x5100, 0x7600, 0xa100},
	{VK_END, 0x4f00, 0x4f00, 0x7500, 0x9f00},
	{VK_HOME, 0x4700, 0x4700, 0x7700, 0x9700},
	{VK_LEFT, 0x4b00, 0x4b00, 0x7300, 0x9b00},
	{VK_UP, 0x4800, 0x4800, 0x8d00, 0x9800},
	{VK_RIGHT, 0x4d00, 0x4d00, 0x7400, 0x9d00},
	{VK_DOWN, 0x5000, 0x5000, 0x9100, 0xa000},
	{VK_INSERT, 0x5200, 0x5200, 0x9200, 0xa200},
	{VK_DELETE, 0x5300, 0x5300, 0x9300, 0xa300},
	{VK_NUMPAD0, '0', 0x5200, 0x9200, 0},
	{VK_NUMPAD1, '1', 0x4f00, 0x7500, 0},
	{VK_NUMPAD2, '2', 0x5000, 0x9100, 0},
	{VK_NUMPAD3, '3', 0x5100, 0x7600, 0},
	{VK_NUMPAD4, '4', 0x4b00, 0x7300, 0},
	{VK_NUMPAD5, '5', 0x4c00, 0x8f00, 0},
	{VK_NUMPAD6, '6', 0x4d00, 0x7400, 0},
	{VK_NUMPAD7, '7', 0x4700, 0x7700, 0},
	{VK_NUMPAD8, '8', 0x4800, 0x8d00, 0},
	{VK_NUMPAD9, '9', 0x4900, 0x8400, 0},
	{VK_MULTIPLY, '*', '*', 0x9600, 0x3700},
	{VK_ADD, '+', '+', 0x9000, 0x4e00},
	{VK_SUBTRACT, '-', '-', 0x8e00, 0x4a00},
	{VK_DECIMAL, '.', '.', 0x5300, 0x9300},
	{VK_DIVIDE, '/', '/', 0x9500, 0xa400},
	{VK_F1, 0x3b00, 0x5400, 0x5e00, 0x6800},
	{VK_F2, 0x3c00, 0x5500, 0x5f00, 0x6900},
	{VK_F3, 0x3d00, 0x5600, 0x6000, 0x6a00},
	{VK_F4, 0x3e00, 0x5700, 0x6100, 0x6b00},
	{VK_F5, 0x3f00, 0x5800, 0x6200, 0x6c00},
	{VK_F6, 0x4000, 0x5900, 0x6300, 0x6d00},
	{VK_F7, 0x4100, 0x5a00, 0x6400, 0x6e00},
	{VK_F8, 0x4200, 0x5b00, 0x6500, 0x6f00},
	{VK_F9, 0x4300, 0x5c00, 0x6600, 0x7000},
	{VK_F10, 0x4400, 0x5d00, 0x6700, 0x7100},
	{VK_F11, 0x8500, 0x8700, 0x8900, 0x8b00},
	{VK_F12, 0x8600, 0x8800, 0x8a00, 0x8c00},
	{0xdc, '\\', '|', 0x1c, 0x2b00},
	{0xbf, '/', '?', 0, 0x3500},
	{0xbd, '-', '_', 0x1f, 0x8200},
	{0xbb, '=', '+', 0, 0x8300},
	{0xdb, '[', '{', 0x1b, 0x1a00},
	{0xdd, ']', '}', 0x1d, 0x1b00},
	{0xba, ';', ':', 0, 0x2700},
	{0xde, '\'', '"', 0, 0x2800},
	{0xbc, ',', '<', 0, 0x3300},
	{0xbe, '.', '>', 0, 0x3400},
	{0xc0, '`', '~', 0, 0x2900},
	{VK_PAUSE, 0x13, 0x13, 0x13, 0x2900},
	{0, 0, 0, 0, 0}	/** END **/
};

/* Mouse related stuff */
static int domouse=1;
static DWORD last_state=0;
static int LastX=-1, LastY=-1;

static int modeidx=3;

#if defined(_DEBUG)
static void dputs(const char* str)
{
	char msg[1024];

	SAFEPRINTF(msg,"%s\r\n",str);
	OutputDebugString(msg);
}
#endif

static void dprintf(const char* fmt, ...)
{
#if defined(_DEBUG)
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    dputs(sbuf);
#endif /* _DEBUG */
}

static WORD DOStoWinAttr(int newattr)
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

static unsigned char WintoDOSAttr(WORD newattr)
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

static int win32_getchcode(WORD code, DWORD state)
{
	int i;

	for(i=0;keyval[i].Key;i++) {
		if(keyval[i].VirtualKeyCode==code) {
			if(state & (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED))
				return(keyval[i].ALT);
			if(state & (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED))
				return(keyval[i].CTRL);
			if((state & (CAPSLOCK_ON)) && isalpha(keyval[i].Key)) {
				if(!(state & SHIFT_PRESSED))
					return(keyval[i].Shift);
			}
			else {
				if(state & (SHIFT_PRESSED))
					return(keyval[i].Shift);
			}
			return(keyval[i].Key);
		}
	}
	return(0);
}

static int win32_keyboardio(int isgetch)
{
	INPUT_RECORD input;
	DWORD num=0;
	HANDLE h;
	static WORD lastch;

	if((h=GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE)
		return(0);

	while(1) {
		if(lastch) {
			if(isgetch) {
				BYTE ch;
				ch=lastch&0xff;
				lastch>>=8;
				return(ch);
			}
			else
				return(TRUE);
		}

		while(1) {
			GetNumberOfConsoleInputEvents(h, &num);
			if(num)
				break;
			if(mouse_trywait()) {
				lastch=CIO_KEY_MOUSE;
				break;
			}
			if(isgetch)
				SLEEP(1);
			else
				return(FALSE);
		}

		if(lastch)
			continue;

		if(!ReadConsoleInput(h, &input, 1, &num)
				|| !num || (input.EventType!=KEY_EVENT && input.EventType!=MOUSE_EVENT))
			continue;

		switch(input.EventType) {
			case KEY_EVENT:

				dprintf("KEY_EVENT: KeyDown=%u"
					,input.Event.KeyEvent.bKeyDown);
				dprintf("           RepeatCount=%u"
					,input.Event.KeyEvent.wRepeatCount);
				dprintf("           VirtualKeyCode=0x%04hX"
					,input.Event.KeyEvent.wVirtualKeyCode);
				dprintf("           VirtualScanCode=0x%04hX"
					,input.Event.KeyEvent.wVirtualScanCode);
				dprintf("           uChar.AsciiChar=0x%02X (%u)"
					,(BYTE)input.Event.KeyEvent.uChar.AsciiChar
					,(BYTE)input.Event.KeyEvent.uChar.AsciiChar);
				dprintf("           ControlKeyState=0x%08lX"
					,input.Event.KeyEvent.dwControlKeyState); 

				if(input.Event.KeyEvent.bKeyDown) {
					/* Is this an AltGr key? */
					if(((input.Event.KeyEvent.dwControlKeyState & (RIGHT_ALT_PRESSED|LEFT_CTRL_PRESSED)) == (RIGHT_ALT_PRESSED|LEFT_CTRL_PRESSED))
							&& (BYTE)input.Event.KeyEvent.uChar.AsciiChar) {
						lastch=(BYTE)input.Event.KeyEvent.uChar.AsciiChar;
					}
					/* Is this a modified char? */
					else if((input.Event.KeyEvent.dwControlKeyState & (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED|RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED|ENHANCED_KEY))
							|| (input.Event.KeyEvent.wVirtualKeyCode >= VK_F1 && input.Event.KeyEvent.wVirtualKeyCode <= VK_F24)
							|| !input.Event.KeyEvent.uChar.AsciiChar) {
						lastch=win32_getchcode(input.Event.KeyEvent.wVirtualKeyCode, input.Event.KeyEvent.dwControlKeyState);
					}
					/* Must be a normal char then! */
					else {
						lastch=(BYTE)input.Event.KeyEvent.uChar.AsciiChar;
					}
				} else if(input.Event.KeyEvent.wVirtualKeyCode == VK_MENU)
					lastch=(BYTE)input.Event.KeyEvent.uChar.AsciiChar;

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
	int ret=win32_keyboardio(TRUE);
	dprintf("win32_getch = 0x%02X (%u)", (BYTE)ret, (BYTE)ret);
	return(ret);
}

#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_INSERT_MODE		0x0020
#define ENABLE_QUICK_EDIT_MODE	0x0040
#define ENABLE_EXTENDED_FLAGS	0x0080
#define ENABLE_AUTO_POSITION	0x0100
#endif

static DWORD	orig_in_conmode=0;
static DWORD	orig_out_conmode=0;
static void *	win32_suspendbuf=NULL;

void win32_suspend(void)
{
	HANDLE h;

	if((h=GetStdHandle(STD_INPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleMode(h, orig_in_conmode);
	if((h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleMode(h, orig_out_conmode);
}

void win32_resume(void)
{
	DWORD	conmode;
	HANDLE	h;

    conmode=orig_in_conmode;
    conmode&=~(ENABLE_PROCESSED_INPUT|ENABLE_QUICK_EDIT_MODE);
    conmode|=ENABLE_MOUSE_INPUT;
	if((h=GetStdHandle(STD_INPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleMode(h, conmode);

    conmode=orig_out_conmode;
    conmode&=~ENABLE_PROCESSED_OUTPUT;
    conmode&=~ENABLE_WRAP_AT_EOL_OUTPUT;
	if((h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleMode(h, conmode);
}

int win32_initciolib(long inmode)
{
	DWORD	conmode;
	int		i,j;
	HANDLE	h;
	CONSOLE_SCREEN_BUFFER_INFO	sbuff;

	if(!isatty(fileno(stdin))) {
		if(!AllocConsole())
			return(0);
	}

	SetConsoleCtrlHandler(NULL,TRUE);
	if((h=GetStdHandle(STD_INPUT_HANDLE))==INVALID_HANDLE_VALUE
		|| !GetConsoleMode(h, &orig_in_conmode))
		return(0);
	conmode=orig_in_conmode;
	conmode&=~(ENABLE_PROCESSED_INPUT|ENABLE_QUICK_EDIT_MODE);
	conmode|=ENABLE_MOUSE_INPUT;
	if(!SetConsoleMode(h, conmode))
		return(0);

	if((h=GetStdHandle(STD_OUTPUT_HANDLE))==INVALID_HANDLE_VALUE
		|| !GetConsoleMode(h, &orig_out_conmode))
		return(0);
	conmode=orig_out_conmode;
	conmode&=~ENABLE_PROCESSED_OUTPUT;
	conmode&=~ENABLE_WRAP_AT_EOL_OUTPUT;
	if(!SetConsoleMode(h, conmode))
		return(0);

	if(GetConsoleScreenBufferInfo(h, &sbuff)==0) {
		win32_textmode(C80);
	}
	else {
		/* Switch to closest mode to current screen size */
		i=sbuff.srWindow.Right-sbuff.srWindow.Left+1;
		j=sbuff.srWindow.Bottom-sbuff.srWindow.Top+1;
		if(i>=132) {
			if(j<25)
				win32_textmode(VESA_132X21);
			else if(j<28)
				win32_textmode(VESA_132X25);
			else if(j<30)
				win32_textmode(VESA_132X28);
			else if(j<34)
				win32_textmode(VESA_132X30);
			else if(j<43)
				win32_textmode(VESA_132X34);
			else if(j<50)
				win32_textmode(VESA_132X43);
			else if(j<60)
				win32_textmode(VESA_132X50);
			else
				win32_textmode(VESA_132X60);
		}
		if(i>=80) {
			if(j<21)
				win32_textmode(C80X14);
			else if(j<25)
				win32_textmode(C80X21);
			else if(j<28)
				win32_textmode(C80);
			else if(j<43)
				win32_textmode(C80X28);
			else if(j<50)
				win32_textmode(C80X43);
			else if(j<60)
				win32_textmode(C80X50);
			else
				win32_textmode(C80X60);
		}
		else {
			if(j<21)
				win32_textmode(C40X14);
			else if(j<25)
				win32_textmode(C40X21);
			else if(j<28)
				win32_textmode(C40);
			else if(j<43)
				win32_textmode(C40X28);
			else if(j<50)
				win32_textmode(C40X43);
			else if(j<60)
				win32_textmode(C40X50);
			else
				win32_textmode(C40X60);
		}
	}

	cio_api.mouse=1;
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
	int		i;
	HANDLE	h;
	COORD	sz;
	SMALL_RECT	rc;

	for(i=0;i<NUMMODES;i++) {
		if(vparams[i].mode==mode)
			modeidx=i;
	}
	sz.X=vparams[modeidx].cols;
	sz.Y=vparams[modeidx].rows;
	rc.Left=0;
	rc.Right=vparams[modeidx].cols-1;
	rc.Top=0;
	rc.Bottom=vparams[modeidx].rows-1;

	if((h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE) {
		SetConsoleScreenBufferSize(h,sz);
		SetConsoleWindowInfo(h,TRUE,&rc);
		SetConsoleScreenBufferSize(h,sz);
	}

	cio_textinfo.attribute=7;
	cio_textinfo.normattr=7;
	cio_textinfo.currmode=vparams[modeidx].mode;
	cio_textinfo.screenheight=(unsigned char)sz.Y;
	cio_textinfo.screenwidth=(unsigned char)sz.X;
	cio_textinfo.curx=1;
	cio_textinfo.cury=1;
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
}

int win32_gettext(int left, int top, int right, int bottom, void* buf)
{
	CHAR_INFO *ci;
	int	x;
	int	y;
	COORD	bs;
	COORD	bc;
	HANDLE	h;
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
	ci=(CHAR_INFO *)alloca(sizeof(CHAR_INFO)*(bs.X*bs.Y));
	if((h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		ReadConsoleOutput(h,ci,bs,bc,&reg);
	for(y=0;y<=(bottom-top);y++) {
		for(x=0;x<=(right-left);x++) {
			bu[((y*bs.X)+x)*2]=ci[(y*bs.X)+x].Char.AsciiChar;
			bu[(((y*bs.X)+x)*2)+1]=WintoDOSAttr(ci[(y*bs.X)+x].Attributes);
		}
	}
	return 1;
}

void win32_gotoxy(int x, int y)
{
	COORD	cp;
	HANDLE	h;
	static int curx=-1;
	static int cury=-1;

	cio_textinfo.curx=x;
	cio_textinfo.cury=y;
	cp.X=cio_textinfo.winleft+x-2;
	cp.Y=cio_textinfo.wintop+y-2;
	if(cp.X != curx || cp.Y != cury) {
		if(!hold_update && (h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE) {
			SetConsoleCursorPosition(h,cp);
			curx=cp.X;
			cury=cp.Y;
		}
	}
}

int win32_puttext(int left, int top, int right, int bottom, void* buf)
{
	CHAR_INFO *ci;
	int	x;
	int	y;
	HANDLE	h;
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
	ci=(CHAR_INFO *)alloca(sizeof(CHAR_INFO)*(bs.X*bs.Y));
	for(y=0;y<bs.Y;y++) {
		for(x=0;x<bs.X;x++) {
			ci[(y*bs.X)+x].Char.AsciiChar=bu[((y*bs.X)+x)*2];
			ci[(y*bs.X)+x].Attributes=DOStoWinAttr(bu[(((y*bs.X)+x)*2)+1]);
		}
	}
	if((h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		WriteConsoleOutput(h,ci,bs,bc,&reg);
	return 1;
}

void win32_setcursortype(int type)
{
	HANDLE h;
	CONSOLE_CURSOR_INFO	ci;

	switch(type) {
		case _NOCURSOR:
			ci.bVisible=FALSE;
			ci.dwSize=1;
			break;
		
		case _SOLIDCURSOR:
			ci.bVisible=TRUE;
			ci.dwSize=99;
			break;
		
		default:	/* Normal cursor */
			ci.bVisible=TRUE;
			ci.dwSize=13;
			break;
	}
	if((h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleCursorInfo(h,&ci);
}

void win32_settitle(const char *title)
{
	SetConsoleTitle(title);
}

void win32_copytext(const char *text, size_t buflen)
{
	HGLOBAL	clipbuf;
	LPTSTR	clip;

	if(!OpenClipboard(NULL))
		return;
	EmptyClipboard();
	clipbuf=GlobalAlloc(GMEM_MOVEABLE, buflen+1);
	if(clipbuf==NULL) {
		CloseClipboard();
		return;
	}
	clip=GlobalLock(clipbuf);
	memcpy(clip, text, buflen);
	clip[buflen]=0;
	GlobalUnlock(clipbuf);
	SetClipboardData(CF_OEMTEXT, clipbuf);
	CloseClipboard();
}

char *win32_getcliptext(void)
{
	HGLOBAL	clipbuf;
	LPTSTR	clip;
	char *ret;

	if(!IsClipboardFormatAvailable(CF_OEMTEXT))
		return(NULL);
	if(!OpenClipboard(NULL))
		return(NULL);
	clipbuf=GetClipboardData(CF_OEMTEXT);
	if(clipbuf!=NULL) {
		clip=GlobalLock(clipbuf);
		ret=(char *)malloc(strlen(clip)+1);
		if(ret != NULL)
			strcpy(ret, clip);
		GlobalUnlock(clipbuf);
	}
	CloseClipboard();
	
	return(ret);
}
