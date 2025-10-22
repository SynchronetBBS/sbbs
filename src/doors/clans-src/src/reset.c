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
#include "win_wrappers.h"

#include "myopen.h"
#include "parsing.h"
#include "readcfg.h"
#include "structs.h"

#define MAX_OPTION      14

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

#ifndef USE_STDIO
static void Video_Init(void);
#endif

static void ResetMenu(bool InterBBSGame);
static void zputs(char *string);

static int16_t YesNo(char *Query);

static void EditOption(int16_t WhichOption);
static void DosHelp(char *Topic, char *File);

#ifndef USE_STDIO
static int32_t DosGetLong(char *Prompt, int32_t DefaultVal, int32_t Maximum);
static void DosGetStr(char *InputStr, int16_t MaxChars);
#endif

#ifdef __unix__
static void set_attrs(unsigned short int attribute);
static short curses_color(short color);
#endif

static void ColorArea(int16_t xPos1, int16_t yPos1, int16_t xPos2, int16_t yPos2, char Color);
static void GoReset(struct ResetData *ResetData, int16_t Option);

static void News_AddNews(char *szString);
static void News_CreateTodayNews(void);

static void CreateVillageDat(struct ResetData *ResetData);
static void KillAlliances(void);

void System_Error(char *szErrorMsg);

static void UpdateOption(char Option);

#if defined(__MSDOS__)
static struct {
	int32_t VideoType;
	int32_t y_lookup[25];
	char FAR *VideoMem;
} Video;
#endif

static struct ResetData ResetData;

static char szTodaysDate[11];

//extern unsigned _stklen = 32U*(1024U);

#if defined(_WIN32)
static void gotoxy(int x, int y);
static void clrscr(void);
static void settextattr(uint16_t);
static void * save_screen(void);
static void restore_screen(void *);

typedef void * SCREENSTATE;
#elif defined(__unix__)
WINDOW *savedwin;
static void gotoxy(int x, int y);
static void clrscr(void);
static void settextattr(uint16_t);
static void save_screen(void);
static void restore_screen(void);

typedef void * SCREENSTATE;
#endif

#ifdef __unix__
static void do_endwin(void)
{
	endwin();
}
#endif

int main(void)
{
	SYSTEMTIME system_time;

#ifndef USE_STDIO
	Video_Init();
#endif
	Config_Init(0, NULL);

#ifdef __unix__
	atexit(do_endwin);
#endif

	// initialize date
	GetSystemTime(&system_time);
	snprintf(szTodaysDate, sizeof(szTodaysDate), "%02d/%02d/%4d", system_time.wMonth, system_time.wDay,
			system_time.wYear);

	// load up config

	ResetMenu(Config.InterBBS);

#ifdef _WIN32
	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE),
		(uint16_t)7);
#endif
#ifdef __unix__
	curs_set(1);
#endif
	return (0);
}

#ifdef USE_STDIO
static char *StrYesNo(bool value)
{
	if (value == true)
		return "Yes";
	return "No";
}
#endif

static void ResetMenu(bool InterBBSGame)
{
#ifdef USE_STDIO
	char szString[50];
	bool Quit = false, DidReset = false, InterBBS = false;
	int16_t iTemp, CurOption;

	/* show options */

	/* init defaults */
	strlcpy(ResetData.szDateGameStart, szTodaysDate, sizeof(ResetData.szDateGameStart));
	strlcpy(ResetData.szLastJoinDate, "12/16/2199", sizeof(ResetData.szLastJoinDate));
	strlcpy(ResetData.szVillageName, "The Village", sizeof(ResetData.szVillageName));
	ResetData.InterBBSGame = Config.InterBBS;
	ResetData.LeagueWide = false;
	ResetData.InProgress = false;
	ResetData.EliminationMode = false;
	ResetData.ClanTravel = true;
	ResetData.LostDays = 3;
	ResetData.ClanEmpires = true;
	ResetData.MineFights = FIGHTSPERDAY;
	ResetData.ClanFights = CLANCOMBATADAY;
	ResetData.DaysOfProtection = 4;
	ResetData.MaxPermanentMembers = 4;


	while (!Quit) {
		printf("\n\nReset for The Clans %s\n" VERSION);
		printf("----------------------------------------------------------------------------\n");
		printf("1)  Game start date %s\n",ResetData.szDateGameStart);
		printf("2)  Last date to join game %s\n",ResetData.szLastJoinDate);
		printf("3)  Village Name %s\n",ResetData.szVillageName);
		printf("4)  Elimination mode %s\n",StrYesNo(ResetData.EliminationMode));
		printf("5)  Allow interBBS clan travel %s\n",StrYesNo(ResetData.ClanTravel));
		printf("6)  Days before lost troops/clans return %d\n",ResetData.LostDays);
		printf("7)  Allow clan empires %s\n",StrYesNo(ResetData.ClanEmpires));
		printf("8)  Mine fights per day %d\n",ResetData.MineFights);
		printf("9)  Clan fights per day %d\n",ResetData.ClanFights);
		printf("10) Max. Permanent Clan Members %d\n",ResetData.MaxPermanentMembers);
		printf("11) Days of Protection for new users %d\n",ResetData.DaysOfProtection);
		if (Config.InterBBS) {
			printf("13) Join a league\n");
			if (Config.BBSID == 1)
				printf("14) Leaguewide reset with these settings\n");
		}
		else {
			printf("12) Reset local game with these settings\n");
		}
		printf("15) Abort reset\n");
		printf("----------------------------------------------------------------------------\n");

		printf("\nChoose: ");
		scanf("%d",&CurOption);
		--CurOption;
		if (!Config.InterBBS && CurOption >= (MAX_OPTION-2) &&
				CurOption < MAX_OPTION)
			continue;
		else if (Config.BBSID == 1 && Config.InterBBS && CurOption == (MAX_OPTION-2))
			continue;
		else if (Config.BBSID != 1 && CurOption == (MAX_OPTION-1)) {
			continue;
		}

		if (CurOption == MAX_OPTION) {
			Quit = true;
			DidReset = false;
		}
		else if (CurOption == (MAX_OPTION-3)) {
			// can't reset locally here
			if (Config.InterBBS)
				continue;

			/* reset local now */
			// ask one last time, are you sure?
			if (YesNo("Are you sure you wish to reset?") == NO) {
				break;
			}

			GoReset(&ResetData, RESET_LOCAL);
			Quit = true;
			DidReset = true;
		}
		else if (CurOption == (MAX_OPTION-2)) {
			// ask one last time, are you sure?
			printf("\n\n");
			if (YesNo("Are you sure you are joining a league?") == NO) {
				break;
			}
			/* join league NOT in progress */
			ResetData.LeagueWide = true;
			ResetData.InProgress = false;
			GoReset(&ResetData, RESET_JOINIBBS);
			Quit = true;
			DidReset = true;
			InterBBS = true;
		}
		else if (CurOption == (MAX_OPTION-1)) {
			// ask one last time, are you sure?
			printf("\n\n");
			if (YesNo("Are you sure you wish to reset the league?") == NO) {
				break;
			}
			/* leaguewide reset now */
			ResetData.LeagueWide = true;
			GoReset(&ResetData, RESET_LC);
			Quit = true;
			DidReset = true;
			InterBBS = true;
		}
		else
			EditOption(CurOption);
	}

	printf("\n\n");

	if (DidReset) {
		printf("> Your game has been reset.\n");
		if (InterBBS)
			printf("> You must now type 'CLANS /I' to complete the IBBS reset.");
	}
	else
		printf("> The game was not reset.  No changes were made.\n");

#else
	char CurOption = 0;
	GETCH_TYPE  cInput;
	char OldOption = -1;
	bool Quit = false, DidReset = false, InterBBS = false;
//  int16_t iTemp;

	/* clear all */
	clrscr();

	/* show options */
	qputs(" |03Reset for The Clans " VERSION, 0,0);
	qputs("|09- |01---------------------------------------------------------------------------",0,1);
	xputs(" Game start date", 0, 2);
	xputs(" Last date to join game", 0, 3);
	xputs(" Village Name", 0, 4);
	xputs(" Elimination mode", 0, 5);

	xputs(" Allow interBBS clan travel", 0, 6);
	xputs(" Days before lost troops/clans return", 0, 7);
	xputs(" Allow clan empires", 0, 8);
	xputs(" Mine fights per day", 0, 9);
	xputs(" Clan fights per day", 0, 10);

	xputs(" Max. Permanent Clan Members", 0, 11);
	xputs(" Days of Protection for new users", 0, 12);

	xputs(" Reset local game with these settings", 0, 13);
	xputs(" Join a league", 0, 14);
	xputs(" Leaguewide reset with these settings", 0, 15);
	xputs(" Abort reset", 0, 16);
	qputs("|01--------------------------------------------------------------------------- |09-",0,17);

	qputs("|09 Press the up and down keys to navigate.", 0, 18);

	/* init defaults */
	strlcpy(ResetData.szDateGameStart, szTodaysDate, sizeof(ResetData.szDateGameStart));
	strlcpy(ResetData.szLastJoinDate, "12/16/2199", sizeof(ResetData.szLastJoinDate));
	strlcpy(ResetData.szVillageName, "The Village", sizeof(ResetData.szVillageName));
	ResetData.InterBBSGame = Config.InterBBS;
	ResetData.LeagueWide = false;
	ResetData.InProgress = false;
	ResetData.EliminationMode = false;
	ResetData.ClanTravel = true;
	ResetData.LostDays = 3;
	ResetData.ClanEmpires = true;
	ResetData.MineFights = FIGHTSPERDAY;
	ResetData.ClanFights = CLANCOMBATADAY;
	ResetData.DaysOfProtection = 4;
	ResetData.MaxPermanentMembers = 4;

	ColorArea(0,2+MAX_OPTION, 39, 2+MAX_OPTION, 7);
	gotoxy(2, CurOption+3);

	/* dehilight all options */
	ColorArea(39,2, 76, 16, 15);    // choices on the right side

	ColorArea(0, 2, 39, MAX_OPTION+2, 7);

	/* dehilight options which can't be activated */
	if (!Config.InterBBS) {
		/*          ColorArea(0,2, 39, 13, 7); */
		ColorArea(0,14, 76, 15,  8);
	}
	else if (Config.BBSID != 1) {
		// not LC, can't use leaguewide reset option
		/*          ColorArea(0,2, 39, 16, 7); */
		ColorArea(0,15, 76, 15, 8);
		ColorArea(0,13, 39, 13, 8);
	}
	else {
		// LC's BBS AND IBBS set
		/*                  ColorArea(0,2, 39, 16, 7); */
		ColorArea(0,13, 39, 14, 8);
	}

#if defined(_WIN32) || defined (__unix__)
	settextattr((uint16_t)15);
#endif
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
	UpdateOption(10);

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
#ifdef __MSDOS__
		if (cInput == 0)
#elif defined(_WIN32)
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
					if ((CurOption == (MAX_OPTION-1)) && (Config.BBSID != 1))
						CurOption--;
					if ((CurOption == (MAX_OPTION-2)) && ((Config.InterBBS == false) || (Config.BBSID == 1)))
						CurOption--;
					if ((CurOption == (MAX_OPTION-3)) && Config.InterBBS)
						CurOption--;
					break;
				case K_DOWN :
				case K_RIGHT:
					if (CurOption == MAX_OPTION)
						CurOption = 0;
					else
						CurOption++;
					if ((CurOption == (MAX_OPTION-3)) && Config.InterBBS)
						CurOption++;
					if ((CurOption == (MAX_OPTION-2)) && ((Config.InterBBS == false) || (Config.BBSID == 1)))
						CurOption++;
					if ((CurOption == (MAX_OPTION-1)) && (Config.BBSID != 1))
						CurOption++;
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
			if (Config.InterBBS == false && CurOption >= (MAX_OPTION-2) &&
					CurOption < MAX_OPTION)
				continue;
			else if (Config.BBSID == 1 && Config.InterBBS && CurOption == (MAX_OPTION-2))
				continue;
			else if (Config.BBSID != 1 && CurOption == (MAX_OPTION-1)) {
				continue;
			}

			if (CurOption == MAX_OPTION) {
				Quit = true;
				DidReset = false;
			}
			else if (CurOption == (MAX_OPTION-3)) {
#if defined(_WIN32)
				SCREENSTATE screen_state;
#endif
				// can't reset locally here
				if (Config.InterBBS)
					continue;

				/* reset local now */
				// ask one last time, are you sure?
#if defined(_WIN32)
				screen_state = save_screen();
#elif defined(__unix__)
	save_screen();
#endif
				clrscr();
				if (YesNo("|07Are you sure you wish to reset?") == NO) {
#if defined(_WIN32)
					restore_screen(screen_state);
					break;
#elif defined(__unix__)
	restore_screen();
	break;
#endif
				}

				GoReset(&ResetData, RESET_LOCAL);
				Quit = true;
				DidReset = true;
#if defined(_WIN32)
				restore_screen(screen_state);
				break;
#elif defined(__unix__)
	restore_screen();
	break;
#endif
			}
			else if (CurOption == (MAX_OPTION-2)) {
#if defined(_WIN32)
				SCREENSTATE screen_state;
				screen_state = save_screen();
#elif  defined(__unix__)
	save_screen();
#endif

				// ask one last time, are you sure?
				clrscr();
				if (YesNo("|07Are you sure you are joining a league?") == NO) {
#if defined(_WIN32)
					restore_screen(screen_state);
#elif defined(__unix__)
	restore_screen();
#endif
					break;
				}
				/* join league NOT in progress */
				ResetData.LeagueWide = true;
				ResetData.InProgress = false;
				GoReset(&ResetData, RESET_JOINIBBS);
				Quit = true;
				DidReset = true;
				InterBBS = true;
#if defined(_WIN32)
				restore_screen(screen_state);
#elif defined(__unix__)
	restore_screen();
#endif
			}
			else if (CurOption == (MAX_OPTION-1)) {
#if defined(_WIN32)
				SCREENSTATE screen_state;
				screen_state = save_screen();
#elif  defined(__unix__)
	save_screen();
#endif
				// ask one last time, are you sure?
				clrscr();
				if (YesNo("|07Are you sure you wish to reset the league?") == NO) {
#if defined(_WIN32)
					restore_screen(screen_state);
#elif defined(__unix__)
	restore_screen();
#endif
					break;
				}
				/* leaguewide reset now */
				ResetData.LeagueWide = true;
				GoReset(&ResetData, RESET_LC);
				Quit = true;
				DidReset = true;
				InterBBS = true;
#if defined(_WIN32)
				restore_screen(screen_state);
#elif defined(__unix__)
	restore_screen();
#endif
			}
			else {
				EditOption(CurOption);
				UpdateOption(CurOption);
			}
		}
	}

	clrscr();

	if (DidReset) {
		qputs("|09> |07Your game has been reset.", 0,0);
		if (InterBBS)
			qputs("|09> |03You must now type |11CLANS /I |03to complete the IBBS reset.", 0,1);
	}
	else
		qputs("|09> |07The game was not reset.  No changes were made.", 0,0);

	gotoxy(1,3);
#endif
}


void CheckMem(void *Test)
/*
 * Gives system error if the pointer is NULL.
 */
{
	if (Test == NULL) {
		printf("Checkmem Failed.\n");
#ifdef __unix__
		curs_set(1);
#endif
		exit(0);
	}
}

// ------------------------------------------------------------------------- //

#ifndef USE_STDIO
static void qputs(char *string, int16_t x, int16_t y)
{
#if defined(__MSDOS__)
	char number[3];
	int16_t cur_char, attr;
	char foreground, background, cur_attr;
	static o_fg = 7, o_bg = 0;
	int16_t i, j;

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
				Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1))] = string[cur_char];
				Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1) + 1L)] = cur_attr;

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
#else
	gotoxy(x, y);
	zputs(string);
#endif
}



// ------------------------------------------------------------------------- //

#ifdef __MSDOS__
static char FAR *vid_address(void)
{
#ifdef __MSDOS__
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
#elif defined(_WIN32)
	Video.VideoType = COLOR;
	return NULL;
#elif defined(__unix__)
	Video.VideoType = COLOR;
	return NULL;
#else
#endif
}
#endif

static void ScrollUp(void)
{
#ifdef __MSDOS__
	asm {
		mov al,1    // number of lines
		mov ch,0    // starting row
		mov cl,0    // starting column
		mov dh, 24  // ending row
		mov dl, 79  // ending column
		mov bh, 7   // color
		mov ah, 6
		int16_t 10h     // do the scroll
	}
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	COORD top_left = { 0, 0 };
	SMALL_RECT scroll;
	CHAR_INFO char_info;

	GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&screen_buffer);

	scroll.Top = 1;
	scroll.Left = 0;
	scroll.Right = screen_buffer.dwSize.X - 1;
	scroll.Bottom = screen_buffer.dwSize.Y - 1;

	char_info.Char.UnicodeChar = (TCHAR)' ';
	char_info.Attributes = screen_buffer.wAttributes;

	ScrollConsoleScreenBuffer(
		GetStdHandle(STD_OUTPUT_HANDLE),
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
#ifdef __MSDOS__
	int16_t iTemp;
	Video.VideoMem = vid_address();
	for (iTemp = 0; iTemp < 25;  iTemp++)
		Video.y_lookup[ iTemp ] = iTemp * 160;
#elif defined(__unix__)
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
	curs_set(0);
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
#ifdef __MSDOS__
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
	while (x < 80 && *string) {
#ifdef __unix__
		addch(*string);
#else
	fputc(*string, stdout);
#endif
		x++;
		string++;
	}
#endif
}
#endif

static int16_t YesNo(char *Query)
{
#ifdef USE_STDIO
	char answer[129];
	printf("%s ([Yes]/No) ",Query);
	fpurge(stdin);
	fflush(stdout);
	fgets(answer,128,stdin);
	answer[strlen(answer)-1]=0;
	if (answer[0]=='N' || answer[0]=='n')
		return (NO);
	return (YES);
#else
	/* show query */
	zputs(Query);

	/* show Yes/no */
	zputs(" |01(|15Yes|07/no|01) |11");

	/* get user input */
	if (get_answer("YN\r\n") == 'N') {
		/* user says NO */
		zputs("No\n");
		return (NO);
	}
	else {  /* user says YES */
		zputs("Yes\n");
		return (YES);
	}
#endif
}

static void zputs(char *string)
{
#ifndef USE_STDIO
	char number[3];
	int16_t cur_char, attr;
	char foreground, background, cur_attr;
	int16_t i, j,  x,y;
	int16_t scr_lines = 25, scr_width = 80;
#ifdef __MSDOS__
	struct text_info TextInfo;
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	COORD cursor_pos;
	HANDLE std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD bytes_written;
	TCHAR space_char = (TCHAR)' ';
#endif
	static char o_fg = 7, o_bg = 0;

#ifdef __MSDOS__
	gettextinfo(&TextInfo);
	x = TextInfo.curx-1;
	y = TextInfo.cury-1;
#elif defined(_WIN32)
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
				cur_char += 3;
			}
			else {
#ifdef __MSDOS__
				Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1))] = string[cur_char];
				Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1) + 1L)] = cur_attr;
#elif defined(_WIN32)
				SetConsoleTextAttribute(std_handle, (uint16_t)cur_attr);
				WriteConsole(std_handle, &string[cur_char], 1, &bytes_written, NULL);
#elif defined(__unix__)
				set_attrs(cur_attr);
				addch(string[cur_char]);
#endif

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
#ifdef __MSDOS__
				Video.VideoMem[ Video.y_lookup[y]+(j<<1)] = ' ';
				Video.VideoMem[ Video.y_lookup[y]+(j<<1) + 1] = cur_attr;
#elif defined(_WIN32)
				SetConsoleTextAttribute(std_handle, (uint16_t)cur_attr);
				WriteConsole(std_handle, &space_char, 1, &bytes_written, NULL);
#elif defined(__unix__)
				set_attrs(cur_attr);
				addch(' ');
#endif
			}
			x += 8;
		}
		else {
#ifdef __MSDOS__
			Video.VideoMem[ Video.y_lookup[y]+(x<<1)] = string[cur_char];
			Video.VideoMem[ Video.y_lookup[y]+(x<<1) + 1] = cur_attr;
#elif defined(_WIN32)
			SetConsoleTextAttribute(std_handle, (uint16_t)cur_attr);
			WriteConsole(std_handle, &string[cur_char], 1, &bytes_written, NULL);
#elif defined(__unix__)
			set_attrs(cur_attr);
			addch(string[cur_char]);
#endif

			cur_char++;
			x++;
		}
	}
#ifdef __MSDOS__
	gotoxy(x+1,y+1);
#elif defined(_WIN32) || defined(__unix__)
	gotoxy(x, y);
#endif

#ifdef __unix__
	refresh();
#endif
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
#ifndef USE_STDIO
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
#endif

static void EditOption(int16_t WhichOption)
{
#ifdef USE_STDIO
	char *aszHelp[11] = {
		"Date Game Starts",
		"Last Join Date",
		"Village Name",
		"Elimination Mode",
		"Clan Travel",
		"Lost Troops",
		"Clan Empires",
		"Mine Fights",
		"Clan Combat",
		"Max Permanent Members",
		"Days Of Protection"
	};
	int16_t Month, Day, Year;

	printf("\n\n");

	/* show help */
	DosHelp(aszHelp[WhichOption], "reset.hlp");


	switch (WhichOption) {
		case 0 :    /* dategame started */
			printf("\n");
			printf("Enter Month: ");
			scanf("%2d",&Month);
			printf("Enter Day: ");
			scanf("%2d",&Day);
			printf("Enter Year: ");
			scanf("%4d",&Year);
			snprintf(ResetData.szDateGameStart, sizeof(ResetData.szDateGameStart), "%02d/%02d/%4d", Month, Day, Year);
			break;
		case 1 :    /* last join date */
			printf("\n");
			printf("Enter Month: ");
			scanf("%2d",&Month);
			printf("Enter Day: ");
			scanf("%2d",&Day);
			printf("Enter Year: ");
			scanf("%4d",&Year);
			snprintf(ResetData.szLastJoinDate, sizeof(ResetData.szLastJoinDate), "%02d/%02d/%4d", Month, Day, Year);
			break;
		case 2 :    /* village name */
			printf("\n");
			printf("Please enter the village name: ");
			fpurge(stdin);
			fflush(stdout);
			fgets(ResetData.szVillageName,29,stdin);
			ResetData.szVillageName[strlen(ResetData.szVillageName)-1]=0;
			break;
		case 3 :    /* elimination mode? */
			printf("\n");
			if (YesNo("Setup elimination mode?") == YES) {
				ResetData.EliminationMode = true;
			}
			else
				ResetData.EliminationMode = false;
			break;
		case 4 :    /* clan travel? */
			printf("\n");
			if (YesNo("Allow clans to travel?") == YES) {
				ResetData.ClanTravel = true;
			}
			else
				ResetData.ClanTravel = false;
			break;
		case 5 :    /* days before lost troops return */
			printf("\n");
			printf("Days before lost troops/clans returned? ");
			scanf("%2d",&ResetData.LostDays);
			break;
		case 6 :    /* clan empires? */
			printf("\n");
			if (YesNo("Allow clans to create empires?") == YES) {
				ResetData.ClanEmpires = true;
			}
			else
				ResetData.ClanEmpires = false;
			break;
		case 7 :    /* mine fights */
			printf("\n");
			printf("How many mine fights per day? ");
			scanf("%2d",&ResetData.MineFights);
			break;
		case 8 :    /* clan combat */
			printf("\n");
			printf("How many clan fights per day? ");
			scanf("%3d",&ResetData.ClanFights);
			break;
		case 9 :    /* max permanent clan members */
			printf("\n");
			printf("How many members? ");
			scanf("%d",&ResetData.MaxPermanentMembers);
			if (ResetData.MaxPermanentMembers == 0)
				ResetData.MaxPermanentMembers = 1;
			break;
		case 10:    /* days of protection */
			printf("\n");
			printf("How many days? ");
			scanf("%d",&ResetData.DaysOfProtection);
			break;
	}
#else
	char *aszHelp[11] = {
		"Date Game Starts",
		"Last Join Date",
		"Village Name",
		"Elimination Mode",
		"Clan Travel",
		"Lost Troops",
		"Clan Empires",
		"Mine Fights",
		"Clan Combat",
		"Max Permanent Members",
		"Days Of Protection"
	};
	int16_t Month, Day, Year;

	/* save screen */
#ifdef __MSDOS__
	char *acScreen;
	acScreen = malloc(4000);
	memcpy(acScreen, Video.VideoMem, 4000);
#else
#if defined(_WIN32)
	SCREENSTATE screen_state;
	screen_state = save_screen();
#elif  defined(__unix__)
	save_screen();
#endif
#endif

	clrscr();

	/* show help */
	DosHelp(aszHelp[WhichOption], "reset.hlp");


	switch (WhichOption) {
		case 0 :    /* dategame started */
			gotoxy(1,15);
			Month = (int16_t)DosGetLong("|07Enter Month", atoi(ResetData.szDateGameStart), 12);
			Day = (int16_t)DosGetLong("|07Enter Day", atoi(&ResetData.szDateGameStart[3]), 31);
			Year = (int16_t)DosGetLong("|07Enter Year", atoi(&ResetData.szDateGameStart[6]), 2500);

			snprintf(ResetData.szDateGameStart, sizeof(ResetData.szDateGameStart), "%02d/%02d/%4d", Month, Day, Year);
			break;
		case 1 :    /* last join date */
			gotoxy(1,15);
			Month = (int16_t)DosGetLong("|07Enter Month", atoi(ResetData.szLastJoinDate), 12);
			Day = (int16_t)DosGetLong("|07Enter Day", atoi(&ResetData.szLastJoinDate[3]), 31);
			Year = (int16_t)DosGetLong("|07Enter Year", atoi(&ResetData.szLastJoinDate[6]), 2500);

			snprintf(ResetData.szLastJoinDate, sizeof(ResetData.szLastJoinDate), "%02d/%02d/%4d", Month, Day, Year);
			break;
		case 2 :    /* village name */
			gotoxy(1,15);
			zputs("|07Please enter the village name|08> |15");
			DosGetStr(ResetData.szVillageName, 29);
			break;
		case 3 :    /* elimination mode? */
			gotoxy(1,15);
			if (YesNo("|07Setup elimination mode?") == YES) {
				ResetData.EliminationMode = true;
			}
			else
				ResetData.EliminationMode = false;
			break;
		case 4 :    /* clan travel? */
			gotoxy(1,15);
			if (YesNo("|07Allow clans to travel?") == YES) {
				ResetData.ClanTravel = true;
			}
			else
				ResetData.ClanTravel = false;
			break;
		case 5 :    /* days before lost troops return */
			gotoxy(1,15);
			ResetData.LostDays = (int16_t)DosGetLong("|07Days before lost troops/clans returned?", ResetData.LostDays, 14);
			break;
		case 6 :    /* clan empires? */
			gotoxy(1,15);
			if (YesNo("|07Allow clans to create empires?") == YES) {
				ResetData.ClanEmpires = true;
			}
			else
				ResetData.ClanEmpires = false;
			break;
		case 7 :    /* mine fights */
			gotoxy(1,15);
			ResetData.MineFights = (int16_t)DosGetLong("|07How many mine fights per day?", ResetData.MineFights, 50);
			break;
		case 8 :    /* clan combat */
			gotoxy(1,15);
			ResetData.ClanFights = (int16_t)DosGetLong("|07How many clan fights per day?", ResetData.ClanFights, MAX_CLANCOMBAT);
			break;
		case 9 :    /* max permanent clan members */
			gotoxy(1,15);
			ResetData.MaxPermanentMembers = (int16_t)DosGetLong("|07How many members?", ResetData.MaxPermanentMembers, 6);
			if (ResetData.MaxPermanentMembers == 0)
				ResetData.MaxPermanentMembers = 1;
			break;
		case 10:    /* days of protection */
			gotoxy(1,15);
			ResetData.DaysOfProtection = (int16_t)DosGetLong("|07How many days?", ResetData.DaysOfProtection, 16);
			break;
	}

	/* restore screen */
#ifdef __MSDOS__
	memcpy(Video.VideoMem, acScreen, 4000);
	free(acScreen);
#else
#if defined(_WIN32)
	restore_screen(screen_state);
#elif defined(__unix__)
	restore_screen();
#endif
#endif
#endif
}

static void DosHelp(char *Topic, char *File)
{
#ifdef USE_STDIO
	char *Lines[22], string[155];
	int16_t cTemp, Found = false, CurLine, NumLines;
	FILE *fp;
	bool EndOfTopic = false;

	printf("\n");
	printf("%s\n",Topic);
	printf("----------------------------------------------------------------------------\n");

	fp = fopen(File, "r");
	if (!fp) {
		printf("Game help not found!\n");
		return;
	}

	for (cTemp = 0; cTemp < 22; cTemp++)
		Lines[cTemp] = malloc(255);

	/* search for topic */
	while (!Found) {
		if (fgets(string, 155, fp) == NULL) {
			fclose(fp);
			printf("Game help not found!\n");
			for (cTemp = 0; cTemp < 22; cTemp++)
				free(Lines[cTemp]);
			return;
		}

		if (string[0] == '^') {
			/* see if topic is correct */
			if (strspn(&string[1], Topic) == strlen(Topic)) {
				Found = true;
			}
		}
	}

	while (EndOfTopic == false) {
		/* read in up to 22 lines */
		for (CurLine = 0; CurLine < 22; CurLine++) {
			fgets(Lines[CurLine], 355, fp);

			if (Lines[CurLine][0] == '^') {
				if (strspn(&Lines[CurLine][1], "END") == 3) {
					EndOfTopic = true;
					break;
				}
			}
		}
		NumLines = CurLine;

		/* display it */
		for (CurLine = 0; CurLine < NumLines; CurLine++) {
			printf(Lines[CurLine]);
		}
	}

	fclose(fp);
	printf("----------------------------------------------------------------------------\n");

	for (cTemp = 0; cTemp < 22; cTemp++)
		free(Lines[cTemp]);
#else
	char *Lines[22], string[155];
	int16_t cTemp, Found = false, CurLine, NumLines;
	FILE *fp;
	bool EndOfTopic = false;

	qputs("|15", 0, 0);
	qputs(Topic, 1, 0);
	qputs("|09- |01---------------------------------------------------------------------------",0,1);

	qputs("|01--------------------------------------------------------------------------- |09-",0,12);

	fp = fopen(File, "r");
	if (!fp) {
		qputs("|12Game help not found!\n", 0, 2);
		return;
	}

	for (cTemp = 0; cTemp < 22; cTemp++)
		Lines[cTemp] = malloc(255);

	/* search for topic */
	while (!Found) {
		if (fgets(string, 155, fp) == NULL) {
			fclose(fp);
			qputs("|12Game help not found!\n", 0, 2);
			for (cTemp = 0; cTemp < 22; cTemp++)
				free(Lines[cTemp]);
			return;
		}

		if (string[0] == '^') {
			/* see if topic is correct */
			if (strspn(&string[1], Topic) == strlen(Topic)) {
				Found = true;
			}
		}
	}

	while (EndOfTopic == false) {
		/* read in up to 22 lines */
		for (CurLine = 0; CurLine < 22; CurLine++) {
			fgets(Lines[CurLine], 355, fp);

			if (Lines[CurLine][0] == '^') {
				if (strspn(&Lines[CurLine][1], "END") == 3) {
					EndOfTopic = true;
					break;
				}
			}
		}
		NumLines = CurLine;

		/* display it */
		for (CurLine = 0; CurLine < NumLines; CurLine++) {
			qputs(Lines[CurLine], 0, (int16_t)(CurLine+2));
		}
	}

	fclose(fp);

	for (cTemp = 0; cTemp < 22; cTemp++)
		free(Lines[cTemp]);

#endif
}

#ifndef USE_STDIO
static int32_t DosGetLong(char *Prompt, int32_t DefaultVal, int32_t Maximum)
{
	char string[255], NumString[13], DefMax[40];
	GETCH_TYPE InputChar;
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
			strlcpy(TempStr, BackSpaces, sizeof(TempStr));
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
			snprintf(szString, sizeof(szString), "%c", InputCh);
			zputs(szString);
		}
	}
}

static void ColorArea(int16_t xPos1, int16_t yPos1, int16_t xPos2, int16_t yPos2, char Color)
{
	int16_t x, y;
#ifdef _WIN32
	HANDLE std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD cursor_pos;
	DWORD cells_written;
	uint16_t line_len;
#elif defined(__unix__)
	chtype this;
	short attrs = 0;
#endif

	for (y = yPos1; y <= yPos2;  y++) {
		for (x = xPos1;  x <= xPos2;  x++) {
#ifdef __MSDOS__
			Video.VideoMem[ Video.y_lookup[y] + (x<<1) +1 ] = Color;
#elif defined(_WIN32)
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

#endif

static void
GenerateGameID(char *ptr, size_t sz)
{
	SYSTEMTIME st;

	GetSystemTime(&st);
	snprintf(ptr, sz, "%04d%02d%02d%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

static void GoReset(struct ResetData *ResetData, int16_t Option)
{
	FILE *fp;
	struct game_data Game_Data;

	// Delete unwanted files here
	unlink("clans.msj");
	unlink("clans.pc");
	unlink("disband.dat");
	unlink("trades.dat");
	unlink("public.txt");
	unlink("pawn.dat");
	unlink("clans.npx");

	KillAlliances();

	unlink("ally.dat");

	// delete IBBS files
	unlink("ibbs.dat");
	unlink("ipscores.dat");
	unlink("userlist.dat");
	unlink("backup.dat");
	unlink("leaving.dat");

	if (Option == RESET_LOCAL) {
		/*
		 * reset local game, no IBBS
		 */

		// create new game.dat file
		Game_Data.GameState = 1;
		Game_Data.InterBBS = false;

		strlcpy(Game_Data.szTodaysDate, szTodaysDate, sizeof(Game_Data.szTodaysDate));
		strlcpy(Game_Data.szDateGameStart, ResetData->szDateGameStart, sizeof(Game_Data.szDateGameStart));
		strlcpy(Game_Data.szLastJoinDate, ResetData->szLastJoinDate, sizeof(Game_Data.szLastJoinDate));

		Game_Data.NextClanID = 0;
		Game_Data.NextAllianceID = 0;

		Game_Data.MaxPermanentMembers = ResetData->MaxPermanentMembers;
		Game_Data.ClanEmpires = ResetData->ClanEmpires;
		Game_Data.MineFights = ResetData->MineFights;
		Game_Data.ClanFights = ResetData->ClanFights;
		Game_Data.DaysOfProtection = ResetData->DaysOfProtection;

		fp = fopen("game.dat", "wb");
		EncryptWrite_s(game_data, &Game_Data, fp, XOR_GAME);
		fclose(fp);

		// update news
		unlink("yest.asc");
		rename("today.asc", "yest.asc");
		News_CreateTodayNews();
		News_AddNews("|0A \xaf\xaf\xaf |0CThe game has been reset!\n\n");
	}
	else if (Option == RESET_JOINIBBS) {
		// create new game.dat file
		Game_Data.GameState = 2;
		Game_Data.InterBBS = true;

		GenerateGameID(Game_Data.GameID, sizeof(Game_Data.GameID));

		strlcpy(Game_Data.szTodaysDate, szTodaysDate, sizeof(Game_Data.szTodaysDate));
		strlcpy(Game_Data.szDateGameStart, ResetData->szDateGameStart, sizeof(Game_Data.szDateGameStart));
		strlcpy(Game_Data.szLastJoinDate, ResetData->szLastJoinDate, sizeof(Game_Data.szLastJoinDate));

		Game_Data.NextClanID = 0;
		Game_Data.NextAllianceID = 0;

		Game_Data.MaxPermanentMembers = ResetData->MaxPermanentMembers;
		Game_Data.ClanEmpires = ResetData->ClanEmpires;
		Game_Data.MineFights = ResetData->MineFights;
		Game_Data.ClanFights = ResetData->ClanFights;
		Game_Data.DaysOfProtection = ResetData->DaysOfProtection;

		fp = fopen("game.dat", "wb");
		EncryptWrite_s(game_data, &Game_Data, fp, XOR_GAME);
		fclose(fp);

		// update news
		unlink("yest.asc");
		rename("today.asc", "yest.asc");
		News_CreateTodayNews();
		News_AddNews("|0A \xaf\xaf\xaf |0CThe game is waiting for LC OK.\n\n");

	}
	else if (Option == RESET_LC) {
		// leaguewide reset

		// REP: make sure only LC can do this somehow...

		// create game.dat file
		Game_Data.GameState = 1;
		Game_Data.InterBBS = true;

		GenerateGameID(Game_Data.GameID, sizeof(Game_Data.GameID));

		strlcpy(Game_Data.szTodaysDate, szTodaysDate, sizeof(Game_Data.szTodaysDate));
		strlcpy(Game_Data.szDateGameStart, ResetData->szDateGameStart, sizeof(Game_Data.szDateGameStart));
		strlcpy(Game_Data.szLastJoinDate, ResetData->szLastJoinDate, sizeof(Game_Data.szLastJoinDate));

		Game_Data.NextClanID = 0;
		Game_Data.NextAllianceID = 0;

		Game_Data.MaxPermanentMembers = ResetData->MaxPermanentMembers;
		Game_Data.ClanEmpires = ResetData->ClanEmpires;
		Game_Data.MineFights = ResetData->MineFights;
		Game_Data.ClanFights = ResetData->ClanFights;
		Game_Data.DaysOfProtection = ResetData->DaysOfProtection;

		fp = fopen("game.dat", "wb");
		EncryptWrite_s(game_data, &Game_Data, fp, XOR_GAME);
		fclose(fp);

		// update news
		unlink("yest.asc");
		rename("today.asc", "yest.asc");
		News_CreateTodayNews();
		News_AddNews("|0A \xaf\xaf\xaf |0CThe game has been reset!\n\n");
	}

	// create new village data
	CreateVillageDat(ResetData);
}


static void News_CreateTodayNews(void)
{
	/* this initializes the TODAY.ASC file */

	char szString[128];

	snprintf(szString, sizeof(szString), "|0CNews for |0B%s\n\n", szTodaysDate);
	News_AddNews(szString);

	/* give other info like increase in cost of etc. */
}

static void News_AddNews(char *szString)
{
	FILE *fpNewsFile;

	/* open news file */

	/* add to it */

	fpNewsFile = _fsopen("today.asc", "at", SH_DENYWR);

	fputs(szString, fpNewsFile);

	fclose(fpNewsFile);
}

void ClearFlags(char *Flags)
{
	int16_t iTemp;

	for (iTemp = 0; iTemp < 8; iTemp++)
		Flags[iTemp] = 0;
}

static void InitEmpire(struct empire *Empire, int16_t UserEmpire)
{
	int16_t iTemp;

	if (UserEmpire)
		Empire->Land = 100;
	else
		Empire->Land = 0;

	Empire->Points = 0;

	for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++)
		Empire->Buildings[iTemp] = 0;

	Empire->WorkerEnergy = 100;

	// deal with army
	Empire->Army.Followers = 0;
	Empire->Army.Footmen = 0;
	Empire->Army.Axemen = 0;
	Empire->Army.Knights = 0;
	Empire->Army.Rating = 100;
	Empire->Army.Level = 0;
	Empire->SpiesToday = 0;
	Empire->AttacksToday = 0;
	Empire->LandDevelopedToday = 0;
	Empire->Army.Strategy.AttackLength = 10;
	Empire->Army.Strategy.DefendLength = 10;
	Empire->Army.Strategy.LootLevel    = 0;
	Empire->Army.Strategy.AttackIntensity = 50;
	Empire->Army.Strategy.DefendIntensity = 50;
}



static void CreateVillageDat(struct ResetData *ResetData)
{
	FILE *fp;
	struct village_data Village_Data = {
		{ 1, 9, 3, 1, 11, 7, 3, 2, 10, 2, 5, 3, 9, 5, 13, 1, 9, 15, 7, 0, 0, 8, 4,
			1, 8, 1 },
		{ -1 }
	};

	Village_Data.PublicMsgIndex = 1;
	strlcpy(Village_Data.szName, ResetData->szVillageName, sizeof(Village_Data.szName));

	Village_Data.TownType = -1;      /* unknown for now */
	Village_Data.TaxRate = 0;      /* no taxes */
	Village_Data.InterestRate = 0;   /* 0% interest rate */
	Village_Data.GST = 0;        /* no gst */
	Village_Data.Empire.VaultGold = 45000L; /* some money to begin with */
	Village_Data.ConscriptionRate = 10;

	Village_Data.RulingClanId[0] = -1; /* no ruling clan yet */
	Village_Data.RulingClanId[1] = -1;

	Village_Data.szRulingClan[0] = 0;
	Village_Data.RulingDays = 0;
	Village_Data.GovtSystem = GS_DEMOCRACY;


	Village_Data.MarketLevel = 0;      /* weaponshop level */
	Village_Data.TrainingHallLevel = 0;
	Village_Data.ChurchLevel = 0;
	Village_Data.PawnLevel = 0;
	Village_Data.WizardLevel = 0;

	Village_Data.SetTaxToday = false;
	Village_Data.SetInterestToday = false;
	Village_Data.SetGSTToday = false;
	Village_Data.SetConToday = false;

	Village_Data.UpMarketToday = false;
	Village_Data.UpTHallToday = false;
	Village_Data.UpChurchToday = false;
	Village_Data.UpPawnToday = false;
	Village_Data.UpWizToday = false;
	Village_Data.ShowEmpireStats = false;

	ClearFlags(Village_Data.HFlags);
	ClearFlags(Village_Data.GFlags);

	InitEmpire(&Village_Data.Empire, false);
	strlcpy(Village_Data.Empire.szName, Village_Data.szName, sizeof(Village_Data.Empire.szName));
	Village_Data.Empire.Land = 500;      // village starts off with this
	Village_Data.Empire.Buildings[B_BARRACKS] = 10;
	Village_Data.Empire.Buildings[B_WALL] = 10;
	Village_Data.Empire.Buildings[B_TOWER] = 5;
	Village_Data.Empire.Buildings[B_STEELMILL] = 5;
	Village_Data.Empire.Buildings[B_BUSINESS] = 10;
	Village_Data.Empire.Army.Footmen = 100;
	Village_Data.Empire.Army.Axemen = 25;

	Village_Data.CostFluctuation = 5 - RANDOM(11);
	Village_Data.MarketQuality = MQ_AVERAGE;

	fp = fopen("village.dat", "wb");
	EncryptWrite_s(village_data, &Village_Data, fp, XOR_VILLAGE);
	fclose(fp);
}



static void GetAlliances(struct Alliance *Alliances[MAX_ALLIANCES])
{
	FILE *fp;
	int16_t iTemp;

	// init alliances as NULLs
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		Alliances[iTemp] = NULL;

	fp = fopen("ally.dat", "rb");
	if (fp) {
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			Alliances[iTemp] = malloc(sizeof(struct Alliance));
			CheckMem(Alliances[iTemp]);

			notEncryptRead_s(Alliance, Alliances[iTemp], fp, XOR_ALLIES) {
				// no more alliances to read in
				free(Alliances[iTemp]);
				Alliances[iTemp] = NULL;
				break;
			}
		}
		fclose(fp);
	}
}


static void KillAlliances(void)
{
	struct Alliance *Alliances[MAX_ALLIANCES];
	char szFileName[13];
	int16_t iTemp;

	GetAlliances(Alliances);

	// delete files
	// free up mem used by alliances
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		if (Alliances[iTemp]) {
			snprintf(szFileName, sizeof(szFileName), "hall%02d.txt", Alliances[iTemp]->ID);
			unlink(szFileName);

			free(Alliances[iTemp]);
			Alliances[iTemp] = NULL;
		}

	// free up mem used by alliances
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		if (Alliances[iTemp])
			free(Alliances[iTemp]);

	// called to destroy ALLY.DAT and remove those pesky HALLxxyy.TXT files
	unlink("ally.dat");
}

void System_Error(char *szErrorMsg)
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */
{
#ifdef USE_STDIO
	printf("Error: %s", szErrorMsg);
	delay(1000);
	exit(0);
#else
	qputs("|12Error: |07", 0,0);
	qputs(szErrorMsg, 0, 7);
	delay(1000);
#ifdef __unix__
	curs_set(1);
#endif
	exit(0);
#endif
}

#ifndef USE_STDIO
static void UpdateOption(char Option)
{
	char szString[50];
	switch (Option) {
		case 0:
			xputs(ResetData.szDateGameStart, 40, 2);
			break;
		case 1:
			xputs(ResetData.szLastJoinDate, 40, 3);
			break;
		case 2:
			xputs("                                        ", 40, 4);
			xputs(ResetData.szVillageName, 40, 4);
			break;
		case 3:
			if (ResetData.EliminationMode)
				xputs("Yes", 40, 5);
			else
				xputs("No ", 40, 5);
			break;
		case 4:
			if (ResetData.ClanTravel)
				xputs("Yes", 40, 6);
			else
				xputs("No ", 40, 6);
			break;
		case 5:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.LostDays);
			xputs(szString, 40, 7);
			break;
		case 6:
			if (ResetData.ClanEmpires)
				xputs("Yes", 40, 8);
			else
				xputs("No ", 40, 8);
			break;
		case 7:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.MineFights);
			xputs(szString, 40, 9);
			break;
		case 8:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.ClanFights);
			xputs(szString, 40, 10);
			break;
		case 9:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.MaxPermanentMembers);
			xputs(szString, 40, 11);
			break;
		case 10:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.DaysOfProtection);
			xputs(szString, 40, 12);
			break;
	}
}
#endif

#ifdef _WIN32
static void gotoxy(int x, int y)
{
	HANDLE std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD cursor_pos;

	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(std_handle, cursor_pos);
}

static void clrscr(void)
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
	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE),
		attribute);
}

static void * save_screen(void)
{
	CHAR_INFO *char_info_buffer;
	uint32_t buffer_len;
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	HANDLE std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD top_left = { 0, 0 };
	SMALL_RECT rect_rw;

	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);

	buffer_len = (screen_buffer.dwSize.X * screen_buffer.dwSize.Y);
	char_info_buffer = (CHAR_INFO *) malloc(buffer_len * sizeof(CHAR_INFO));
	if (!char_info_buffer) {
		MessageBox(NULL, TEXT("Memory Allocation Failure"),
				   TEXT("ERROR"), MB_OK | MB_ICONERROR);
#ifdef __unix__
		curs_set(1);
#endif
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
	HANDLE std_handle = GetStdHandle(STD_OUTPUT_HANDLE);

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

static void save_screen(void)
{
	savedwin=dupwin(stdscr);
}

static void restore_screen(void)
{
	overwrite(savedwin,stdscr);
	refresh();
}
#endif
