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

void input_thread(void *args)
{
	int					key;
	INPUT_RECORD		ckey;
	int					alt=0;
	DWORD d;
	SHORT s;

	input_thread_running=1;
	while(!terminate) {
		key=getch();
		if(key==0 || key == 0xff)
			key|=getch()<<8;
		if(key==1) {
			if(alt==0) {
				alt=1;
				ckey.Event.KeyEvent.bKeyDown=TRUE;
				ckey.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED;
			}
			else {
				alt=0;
				ckey.Event.KeyEvent.bKeyDown=FALSE;
				ckey.Event.KeyEvent.dwControlKeyState = 0;
			}
			ckey.Event.KeyEvent.wRepeatCount=1;
			ckey.Event.KeyEvent.wVirtualKeyCode=VK_MENU;	/* ALT key */
			ckey.Event.KeyEvent.wVirtualScanCode=MapVirtualKey(VK_MENU, 0);
			ckey.Event.KeyEvent.uChar.AsciiChar=0;
			d=0;
			while(!d) {
				if(!WriteConsoleInput(console_input, &ckey, 1, &d))
					d=0;
			}
			if(alt)
				continue;
		}

		ckey.Event.KeyEvent.bKeyDown=TRUE;
		ckey.Event.KeyEvent.wRepeatCount=1;
		if(key < 256) {
			s=VkKeyScan(key);

			/* No translation */
			if(s==-1)
				continue;

			ckey.Event.KeyEvent.wVirtualKeyCode = s&0xff;
			ckey.Event.KeyEvent.dwControlKeyState=0;
			if(s&0x0100)
				ckey.Event.KeyEvent.dwControlKeyState |= SHIFT_PRESSED;
			if(s&0x0200)
				ckey.Event.KeyEvent.dwControlKeyState |= LEFT_CTRL_PRESSED;
			if(s&0x0400)
				ckey.Event.KeyEvent.dwControlKeyState |= LEFT_ALT_PRESSED;

			ckey.Event.KeyEvent.wVirtualScanCode=MapVirtualKey(s & 0xff, 0);
			ckey.Event.KeyEvent.uChar.AsciiChar=key;
		}
		else {
			ckey.Event.KeyEvent.dwControlKeyState=0;
			ckey.Event.KeyEvent.uChar.AsciiChar=0;
			switch(key) {
			case CIO_KEY_HOME:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_HOME;
				break;
			case CIO_KEY_UP:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_UP;
				break;
			case CIO_KEY_END:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_END;
				break;
			case CIO_KEY_DOWN:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_DOWN;
				break;
			case CIO_KEY_IC:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_INSERT;
				break;
			case CIO_KEY_DC:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_DELETE;
				break;
			case CIO_KEY_LEFT:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_LEFT;
				break;
			case CIO_KEY_RIGHT:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_RIGHT;
				break;
			case CIO_KEY_PPAGE:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_PRIOR;
				break;
			case CIO_KEY_NPAGE:
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_NEXT;
				break;
			case CIO_KEY_F(1):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F1;
				break;
			case CIO_KEY_F(2):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F2;
				break;
			case CIO_KEY_F(3):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F3;
				break;
			case CIO_KEY_F(4):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F4;
				break;
			case CIO_KEY_F(5):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F5;
				break;
			case CIO_KEY_F(6):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F6;
				break;
			case CIO_KEY_F(7):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F7;
				break;
			case CIO_KEY_F(8):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F8;
				break;
			case CIO_KEY_F(9):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F9;
				break;
			case CIO_KEY_F(10):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F10;
				break;
			case CIO_KEY_F(11):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F11;
				break;
			case CIO_KEY_F(12):
				ckey.Event.KeyEvent.wVirtualKeyCode=VK_F12;
				break;
			default:
				continue;
			}
			ckey.Event.KeyEvent.wRepeatCount=1;
			ckey.Event.KeyEvent.wVirtualScanCode=MapVirtualKey(ckey.Event.KeyEvent.wVirtualKeyCode, 0);
			ckey.Event.KeyEvent.uChar.AsciiChar=0;
			ckey.Event.KeyEvent.dwControlKeyState=0;
		}
		if(alt)
			ckey.Event.KeyEvent.dwControlKeyState |= LEFT_ALT_PRESSED;

		ckey.EventType=KEY_EVENT;
		d=0;
		while(!d) {
			if(!WriteConsoleInput(console_input, &ckey, 1, &d))
				d=0;
		}
		ckey.Event.KeyEvent.bKeyDown=FALSE;
		d=0;
		while(!d) {
			if(!WriteConsoleInput(console_input, &ckey, 1, &d))
				d=0;
		}
		if(alt) {
			ckey.Event.KeyEvent.bKeyDown=FALSE;
			ckey.Event.KeyEvent.wRepeatCount=1;
			ckey.Event.KeyEvent.wVirtualKeyCode=VK_MENU;	/* ALT key */
			ckey.Event.KeyEvent.wVirtualScanCode=MapVirtualKey(VK_MENU, 0);
			ckey.Event.KeyEvent.uChar.AsciiChar=0;
			ckey.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED;
			d=0;
			while(!d) {
				if(!WriteConsoleInput(console_input, &ckey, 1, &d))
					d=0;
			}
			alt=0;
		}
	}
	input_thread_running=0;
}

void output_thread(void *args)
{
	SMALL_RECT			screen_area;
	CHAR_INFO			from_screen[80*24];
	unsigned char		write_buf[80*24*2];
	unsigned char		current_screen[80*24*2];
	CONSOLE_SCREEN_BUFFER_INFO console_output_info;
	COORD				size;
	COORD				pos;
	int i,j;

	pos.X=0;
	pos.Y=0;
	size.X=80;
	size.Y=24;

	output_thread_running=1;
	while(!terminate) {
		SLEEP(10);

		/* Read the console screen buffer */
		screen_area.Left=0;
		screen_area.Right=79;
		screen_area.Top=0;
		screen_area.Bottom=23;
		ReadConsoleOutput(console_output, from_screen, size, pos, &screen_area);

		/* Translate to a ciolib buffer */
		j=0;
		for(i=0; i<sizeof(from_screen)/sizeof(CHAR_INFO); i++) {
			write_buf[j++]=from_screen[i].Char.AsciiChar;
			write_buf[j++]=from_screen[i].Attributes & 0xff;
		}

		/* Compare against the current screen */
		gettext(1,1,80,24,current_screen);
		if(memcmp(current_screen,write_buf,sizeof(write_buf)))
			puttext(1,1,80,24,write_buf);

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

	initciolib(CIOLIB_MODE_ANSI);
	FreeConsole();

	if(!AllocConsole())
		return(1);

	memset(&sec_attrib,0,sizeof(sec_attrib));
	sec_attrib.nLength=sizeof(sec_attrib);
	sec_attrib.bInheritHandle=TRUE;

	console_output=CreateFile("CONOUT$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sec_attrib, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0L);
	if(console_output==INVALID_HANDLE_VALUE) {
		printf("CONOUT Error: %u\r\n",GetLastError());
		return(1);
	}
	console_input=CreateFile("CONIN$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sec_attrib, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0L);
	if(console_input==INVALID_HANDLE_VALUE) {
		printf("CONIN Error: %u\r\n",GetLastError());
		return(1);
	}

	size.X=80;
	size.Y=24;
	screen_area.Left=0;
	screen_area.Right=79;
	screen_area.Top=0;
	screen_area.Bottom=23;

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
				return(0);
			}
		}
		SLEEP(1);
	}
}
