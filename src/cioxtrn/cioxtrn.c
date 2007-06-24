#include <Windows.h>
#include <stdio.h>
#include "genwrap.h"
#include "ciolib.h"
#include "keys.h"

HANDLE				console_output;
HANDLE				console_input;
int					terminate=0;
int					input_thread_running=0;
int					output_thread_running=0;
struct text_info ti;

enum key_modifier {
	 MOD_SHIFT
	,MOD_CONTROL
	,MOD_ALT
};
int					shift=0;
int					alt=0;
int					ctrl=0;
int					force_redraw=0;
int					display_top=1;

/* Stolen from win32cio.c */
struct keyvals {
	int	VirtualKeyCode
		,Key
		,Shift
		,CTRL
		,ALT;
};

/* Get this from win32cio.c */
extern const struct keyvals keyval[];

void frobkey(int press, int release, int scan, int key, int ascii)
{
	INPUT_RECORD		ckey;
	DWORD d;
	SHORT s;

	ckey.Event.KeyEvent.bKeyDown=TRUE;
	ckey.Event.KeyEvent.dwControlKeyState=0;
	if(alt)
		ckey.Event.KeyEvent.dwControlKeyState |= LEFT_ALT_PRESSED;
	if(ctrl)
		ckey.Event.KeyEvent.dwControlKeyState |= LEFT_CTRL_PRESSED;
	if(shift)
		ckey.Event.KeyEvent.dwControlKeyState |= LEFT_SHIFT_PRESSED;
	ckey.Event.KeyEvent.wRepeatCount=1;
	ckey.Event.KeyEvent.wVirtualKeyCode=key;	/* ALT key */
	ckey.Event.KeyEvent.wVirtualScanCode=scan;
	ckey.Event.KeyEvent.uChar.AsciiChar=ascii;
	d=0;
	if(press) {
		while(!d) {
			if(!WriteConsoleInput(console_input, &ckey, 1, &d))
				d=0;
		}
	}
	ckey.Event.KeyEvent.bKeyDown=FALSE;
	if(release) {
		while(!d) {
			if(!WriteConsoleInput(console_input, &ckey, 1, &d))
				d=0;
		}
	}
}

void toggle_modifier(enum key_modifier mod)
{
	int *mod;
	WORD key;

	switch(mod) {
		case MOD_SHIFT:
			mod=&shift;
			key=VK_SHIFT;
			break;
		case MOD_CONTROL:
			mod=&ctrl;
			key=VK_CONTROL;
			break;
		case MOD_ALT:
			mod=&alt;
			key=VK_MENU;
			break;
	}
	frobkey(!(*mod), (*mod), MapVirtualKey(key, 0), key, 0);
	*mod=!(*mod);
}

void input_thread(void *args)
{
	int					key=-1;
	int					lastkey;
	INPUT_RECORD		ckey;
	int i;
	DWORD d;
	SHORT s;

	input_thread_running=1;
	/* Request DoorWay mode */
	while(!terminate) {
		if(kbhit()) {
			lastkey=key;
			key=getch();
			if(key==0 || key == 0xff)
				key|=getch()<<8;
			if(key==1) {
				toggle_modifier(MOD_ALT);
				if(alt)
					continue;
			}
			if(key==18) {	/* CTRL-R */
				if(lastkey != 18) {
					force_redraw=1;
					continue;
				}
			}
			if(key==26) {	/* CTRL-Z */
				force_redraw=1;
				display_top=!display_top;
				if(lastkey != 26)
					continue;
			}
			if(key < 256) {
				s=VkKeyScan(key);

				/* No translation */
				if(s==-1)
					continue;

				/* Make the mod states match up... */
				/* Wish I had a ^^ operator */
				if((s & 0x0100 == 0x0100) != (shift != 0))
					toggle_modifier(MOD_SHIFT);
				if((s & 0x0200 == 0x0200) != (ctrl != 0))
					toggle_modifier(MOD_CONTROL);
				/* ALT is handled via CTRL-A, not here */
				/* if((s & 0x0400 == 0x0400) != (alt != 0))
					toggle_modifier(MOD_ALT); */

				frobkey(TRUE, TRUE, MapVirtualKey(s & 0xff, 0), s&0xff, key);
			}
			else {
				/* Check if CTRL or ALT are "pressed" */
				for(i=0;keyval[i].Key;i++) {
					if(keyval[i].Key==key)
						break;
					if(keyval[i].Shift==key)
						/* Release the CTRL key */
						if(ctrl)
							toggle_modifier(MOD_CONTROL);
						/* ALT is handled via CTRL-A, not here */
						/* if((s & 0x0400 == 0x0400) != (alt != 0))
							toggle_modifier(MOD_ALT); */
						/* Press the shift key */
						if(!shift)
							toggle_modifier(MOD_SHIFT);
						break;
					if(keyval[i].CTRL==key) {
						/* Release the shift key */
						if(shift)
							toggle_modifier(MOD_SHIFT);
						/* ALT is handled via CTRL-A, not here */
						/* if((s & 0x0400 == 0x0400) != (alt != 0))
							toggle_modifier(MOD_ALT); */
						/* Press the CTRL key */
						if(!ctrl)
							toggle_modifier(MOD_CONTROL);
						break;
					}
					if(keyval[i].ALT==key) {
						/* Release the shift key */
						if(shift)
							toggle_modifier(MOD_SHIFT);
						/* Release the CTRL key */
						if(ctrl)
							toggle_modifier(MOD_CONTROL);
						/* Press the ALT key */
						if(!alt)
							toggle_modifier(MOD_ALT);
					}
				}
				frobkey(TRUE, TRUE, MapVirtualKey(key>>8, 1), key>>8, 0);
			}
			if(alt)
				toggle_modifier(MOD_ALT);
			if(ctrl)
				toggle_modifier(MOD_CONTROL);
			if(shift)
				toggle_modifier(MOD_SHIFT);
		}
		else
			SLEEP(1);
	}
	input_thread_running=0;
}

void output_thread(void *args)
{
	SMALL_RECT			screen_area;
	CHAR_INFO			*from_screen;
	unsigned char		*write_buf;
	unsigned char		*current_screen;
	CONSOLE_SCREEN_BUFFER_INFO console_output_info;
	COORD				size;
	COORD				pos;
	int i,j;

	GetConsoleScreenBufferInfo(console_output, &console_output_info);
	pos.X=0;
	pos.Y=0;
	size.X=ti.screenwidth;
	size.Y=ti.screenheight;

	from_screen=(CHAR_INFO *)malloc(sizeof(CHAR_INFO)*ti.screenheight*ti.screenwidth);
	write_buf=(unsigned char *)malloc(2*ti.screenheight*ti.screenwidth);
	current_screen=(unsigned char *)malloc(2*ti.screenheight*ti.screenwidth);

	output_thread_running=1;
	while(!terminate) {
		SLEEP(10);

		/* Read the console screen buffer */
		if(display_top) {
			screen_area.Left=console_output_info.srWindow.Left;
			screen_area.Right=console_output_info.srWindow.Left+ti.screenwidth-1;
			screen_area.Top=console_output_info.srWindow.Top;
			screen_area.Bottom=console_output_info.srWindow.Top+ti.screenheight-1;
		}
		else {
			screen_area.Left=console_output_info.srWindow.Right-ti.screenwidth+1;
			screen_area.Right=console_output_info.srWindow.Right;
			screen_area.Top=console_output_info.srWindow.Bottom-ti.screenheight+1;
			screen_area.Bottom=console_output_info.srWindow.Bottom;
		}
		ReadConsoleOutput(console_output, from_screen, size, pos, &screen_area);

		/* Translate to a ciolib buffer */
		j=0;
		for(i=0; i<ti.screenwidth*ti.screenheight; i++) {
			write_buf[j++]=from_screen[i].Char.AsciiChar;
			write_buf[j++]=from_screen[i].Attributes & 0xff;
		}

		if(force_redraw) {
			clrscr();
			force_redraw=0;
		}

		/* Compare against the current screen */
		gettext(1,1,ti.screenwidth,ti.screenheight,current_screen);
		if(memcmp(current_screen,write_buf,2*ti.screenwidth*ti.screenheight))
			puttext(1,1,ti.screenwidth,ti.screenheight,write_buf);

		/* Update cursor position */
		if(GetConsoleScreenBufferInfo(console_output, &console_output_info))
			gotoxy(console_output_info.dwCursorPosition.X+1,console_output_info.dwCursorPosition.Y+1);
	}
	output_thread_running=0;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE pinst, char *cmd, int cshow)
{
	PROCESS_INFORMATION	process_info;
	STARTUPINFO			startup_info;
	COORD				size;
	SMALL_RECT			screen_area;
	HANDLE				ntvdm;
	DWORD d;
	SECURITY_ATTRIBUTES	sec_attrib;

	puttext_can_move=1;
	initciolib(CIOLIB_MODE_ANSI);
	ansi_ciolib_setdoorway(1);
	gettextinfo(&ti);
	FreeConsole();

	if(!AllocConsole()) {
		ansi_ciolib_setdoorway(0);
		return(1);
	}

	memset(&sec_attrib,0,sizeof(sec_attrib));
	sec_attrib.nLength=sizeof(sec_attrib);
	sec_attrib.bInheritHandle=TRUE;

	console_output=CreateFile("CONOUT$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sec_attrib, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0L);
	if(console_output==INVALID_HANDLE_VALUE) {
		printf("CONOUT Error: %u\r\n",GetLastError());
		ansi_ciolib_setdoorway(0);
		return(1);
	}
	console_input=CreateFile("CONIN$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sec_attrib, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0L);
	if(console_input==INVALID_HANDLE_VALUE) {
		printf("CONIN Error: %u\r\n",GetLastError());
		ansi_ciolib_setdoorway(0);
		return(1);
	}

	size.X=ti.screenwidth;
	size.Y=ti.screenheight;
	screen_area.Left=0;
	screen_area.Right=size.X-1;
	screen_area.Top=0;
	screen_area.Bottom=size.Y-1;

	SetConsoleCP(437);
	SetConsoleOutputCP(437);

	/* Size console to size */
	SetConsoleScreenBufferSize(console_output, size);
	SetConsoleWindowInfo(console_output, TRUE, &screen_area);
	SetConsoleScreenBufferSize(console_output, size);

	memset(&startup_info, 0, sizeof(startup_info));
	startup_info.cb=sizeof(startup_info);
	startup_info.hStdInput=console_input;
	startup_info.hStdOutput=console_output;
	startup_info.hStdError=console_output;
	startup_info.dwFlags = STARTF_USESTDHANDLES;

	if(!CreateProcess(NULL	/* Read name from command line */
			, cmd
			, NULL
			, NULL
			, TRUE	/* Inherit Handles */
			, CREATE_SEPARATE_WOW_VDM
			, NULL
			, NULL
			, &startup_info
			, &process_info
			)) {
		printf("Error: %u\r\n",GetLastError());
		ansi_ciolib_setdoorway(0);
		return(1);
	}
	CloseHandle(process_info.hThread);
	ntvdm=OpenProcess(PROCESS_TERMINATE, FALSE, process_info.dwProcessId);

	/* Handle input */
	_beginthread(input_thread,0,NULL);
	/* Handle output */
	_beginthread(output_thread,0,NULL);

	while(1) {
		if(GetExitCodeProcess(process_info.hProcess, &d)) {
			if(d != STILL_ACTIVE) {
				terminate=1;
				while(input_thread_running || output_thread_running)
					SLEEP(1);
				TerminateProcess(ntvdm,0);
				CloseHandle(ntvdm);
				CloseHandle(console_output);
				CloseHandle(console_input);
				CloseHandle(process_info.hProcess);
				FreeConsole();
				ansi_ciolib_setdoorway(0);
				return(0);
			}
		}
		SLEEP(1);
	}
}
