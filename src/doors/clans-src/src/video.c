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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __unix__
# include <locale.h>
# include <curses.h>
#else
# include <conio.h>
#endif
#ifdef _WIN32
# include "cmdline.h" // defines display_win32_error ()
#endif

#include "defines.h"
#include "door.h"
#include "video.h"

#define COLOR   0xB800
#define MONO    0xB000

#define CURS_NORMAL     0
#define CURS_FAT        1
#define PADDING         '\xb0'
#define SPECIAL_CODE    '%'

static char o_fg4 = 7, o_bg4 = 0;

#ifdef _WIN32
static int default_cursor_size = 1;
static HANDLE std_handle;
#endif

#ifdef __unix__
static void set_attrs(uint8_t attrib);
static void putch(unsigned char ch);
static short curses_color(short color);
attr_t cur_attr = 0;
short cur_pair = 8;
#endif

// ------------------------------------------------------------------------- //

static struct {
	int32_t VideoType;
	int32_t y_lookup[25];
	char FAR *VideoMem;
} Video;

// ------------------------------------------------------------------------- //
int32_t Video_VideoType(void)
{
	return Video.VideoType;
}

// ------------------------------------------------------------------------- //

char FAR *vid_address(void)
{
// As usual, block off the local stuff
#if !defined(__unix__) && !defined(_WIN32)
	int16_t tmp1, tmp2;

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
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	SMALL_RECT scroll_rect;
	COORD top_left = { 0, 0 };
	CHAR_INFO char_info;

	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);
	scroll_rect.Top = 1;
	scroll_rect.Left = 0;
	scroll_rect.Bottom = screen_buffer.dwSize.Y;
	scroll_rect.Right = screen_buffer.dwSize.X;

	char_info.Attributes = screen_buffer.wAttributes;
	char_info.Char.UnicodeChar = (TCHAR)' ';

	if (!ScrollConsoleScreenBuffer(std_handle,
								   &scroll_rect, NULL, top_left, &char_info)) {
		display_win32_error();
		exit(0);
	}
#elif defined(__unix__)
	scrollok(stdscr,true);
	scrl(1);
	scrollok(stdscr,false);
#else
//# pragma message("ScrollUp() Nullified by no defines")
#endif
}



// ------------------------------------------------------------------------- //
void put_character(char ch, short attrib, int x, int y)
{
#ifdef __MSDOS__
	Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1))] = ch;
	Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1) + 1L)] = (char)(attrib & 0xff);
#elif defined(_WIN32)
	COORD cursor_pos;
	DWORD bytes_written;

	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(
		std_handle, cursor_pos);
	SetConsoleTextAttribute(
		std_handle,
		(uint16_t)attrib);
	WriteConsole(
		std_handle,
		&ch,
		1,
		&bytes_written,
		NULL);
#elif defined(__unix__)
	move(y, x);
	set_attrs(attrib);
	putch(ch);
#endif
}

void zputs(char *string)
{
	char number[3];
	int16_t cur_char, attr;
	char foreground, background, cur_attrs;
	int16_t i, j;
	int x, y;
#ifdef __MSDOS__
	struct text_info TextInfo;
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	/*  TCHAR space_char = (TCHAR)' ';*/
#endif
	static char o_fg = 7, o_bg = 0;
	int scr_lines = 25, scr_width = 80;

#ifdef _WIN32
	GetConsoleScreenBufferInfo(
		std_handle,
		&screen_buffer);
	x = screen_buffer.dwCursorPosition.X;
	y = screen_buffer.dwCursorPosition.Y;
	scr_lines = screen_buffer.dwSize.Y;
	scr_width = screen_buffer.dwSize.X;
#elif defined(__unix__)
	getyx(stdscr, y, x);
#else
	gettextinfo(&TextInfo);
	x = TextInfo.curx-1;
	y = TextInfo.cury-1;
#endif

	cur_attrs = o_fg | o_bg;
	cur_char = 0;

	for (;;) {
#ifdef _WIN32
		COORD cursor_pos;
		/* Resync with x/y position */
		cursor_pos.X = x;
		cursor_pos.Y = y;
		SetConsoleCursorPosition(std_handle, cursor_pos);
#elif defined(__unix__)
		/* Resync with x/y position */
		move(y, x);
#endif
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
					cur_attrs = attr;
				}
				else {
					foreground=attr;
					o_fg=foreground;
					attr = o_fg | o_bg;
					cur_attrs = attr;
				}
				cur_char += 3;
			}
			else {
				put_character(string[cur_char], cur_attrs, x, y);

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
				put_character(' ', cur_attrs, j, y);
			}
			x += 8;
		}
		else {
			put_character(string[cur_char], cur_attrs, x, y);

			cur_char++;
			x++;
		}
	}
	gotoxy(x+1,y+1);
#ifdef __unix__
	refresh();
#endif
}

void SetCurs(int16_t CursType)
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
	cursor_info.bVisible = true;
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
		std_handle,
		&cursor_info);
#elif defined(__unix__)
	switch (CursType) {
		case CURS_FAT:
			curs_set(2);
			break;
		default:
			curs_set(1);
	}
#endif
}

void qputs(char *string, int16_t x, int16_t y)
{
// As usual, block off the local stuff
#if defined(_WIN32) || defined(__unix__)
	gotoxy(x+1, y+1);
	zputs(string);
#else
	char number[3];
	int16_t cur_char, attr;
	char foreground, background, cur_attrs;
	static o_fg = 7, o_bg = 0;
	int16_t i, j;

	cur_attrs = o_fg | o_bg;
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
					cur_attrs = attr;
				}
				else  {
					foreground=attr;
					o_fg=foreground;
					attr = o_fg | o_bg;
					cur_attrs = attr;
				}
				cur_char += 3;
			}
			else  {
				Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1))] = string[cur_char];
				Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1) + 1L)] = cur_attrs;

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
				Video.VideoMem[ Video.y_lookup[y]+(j<<1) + 1] = cur_attrs;
			}
			x += 8;
		}
		else  {
			Video.VideoMem[ Video.y_lookup[y]+(x<<1)] = string[cur_char];
			Video.VideoMem[ Video.y_lookup[y]+(x<<1) + 1] = cur_attrs;

			cur_char++;
			x++;
		}
	}
#endif
}


static void sdisplay(char *string, int16_t x, int16_t y, char color, int16_t length, int16_t input_length)
{
#ifdef __unix__
	attr_t orig_attr = cur_attr;
	short orig_pair = cur_pair;

	move(y, x);
	set_attrs(color);
	for (char *i = string; *i; i++)
		putch(*i);
	set_attrs(8);
	for (int i = length; i < input_length; i++)
		putch(PADDING);
	attr_set(orig_attr, orig_pair, NULL);
	cur_attr = orig_attr;
	cur_pair = orig_pair;
	refresh();
#elif defined(_WIN32)
	COORD cursor_pos;
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	uint16_t current_attr, i;
	DWORD bytes_written;
	TCHAR padding_char = (TCHAR)PADDING;

	/* Save Current Color Attribute */
	GetConsoleScreenBufferInfo(
		std_handle,
		&screen_buffer);

	current_attr = screen_buffer.wAttributes;

	/* Set New Cursor Position (based on x, y) */
	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(
		std_handle,
		cursor_pos);

	/* Set new color */
	SetConsoleTextAttribute(
		std_handle,
		(uint16_t) color);

	/* Output Text */
	WriteConsole(
		std_handle,
		string,
		strlen(string),
		&bytes_written,
		NULL);

	/* Set color for padding */
	SetConsoleTextAttribute(
		std_handle,
		(uint16_t) 8);

	/* Display Padding */
	for (i = length; i < input_length; i++) {
		WriteConsole(
			std_handle,
			&padding_char,
			1,
			&bytes_written,
			NULL);
	}

	/* Reset color */
	SetConsoleTextAttribute(
		std_handle,
		current_attr);
#else
	int16_t i, offset;
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


int16_t LongInput(char *string, int16_t x, int16_t y, int16_t input_length, char attr,
				 int16_t low_char, int16_t high_char)
{
	int16_t length = strlen(string);  // length of string
	int16_t i,
	insert = false,
			 key,        // key inputted
			 cur_letter = length; // letter we're editing right now
	char tmp_str[255];
	char first_time = true; // flags if this is the first time to input
	// if true, it will clear the line and input
	char old_fg4;

	// initialize the string here

	SetCurs(insert);
#ifdef __MSDOS__
	textattr(attr);
#elif defined(__unix__)
	set_attrs(attr);
#elif defined(_WIN32)
	SetConsoleTextAttribute(
		std_handle,
		(uint16_t)attr);
#endif

	o_fg4 = attr&0x00FF;
	o_bg4 = (attr&0xFF00) >> 8;
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
		first_time = false;

	}
}

void Input(char *string, int16_t length)
{
	int cur_x, cur_y;

#ifdef __MSDOS__
	cur_x = wherex()-1;
	cur_y = wherey()-1;
#elif defined(__unix__)
	getyx(stdscr, cur_y, cur_x);
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	GetConsoleScreenBufferInfo(
		std_handle,
		&screen_buffer);
	cur_x = screen_buffer.dwCursorPosition.X;
	cur_y = screen_buffer.dwCursorPosition.Y;
#endif

	LongInput(string, cur_x, cur_y, length, 15, 2, 255);
	zputs("\n");

	SetCurs(CURS_NORMAL);
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
#elif defined(__unix__)
	if (x2 == 79) {
		set_attrs(attr);
		for (int y = y1; y <= y2; y++) {
			move(y, x1);
			clrtoeol();
		}
	}
#endif
}

void xputs(char *string, int16_t x, int16_t y)
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
#elif defined(__unix__)
	move(y, x);
	for (char *i = string; *i; i++)
		putch(*i);
#elif defined(_WIN32)
	COORD cursor_pos;
	DWORD bytes_written;

	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(
		std_handle,
		cursor_pos);

	WriteConsole(
		std_handle,
		string,
		strlen(string),
		&bytes_written,
		NULL);
#endif
}

void DisplayStr(char *szString)
{
	if (Door_Initialized())
		rputs(szString);
	else
		zputs(szString);
}

// ------------------------------------------------------------------------- //

void Video_Init(void)
{
#if defined(_WIN32)
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

	std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (!GetConsoleCursorInfo(
				std_handle,
				&cursor_info)) {
		display_win32_error();
		exit(0);
	}
	default_cursor_size = cursor_info.dwSize;
#elif defined(__unix__)
	short fg;
	short bg;
	short pair=0;

	// Initialize Curses lib.
	setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	nonl();
	keypad(stdscr, true);
	scrollok(stdscr,false);
	start_color();
	clear();
	refresh();

	// Set up color pairs
	for (bg=0; bg<8; bg++)  {
		for (fg=0; fg<16; fg++) {
			init_pair(++pair,curses_color(fg),curses_color(bg));
		}
	}
#else
	int16_t iTemp;

	Video.VideoMem = vid_address();
	for (iTemp = 0; iTemp < 25;  iTemp++)
		Video.y_lookup[ iTemp ] = iTemp * 160;
#endif
}

void Video_Close(void)
{
#ifdef _WIN32
	FreeConsole();
#elif defined(__unix__)
	set_attrs(8);
	SetCurs(CURS_NORMAL);
	endwin();
#endif
}

#ifdef _WIN32
void gotoxy(int x, int y)
{
	COORD cursor_pos;

	cursor_pos.X = x - 1;
	cursor_pos.Y = y - 1;

	SetConsoleCursorPosition(
		std_handle,
		cursor_pos);
}

void clrscr(void)
{
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
		(uint16_t)7,
		screen_buffer.dwSize.X * screen_buffer.dwSize.Y,
		top_left,
		&cells_written);

	SetConsoleCursorPosition(
		std_handle,
		top_left);
}
#elif defined(__unix__)
void gotoxy(int x, int y)
{
	move(y - 1, x - 1);
}

void clrscr(void)
{
	clear();
	refresh();
}

static void set_attrs(uint8_t attrib)
{
	cur_attr = A_NORMAL;
	if (attrib & 8)
		cur_attr |= A_BOLD;
	if (attrib & 128)
		cur_attr |= A_BLINK;
	cur_pair = (attrib & 0x7F) + 1;
	attr_set(cur_attr, cur_pair, NULL);
}

const static uint32_t cp437_unicode_table[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
	0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
	0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
	0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
	0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
	0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
	0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
	0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
	0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

static void putch(unsigned char ch)
{
	cchar_t wch;
	wchar_t wc[2] = {0, 0};

	if (ch > 127)
		wc[0] = cp437_unicode_table[ch - 128];
	else if (ch < 32)
		wc[0] = ' ';
	else
		wc[0] = ch;
	setcchar(&wch, wc, cur_attr, cur_pair, NULL);
	add_wch(&wch);
}

static short curses_color(short color)
{
	switch (color) {
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
			return(COLOR_BLACK + 8);
		case 9 :
			return(COLOR_BLUE + 8);
		case 10 :
			return(COLOR_GREEN + 8);
		case 11 :
			return(COLOR_CYAN + 8);
		case 12 :
			return(COLOR_RED + 8);
		case 13 :
			return(COLOR_MAGENTA + 8);
		case 14 :
			return(COLOR_YELLOW + 8);
		case 15 :
			return(COLOR_WHITE + 8);
	}
	return(0);
}
#endif
