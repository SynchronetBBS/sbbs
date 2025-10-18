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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __unix__
# include <curses.h>
# include <time.h>
#else
# include <conio.h>
# include <dos.h>
# include <share.h>
# ifndef _WIN32
#  include <mem.h>
# else
#  include <windows.h>
# endif
#endif
#include "unix_wrappers.h"

#include "k_config.h"
#include "parsing.h"
#include "structs.h"

#define MAX_OPTION      12

#ifdef __unix__
#define K_UP        KEY_UP
#define K_DOWN      KEY_DOWN
#define K_HOME      KEY_HOME
#define K_END       KEY_END
#define K_PGUP      KEY_PPAGE
#define K_PGDN      KEY_NPAGE
#define K_LEFT      KEY_LEFT
#define K_RIGHT     KEY_RIGHT
#define K_BS        KEY_BACKSPACE
#define GETCH_TYPE  int
#else
#define K_UP        72
#define K_DOWN          80
#define K_HOME          71
#define K_END       79
#define K_PGUP          73
#define K_PGDN          81
#define K_LEFT          75
#define K_RIGHT         77
#define K_BS            8
#define GETCH_TYPE  char
#endif

#define HILIGHT     11|(1<<4)

#define COLOR   0xB800
#define MONO    0xB000

/* reset types */
#define RESET_LOCAL     1   // normal reset -- local, NO ibbs junk
#define RESET_JOINIBBS  2   // joining an IBBS league game
#define RESET_LC        3   // leaguewide reset by LC


#define FIGHTSPERDAY    12
#define CLANCOMBATADAY  5


static char get_answer(char *szAllowableChars);
static void qputs(char *string, int16_t x, int16_t y);
static void xputs(char *string, int16_t x, int16_t y);

static void Video_Init(void);

static void ConfigMenu(void);
static void zputs(char *string);

static void EditOption(int16_t WhichOption);
static void EditNodeOption(int16_t WhichOption);

static int32_t DosGetLong(char *Prompt, int32_t DefaultVal, int32_t Maximum);
static void DosGetStr(char *InputStr, int16_t MaxChars);

#ifdef __unix__
static void set_attrs(unsigned short int attribute);
static short curses_color(short color);
#endif
#ifdef _WIN32
static DWORD origCursorSize;
static HANDLE std_handle;
#endif

static void ColorArea(int16_t xPos1, int16_t yPos1, int16_t xPos2, int16_t yPos2, char Color);

static bool setCurrentNode(int node);
static void Config_Init(void);

static void System_Error(char *szErrorMsg);

static void UpdateOption(char Option);
static void UpdateNodeOption(char Option);

struct NodeData {
	int number;
	char dropDir[1024];
	bool fossil;
	uintptr_t addr;
	int irq;
};

struct NodeData *nodes;
struct NodeData *currNode;
struct config Config;
static bool ConfigUseLog;

static void gotoxy(int x, int y);
static void clrscr(void);
static void settextattr(uint16_t);
static void * save_screen(void);
static void restore_screen(void *);
static void writeChar(char ch);
static void showCursor(bool sh);
typedef void * SCREENSTATE;

#ifdef __unix__
WINDOW *savedwin;
static void do_endwin(void)
{
	endwin();
}
#endif

int main(void)
{
	Video_Init();
	Config_Init();

#ifdef __unix__
	atexit(do_endwin);
#endif

	// load up config
	ConfigMenu();

#ifdef _WIN32
	SetConsoleTextAttribute(
		std_handle,
		(uint16_t)7);
#endif
	showCursor(true);
	return (0);
}

static void
WriteCfg(void)
{
	FILE *f = fopen("clans.cfg", "w");

	fputs("# Clans Config File ---------------------------------------------------------\n"
	    "#\n"
	    "# Edit it as you wish with a text file or use CONFIG.EXE to modify.\n"
	    "#\n\n"
	    "# [GameData] ----------------------------------------------------------------\n", f);
	fprintf(f, "SysopName       %s\n", Config.szSysopName);
	fprintf(f, "BBSName         %s\n", Config.szBBSName);
	fprintf(f, "UseLog          %s\n", ConfigUseLog ? "Yes" : "No");
	fprintf(f, "ScoreANSI       %s\n", Config.szScoreFile[1]);
	fprintf(f, "ScoreASCII      %s\n", Config.szScoreFile[0]);
	fprintf(f, "ScoreASCII      %s\n", Config.szScoreFile[0]);
	fputs("\n", f);
	fputs("# [InterBBSData] ------------------------------------------------------------\n", f);
	fprintf(f, "%sInterBBS\n", Config.InterBBS ? "" : "#");
	fprintf(f, "%sNetmailDir      %s\n", Config.InterBBS ? "" : "#", Config.szNetmailDir);
	fprintf(f, "%sInboundDir      %s\n", Config.InterBBS ? "" : "#", Config.szInboundDir);
	fprintf(f, "%sMailerType      %s\n", Config.InterBBS ? "" : "#", Config.MailerType == MAIL_BINKLEY ? "Binkley" : "Not Binkly");
	for (struct NodeData *nd = nodes; nd && nd->number > 0; nd++) {
		fputs("\n", f);
		fputs("# [NodeData] ----------------------------------------------------------------\n", f);
		fprintf(f, "Node            %d\n", nd->number);
		fprintf(f, "DropDirectory   %s\n", nd->dropDir);
		fprintf(f, "UseFOSSIL       %s\n", nd->fossil ? "Yes" : "No");
		if (nd->addr != UINTPTR_MAX)
			fprintf(f, "SerialPortAddr  0x%" PRIXPTR "\n", nd->addr);
		else
			fputs("SerialPortAddr  Default\n", f);
		if (nd->irq >= 0)
			fprintf(f, "SerialPortIRQ   %d\n", nd->irq);
		else
			fputs("SerialPortIRQ   Default\n", f);
	}
	fclose(f);
}

static void ConfigMenu(void)
{
	char CurOption = 0;
	GETCH_TYPE  cInput;
	char OldOption = -1;
	bool Quit = false, DidReset = false;

	/* clear all */
	clrscr();

	/* show options */
	qputs(" |03Config for The Clans " VERSION, 0,0);
	qputs("|09- |01---------------------------------------------------------------------------",0,1);
	xputs(" Sysop Name", 0, 2);
	xputs(" BBS Name", 0, 3);
	xputs(" Use Log", 0, 4);
	xputs(" ANSI Score File", 0, 5);
	xputs(" ASCII Score File", 0, 6);
	xputs(" InterBBS Enabled", 0, 7);
	xputs(" BBS ID", 0, 8);
	xputs(" Netmail Directory", 0, 9);
	xputs(" Inbound Directory", 0, 10);
	xputs(" Mailer Type", 0, 11);
	xputs(" Configure Node", 0, 12);
	xputs(" Save Config", 0, 13);
	xputs(" Abort Config", 0, 14);
	qputs("|01--------------------------------------------------------------------------- |09-",0,15);

	qputs("|09 Press the up and down keys to navigate.", 0, 16);

	/* init defaults */

	ColorArea(0,2+MAX_OPTION, 39, 2+MAX_OPTION, 7);
	gotoxy(2, CurOption+3);

	/* dehilight all options */
	ColorArea(39,2, 76, 16, 15);    // choices on the right side

	ColorArea(0, 2, 39, MAX_OPTION+2, 7);

	/* dehilight options which can't be activated */
	if (!Config.InterBBS) {
		ColorArea(0, 8, 76, 11,  8);
	}

	settextattr((uint16_t)15);
	/* show data */
	UpdateOption(0);
	UpdateOption(1);
	UpdateOption(2);
	UpdateOption(3);
	UpdateOption(4);
	UpdateOption(5);
	UpdateOption(6);
	UpdateOption(7);
	UpdateOption(8);
	UpdateOption(9);

	while (!Quit) {
		if (OldOption != CurOption) {
			if (OldOption != -1)
				ColorArea(0, (int16_t)(OldOption+2), 39, (int16_t)(OldOption+2), 7);
			OldOption = CurOption;
			/* show which is highlighted */
			ColorArea(0, (int16_t)(CurOption+2), 39, (int16_t)(CurOption+2), HILIGHT);
			/* Unhighlight old option */
		}

		cInput = getch();
#if defined(_WIN32)
	if (cInput == 0 || cInput == (char)0xE0)
#elif defined(__unix__)
	if (cInput > 256)
#endif /* !_WIN32 */
		{
#ifndef __unix__
			cInput = getch();
#endif
			switch (cInput) {
				case K_UP   :
				case K_LEFT :
					if (CurOption == 0)
						CurOption = MAX_OPTION;
					else
						CurOption--;
					if ((CurOption == (MAX_OPTION-3)) && (!Config.InterBBS))
						CurOption -= 4;
					break;
				case K_DOWN :
				case K_RIGHT:
					if (CurOption == MAX_OPTION)
						CurOption = 0;
					else
						CurOption++;
					if ((CurOption == (MAX_OPTION-6)) && (!Config.InterBBS))
						CurOption += 4;
					break;
				case K_HOME :
				case K_PGUP :
					CurOption = 0;
					break;
				case K_END :
				case K_PGDN :
					CurOption = MAX_OPTION;
					break;
			}
		}
		else if (cInput == 0x1B)
			Quit = true;
		else if (cInput == 13) {
			if (Config.InterBBS == false && CurOption >= (MAX_OPTION-6) &&
					CurOption < (MAX_OPTION - 2))
				continue;

			if (CurOption == MAX_OPTION) {
				Quit = true;
				DidReset = false;
			}
			else if (CurOption == (MAX_OPTION-1)) {
				WriteCfg();
				SCREENSTATE screen_state;
				screen_state = save_screen();
				/* leaguewide reset now */
				Quit = true;
				DidReset = true;
				restore_screen(screen_state);
			}
			else {
				EditOption(CurOption);
				UpdateOption(CurOption);
			}
		}
	}

	clrscr();

	if (DidReset) {
		qputs("|09> |07Your game has been configured.", 0,0);
		qputs("|09> |03You may now type |11RESET |03to complete the setup.", 0,1);
	}
	else
		qputs("|09> |07The game was not configured.  No changes were made.", 0,0);

	gotoxy(1,3);
}

static void NodeMenu(void)
{
	char CurOption = 0;
	GETCH_TYPE  cInput;
	char OldOption = -1;
	bool Quit = false;
	char str[80];
	const int last = 4;

	/* clear all */
	clrscr();

	/* show options */
	sprintf(str, " |03Node %d Config for The Clans " VERSION, currNode->number);
	qputs(str, 0,0);
	qputs("|09- |01---------------------------------------------------------------------------",0,1);
	xputs(" Dropfile Directory", 0, 2);
	xputs(" Use FOSSIL", 0, 3);
	xputs(" Serial Port Address", 0, 4);
	xputs(" Serial Port IRQ", 0, 5);
	xputs(" Return to main menu", 0, 6);
	qputs("|01--------------------------------------------------------------------------- |09-",0,15);

	qputs("|09 Press the up and down keys to navigate.", 0, 16);

	/* init defaults */

	ColorArea(0,2+last, 39, 2+last, 7);
	gotoxy(2, CurOption+3);

	/* dehilight all options */
	ColorArea(39,2, 76, 6, 15);    // choices on the right side

	ColorArea(0, 2, 39, last+2, 7);

	settextattr((uint16_t)15);
	/* show data */
	UpdateNodeOption(0);
	UpdateNodeOption(1);
	UpdateNodeOption(2);
	UpdateNodeOption(3);

	while (!Quit) {
		if (OldOption != CurOption) {
			if (OldOption != -1)
				ColorArea(0, (int16_t)(OldOption+2), 39, (int16_t)(OldOption+2), 7);
			OldOption = CurOption;
			/* show which is highlighted */
			ColorArea(0, (int16_t)(CurOption+2), 39, (int16_t)(CurOption+2), HILIGHT);
			/* Unhighlight old option */
		}

		cInput = getch();
#if defined(_WIN32)
	if (cInput == 0 || cInput == (char)0xE0)
#elif defined(__unix__)
	if (cInput > 256)
#endif /* !_WIN32 */
		{
#ifndef __unix__
			cInput = getch();
#endif
			switch (cInput) {
				case K_UP   :
				case K_LEFT :
					if (CurOption == 0)
						CurOption = last;
					else
						CurOption--;
					break;
				case K_DOWN :
				case K_RIGHT:
					if (CurOption == last)
						CurOption = 0;
					else
						CurOption++;
					break;
				case K_HOME :
				case K_PGUP :
					CurOption = 0;
					break;
				case K_END :
				case K_PGDN :
					CurOption = last;
					break;
			}
		}
		else if (cInput == 0x1B)
			Quit = true;
		else if (cInput == 13) {
			if (CurOption == last) {
				Quit = true;
			}
			else {
				EditNodeOption(CurOption);
				UpdateNodeOption(CurOption);
			}
		}
	}
}

// ------------------------------------------------------------------------- //

static void qputs(char *string, int16_t x, int16_t y)
{
	gotoxy(x, y);
	zputs(string);
}

static void ScrollUp(void)
{
#if defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	COORD top_left = { 0, 0 };
	SMALL_RECT scroll;
	CHAR_INFO char_info;

	GetConsoleScreenBufferInfo(
		std_handle,
		&screen_buffer);

	scroll.Top = 1;
	scroll.Left = 0;
	scroll.Right = screen_buffer.dwSize.X - 1;
	scroll.Bottom = screen_buffer.dwSize.Y - 1;

	char_info.Char.UnicodeChar = (TCHAR)' ';
	char_info.Attributes = screen_buffer.wAttributes;

	ScrollConsoleScreenBuffer(
		std_handle,
		&scroll,
		NULL,
		top_left,
		&char_info);
#elif defined(__unix__)
	scrollok(stdscr,true);
	scrl(1);
	scrollok(stdscr,false);
#endif
}

// ------------------------------------------------------------------------- //

static void Video_Init(void)
{
#ifdef _WIN32
	CONSOLE_CURSOR_INFO ci;
	GetConsoleCursorInfo(std_handle, &ci);
	origCursorSize = ci.dwSize;
#endif

#if defined(__unix__)
	short fg;
	short bg;
	short pair=0;

	// Initialize Curses lib.
	initscr();
	cbreak();
	noecho();
	nonl();
//      intrflush(stdscr, false);
	keypad(stdscr, true);
	scrollok(stdscr,false);
	start_color();
	showCursor(false);
	clear();
	refresh();

	// Set up color pairs
	for (bg=0; bg<8; bg++)  {
		for (fg=0; fg<16; fg++) {
			init_pair(++pair,curses_color(fg),curses_color(bg));
		}
	}

#endif
}

#ifdef __unix__
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
			return(COLOR_BLACK);
		case 9 :
			return(COLOR_BLUE);
		case 10 :
			return(COLOR_GREEN);
		case 11 :
			return(COLOR_CYAN);
		case 12 :
			return(COLOR_RED);
		case 13 :
			return(COLOR_MAGENTA);
		case 14 :
			return(COLOR_YELLOW);
		case 15 :
			return(COLOR_WHITE);
	}
	return(0);
}
#endif

static void xputs(char *string, int16_t x, int16_t y)
{
	gotoxy(x, y);
	while (x < 80 && *string) {
#ifdef __unix__
		addch(*string);
#else
	fputc(*string, stdout);
#endif
		x++;
		string++;
	}
}

static void zputs(char *string)
{
	char number[3];
	int16_t cur_char, attr;
	char foreground, background, cur_attr;
	int16_t i, j,  x,y;
	int16_t scr_lines = 25, scr_width = 80;
#if defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	COORD cursor_pos;
#endif
	static char o_fg = 7, o_bg = 0;

#if defined(_WIN32)
	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);
	x = screen_buffer.dwCursorPosition.X;
	y = screen_buffer.dwCursorPosition.Y;
	scr_lines = screen_buffer.dwSize.Y;
	scr_width = screen_buffer.dwSize.X;
#elif defined(__unix__)
	getyx(stdscr,y,x);
#endif

	cur_attr = o_fg | o_bg;
	cur_char = 0;

	for (;;) {
#ifdef _WIN32
		/* Resync with x/y position */
		cursor_pos.X = x;
		cursor_pos.Y = y;
		SetConsoleCursorPosition(std_handle, cursor_pos);
#elif defined(__unix__)
		/* Resync with x/y position */
		move(y,x);
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
					cur_attr = (char)attr;
				}
				else {
					foreground=(char)attr;
					o_fg=foreground;
					attr = o_fg | o_bg;
					cur_attr = (char)attr;
				}
				settextattr((uint16_t)cur_attr);
				cur_char += 3;
			}
			else {
				writeChar(string[cur_char]);
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
				writeChar(' ');
			}
			x += 8;
		}
		else {
			writeChar(string[cur_char]);
			cur_char++;
			x++;
		}
	}
#if defined(_WIN32) || defined(__unix__)
	gotoxy(x, y);
#endif

#ifdef __unix__
	refresh();
#endif
}

#ifdef __unix__
static void set_attrs(unsigned short int attribute)
{
	int   attrs=A_NORMAL;

	if (attribute & 128)
		attrs |= A_BLINK;
	attrset(COLOR_PAIR(attribute+1)|attrs);
}
#endif

static char get_answer(char *szAllowableChars)
{
	GETCH_TYPE cKey;
	uint16_t iTemp;

	for (;;) {
		cKey = getch();
		if (cKey == K_BS)
			cKey='\b';

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

static void EditOption(int16_t WhichOption)
{
	/* save screen */
	SCREENSTATE screen_state;
	screen_state = save_screen();

	settextattr(15);
	switch (WhichOption) {
		case 0 :    /* sysop name started */
			gotoxy(40, 2);
			DosGetStr(Config.szSysopName, sizeof(Config.szSysopName) - 1);
			break;
		case 1 :    /* BBS name */
			gotoxy(40, 3);
			DosGetStr(Config.szBBSName, sizeof(Config.szBBSName) - 1);
			break;
		case 2 :    /* Use Log */
			ConfigUseLog = !ConfigUseLog;
			break;
		case 3 :    /* ANSI Score File */
			gotoxy(40, 5);
			DosGetStr(Config.szScoreFile[1], sizeof(Config.szScoreFile[1]) - 1);
			break;
		case 4 :    /* ASCII Score File */
			gotoxy(40, 6);
			DosGetStr(Config.szScoreFile[0], sizeof(Config.szScoreFile[0]) - 1);
			break;
		case 5 :    /* InterBBS Enable */
			Config.InterBBS = !Config.InterBBS;
			break;
		case 6 :    /* BBS ID */
			gotoxy(40,8);
			Config.BBSID = DosGetLong("", Config.BBSID, 32767);
			break;
		case 7 :    /* Netmail Dir */
			gotoxy(40, 9);
			DosGetStr(Config.szNetmailDir, sizeof(Config.szNetmailDir) - 1);
			break;
		case 8 :    /* Inbound Dir */
			gotoxy(40, 10);
			DosGetStr(Config.szNetmailDir, sizeof(Config.szNetmailDir) - 1);
			break;
		case 9 :    /* Mailer type */
			if (Config.MailerType == MAIL_BINKLEY)
				Config.MailerType = MAIL_OTHER;
			else
				Config.MailerType = MAIL_BINKLEY;
			break;
		case 10:    /* Configure node */
			gotoxy(40, 12);
			if (setCurrentNode(DosGetLong("Node Number to Edit", 1, 32767)))
				NodeMenu();
			break;
	}

	/* restore screen */
	restore_screen(screen_state);
}

static void EditNodeOption(int16_t WhichOption)
{
	char addrstr[19];
	settextattr(15);
	switch (WhichOption) {
		case 0 :    /* dropfile directory */
			gotoxy(40, 2);
			DosGetStr(currNode->dropDir, 39);
			break;
		case 1 :    /* use fossil */
			currNode->fossil = !currNode->fossil;
			break;
		case 2 :    /* Address */
			gotoxy(40, 4);
			if (currNode->addr != UINTPTR_MAX)
				sprintf(addrstr, "0x%" PRIXPTR, currNode->addr);
			else
				strcpy(addrstr, "Default");
			DosGetStr(addrstr, sizeof(addrstr) - 1);
			currNode->addr = strtoull(addrstr, NULL, 0);
			break;
		case 3 :    /* IRQ */
			gotoxy(40, 5);
			currNode->irq = DosGetLong("", currNode->irq, 2048);
			break;
	}
}

static int32_t DosGetLong(char *Prompt, int32_t DefaultVal, int32_t Maximum)
{
	char string[255], NumString[13], DefMax[40];
	GETCH_TYPE InputChar;
	int16_t NumDigits, CurDigit = 0, cTemp;
	int32_t TenPower;

	/* init screen */
	zputs(" ");
	zputs(Prompt);

	sprintf(DefMax, " |01(|15%" PRId32 "|07; %" PRId32 "|01) |11", DefaultVal, Maximum);
	zputs(DefMax);

	/* NumDigits contains amount of digits allowed using max. value input */

	TenPower = 10;
	for (NumDigits = 1; NumDigits < 11; NumDigits++) {
		if (Maximum < TenPower)
			break;

		TenPower *= 10;
	}

	/* now get input */
	showCursor(true);
	for (;;) {
		InputChar = get_answer("0123456789><.,\r\n\b\x19");

		if (isdigit(InputChar)) {
			if (CurDigit < NumDigits) {
				NumString[CurDigit++] = InputChar;
				NumString[CurDigit] = 0;
				sprintf(string, "%c", InputChar);
				zputs(string);
			}

		}
		else if (InputChar == '\b'
#ifdef __unix__
				 || InputChar == KEY_BACKSPACE
#endif
				) {
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

			sprintf(string, "%-" PRId32, Maximum);
			string[NumDigits] = 0;
			zputs(string);

			strcpy(NumString, string);
			CurDigit = NumDigits;
		}
		else if (InputChar == '<' || InputChar == ',' || InputChar == 25) {
			/* get rid of old value, by showing backspaces */
			for (cTemp = 0; cTemp < CurDigit; cTemp++)
				zputs("\b \b");

			CurDigit = 0;
			string[CurDigit] = 0;

			strcpy(NumString, string);
		}
		else if (InputChar == '\r' || InputChar == '\n') {
			if (CurDigit == 0) {
				sprintf(string, "%-" PRId32, DefaultVal);
				string[NumDigits] = 0;
				zputs(string);

				strcpy(NumString, string);
				CurDigit = NumDigits;

				zputs("\n");
				break;
			}
			/* see if number too high, if so, make it max */
			else if (atol(NumString) > Maximum) {
				/* get rid of old value, by showing backspaces */
				for (cTemp = 0; cTemp < CurDigit; cTemp++)
					zputs("\b \b");

				sprintf(string, "%-" PRId32, Maximum);
				string[NumDigits] = 0;
				zputs(string);

				strcpy(NumString, string);
				CurDigit = NumDigits;
			}
			else {  /* is a valid value */
				zputs("\n");
				break;
			}
		}
	}
	showCursor(false);

	return (atol(NumString));
}


static void DosGetStr(char *InputStr, int16_t MaxChars)
{
	int16_t CurChar;
	GETCH_TYPE InputCh;
	char Spaces[85] = "                                                                                     ";
	char BackSpaces[85] = "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
	char TempStr[100], szString[100];

	Spaces[MaxChars] = 0;
	BackSpaces[MaxChars] = 0;

	CurChar = strlen(InputStr);

	zputs(Spaces);
	zputs(BackSpaces);
	zputs(InputStr);

	showCursor(true);
	for (;;) {
		InputCh = getch();

		if (InputCh == '\b'
#ifdef __unix__
				|| InputCh == KEY_BACKSPACE
#endif
		   ) {
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
		else if (InputCh== '' || InputCh == '\x1B') { // ctrl-y
			InputStr [0] = 0;
			strcpy(TempStr, BackSpaces);
			TempStr[ CurChar ] = 0;
			zputs(TempStr);
			Spaces[MaxChars] = 0;
			BackSpaces[MaxChars] = 0;
			zputs(Spaces);
			zputs(BackSpaces);
			CurChar = 0;
		}
		else if (InputCh >= '')
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
			sprintf(szString, "%c", InputCh);
			zputs(szString);
		}
	}
	showCursor(false);
}

static void ColorArea(int16_t xPos1, int16_t yPos1, int16_t xPos2, int16_t yPos2, char Color)
{
	int16_t x, y;
#ifdef _WIN32
	COORD cursor_pos;
	DWORD cells_written;
	uint16_t line_len;
#elif defined(__unix__)
	chtype this;
	short attrs = 0;
#endif

	for (y = yPos1; y <= yPos2;  y++) {
		for (x = xPos1;  x <= xPos2;  x++) {
#if defined(_WIN32)
			line_len = (xPos2 - xPos1);

			// Is it just me or is this a replace loop?
			for (y = yPos1; y <= yPos2; y++) {
				cursor_pos.X = xPos1;
				cursor_pos.Y = y;
				FillConsoleOutputAttribute(
					std_handle,
					(uint16_t)Color,
					line_len,
					cursor_pos,
					&cells_written);
			}
#elif defined(__unix__)
			this=mvinch(y, x);
			this &= 255;
			if (Color & 128)
				attrs |= A_BLINK;
			this |= attrs|COLOR_PAIR(Color+1);
			mvaddch(y, x, this);
#endif
		}
	}
#ifdef __unix__
	refresh();
#endif
}

static bool
setCurrentNode(int node)
{
	if (node < 1)
		return false;
	size_t nodeCount = 0;
	currNode = NULL;
	for (struct NodeData *nd = nodes; nd && nd->number > 0; nd++) {
		nodeCount++;
		if (nd->number == node) {
			currNode = nd;
			return true;
		}
	}
	struct NodeData *nnd = realloc(nodes, sizeof(struct NodeData) * (nodeCount + 2));
	if (nnd == NULL) {
		System_Error("realloc() failed");
		return false; // Won't actually return.
	}
	nodes = nnd;
	currNode = &nnd[nodeCount];
	nnd[nodeCount+1].number = -1;
	currNode->number = node;
	snprintf(currNode->dropDir, sizeof(currNode->dropDir), "/bbs/node%d", node);
	currNode->fossil = false;
	currNode->addr = -1;
	currNode->irq = -1;
	return true;
}

static void Config_Init(void)
/*
 * Loads data from .CFG file into Config.
 *
 */
{
	FILE *fpConfigFile;
	char szConfigName[40], szConfigLine[255];
	char *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	int16_t iKeyWord;
	int i;
//  int16_t iCurrentNode = 1;

	// --- Set defaults
	strcpy(szConfigName, "clans.cfg");
	strcpy(Config.szScoreFile[0], "scores.asc");
	strcpy(Config.szScoreFile[1], "scores.ans");
	Config.BBSID = 1;
	strcpy(Config.szNetmailDir, "/bbs/outbound");
	strcpy(Config.szInboundDir, "/bbs/inbound");
	Config.MailerType = MAIL_BINKLEY;
	Config.InterBBS = false;

	fpConfigFile = _fsopen(szConfigName, "rt", SH_DENYWR);
	if (!fpConfigFile) {
		return;
	}

	for (;;) {
		/* read in a line */
		if (fgets(szConfigLine, 255, fpConfigFile) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szConfigLine;
		ParseLine(pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;

		GetToken(pcCurrentPos, szToken);

		/* Loop through list of keywords */
		for (iKeyWord = 0; iKeyWord < MAX_CONFIG_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (stricmp(szToken, papszConfigKeyWords[iKeyWord]) == 0) {
				/* Process config token */
				switch (iKeyWord) {
					case 0 :  /* sysopname */
						strcpy(Config.szSysopName, pcCurrentPos);
						break;
					case 1 :  /* bbsname */
						strcpy(Config.szBBSName, pcCurrentPos);
						break;
					case 2 :  /* use log */
						if (stricmp(pcCurrentPos, "Yes") == 0)
							ConfigUseLog = true;
						else
							ConfigUseLog = false;
						break;
					case 3 :  /* ANSI score file */
						strcpy(Config.szScoreFile[1], pcCurrentPos);
						break;
					case 4 :  /* ASCII score file */
						strcpy(Config.szScoreFile[0], pcCurrentPos);
						break;


					case 5 :  /* Node */
						i = atoi(pcCurrentPos);
						if (i > 0)
							setCurrentNode(i);
						break;
					case 6 :  /* Drop Directory */
						if (currNode)
							strcpy(currNode->dropDir, pcCurrentPos);
						break;
					case 7 :  /* Use FOSSIL */
						if (currNode) {
							if (stricmp(pcCurrentPos, "No") == 0)
								currNode->fossil = false;
							else
								currNode->fossil = true;
						}
						break;
					case 8 :  /* Serial port address */
						if (currNode)
							currNode->addr = atoi(pcCurrentPos);
						break;
					case 9 :  /* Serial port IRQ */
						if (currNode)
							currNode->irq = atoi(pcCurrentPos);
						break;
					case 10 : /* BBS Id */
						Config.BBSID = atoi(pcCurrentPos);
						break;
					case 11 : /* netmail dir */
						strcpy(Config.szNetmailDir, pcCurrentPos);

						/* remove '\' if last char is it */
						if (Config.szNetmailDir [ strlen(Config.szNetmailDir) - 1] == '\\' || Config.szNetmailDir [strlen(Config.szNetmailDir) - 1] == '/')
							Config.szNetmailDir [ strlen(Config.szNetmailDir) - 1] = 0;
						break;
					case 12 : /* inbound dir */
						strcpy(Config.szInboundDir, pcCurrentPos);

						/* add '\' if last char is not it */
						if (Config.szInboundDir [ strlen(Config.szInboundDir) - 1] != '\\' &&  Config.szInboundDir [strlen(Config.szInboundDir) - 1] != '/')
							strcat(Config.szInboundDir, "/");
						break;
					case 13 : /* mailer type */
						if (stricmp(pcCurrentPos, "BINKLEY") == 0)
							Config.MailerType = MAIL_BINKLEY;
						else
							Config.MailerType = MAIL_OTHER;
						break;
					case 14 : /* in a league? */
						Config.InterBBS = true;
						break;
				}
			}
		}
	}

	fclose(fpConfigFile);

}

static void System_Error(char *szErrorMsg)
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */
{
	qputs("|12Error: |07", 0,0);
	qputs(szErrorMsg, 0, 7);
	delay(1000);
	showCursor(true);
	exit(0);
}

static void UpdateOption(char Option)
{
	char szString[50];
	switch (Option) {
		case 0:
			settextattr(15);
			xputs("                                        ", 40, 2);
			xputs(Config.szSysopName, 40, 2);
			break;
		case 1:
			settextattr(15);
			xputs("                                        ", 40, 3);
			xputs(Config.szBBSName, 40, 3);
			break;
		case 2:
			settextattr(15);
			xputs(ConfigUseLog ? "Yes" : "No ", 40, 4);
			break;
		case 3:
			settextattr(15);
			xputs("                                        ", 40, 5);
			xputs(Config.szScoreFile[1], 40, 5);
			break;
		case 4:
			settextattr(15);
			xputs("                                        ", 40, 6);
			xputs(Config.szScoreFile[0], 40, 6);
			break;
		case 5:
			settextattr(15);
			xputs(Config.InterBBS ? "Yes" : "No ", 40, 7);
			ColorArea(0, 8, 76, 11,  Config.InterBBS ? 7 : 8);
			break;
		case 6:
			sprintf(szString, "%d\n", Config.BBSID);
			settextattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 8);
			xputs(szString, 40, 8);
			break;
		case 7:
			settextattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 9);
			xputs(Config.szNetmailDir, 40, 9);
			break;
		case 8:
			settextattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 10);
			xputs(Config.szInboundDir, 40, 10);
			break;
		case 9:
			settextattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 11);
			xputs(Config.MailerType == MAIL_BINKLEY ? "Binkley    " : "Not Binkley", 40, 11);
			break;
	}
}

static void UpdateNodeOption(char Option)
{
	char szString[50];
	switch (Option) {
		case 0:
			settextattr(15);
			xputs("                                        ", 40, 2);
			xputs(currNode->dropDir, 40, 2);
			break;
		case 1:
			settextattr(15);
			xputs(currNode->fossil ? "Yes" : "No ", 40, 3);
			break;
		case 2:
			settextattr(15);
			xputs("                                        ", 40, 4);
			if (currNode->addr == UINTPTR_MAX)
				strcpy(szString, "Default");
			else
				sprintf(szString, "0x%" PRIXPTR, currNode->addr);
			xputs(szString, 40, 4);
			break;
		case 3:
			settextattr(15);
			xputs("                                        ", 40, 5);
			if (currNode->irq < 0)
				strcpy(szString, "Default");
			else
				sprintf(szString, "%d", currNode->irq);
			xputs(szString, 40, 5);
			break;
	}
}

#ifdef _WIN32
static void gotoxy(int x, int y)
{
	COORD cursor_pos;

	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(std_handle, cursor_pos);
}

static void clrscr(void)
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

static void settextattr(uint16_t attribute)
{
	SetConsoleTextAttribute(std_handle, attribute);
}

static void * save_screen(void)
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

static void restore_screen(void *state)
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

static void writeChar(char ch)
{
	DWORD bytes_written;

	WriteConsole(std_handle, &ch, 1, &bytes_written, NULL);
}

static void showCursor(bool sh)
{
	CONSOLE_CURSOR_INFO ci = {origCursorSize, sh};
	SetConsoleCursorInfo(std_handle, &ci);
}
#elif defined(__unix__)
static void gotoxy(int x, int y)
{
	move(y, x);
}

static void clrscr(void)
{
	clear();
	refresh();
}

static void settextattr(uint16_t attribute)
{
	set_attrs(attribute);
}

static void * save_screen(void)
{
	savedwin=dupwin(stdscr);
	return NULL;
}

static void restore_screen(void *unused)
{
	overwrite(savedwin,stdscr);
	refresh();
}

static void writeChar(char ch)
{
	addch(ch);
}

static void showCursor(bool sh)
{
	curs_set(sh ? 1 : 0);
}
#endif
