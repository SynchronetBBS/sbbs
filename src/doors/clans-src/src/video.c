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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __unix__
# include <sys/select.h>
# include <termios.h>
# include <unistd.h>
#else
# include <conio.h>
#endif
#ifdef _WIN32
# include "cmdline.h" // defines display_win32_error ()
#endif
#include "win_wrappers.h"

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

static unsigned ScrollTop = 0;
static unsigned ScrollBottom = UINT_MAX;

#ifdef __MSDOS__
static void getxy(int *x, int*y);
#endif

#ifdef _WIN32
static int default_cursor_size = 1;
static HANDLE std_handle;
static void clans_putch(unsigned char ch);
uint8_t cur_attr;
static void getxy(int *x, int*y);
#endif

#ifdef __unix__
static void clans_putch(unsigned char ch);
static void getxy(int *x, int*y);
int getch(void);
static short ansi_colours(short color);
uint8_t cur_attr;
struct termios orig_tio;
int CurrentX = 0;
int CurrentY = 0;
uint8_t CurrentAttr = 7;
static void forced_gotoxy(int x, int y);
#endif

int ScreenWidth = 80;
int ScreenLines = 24;

static bool VideoInitialized;

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

#ifdef __MSDOS__
static char FAR *vid_address(void)
{
// As usual, block off the local stuff
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
}
#endif

void ScrollUp(void)
{
	int bottom = ScrollBottom;
	if (bottom >= ScreenLines)
		bottom = ScreenLines - 1;
// As usual, block off the local stuff
#if defined(__MSDOS__)
	asm {
		mov al,1    // number of lines
		mov ch,ScrollTop    // starting row
		mov cl,0    // starting column
		mov dh, bottom  // ending row
		mov dl, 79  // ending column
		mov bh, 7   // color
		mov ah, 6
		int 10h     // do the scroll
	}
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	SMALL_RECT scroll_rect;
	COORD top_left = { 0, ScrollTop };
	CHAR_INFO char_info;

	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);
	scroll_rect.Top = ScrollTop + 1;
	scroll_rect.Left = 0;
	scroll_rect.Bottom = bottom;
	scroll_rect.Right = screen_buffer.dwSize.X;

	char_info.Attributes = screen_buffer.wAttributes;
	char_info.Char.UnicodeChar = (TCHAR)' ';

	if (!ScrollConsoleScreenBuffer(std_handle,
	    &scroll_rect, NULL, top_left, &char_info)) {
		display_win32_error();
		exit(0);
	}
#elif defined(__unix__)
	fputs("\x1b[S", stdout);
#else
//# pragma message("ScrollUp() Nullified by no defines")
#endif
}

void SetScrollRegion(int top, int bottom)
{
	if (top < 0)
		top = 0;
	ScrollTop = top;
	if (bottom >= ScreenLines)
		bottom = ScreenLines - 1;
	ScrollBottom = bottom;
#if defined(__unix__)
	fprintf(stdout, "\x1b[%d;%dr", top + 1, bottom + 1);
	int x = CurrentX;
	int y = CurrentY;
	CurrentX = -1;
	CurrentY = -1;
	gotoxy(x, y);
#endif
}

void ClearScrollRegion(void)
{
#ifdef __MSDOS__
	asm {
		mov al,0    // number of lines
		mov ch,ScrollTop // starting row
		mov cl,0    // starting column
		mov dh, ScrollBottom  // ending row
		mov dl, 79  // ending column
		mov bh, 7   // color
		mov ah, 6
		int 10h     // do the scroll
	}
#elif defined(_WIN32)
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD top_corner;
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	DWORD display_len, written;

	top_corner.X = 0;
	top_corner.Y = ScrollTop;

	GetConsoleScreenBufferInfo(stdout_handle, &screen_buffer);

	display_len = (screen_buffer.dwSize.Y - (ScreenLines - 1 - ScrollBottom)) *
				  (screen_buffer.dwSize.X);

	FillConsoleOutputCharacter(
		stdout_handle,
		(TCHAR)' ',
		display_len,
		top_corner,
		&written);

	FillConsoleOutputAttribute(
		stdout_handle,
		(WORD)(FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_RED),
		display_len,
		top_corner,
		&written);
#elif defined(__unix__)
	if (ScrollBottom >= (ScreenLines - 1)) {
		gotoxy(0, ScrollTop);
		fputs("\x1b[J", stdout);
	}
	else {
		for (int i = ScrollTop; i <= ScrollBottom; i++) {
			gotoxy(0, i);
			fputs("\x1b[K", stdout);
		}
		gotoxy(0, ScrollTop);
	}
#else
# error No ClearScrollRegiob()
#endif
}

// ------------------------------------------------------------------------- //
static void put_character(char ch, short attrib, int x, int y)
{
#ifdef __MSDOS__
	Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1))] = ch;
	Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1) + 1L)] = (char)(attrib & 0xff);
#else
	gotoxy(x, y);
	textattr(attrib);
	clans_putch(ch);
#endif
}

void zputs(const char *string)
{
	char number[3];
	int16_t cur_char, attr;
	char foreground, background, cur_attrs;
	int16_t i, j;
	int x, y;
	static char o_fg = 7, o_bg = 0;

	cur_attrs = o_fg | o_bg;
	cur_char = 0;
	getxy(&x, &y);

	for (;;) {
#ifdef _WIN32
		gotoxy(x, y);
#endif
		if (string[cur_char] == 0)
			break;
		if (string[cur_char] == '\b') {
			if (x)
				x--;
			gotoxy(x, y);
			cur_char++;
		}
		else if (string[cur_char] == '\r') {
			x = 0;
			cur_char++;
			gotoxy(x, y);
		}
		else if (string[cur_char]=='|') {
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
			if (y >= ScreenLines) {
				/* scroll line up */
				ScrollUp();
				y = ScreenLines - 1;
			}
			cur_char++;
			x = 0;
			gotoxy(x, y);
		}
		else if (string[cur_char] == 9) {
			/* tab */
			cur_char++;
			for (i=0; i<8; i++) {
				j = i + x;
				if (j > ScreenWidth)
					break;
				put_character(' ', cur_attrs, j, y);
			}
		}
		else {
			put_character(string[cur_char], cur_attrs, x, y);

			cur_char++;
			x++;
		}

		if (y > ScrollBottom) {
			/* scroll line up */
			ScrollUp();
			y = ScrollBottom;
			gotoxy(x, y);
		}
		if (x >= ScreenWidth) {
			x = 0;
			y++;

			if (y > ScrollBottom) {
				/* scroll line up */
				ScrollUp();
				y = ScrollBottom;
			}
			gotoxy(x, y);
		}
	}
	gotoxy(x, y);
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
		case CURS_FAT:
			cursor_info.dwSize = 75;
			break;
		case CURS_NORMAL:
		default:
			cursor_info.dwSize = default_cursor_size;
	}
	SetConsoleCursorInfo(
		std_handle,
		&cursor_info);
#elif defined(__unix__)
	switch (CursType) {
		case CURS_FAT:
			fputs("\x1b[0 q", stdout);
			break;
		case CURS_NORMAL:
		default:
			fputs("\x1b[3 q", stdout);
			break;
	}
#endif
}

void qputs(const char *string, int16_t x, int16_t y)
{
#if defined(_WIN32) || defined(__unix__)
	gotoxy(x, y);
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
#ifdef __MSDOS__
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
#else
	uint8_t orig_attr = cur_attr;

	gotoxy(x, y);
	textattr(color);
	for (char *i = string; *i; i++)
		clans_putch(*i);
	textattr(8);
	for (int i = length; i < input_length; i++)
		clans_putch(PADDING);
	textattr(orig_attr);
#endif
}


static int16_t LongInput(char *string, int16_t x, int16_t y, int16_t input_length, char attr,
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
	bool update = true;
	int16_t slength = length;

	// initialize the string here

	SetCurs(insert);
	textattr(attr);

	o_fg4 = attr&0x00FF;
	o_bg4 = (attr&0xFF00) >> 8;
	qputs(string, x, y);
	gotoxy(x+length, y);

	old_fg4 = o_fg4;
	o_fg4 = 8;

	for (i = length; i < input_length; i++)
		clans_putch(PADDING);

	o_fg4 = old_fg4;

	// now edit it

	for (;;) {
		if (update) {
			sdisplay(string, x, y, attr, length, input_length);
			gotoxy(x + cur_letter, y);
			update = false;
		}

		key = getch();

		switch ((unsigned char)key) {
			case 0xE0 :
			case 0 :
				switch (getch()) {
					case K_DOWN : // down
					case K_UP : // up
						break;
					case K_END : // end
						cur_letter = length;
						update = true;
						break;
					case K_RIGHT :   // right
						if (cur_letter < length)
							cur_letter++;
						update = true;
						break;
					case K_LEFT :   // left
						if (cur_letter)
							cur_letter--;
						update = true;
						break;
					case K_HOME : // home
						cur_letter = 0;
						update = true;
						break;
					case K_INSERT : // insert
						insert = !insert;
						SetCurs(insert);
						// change cursor here
						update = true;
						break;
					case K_DELETE :   // delete
						if (length && cur_letter < length) {
							i = cur_letter;
							while (i<length)  {
								string[i] = string[i+1];
								i++;
							}
							string[length] = 0;

							length--;
						}
						update = true;
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
				update = true;
				break;
			case 127  : // maybe backspace (maybe delete)
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
				update = true;
				break;
			case 0x1B : // esc
				break;
			default :
				if (first_time) { // clear line!
					string[0] = 0;
					length = 0;
					cur_letter = 0;
				}
				if (key < low_char || key > high_char)  break;
				update = true;
				if (insert && length < input_length)  {
					strlcpy(tmp_str, &string[cur_letter], sizeof(tmp_str));
					strlcpy(&string[cur_letter+1], tmp_str, slength - cur_letter+1);

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

	getxy(&cur_x, &cur_y);

	LongInput(string, cur_x, cur_y, length, 15, 2, 255);
	zputs("\n");

	SetCurs(CURS_NORMAL);
}


void ClearArea(int x1, int y1,  int x2, int y2, int attr)
{
#if defined(__MSDOS__)
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
		textattr(attr);
		for (int y = y1; y <= y2; y++) {
			gotoxy(x1, y);
			fputs("\x1b[K", stdout);
		}
	}
#elif defined(_WIN32)
	int len = x2 - x1 + 1;
	DWORD written = 0;
	COORD pos = {
		.X = x1,
	};

	for (; y1 <= y2; y1++) {
		FillConsoleOutputCharacter(std_handle, ' ', len, pos, &written);
		FillConsoleOutputAttribute(std_handle, attr, len, pos, &written);
	}
#else
# error No implementation of ClearArea()
#endif
}

void xputs(const char *string, int16_t x, int16_t y)
{
#if defined(__MSDOS__)
	char FAR *VidPtr;

	VidPtr = Video.VideoMem + Video.y_lookup[y] + (x<<1);

	while (x < 80 && *string) {
		*VidPtr = *string;

		VidPtr += 2;
		x++;
		string++;
	}
#else
	gotoxy(x, y);
	for (const char *i = string; *i; i++)
		clans_putch(*i);
#endif
}

void DisplayStr(char *szString)
{
	if (Door_Initialized())
		rputs(szString);
	else
		zputs(szString);
}

char get_answer(char *szAllowableChars)
{
	char cKey;
	uint16_t iTemp;

	for (;;) {
		cKey = getch();
		if (cKey == 0 || cKey == (char)0xE0) {
			getch();
			continue;
		}

		/* see if allowable */
		for (iTemp = 0; iTemp < strlen(szAllowableChars); iTemp++) {
			if (toupper(cKey) == toupper(szAllowableChars[iTemp]))
				break;
		}

		if (iTemp < strlen(szAllowableChars))
			break;  /* found allowable key */
	}

	return (toupper(cKey));
}

int32_t DosGetLong(char *Prompt, int32_t DefaultVal, int32_t Maximum)
{
	char string[255], NumString[13], DefMax[40];
	char InputChar;
	int16_t NumDigits, CurDigit = 0, cTemp;
	int32_t TenPower;

	/* init screen */
	zputs(" ");
	zputs(Prompt);

	snprintf(DefMax, sizeof(DefMax), " |01(|15%" PRId32 "|07; %" PRId32 "|01) |11", DefaultVal, Maximum);
	zputs(DefMax);

	/* NumDigits contains amount of digits allowed using max. value input */

	TenPower = 10;
	for (NumDigits = 1; NumDigits < 11; NumDigits++) {
		if (Maximum < TenPower)
			break;

		TenPower *= 10;
	}

	/* now get input */
	ShowTextCursor(true);
	for (;;) {
		InputChar = get_answer("0123456789><.,\r\n\b\x19");

		if (isdigit(InputChar)) {
			if (CurDigit < NumDigits) {
				NumString[CurDigit++] = InputChar;
				NumString[CurDigit] = 0;
				snprintf(string, sizeof(string), "%c", InputChar);
				zputs(string);
			}

		}
		else if (InputChar == '\b' || InputChar == '\x7f') {
			if (CurDigit > 0) {
				CurDigit--;
				NumString[CurDigit] = 0;
				zputs("\b \b");
			}
		}
		else if (InputChar == '>' || InputChar == '.') {
			/* get rid of old value, by showing backspaces */
			for (cTemp = 0; cTemp < CurDigit; cTemp++)
				zputs("\b \b");

			snprintf(string, sizeof(string), "%-" PRId32, Maximum);
			string[NumDigits] = 0;
			zputs(string);

			strlcpy(NumString, string, sizeof(NumString));
			CurDigit = NumDigits;
		}
		else if (InputChar == '<' || InputChar == ',' || InputChar == 25) {
			/* get rid of old value, by showing backspaces */
			for (cTemp = 0; cTemp < CurDigit; cTemp++)
				zputs("\b \b");

			CurDigit = 0;
			string[CurDigit] = 0;

			strlcpy(NumString, string, sizeof(NumString));
		}
		else if (InputChar == '\r' || InputChar == '\n') {
			if (CurDigit == 0) {
				snprintf(string, sizeof(string), "%-" PRId32, DefaultVal);
				string[NumDigits] = 0;
				zputs(string);

				strlcpy(NumString, string, sizeof(NumString));
				CurDigit = NumDigits;

				zputs("\n");
				break;
			}
			/* see if number too high, if so, make it max */
			else if (atol(NumString) > Maximum) {
				/* get rid of old value, by showing backspaces */
				for (cTemp = 0; cTemp < CurDigit; cTemp++)
					zputs("\b \b");

				snprintf(string, sizeof(string), "%-" PRId32, Maximum);
				string[NumDigits] = 0;
				zputs(string);

				strlcpy(NumString, string, sizeof(NumString));
				CurDigit = NumDigits;
			}
			else {  /* is a valid value */
				zputs("\n");
				break;
			}
		}
	}
	ShowTextCursor(false);

	return (atol(NumString));
}

void DosGetStr(char *InputStr, int16_t MaxChars, bool HiBit)
{
	int16_t CurChar;
	int InputCh;
	char Spaces[85] = "                                                                                     ";
	char BackSpaces[85] = "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
	char TempStr[100], szString[100];

	Spaces[MaxChars] = 0;
	BackSpaces[MaxChars] = 0;

	CurChar = strlen(InputStr);

	zputs(Spaces);
	zputs(BackSpaces);
	zputs(InputStr);

	ShowTextCursor(true);
	for (;;) {
		InputCh = getch();

		if (InputCh == '\b' || InputCh == '\x7f') {
			if (CurChar>0) {
				CurChar--;
				zputs("\b \b");
			}
		}
		else if (InputCh == '\r' || InputCh == '\n') {
			zputs("|16\n");
			InputStr[CurChar]=0;
			break;
		}
		else if (InputCh== '\x19' || InputCh == '\x1B') { // ctrl-y
			InputStr [0] = 0;
			strlcpy(TempStr, BackSpaces, sizeof(TempStr));
			TempStr[ CurChar ] = 0;
			zputs(TempStr);
			Spaces[MaxChars] = 0;
			BackSpaces[MaxChars] = 0;
			zputs(Spaces);
			zputs(BackSpaces);
			CurChar = 0;
		}
		else if (InputCh == '\x7f')
			continue;
		else if (InputCh > '\x7f' && !HiBit)
			continue;
		else if (InputCh == 0)
			continue;
		else if (iscntrl(InputCh))
			continue;
		else if (isalpha(InputCh) && CurChar && InputStr[CurChar-1] == SPECIAL_CODE)
			continue;
		else {  /* valid character input */
			if (CurChar==MaxChars)   continue;
			InputStr[CurChar++]=InputCh;
			InputStr[CurChar] = 0;
			snprintf(szString, sizeof(szString), "%c", InputCh);
			zputs(szString);
		}
	}
	ShowTextCursor(false);
}

// ------------------------------------------------------------------------- //

#if defined(__unix__)
static void makeraw(struct termios *raw)
{
	raw->c_iflag &= ~(IXOFF|INPCK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IGNPAR);
	raw->c_iflag |= IGNBRK;
	raw->c_oflag &= ~OPOST;
	raw->c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|ISIG|NOFLSH|TOSTOP);
	raw->c_cflag &= ~(CSIZE|PARENB);
	raw->c_cflag |= CS8|CREAD;
	raw->c_cc[VMIN] = 1;
	raw->c_cc[VTIME] = 0;
}
#endif

void Video_Init(void)
{
#if defined(_WIN32)
	CONSOLE_CURSOR_INFO cursor_info;
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;

	std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (std_handle == NULL) {
		if (!AllocConsole()) {
			display_win32_error();
			exit(0);
		}
		std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	if (!GetConsoleCursorInfo(
				std_handle,
				&cursor_info)) {
		display_win32_error();
		exit(0);
	}
	default_cursor_size = cursor_info.dwSize;

	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);
	ScreenWidth = screen_buffer.srWindow.Right - screen_buffer.srWindow.Left + 1;
	ScreenLines = screen_buffer.srWindow.Bottom - screen_buffer.srWindow.Top + 1;
#elif defined(__unix__)
	struct termios raw;
	int x, y;

	tcgetattr(STDIN_FILENO, &orig_tio);
	raw = orig_tio;
	makeraw(&raw);
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	setvbuf(stdout, NULL, _IONBF, 0);
	getxy(&x, &y);
	fputs("\x1b[?7l\x1b[255B\x1b[255C", stdout);
	getxy(&ScreenWidth, &ScreenLines);
	ScreenWidth++;
	ScreenLines++;
	gotoxy(x, y);
	textattr(7);
	// character then attribute
	Video.VideoMem = calloc(1, ScreenWidth * ScreenLines * 2);
	clrscr();
#elif defined(__MSDOS__)
	int16_t iTemp;
	struct text_info TextInfo
	gettextinfo(&TextInfo);

	Video.VideoMem = vid_address();
	for (iTemp = 0; iTemp < 25;  iTemp++)
		Video.y_lookup[ iTemp ] = iTemp * 160;
	ScreenLines = TextInfo.screenheight;
	ScreenWidth = TextInfo.screenwidth;
#else
# error No Video_Init() implementation for this platform
#endif
	VideoInitialized = true;
}

void Video_Close(void)
{
	if (VideoInitialized) {
#ifdef _WIN32
		//FreeConsole();
#elif defined(__unix__)
		int x, y;
		getxy(&x, &y);
		fputs("\x1b[?7h\x1b[r", stdout);
		textattr(7);
		ShowTextCursor(true);
		CurrentX = -1;
		CurrentY = -1;
		gotoxy(x, y);
		free(Video.VideoMem);
		Video.VideoMem = NULL;
		tcsetattr(STDIN_FILENO, TCSANOW, &orig_tio);
#endif
	}
}

void ColorArea(int16_t xPos1, int16_t yPos1, int16_t xPos2, int16_t yPos2, char Color)
{
#if defined(_WIN32)
	int16_t y;

	COORD cursor_pos;
	DWORD cells_written;
	uint16_t line_len;

	line_len = xPos2 - xPos1 + 1;

	for (y = yPos1; y <= yPos2; y++) {
		cursor_pos.X = xPos1;
		cursor_pos.Y = y;
		FillConsoleOutputAttribute( std_handle, (uint16_t)Color, line_len, cursor_pos, &cells_written);
	}
#elif defined(__unix__)
	int16_t x, y;

	for (y = yPos1; y <= yPos2;  y++) {
		for (x = xPos1;  x <= xPos2;  x++) {
			gotoxy(x, y);
			textattr(Color);
			clans_putch(Video.VideoMem[(((y) * ScreenWidth) + (x)) * 2]);
		}
	}
#endif
}

#ifdef __MSDOS__
void textattr(uint8_t attrib)
{
	textattr(attrib);
}

static void getxy(int *x, int*y)
{
	if (x)
		*x = wherex()-1;
	if (y)
		*y = wherey()-1;
}
#elif defined(_WIN32)
void * save_screen(void)
{
	CHAR_INFO *char_info_buffer;
	uint32_t buffer_len;
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	COORD top_left = { 0, 0 };
	SMALL_RECT rect_rw;

	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);

	buffer_len = (screen_buffer.dwSize.X * screen_buffer.dwSize.Y);
	char_info_buffer = (CHAR_INFO *) malloc(buffer_len * sizeof(CHAR_INFO));
	if (!char_info_buffer) {
		MessageBox(NULL, TEXT("Memory Allocation Failure"),
				   TEXT("ERROR"), MB_OK | MB_ICONERROR);
		exit(0);
	}

	rect_rw.Top = 0;
	rect_rw.Left = 0;
	rect_rw.Right = screen_buffer.dwSize.X;
	rect_rw.Bottom = screen_buffer.dwSize.Y;

	ReadConsoleOutput(
		std_handle,
		char_info_buffer,
		screen_buffer.dwSize,
		top_left,
		&rect_rw);

	return (void *)char_info_buffer;
}

void restore_screen(void *state)
{
	CHAR_INFO *char_info_buffer = (CHAR_INFO *)state;
	COORD top_left = { 0, 0 };
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	SMALL_RECT rect_write;

	if (!char_info_buffer)
		return;

	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);

	rect_write.Top = 0;
	rect_write.Left = 0;
	rect_write.Bottom = screen_buffer.dwSize.Y;
	rect_write.Right = screen_buffer.dwSize.X;

	WriteConsoleOutput(
		std_handle,
		char_info_buffer,
		screen_buffer.dwSize,
		top_left,
		&rect_write);

	free(char_info_buffer);
}

void ShowTextCursor(bool sh)
{
	CONSOLE_CURSOR_INFO ci = {default_cursor_size, sh};
	SetConsoleCursorInfo(std_handle, &ci);
}

void gotoxy(int x, int y)
{
	COORD cursor_pos;

	cursor_pos.X = x;
	cursor_pos.Y = y;

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

	gotoxy(0, 0);
}

void textattr(uint8_t attrib)
{
	SetConsoleTextAttribute(std_handle, (uint16_t)attrib);
}

static void clans_putch(unsigned char ch)
{
	DWORD bytes_written;

	WriteConsole(
		std_handle,
		&ch,
		1,
		&bytes_written,
		NULL);
}

static void getxy(int *x, int*y)
{
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	GetConsoleScreenBufferInfo(
		std_handle,
		&screen_buffer);
	if (x)
		*x = screen_buffer.dwCursorPosition.X;
	if (y)
		*y = screen_buffer.dwCursorPosition.Y;
}
#elif defined(__unix__)
void * save_screen(void)
{
	size_t sz = ScreenLines * ScreenWidth * 2;
	void *ret = malloc(sz);
	if (ret == NULL)
		return NULL;
	memcpy(ret, Video.VideoMem, sz);
	return ret;
}

static void redraw(void)
{
	char *ch = Video.VideoMem;
	gotoxy(0, 0);
	for (int y = 0; y < ScreenLines; y++) {
		gotoxy(0, y);
		for (int x = 0; x < ScreenWidth; x++) {
			textattr(ch[1]);
			if (y == ScreenLines - 1 && x == ScreenWidth - 1) {
				fputs("\x1b[K", stdout);
			}
			else {
				clans_putch(ch[0]);
				ch += 2;
			}
		}
	}
}

void restore_screen(void *state)
{
	size_t sz = ScreenLines * ScreenWidth * 2;
	memcpy(Video.VideoMem, state, sz);
	free(state);
	redraw();
}

void ShowTextCursor(bool sh)
{
	fputs(sh ? "\x1b[?25h" : "\x1b[?25l", stdout);
}

static void forced_gotoxy(int x, int y)
{
	CurrentX = x;
	CurrentY = y;
	printf("\x1b[%d;%dH", y + 1, x + 1);
}

void gotoxy(int x, int y)
{
	if (CurrentX == x && CurrentY == y)
		return;
	if (x == 0 && CurrentX != 0 && (y == CurrentY || y == CurrentY + 1)) {
		printf("\r");
		CurrentX = 0;
		if (y == CurrentY)
			return;
	}
	if (x == CurrentX - 1) {
		printf("\b");
		CurrentX--;
		if (y == CurrentY)
			return;
	}
	if (x == CurrentX && y == CurrentY + 1) {
		printf("\n");
		CurrentY++;
		return;
	}
	forced_gotoxy(x, y);
}

void clrscr(void)
{
	char *ch = Video.VideoMem;
	for (int y = 0; y < ScreenLines; y++) {
		for (int x = 0; x < ScreenWidth; x++) {
			*(ch++) = ' ';
			*(ch++) = CurrentAttr;
		}
	}

	fputs("\x1b[H\x1b[J", stdout);
	CurrentX = 0;
	CurrentY = 0;
}

void textattr(uint8_t attrib)
{
	if (attrib == CurrentAttr)
		return;
	char seq[23];
	snprintf(seq, sizeof(seq), "\x1b[0;%d;%d", ansi_colours(attrib & 0x07), ansi_colours((attrib >> 4) & 0x07) + 10);
	if (attrib & 0x08)
		strlcat(seq, ";1", sizeof(seq));
	if (attrib & (0x80))
		strlcat(seq, ";5", sizeof(seq));
	strlcat(seq, "m", sizeof(seq));
	fputs(seq, stdout);
	CurrentAttr = attrib;
}

const static char *cp437_unicode_table[128] = {
	"\xc3\x87", "\xc3\xbc", "\xc3\xa9", "\xc3\xa2", "\xc3\xa4", "\xc3\xa0", "\xc3\xa5", "\xc3\xa7",
	"\xc3\xaa", "\xc3\xab", "\xc3\xa8", "\xc3\xaf", "\xc3\xae", "\xc3\xac", "\xc3\x84", "\xc3\x85",
	"\xc3\x89", "\xc3\xa6", "\xc3\x86", "\xc3\xb4", "\xc3\xb6", "\xc3\xb2", "\xc3\xbb", "\xc3\xb9",
	"\xc3\xbf", "\xc3\x96", "\xc3\x9c", "\xc2\xa2", "\xc2\xa3", "\xc2\xa5", "\xe2\x82\xa7", "\xc6\x92",
	"\xc3\xa1", "\xc3\xad", "\xc3\xb3", "\xc3\xba", "\xc3\xb1", "\xc3\x91", "\xc2\xaa", "\xc2\xba",
	"\xc2\xbf", "\xe2\x8c\x90", "\xc2\xac", "\xc2\xbd", "\xc2\xbc", "\xc2\xa1", "\xc2\xab", "\xc2\xbb",
	"\xe2\x96\x91", "\xe2\x96\x92", "\xe2\x96\x93", "\xe2\x94\x82", "\xe2\x94\xa4", "\xe2\x95\xa1", "\xe2\x95\xa2", "\xe2\x95\x96",
	"\xe2\x95\x95", "\xe2\x95\xa3", "\xe2\x95\x91", "\xe2\x95\x97", "\xe2\x95\x9d", "\xe2\x95\x9c", "\xe2\x95\x9b", "\xe2\x94\x90",
	"\xe2\x94\x94", "\xe2\x94\xb4", "\xe2\x94\xac", "\xe2\x94\x9c", "\xe2\x94\x80", "\xe2\x94\xbc", "\xe2\x95\x9e", "\xe2\x95\x9f",
	"\xe2\x95\x9a", "\xe2\x95\x94", "\xe2\x95\xa9", "\xe2\x95\xa6", "\xe2\x95\xa0", "\xe2\x95\x90", "\xe2\x95\xac", "\xe2\x95\xa7",
	"\xe2\x95\xa8", "\xe2\x95\xa4", "\xe2\x95\xa5", "\xe2\x95\x99", "\xe2\x95\x98", "\xe2\x95\x92", "\xe2\x95\x93", "\xe2\x95\xab",
	"\xe2\x95\xaa", "\xe2\x94\x98", "\xe2\x94\x8c", "\xe2\x96\x88", "\xe2\x96\x84", "\xe2\x96\x8c", "\xe2\x96\x90", "\xe2\x96\x80",
	"\xce\xb1", "\xc3\x9f", "\xce\x93", "\xcf\x80", "\xce\xa3", "\xcf\x83", "\xc2\xb5", "\xcf\x84",
	"\xce\xa6", "\xce\x98", "\xce\xa9", "\xce\xb4", "\xe2\x88\x9e", "\xcf\x86", "\xce\xb5", "\xe2\x88\xa9",
	"\xe2\x89\xa1", "\xc2\xb1", "\xe2\x89\xa5", "\xe2\x89\xa4", "\xe2\x8c\xa0", "\xe2\x8c\xa1", "\xc3\xb7", "\xe2\x89\x88",
	"\xc2\xb0", "\xe2\x88\x99", "\xc2\xb7", "\xe2\x88\x9a", "\xe2\x81\xbf", "\xc2\xb2", "\xe2\x96\xa0", "\xc2\xa0"
};

static void clans_putch(unsigned char ch)
{
	char *ptr = &Video.VideoMem[(CurrentY * ScreenWidth + CurrentX) * 2];
	if (ch > 127)
		fputs(cp437_unicode_table[ch - 128], stdout);
	else if (ch < 32)
		fputc(' ', stdout);
	else
		fputc(ch, stdout);
	ptr[0] = ch;
	ptr[1] = CurrentAttr;
	CurrentX++;
	if (CurrentX >= ScreenWidth) {
		CurrentX = ScreenWidth - 1;
		gotoxy(CurrentX, CurrentY);
	}
}

static short ansi_colours(short color)
{
	switch (color) {
		case 0 :
			return(30);
		case 1 :
			return(34);
		case 2 :
			return(32);
		case 3 :
			return(36);
		case 4 :
			return(31);
		case 5 :
			return(35);
		case 6 :
			return(33);
		case 7 :
			return(37);
	}
	return(0);
}

static int getbyte(void)
{
	struct timeval tv = {
		.tv_sec = 0,
		.tv_usec = 500000,
	};
	fd_set rfd;

	FD_ZERO(&rfd);
	FD_SET(STDIN_FILENO, &rfd);
	if (select(STDIN_FILENO + 1, &rfd, NULL, NULL, &tv) == 1) {
		uint8_t ch;
		if (read(STDIN_FILENO, &ch, 1) == 1) {
			return ch;
		}
	}
	return -1;
}

uint8_t keybuf[1024];
unsigned keypos;

enum parseState {
	PSstart = 0,
	PSesc,
	PScsiparams,
	PScsiibs,
	PSctrlseq,
	PSnF = 10,
};

static int getseq(void)
{
	int state = PSstart;
	int ch;

	keypos = 0;
	while ((ch = getbyte()) != -1) {
		// NULs are ignored (even inside a sequence)
		if (ch == 0)
			continue;
		keybuf[keypos++] = ch;
		// If we hit this, just do whatever.
		if (keypos == sizeof(keybuf)) {
			keypos = 0;
			continue;
		}
		switch (state) {
			case PSstart:
				if (ch == '\x1b') {
					state = PSesc;
					break;
				}
				else {
					keybuf[keypos] = 0;
					return keypos;
				}
				break;
			case PSesc:
				if (ch == '[') {
					state = PScsiparams;
					break;
				}
				// nF escape sequence
				else if (ch >= ' ' && ch <= '/') {
					state = PSnF;
					break;
				}
				else {
					// Fp escape sequence...
					if (ch >= '0' && ch <= '?') {
						keybuf[keypos] = 0;
						return keypos;
					}
					// Fe escape sequence
					if (ch >= '@' && ch <= '_') {
						keybuf[keypos] = 0;
						return keypos;
					}
					// Fs escape sequence
					if (ch >= '`' && ch <= '~') {
						keybuf[keypos] = 0;
						return keypos;
					}
				}
				// Anything else is invalid
				keypos = 0;
				break;
			case PSnF:
				if (ch >= '0' && ch <= '~') {
					keybuf[keypos] = 0;
					return keypos;
				}
				if (ch >= ' ' && ch <= '/')
					continue;
				// Anything else is invalid
				keypos = 0;
				break;
			case PScsiparams:
				if (ch >= '0' && ch <= '?')
					break;
				if (ch >= ' ' && ch <= '/') {
					state = PScsiibs;
					break;
				}
				if (ch >= '@' && ch <= '~') {
					keybuf[keypos] = 0;
					return keypos;
				}
				// Anything else is invalid
				keypos = 0;
				break;
			case PScsiibs:
				if (ch >= ' ' && ch <= '/') {
					break;
				}
				if (ch >= '@' && ch <= '~') {
					keybuf[keypos] = 0;
					return keypos;
				}
				// Anything else is invalid
				keypos = 0;
				break;
		}
	}
	return -1;
}

#define K_UP        72
#define K_DOWN      80
#define K_HOME      71
#define K_END       79
#define K_PGUP      73
#define K_PGDN      81
#define K_LEFT      75
#define K_RIGHT     77

static struct keys {
	int ret;
	const char *seq;
} allkeys[] = {
	{K_UP     << 8, "\x1b[A"},
	{K_DOWN   << 8, "\x1b[B"},
	{K_RIGHT  << 8, "\x1b[C"},
	{K_LEFT   << 8, "\x1b[D"},
	{K_HOME   << 8, "\x1b[H"},
	{K_PGUP   << 8, "\x1b[V"},
	{K_PGDN   << 8, "\x1b[U"},
	{K_END    << 8, "\x1b[F"},
	{K_END    << 8, "\x1b[K"},
	{K_INSERT << 8, "\x1b[@"},
	{K_HOME   << 8, "\x1b[1~"},
	{K_INSERT << 8, "\x1b[2~"},
	{K_DELETE << 8, "\x1b[3~"},
	{K_END    << 8, "\x1b[4~"},
	{K_PGUP   << 8, "\x1b[5~"},
	{K_PGDN   << 8, "\x1b[6~"},
	{K_HOME   << 8, "\x1b[L"},
	{K_UP     << 8, "\x1b" "A"},
	{K_DOWN   << 8, "\x1b" "B"},
	{K_RIGHT  << 8, "\x1b" "C"},
	{K_LEFT   << 8, "\x1b" "D"},
	{K_HOME   << 8, "\x1b" "H"},
	{K_END    << 8, "\x1b" "K"},
	{0, NULL}
};

static int nextch;
int getch(void)
{
	int ret = -1;
	if (nextch) {
		ret = nextch;
		nextch = 0;
		return ret;
	}
	for (;;) {
		int seqlen = getseq();
		if (seqlen != -1) {
			if (seqlen == 1)
				return keybuf[0];
			for (int i = 0; allkeys[i].ret; i++) {
				if (strcmp((char *)keybuf, allkeys[i].seq) == 0) {
					if (allkeys[i].ret < 256)
						return allkeys[i].ret;
					nextch = allkeys[i].ret >> 8;
					return 0;
				}
			}
		}
	}
	return -1;
}

static const char *digits = "0123456789";
static void getxy(int *x, int*y)
{
	char *p;
	int seqlen;
	size_t span;

	fputs("\x1b[6n", stdout);
	while((seqlen = getseq()) != -1) {
		if (seqlen < 6)
			continue;
		if (keybuf[0] != '\x1b')
			continue;
		if (keybuf[1] != '[')
			continue;
		p = (char *)&keybuf[2];
		span = strspn(p, digits);
		if (span < 1)
			continue;
		if (span > 5)
			continue;
		if (p[span] != ';')
			continue;
		p[span] = 0;
		*y = atoi(p) - 1;
		p += span;
		p++;
		span = strspn(p, digits);
		if (span < 1)
			continue;
		if (span > 5)
			continue;
		if (p[span] != 'R')
			continue;
		p[span] = 0;
		*x = atoi(p) - 1;
		return;
	}

	*x = 0;
	*y = 0;
	return;
}
#endif
