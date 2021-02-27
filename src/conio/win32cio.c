/* $Id: win32cio.c,v 1.115 2020/06/27 00:04:45 deuce Exp $ */

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

#include "ciolib.h"
#include "vidmodes.h"
#include "win32cio.h"

struct keyvals {
	int	VirtualKeyCode
		,Key
		,Shift
		,CTRL
		,ALT;
};

CIOLIBEXPORTVAR const struct keyvals keyval[] =
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
	{VK_INSERT, CIO_KEY_IC, CIO_KEY_SHIFT_IC, CIO_KEY_CTRL_IC, CIO_KEY_ALT_IC},
	{VK_DELETE, CIO_KEY_DC, CIO_KEY_SHIFT_DC, CIO_KEY_CTRL_DC, CIO_KEY_CTRL_IC},
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

#ifdef DEBUG_KEY_EVENTS
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
#endif

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
						ciomouse_gotevent(CIOLIB_MOUSE_MOVE,LastX,LastY, -1, -1);
					}
					if (input.Event.MouseEvent.dwEventFlags == 0) {
						if(last_state != input.Event.MouseEvent.dwButtonState) {
							switch(input.Event.MouseEvent.dwButtonState ^ last_state) {
								case FROM_LEFT_1ST_BUTTON_PRESSED:
									if(input.Event.MouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
										ciomouse_gotevent(CIOLIB_BUTTON_1_PRESS,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1, -1, -1);
									else
										ciomouse_gotevent(CIOLIB_BUTTON_1_RELEASE,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1, -1, -1);
									break;
								case FROM_LEFT_2ND_BUTTON_PRESSED:
									if(input.Event.MouseEvent.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED)
										ciomouse_gotevent(CIOLIB_BUTTON_2_PRESS,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1, -1, -1);
									else
										ciomouse_gotevent(CIOLIB_BUTTON_2_RELEASE,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1, -1, -1);
									break;
								case RIGHTMOST_BUTTON_PRESSED:
									if(input.Event.MouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
										ciomouse_gotevent(CIOLIB_BUTTON_3_PRESS,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1, -1, -1);
									else
										ciomouse_gotevent(CIOLIB_BUTTON_3_RELEASE,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1, -1, -1);
									break;
							}
							last_state=input.Event.MouseEvent.dwButtonState;
						}
					}
					else if (input.Event.MouseEvent.dwEventFlags == MOUSE_WHEELED) {
						// If the high word of the dwButtonState member contains a positive value... ARGH!
						if (input.Event.MouseEvent.dwButtonState & 0x80000000) {
							ciomouse_gotevent(CIOLIB_BUTTON_5_PRESS,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1, -1, -1);
						}
						else {
							ciomouse_gotevent(CIOLIB_BUTTON_4_PRESS,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1, -1, -1);
						}
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

#ifndef CONSOLE_FULLSCREEN_MODE
/* SetConsoleDisplayMode parameter value */
#define CONSOLE_FULLSCREEN_MODE	1		// Text is displayed in full-screen mode.
#define CONSOLE_WINDOWED_MODE	2		// Text is displayed in a console window.
#endif

static DWORD	orig_display_mode=0;

/*-----------------------------------------------------------------------------
NT_SetConsoleDisplayMode - Set the console display to fullscreen or windowed.

Parameters:
    hOutputHandle - Output handle of cosole, usually 
                        "GetStdHandle(STD_OUTPUT_HANDLE)"
    dwNewMode - 0=windowed, 1=fullscreen

Returns Values: 
    TRUE if successful, otherwise FALSE is returned. Call GetLastError() for 
    extened information.

Remarks:
    This only works on NT based versions of Windows.

    If dwNewMode is anything other than 0 or 1, FALSE is returned and 
    GetLastError() returns ERROR_INVALID_PARAMETER.
    
    If dwNewMode specfies the current mode, FALSE is returned and 
    GetLastError() returns ERROR_INVALID_PARAMETER. Use the (documented) 
    function GetConsoleDisplayMode() to determine the current display mode.
-----------------------------------------------------------------------------*/
BOOL NT_SetConsoleDisplayMode(HANDLE hOutputHandle, DWORD dwNewMode)
{
    typedef BOOL (WINAPI *SCDMProc_t) (HANDLE, DWORD, LPDWORD);
    SCDMProc_t SetConsoleDisplayMode;
    HMODULE hKernel32;
    BOOL	ret;
    const char KERNEL32_NAME[] = "kernel32.dll";

    hKernel32 = LoadLibrary(KERNEL32_NAME);
    if (hKernel32 == NULL)
        return FALSE;

    SetConsoleDisplayMode = 
        (SCDMProc_t)GetProcAddress(hKernel32, "SetConsoleDisplayMode");
    if (SetConsoleDisplayMode == NULL)
    {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        ret = FALSE;
    }
    else
    {
        DWORD dummy=0;
        ret = SetConsoleDisplayMode(hOutputHandle, dwNewMode, &dummy);
		dprintf("SetConsoleDisplayMode(%d) returned %d", dwNewMode, ret);
    }
        
    FreeLibrary(hKernel32);

	return ret;
}

BOOL NT_GetConsoleDisplayMode(DWORD* mode)
{
    typedef BOOL (WINAPI *GCDMProc_t) (LPDWORD);
    GCDMProc_t GetConsoleDisplayMode;
    HMODULE hKernel32;
    BOOL	ret;
    const char KERNEL32_NAME[] = "kernel32.dll";

    hKernel32 = LoadLibrary(KERNEL32_NAME);
    if (hKernel32 == NULL)
        return FALSE;

    GetConsoleDisplayMode = 
        (GCDMProc_t)GetProcAddress(hKernel32, "GetConsoleDisplayMode");
    if (GetConsoleDisplayMode == NULL)
    {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        ret = FALSE;
    }
    else
    {
        ret = GetConsoleDisplayMode(mode);
		dprintf("GetConsoleDisplayMode() returned %d (%d)", ret, *mode);
    }
        
    FreeLibrary(hKernel32);

	return ret;
}


void RestoreDisplayMode(void)
{
	if(orig_display_mode==0)
		NT_SetConsoleDisplayMode(GetStdHandle(STD_OUTPUT_HANDLE),CONSOLE_WINDOWED_MODE);
}

void win32_suspend(void)
{
	HANDLE h;

	if((h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleMode(h, orig_out_conmode);
	if((h=GetStdHandle(STD_INPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleMode(h, orig_in_conmode);
}

void win32_resume(void)
{
	DWORD	conmode;
	HANDLE	h;

	conmode=ENABLE_MOUSE_INPUT|ENABLE_EXTENDED_FLAGS;
	if((h=GetStdHandle(STD_INPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleMode(h, conmode);

    conmode=orig_out_conmode;
    conmode&=~ENABLE_PROCESSED_OUTPUT;
    conmode&=~ENABLE_WRAP_AT_EOL_OUTPUT;
	if((h=GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE)
		SetConsoleMode(h, conmode);
}

static BOOL WINAPI ControlHandler(unsigned long CtrlType)
{
	return TRUE;
}

int win32_initciolib(long inmode)
{
	DWORD	conmode;
	HANDLE	h;
	CONSOLE_SCREEN_BUFFER_INFO	sbuff;

	dprintf("win32_initciolib(%u)", inmode);
	if(!isatty(fileno(stdin))) {
		if(!AllocConsole())
			return(0);
	}

	SetConsoleCtrlHandler(ControlHandler,TRUE);
	if((h=GetStdHandle(STD_INPUT_HANDLE))==INVALID_HANDLE_VALUE
		|| !GetConsoleMode(h, &orig_in_conmode))
		return(0);
	conmode=ENABLE_MOUSE_INPUT|ENABLE_EXTENDED_FLAGS;
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
		win32_textmode(C80);	// TODO: This likely won't work...
	}
	else {
		/* Switch to closest mode to current screen size */
		unsigned screenwidth = sbuff.srWindow.Right - sbuff.srWindow.Left + 1;
		unsigned screenheight = sbuff.srWindow.Bottom - sbuff.srWindow.Top + 1;
		if (screenwidth > 0xff)
			cio_textinfo.screenwidth = 0xff;
		else
			cio_textinfo.screenwidth = screenwidth;
		if (screenheight > 0xff)
			cio_textinfo.screenheight = 0xff;
		else
			cio_textinfo.screenheight = screenheight;

		if(cio_textinfo.screenwidth>=132) {
			if(cio_textinfo.screenheight<25)
				win32_textmode(VESA_132X21);
			else if(cio_textinfo.screenheight<28)
				win32_textmode(VESA_132X25);
			else if(cio_textinfo.screenheight<30)
				win32_textmode(VESA_132X28);
			else if(cio_textinfo.screenheight<34)
				win32_textmode(VESA_132X30);
			else if(cio_textinfo.screenheight<43)
				win32_textmode(VESA_132X34);
			else if(cio_textinfo.screenheight<50)
				win32_textmode(VESA_132X43);
			else if(cio_textinfo.screenheight<60)
				win32_textmode(VESA_132X50);
			else
				win32_textmode(VESA_132X60);
		}
		else if(cio_textinfo.screenwidth>=80) {
			if(cio_textinfo.screenheight<21)
				win32_textmode(C80X14);
			else if(cio_textinfo.screenheight<25)
				win32_textmode(C80X21);
			else if(cio_textinfo.screenheight<28)
				win32_textmode(C80);
			else if(cio_textinfo.screenheight<43)
				win32_textmode(C80X28);
			else if(cio_textinfo.screenheight<50)
				win32_textmode(C80X43);
			else if(cio_textinfo.screenheight<60)
				win32_textmode(C80X50);
			else
				win32_textmode(C80X60);
		}
		else {
			if(cio_textinfo.screenheight<21)
				win32_textmode(C40X14);
			else if(cio_textinfo.screenheight<25)
				win32_textmode(C40X21);
			else if(cio_textinfo.screenheight<28)
				win32_textmode(C40);
			else if(cio_textinfo.screenheight<43)
				win32_textmode(C40X28);
			else if(cio_textinfo.screenheight<50)
				win32_textmode(C40X43);
			else if(cio_textinfo.screenheight<60)
				win32_textmode(C40X50);
			else
				win32_textmode(C40X60);
		}
	}

	NT_GetConsoleDisplayMode(&orig_display_mode);
	if(inmode==CIOLIB_MODE_CONIO_FULLSCREEN) {
		NT_SetConsoleDisplayMode(h,CONSOLE_FULLSCREEN_MODE);
		atexit(RestoreDisplayMode);
	}
	cio_api.mouse=1;
	cio_api.options = CONIO_OPT_BRIGHT_BACKGROUND | CONIO_OPT_CUSTOM_CURSOR | CONIO_OPT_SET_TITLE;
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
#if 0
	CONSOLE_SCREEN_BUFFER_INFOEX	bi;
#endif

	for(i=0;i<NUMMODES;i++) {
		if(vparams[i].mode==mode)
			modeidx=i;
	}
	sz.X = cio_textinfo.screenwidth > vparams[modeidx].cols ? cio_textinfo.screenwidth : vparams[modeidx].cols;
	sz.Y = cio_textinfo.screenheight > vparams[modeidx].rows ? cio_textinfo.screenheight : vparams[modeidx].rows;
	rc.Left=0;
	rc.Right=vparams[modeidx].cols-1;
	rc.Top=0;
	rc.Bottom=vparams[modeidx].rows-1;

	if ((h=GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
		return;
	if (!SetConsoleScreenBufferSize(h,sz))
		return;	// Note: This fails and returns here with large windows (e.g. width > 255)
	if (!SetConsoleWindowInfo(h,TRUE,&rc))
		return;
	sz.X=vparams[modeidx].cols;
	sz.Y=vparams[modeidx].rows;
	if (!SetConsoleScreenBufferSize(h,sz))
		return;

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
#if 0
	if (GetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE), &bi)) {
		for (i = 0; i < 16; i++) {
			bi.ColorTable[] = RGB(dac_default[palettes[vparams[modeidx]][i]].red, dac_default[palettes[vparams[modeidx].palette][i]].green, dac_default[palettes[vparams[modeidx]][i]].blue);
		}
		if (SetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE), &bi)) {
			cio_api.options |= CONIO_OPT_PALETTE_SETTING;
		}
	}
#endif
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
	LPWSTR	clip;
	int new_buflen = MultiByteToWideChar(CP_UTF8, 0, text, buflen, NULL, 0);

	new_buflen = MultiByteToWideChar(CP_UTF8, 0, text, buflen, NULL, 0);
	if (new_buflen == 0) {
		return;
	}
	clipbuf=GlobalAlloc(GMEM_MOVEABLE, new_buflen * sizeof(WCHAR));
	if (clipbuf == NULL) {
		return;
	}
	clip=GlobalLock(clipbuf);
	if (MultiByteToWideChar(CP_UTF8, 0, text, buflen, clip, new_buflen) != new_buflen) {
		GlobalUnlock(clipbuf);
		GlobalFree(clipbuf);
		return;
	}
	GlobalUnlock(clipbuf);
	if(!OpenClipboard(NULL)) {
		GlobalFree(clipbuf);
		return;
	}
	EmptyClipboard();
	if (SetClipboardData(CF_UNICODETEXT, clipbuf) == NULL) {
		GlobalFree(clipbuf);
	}
	CloseClipboard();
}

char *win32_getcliptext(void)
{
	HGLOBAL	clipbuf;
	LPWSTR	clip;
	char *ret = NULL;
	int u8sz;

	if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
		return(NULL);
	if (!OpenClipboard(NULL))
		return(NULL);
	clipbuf = GetClipboardData(CF_UNICODETEXT);
	if (clipbuf != NULL) {
		clip = GlobalLock(clipbuf);
		if (clip != NULL) {
			u8sz = WideCharToMultiByte(CP_UTF8, 0, clip, -1, NULL, 0, NULL, NULL);
			if (u8sz > 0) {
				ret = (char *)malloc(u8sz);
				if(ret != NULL) {
					if (WideCharToMultiByte(CP_UTF8, 0, clip, -1, ret, u8sz, NULL, NULL) == 0)
						FREE_AND_NULL(ret);
				}
			}
			GlobalUnlock(clipbuf);
		}
	}
	CloseClipboard();

	return(ret);
}

void win32_getcustomcursor(int *s, int *e, int *r, int *b, int *v)
{
	CONSOLE_CURSOR_INFO	ci;
	HANDLE				h;

	if((h=GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE)
		return;

	GetConsoleCursorInfo(h, &ci);
	if(s)
		*s=100-ci.dwSize;
	if(e)
		*e=99;
	if(r)
		*r=100;
	if(b)
		*b=1;
	if(v)
		*v=ci.bVisible?1:0;
}

void win32_setcustomcursor(int s, int e, int r, int b, int v)
{
	CONSOLE_CURSOR_INFO	ci;
	HANDLE				h;

	if((h=GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE)
		return;

	ci.bVisible=v;
	if(e>s)
		ci.bVisible=0;
	else {
		if(r>0)
			ci.dwSize=(1+e-s)/r;
		else
			ci.dwSize=100;
	}
}

int win32_getvideoflags(void)
{
	DWORD	mode;

	if(!NT_GetConsoleDisplayMode(&mode))
		return(CIOLIB_VIDEO_BGBRIGHT);
	if(mode==CONSOLE_FULLSCREEN_MODE)
		return(0);
	return(CIOLIB_VIDEO_BGBRIGHT);
}

int win32_setpalette(uint32_t entry, uint16_t r, uint16_t g, uint16_t b)
{
#if 0
	CONSOLE_SCREEN_BUFFER_INFOEX	bi;

	if (entry > 15)
		return 0;

	if (!GetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE), &bi))
		return 0;

	bi.ColorTable[entry] = RGB(r >> 8, g >> 8, b >> 8);
	if (!SetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE), &bi))
		return 0;
#endif

	return 1;
}
