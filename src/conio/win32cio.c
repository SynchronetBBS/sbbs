#include <windows.h>	/* INPUT_RECORD, etc. */
#include <stdio.h>		/* stdin */
#include "conio.h"
#define CIOLIB_NO_MACROS
#include "ciolib.h"
#include "keys.h"

static struct cio_mouse_event	cio_last_button_press;
static struct cio_mouse_event	last_mouse_click;

static int lastch=0;
static int domouse=0;

int win32_kbhit(void)
{
	INPUT_RECORD input;
	DWORD num=0;

	if(lastch)
		return(1);
	while(1) {
		if(!PeekConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &input, 1, &num)
			|| !num)
			break;
		if(input.EventType==KEY_EVENT && input.Event.KeyEvent.bKeyDown)
			return(1);
		if(domouse) {
			if(input.EventType==MOUSE_EVENT) {
				if(!input.Event.MouseEvent.dwEventFlags
					&& (!input.Event.MouseEvent.dwButtonState
						|| input.Event.MouseEvent.dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED
						|| input.Event.MouseEvent.dwButtonState==RIGHTMOST_BUTTON_PRESSED))
					return(1);
			}
		}
		if(ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &input, 1, &num)
			&& num) {
			continue;
		}
	}
	return(0);
}

int win32_getch(void)
{
	char str[128];
	INPUT_RECORD input;
	DWORD num=0;

	while(1) {
		if(lastch) {
			int ch;
			ch=lastch&0xff;
			lastch>>=8;
			return(ch);
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

				if(input.Event.KeyEvent.uChar.AsciiChar)
					lastch=input.Event.KeyEvent.uChar.AsciiChar;
				else
					lastch=input.Event.KeyEvent.wVirtualScanCode<<8;
				break;
			case MOUSE_EVENT:
				if(domouse) {
					if(input.Event.MouseEvent.dwEventFlags!=0)
						continue;
					if(input.Event.MouseEvent.dwButtonState==0) {
						if(cio_last_button_press.button
								&& cio_last_button_press.x==input.Event.MouseEvent.dwMousePosition.X
								&& cio_last_button_press.y==input.Event.MouseEvent.dwMousePosition.Y) {
							memcpy(&last_mouse_click,&cio_last_button_press,sizeof(last_mouse_click));
							memset(&cio_last_button_press,0,sizeof(cio_last_button_press));
							lastch=CIO_KEY_MOUSE;
							break;
						}
						else {
							memset(&cio_last_button_press,0,sizeof(cio_last_button_press));
						}
					}
					else {
						memset(&cio_last_button_press,0,sizeof(cio_last_button_press));
						switch(input.Event.MouseEvent.dwButtonState) {
							case FROM_LEFT_1ST_BUTTON_PRESSED:
								cio_last_button_press.x=input.Event.MouseEvent.dwMousePosition.X;
								cio_last_button_press.y=input.Event.MouseEvent.dwMousePosition.Y;
								cio_last_button_press.button=1;
								break;
							case RIGHTMOST_BUTTON_PRESSED:
								cio_last_button_press.x=input.Event.MouseEvent.dwMousePosition.X;
								cio_last_button_press.y=input.Event.MouseEvent.dwMousePosition.Y;
								cio_last_button_press.button=2;
								break;
						}
					}
				}
		}
	}

	return(0);
}

int win32_getche(void)
{
	int ch;

#if 0	/* what is this? */
	if(ansi_nextchar)
		return(ansi_getch());
#endif
	ch=win32_getch();
	if(ch)
		putch(ch);
	return(ch);
}

int win32_initciolib(long inmode)
{
	DWORD conmode;

	if(!isatty(fileno(stdin)))
		return(0);
	textmode(inmode);
	if(!GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &conmode))
		return(0);
	conmode&=~ENABLE_PROCESSED_INPUT;
	conmode|=ENABLE_MOUSE_INPUT;
	if(!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), conmode))
		return(0);
	cio_api.mouse=1;
	memset(&cio_last_button_press,0,sizeof(cio_last_button_press));
	memset(&last_mouse_click,0,sizeof(last_mouse_click));
	return(1);
}

int win32_getmouse(struct cio_mouse_event *mevent)
{
	memcpy(mevent,&last_mouse_click,sizeof(last_mouse_click));
	return(0);
}

int win32_hidemouse(void)
{
	domouse=0;
	return(0);
}

int win32_showmouse(void)
{
	domouse=1;
	return(0);
}

#if !defined(__BORLANDC__)

void textmode(int mode)
{
}

void clreol(void)
{
}

void clrscr(void)
{
}

void delline(void)
{
}


int gettext(int left, int top, int right, int bottom, void* buf)
{
	return 1;
}

void gettextinfo(struct text_info* info)
{
}


void gotoxy(int x, int y)
{
}


void highvideo(void)
{
}


void insline(void)
{
}


void lowvideo(void)
{
}


int movetext(int left, int top, int right, int bottom, int destleft, int desttop)
{
	return 1;
}


void normvideo(void)
{
}


int puttext(int left, int top, int right, int bottom, void* buf)
{
	return 1;
}


void textattr(int newattr)
{
}


void textbackground(int newcolor)
{
}


void textcolor(int newcolor)
{
}

void window(int left, int top, int right, int bottom)
{
}

void _setcursortype(int type)
{
}

int wherex(void)
{
	return 0;
}

int wherey(void)
{
	return 0;
}

#endif
