/*

The Clans BBS Door Game
Copyright (C) 1997-2002 Allen Ussher

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
 * Video ADT -- x86 Architecture-specific
 *
 */


#include <stdio.h>
#include <stdlib.h>

#ifndef __unix__
#include <conio.h>
#endif

#ifdef _WIN32
#include "cmdline.h" // defines display_win32_error ()
#endif

#include <ctype.h>
#include <string.h>

#include "defines.h"
#include "door.h"
#include "video.h"

#define COLOR   0xB800
#define MONO    0xB000

#define CURS_NORMAL     0
#define CURS_FAT        1
#define PADDING         '°'
#define SPECIAL_CODE    '%'

char o_fg4 = 7, o_bg4 = 0;

#ifdef _WIN32
static int default_cursor_size = 1;
#endif

// ------------------------------------------------------------------------- //

struct {
	long VideoType;
	long y_lookup[25];
	char FAR *VideoMem;
} Video;

// ------------------------------------------------------------------------- //
long Video_VideoType(void)
{
	return Video.VideoType;
}

// ------------------------------------------------------------------------- //

char FAR *vid_address(void)
{
// As usual, block off the local stuff
#if !defined(__unix__) && !defined(_WIN32)
	_INT16 tmp1, tmp2;

	asm {
		mov bx,0040h
		mov es,bx
		mov dx,es:63h
		add dx,6
		mov tmp1, dx           // move this into status port
	};
	Video.VideoType = COLOR;

	asm {
		mov bx,es:10h
		and bx,30h
		cmp bx,30h
		jne FC1
	};
	Video.VideoType = MONO;

FC1:

	if (Video.VideoType == MONO)
		return ((void FAR *) 0xB0000000L) ;
	else
		return ((void FAR *) 0xB8000000L) ;

#else
	Video.VideoType = COLOR; /* Assume Color on Win32 & UNIX */
	return NULL;
#endif
}

void ScrollUp(void)
{
// As usual, block off the local stuff
#if !defined(__unix__) && !defined(_WIN32)
	asm {
		mov al,1    // number of lines
		mov ch,0    // starting row
		mov cl,0    // starting column
		mov dh, 24  // ending row
		mov dl, 79  // ending column
		mov bh, 7   // color
		mov ah, 6
		int 10h     // do the scroll
	}
#elif defined(_WIN32)
	HANDLE handle_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	SMALL_RECT scroll_rect;
	COORD top_left = { 0, 0 };
	CHAR_INFO char_info;

	GetConsoleScreenBufferInfo(handle_stdout, &screen_buffer);
	scroll_rect.Top = 1;
	scroll_rect.Left = 0;
	scroll_rect.Bottom = screen_buffer.dwSize.Y;
	scroll_rect.Right = screen_buffer.dwSize.X;

	char_info.Attributes = screen_buffer.wAttributes;
	char_info.Char.UnicodeChar = (TCHAR)' ';

	if (!ScrollConsoleScreenBuffer(handle_stdout,
								   &scroll_rect, NULL, top_left, &char_info)) {
		display_win32_error();
		exit(0);
	}
#else
//# pragma message("ScrollUp() Nullified by no defines")
#endif
}



// ------------------------------------------------------------------------- //
void put_character(char ch, short attrib, int x, int y)
{
#ifdef __MSDOS__
	Video.VideoMem[(long)(Video.y_lookup[(long) y]+ (long)(x<<1))] = ch;
	Video.VideoMem[(long)(Video.y_lookup[(long) y]+ (long)(x<<1) + 1L)] = (char)(attrib & 0xff);
#elif defined(_WIN32)
	COORD cursor_pos;
	DWORD bytes_written;
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(
		stdout_handle, cursor_pos);
	SetConsoleTextAttribute(
		stdout_handle,
		(WORD)attrib);
	WriteConsole(
		stdout_handle,
		&ch,
		1,
		&bytes_written,
		NULL);
#endif

}

void zputs(char *string)
{
#ifdef __unix__
	int i;
	int len;

	len = strlen(string);
	if (!Door_Initialized())  {
		for (i=0; i<len; i++)  {
			if (string[i] == '|' && ((i + 1) < len && (i + 2) < len) && isdigit(string[i+1]) && isdigit(string[i+2]))  {
				i+=2;
			}
			else  {
				putchar(string[i]);
			}
		}
	}
#else
	char number[3];
	_INT16 cur_char, attr;
	char foreground, background, cur_attr;
	_INT16 i, j,  x,y;
#ifndef _WIN32
	struct text_info TextInfo;
#else
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	int bytes_written;
	COORD cursor_pos;
	/*  TCHAR space_char = (TCHAR)' ';*/
#endif
	static char o_fg = 7, o_bg = 0;
	int scr_lines = 25, scr_width = 80;

#ifndef _WIN32
	gettextinfo(&TextInfo);
	x = TextInfo.curx-1;
	y = TextInfo.cury-1;
#else
	GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&screen_buffer);
	x = screen_buffer.dwCursorPosition.X;
	y = screen_buffer.dwCursorPosition.Y;
	scr_lines = screen_buffer.dwSize.Y;
	scr_width = screen_buffer.dwSize.X;
#endif

	cur_attr = o_fg | o_bg;
	cur_char = 0;

	for (;;) {
		if (y == scr_lines) {
			/* scroll line up */
			ScrollUp();
			y = scr_lines - 1;
		}
		if (string[cur_char] == 0)
			break;
		if (string[cur_char] == '\b') {
			x--;
			cur_char++;
			continue;
		}
		if (string[cur_char] == '\r') {
			x = 0;
			cur_char++;
			continue;
		}
		if (x == scr_width) {
			x = 0;
			y++;

			if (y == scr_lines) {
				/* scroll line up */
				ScrollUp();
				y = scr_lines - 1;
			}
			break;
		}
		if (string[cur_char]=='|') {
			if (isdigit(string[cur_char+1]) && isdigit(string[cur_char+2])) {
				number[0]=string[cur_char+1];
				number[1]=string[cur_char+2];
				number[2]=0;

				attr=atoi(number);
				if (attr>15) {
					background=attr-16;
					o_bg = background << 4;
					attr = o_bg | o_fg;
					cur_attr = attr;
				}
				else {
					foreground=attr;
					o_fg=foreground;
					attr = o_fg | o_bg;
					cur_attr = attr;
				}
				cur_char += 3;
			}
			else {
				put_character(string[cur_char], cur_attr, x, y);

				cur_char++;
			}
		}
		else if (string[cur_char] == '\n')  {
			y++;
			if (y == scr_lines) {
				/* scroll line up */
				ScrollUp();
				y = scr_lines - 1;
			}
			cur_char++;
			x = 0;
		}
		else if (string[cur_char] == 9) {
			/* tab */
			cur_char++;
			for (i=0; i<8; i++) {
				j = i + x;
				if (j > scr_width) break;
				put_character(' ', cur_attr, j, y);
			}
			x += 8;
		}
		else {
			put_character(string[cur_char], cur_attr, x, y);

			cur_char++;
			x++;
		}
	}
	gotoxy(x+1,y+1);
#endif
}

void SetCurs(_INT16 CursType)
{
// As usual, block off the local stuff
#if !defined(__unix__) && !defined(_WIN32)
	if (CursType == CURS_NORMAL) {
		asm mov ah,01h
		asm mov ch,11
		asm mov cl,12
		asm int 10h
	}
	else if (CursType == CURS_FAT) {
		asm mov ah,01h
		asm mov ch,8
		asm mov cl,13
		asm int 10h
	}
#elif defined(_WIN32)
	CONSOLE_CURSOR_INFO cursor_info;
	cursor_info.bVisible = TRUE;
	switch (CursType) {
		case CURS_NORMAL:
			cursor_info.dwSize = default_cursor_size;
			break;
		case CURS_FAT:
			cursor_info.dwSize = 75;
			break;
		default:
			cursor_info.dwSize = default_cursor_size;
	}
	SetConsoleCursorInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&cursor_info);
#endif
}

void qputs(char *string, _INT16 x, _INT16 y)
{
// As usual, block off the local stuff
#if defined(_WIN32) && !defined(__unix__)
	gotoxy(x+1, y+1);
	zputs(string);
#elif defined(__unix__)
#else
	char number[3];
	_INT16 cur_char, attr;
	char foreground, background, cur_attr;
	static o_fg = 7, o_bg = 0;
	_INT16 i, j;

	cur_attr = o_fg | o_bg;
	cur_char=0;

	while (1) {
		if (string[cur_char] == 0)
			break;
		if (x == 80)
			break;
		if (string[cur_char]=='|')    {
			if (isdigit(string[cur_char+1]))  {
				number[0]=string[cur_char+1];
				number[1]=string[cur_char+2];
				number[2]=0;

				attr=atoi(number);
				if (attr>15)  {
					background=attr-16;
					o_bg = background << 4;
					attr = o_bg | o_fg;
					cur_attr = attr;
				}
				else  {
					foreground=attr;
					o_fg=foreground;
					attr = o_fg | o_bg;
					cur_attr = attr;
				}
				cur_char += 3;
			}
			else  {
				Video.VideoMem[(long)(Video.y_lookup[(long) y]+ (long)(x<<1))] = string[cur_char];
				Video.VideoMem[(long)(Video.y_lookup[(long) y]+ (long)(x<<1) + 1L)] = cur_attr;

				cur_char++;
				x++;
			}
		}
		else if (string[cur_char] == '\n')  {
			y++;
			cur_char++;
			x = 0;
		}
		else if (string[cur_char] == 9)  {  // tab
			cur_char++;
			for (i=0; i<8; i++) {
				j = i+x;
				if (j > 80) break;
				Video.VideoMem[ Video.y_lookup[y]+(j<<1)] = ' ';
				Video.VideoMem[ Video.y_lookup[y]+(j<<1) + 1] = cur_attr;
			}
			x += 8;
		}
		else  {
			Video.VideoMem[ Video.y_lookup[y]+(x<<1)] = string[cur_char];
			Video.VideoMem[ Video.y_lookup[y]+(x<<1) + 1] = cur_attr;

			cur_char++;
			x++;
		}
	}
#endif
}


void sdisplay(char *string, _INT16 x, _INT16 y, char color, _INT16 length, _INT16 input_length)
{
#ifdef __unix__
	fprintf(stderr,"%s\r\n",string);
#elif defined(_WIN32)
	COORD cursor_pos, current_cursor_pos;
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	WORD current_attr, i;
	DWORD bytes_written;
	TCHAR padding_char = (TCHAR)PADDING;

	/* Save Current Color Attribute */
	GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&screen_buffer);

	current_attr = screen_buffer.wAttributes;

	/* Save Current Cursor Position */
	current_cursor_pos.X = screen_buffer.dwCursorPosition.X;
	current_cursor_pos.Y = screen_buffer.dwCursorPosition.Y;

	/* Set New Cursor Position (based on x, y) */
	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(
		GetStdHandle(STD_OUTPUT_HANDLE),
		cursor_pos);

	/* Set new color */
	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE),
		(WORD) color);

	/* Output Text */
	WriteConsole(
		GetStdHandle(STD_OUTPUT_HANDLE),
		string,
		strlen(string),
		&bytes_written,
		NULL);

	/* Set color for padding */
	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE),
		(WORD) 8);

	/* Display Padding */
	for (i = length; i < input_length; i++) {
		WriteConsole(
			GetStdHandle(STD_OUTPUT_HANDLE),
			&padding_char,
			1,
			&bytes_written,
			NULL);
	}

	/* Reset color */
	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE),
		current_attr);
#else
	_INT16 i, offset;
	char *pString;

	pString = string;

	offset = y*160 + x*2;

	while (*pString) {
		Video.VideoMem[ offset ] = *pString;
		Video.VideoMem[ offset+1 ] = color;
		offset+=2;
		pString++;
	}
	for (i=length; i<input_length; i++) {
		Video.VideoMem[ offset ] = PADDING;
		Video.VideoMem[ offset+1 ] = 8;
		offset+=2;
	}
#endif
}


_INT16 LongInput(char *string, _INT16 x, _INT16 y, _INT16 input_length, char attr,
				 _INT16 low_char, _INT16 high_char)
{
// As usual, block off the local stuff
#if !defined(__unix__)
	_INT16 length = strlen(string);  // length of string
	_INT16 i,
	insert = FALSE,
			 key,        // key inputted
			 cur_letter = length; // letter we're editing right now
	char tmp_str[255];
	char first_time = TRUE; // flags if this is the first time to input
	// if TRUE, it will clear the line and input
	char old_fg4, old_bg4;

	// initialize the string here

	SetCurs(insert);
#ifdef __MSDOS__
	textattr(attr);
#else
	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE),
		(WORD)attr);
#endif

	o_fg4 = attr&0x00FF;
	o_bg4 = attr&0xFF00;
	qputs(string, x, y);
	gotoxy(x+length+1, y+1);

	old_fg4 = o_fg4;
	o_fg4 = 8;

	for (i = length; i < input_length; i++)
		putch(PADDING);

	o_fg4 = old_fg4;

	// now edit it

	for (;;) {
		sdisplay(string, x, y, attr, length, input_length);

		gotoxy(x+ 1 + cur_letter, y+1);

		key = getch();

		switch ((unsigned char)key) {
			case 0xE0 :
			case 0 :
				switch (getch()) {
					case 80 : // down
					case 72 : // up
						break;
					case 79 : // end
						cur_letter = length;
						break;
					case 77 :   // right
						if (cur_letter < length)
							cur_letter++;
						break;
					case 75 :   // left
						if (cur_letter)
							cur_letter--;
						break;
					case 71 : // home
						cur_letter = 0;
						break;
					case 82 : // insert
						insert = !insert;
						SetCurs(insert);
						// change cursor here
						break;
					case 83 :   // delete
						if (length && cur_letter < length) {
							i = cur_letter;
							while (i<length)  {
								string[i] = string[i+1];
								i++;
							}
							string[length] = 0;

							length--;
						}
						break;
				}
				break;

			case '\t' :
			case '\r' :     // enter
				return 1;
			case 25 : // ctrl-Y
				string[0] = 0;
				length = 0;
				cur_letter = 0;
				break;
			case '\b' : // backspace
				if (cur_letter > 0)   {
					i = cur_letter-1;

					while (i<length) {
						string[i] = string[i+1];
						i++;
					}
					string[length] = 0;

					length--;
					cur_letter--;
				}
				break;
			case 0x1B : // esc
				break;
			default :
				if (first_time) { // clear line!
					string[0] = 0;
					length = 0;
					cur_letter = 0;
				}
				if ((char)key < low_char || (char)key > high_char)  break;
				if (insert && length < input_length)  {

					strcpy(tmp_str, &string[cur_letter]);
					strcpy(&string[cur_letter+1], tmp_str);

					string[cur_letter] = key;

					length++;

					if (cur_letter<input_length)
						cur_letter++;
				}
				else if (!insert && cur_letter<input_length) {

					string[ cur_letter ] = key;

					if (cur_letter < length)
						cur_letter++;
					else if (cur_letter == length)  {
						string[cur_letter+1]=0;
						cur_letter++;
						length++;
					}
				}
				break;
		} // end of while loop
		first_time = FALSE;

	}
#else
	return 0;
#endif
}

void Input(char *string, _INT16 length)
{
// As usual, block off the local stuff
#if !defined(__unix__)
	_INT16 cur_x, cur_y;

#ifdef __MSDOS__
	cur_x = wherex()-1;
	cur_y = wherey()-1;
#else
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&screen_buffer);
	cur_x = screen_buffer.dwCursorPosition.X;
	cur_y = screen_buffer.dwCursorPosition.Y;
#endif

	LongInput(string, cur_x, cur_y, length, 15, 2, 255);
	zputs("\n");

	SetCurs(CURS_NORMAL);
#endif
}


void ClearArea(char x1, char y1,  char x2, char y2, char attr)
{
#if !defined(__unix__) && !defined(_WIN32)
	asm {
		push ax
		push bx
		push cx
		push dx

		mov al, 0
		mov ch, y1
		mov cl, x1
		mov dh, y2
		mov dl, x2
		mov bh, attr
		mov ah, 7
		int 10h

		pop dx
		pop cx
		pop bx
		pop ax
	}
#endif
}

void xputs(char *string, _INT16 x, _INT16 y)
{
// As usual, block off the local stuff
#if !defined(__unix__) && !defined(_WIN32)
	char FAR *VidPtr;

	VidPtr = Video.VideoMem + Video.y_lookup[y] + (x<<1);

	while (x < 80 && *string) {
		*VidPtr = *string;

		VidPtr += 2;
		x++;
		string++;
	}
#elif defined(_WIN32)
	COORD cursor_pos;
	DWORD bytes_written;

	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(
		GetStdHandle(STD_OUTPUT_HANDLE),
		cursor_pos);

	WriteConsole(
		GetStdHandle(STD_OUTPUT_HANDLE),
		string,
		strlen(string),
		&bytes_written,
		NULL);
#endif
}

void DisplayStr(char *szString)
{
// As usual, block off the local stuff
#ifdef __unix__
	if (Door_Initialized())
		rputs(szString);
	else
		zputs(szString);
#else
	if (Door_Initialized())
		rputs(szString);
	else
		zputs(szString);
#endif
}

// ------------------------------------------------------------------------- //

void Video_Init(void)
{
// As usual, block off the local stuff
#if defined(__unix__) && !defined(_WIN32)
#elif defined(_WIN32)
	CONSOLE_CURSOR_INFO cursor_info;

	if (!AllocConsole()) {
		display_win32_error();
		exit(0);
	}

	if (!CreateConsoleScreenBuffer(
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				CONSOLE_TEXTMODE_BUFFER,
				NULL)) {
		display_win32_error();
		exit(0);
	}

	if (!GetConsoleCursorInfo(
				GetStdHandle(STD_OUTPUT_HANDLE),
				&cursor_info)) {
		display_win32_error();
		exit(0);
	}
	default_cursor_size = cursor_info.dwSize;
#else
	_INT16 iTemp;

	Video.VideoMem = vid_address();
	for (iTemp = 0; iTemp < 25;  iTemp++)
		Video.y_lookup[ iTemp ] = iTemp * 160;
#endif
}

#ifdef _WIN32
void gotoxy(int x, int y)
{
	COORD cursor_pos;

	cursor_pos.X = x - 1;
	cursor_pos.Y = y - 1;

	SetConsoleCursorPosition(
		GetStdHandle(STD_OUTPUT_HANDLE),
		cursor_pos);
}

void clrscr(void)
{
	HANDLE std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	COORD top_left = { 0, 0 };
	DWORD cells_written;

	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);

	FillConsoleOutputCharacter(
		std_handle,
		(TCHAR)' ',
		screen_buffer.dwSize.X * screen_buffer.dwSize.Y,
		top_left,
		&cells_written);

	FillConsoleOutputAttribute(
		std_handle,
		(WORD)7,
		screen_buffer.dwSize.X * screen_buffer.dwSize.Y,
		top_left,
		&cells_written);

	SetConsoleCursorPosition(
		std_handle,
		top_left);
}
#endif
