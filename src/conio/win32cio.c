#include <windows.h>	/* INPUT_RECORD, etc. */
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

int win32_kbhit(void)
{
	INPUT_RECORD input;
	DWORD num=0;

	if(lastch)
		return(1);
	while(1) {
		if(mouse_pending())
			return(1);
		if(!PeekConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &input, 1, &num)
			|| !num)
			break;
		if(input.EventType==KEY_EVENT && input.Event.KeyEvent.bKeyDown)
			return(1);
		if(domouse) {
			if(input.EventType==MOUSE_EVENT) {
				if(input.Event.MouseEvent.dwEventFlags==MOUSE_MOVED) {
					ciomouse_gotevent(CIOLIB_MOUSE_MOVE,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1);
				}
				if(!input.Event.MouseEvent.dwEventFlags) {
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

				if(input.Event.KeyEvent.uChar.AsciiChar)
					lastch=input.Event.KeyEvent.uChar.AsciiChar;
				else
					lastch=input.Event.KeyEvent.wVirtualScanCode<<8;
				break;
			case MOUSE_EVENT:
				if(domouse) {
					if(input.EventType==MOUSE_EVENT) {
						if(input.Event.MouseEvent.dwEventFlags==MOUSE_MOVED) {
							ciomouse_gotevent(CIOLIB_MOUSE_MOVE,input.Event.MouseEvent.dwMousePosition.X+1,input.Event.MouseEvent.dwMousePosition.Y+1);
						}
						if(!input.Event.MouseEvent.dwEventFlags) {
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
}

int win32_getche(void)
{
	int ch;

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
	win32_textmode(inmode);
	if(!GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &conmode))
		return(0);
	conmode&=~ENABLE_PROCESSED_INPUT;
	conmode|=ENABLE_MOUSE_INPUT;
	if(!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), conmode))
		return(0);
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
	int i;
	COORD	sz;
	SMALL_RECT	rc;
	CONSOLE_SCREEN_BUFFER_INFO	sb;

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
	WORD sch;
	int i;
	unsigned char buf[2];
	DWORD wr;

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
