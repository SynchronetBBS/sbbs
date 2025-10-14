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
 * Door-specific ADT
 *
 * -- All other functions which deal with door specific stuff, should use this
 *    ADT and NOT OpenDoors...
 *
 *
 * DoorInit MUST be called before all other functions.
 */

#include <stdio.h>
#include <stdlib.h>
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <string.h>
#include <ctype.h>

#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <dos.h>
#include <share.h>
#include <conio.h>
#endif

#include <OpenDoor.h>
#include "structs.h"
#include "ibbs.h"
#include "language.h"
#include "mstrings.h"
#include "system.h"
// #include "tslicer.h"
#include "video.h"
#include "door.h"
#include "village.h"
#include "myopen.h"
#include "parsing.h"
#include "help.h"

#define ASCIIFILE   0
#define ANSIFILE  1

#define COLOR   0xB800
#define MONO    0xB000

extern struct system System;
extern struct config *Config;
extern struct Language *Language;
extern struct clan *PClan;
extern char Spells_szCastDestination[25];
extern char Spells_szCastSource[25];
extern int Spells_CastValue;
extern struct village Village;
extern struct game Game;
extern BOOL Verbose;

struct {
	BOOL Initialized;
	BOOL AllowScreenPause;

	char ColorScheme[50];

	BOOL UserBooted;            // True if a user was online already
} Door = { FALSE, TRUE,
		   { 1, 9, 3, 1, 11, 7, 3, 2, 10, 2, 5, 3, 9, 5, 13, 1, 9, 15, 7, 0, 0, 8, 4,
			 1, 1, 1 },
		   FALSE
		 };

// ------------------------------------------------------------------------- //

__BOOL Door_Initialized(void)
/*
 * Returns TRUE if od_init was already called.
 */
{
	return Door.Initialized;
}

// ------------------------------------------------------------------------- //

void CreateSemaphor(void)
{
	FILE *fp;

	fp = _fsopen("online.flg", "wb", SH_DENYRW);
	fwrite(&System.Node, sizeof(_INT16), 1, fp);
	fclose(fp);
}

void RemoveSemaphor(void)
{
	unlink("online.flg");
}

BOOL SomeoneOnline(void)
/*
 * Function will check if a user is online by seeing if a semaphor file
 * exists.
 */
{
	FILE *fp;
	_INT16 WhichNode;

	fp = _fsopen("online.flg", "rb", SH_DENYWR);
	if (fp) {
		fread(&WhichNode, sizeof(_INT16), 1, fp);
		fclose(fp);

		// if node is same as current node, disregard this file
		if (WhichNode == System.Node)
			return FALSE;
		else
			return TRUE;
	}
	else
		return FALSE;
}

// ------------------------------------------------------------------------- //

void Door_SetColorScheme(char *ColorScheme)
/*
 * This function will set the color scheme.
 */
{
	_INT16 iTemp;

	for (iTemp = 0; iTemp < 50; iTemp++)
		Door.ColorScheme[iTemp] = ColorScheme[iTemp];
}

// ------------------------------------------------------------------------- //

void LocalLoad(void)
/*
 * This function will read in the local user's login name.
 *
 */
{
#ifdef __unix__

//#elif defined(ODPLAT_WIN32)
	// Todo create local login screen
#else
	char name[80], szString[128];
	_INT16 i;

	/* display box */
	clrscr();

	zputs(ST_LOCAL1);
	zputs(ST_LOCAL2);
	zputs(ST_LOCAL3);
	zputs(ST_LOCAL4);
	zputs(ST_LOCAL5);
	zputs(ST_LOCAL6);
	zputs(ST_LOCAL7);
	zputs(ST_LOCAL8);
	zputs(ST_LOCAL9);
	zputs(ST_LOCAL10);
	zputs(ST_LOCAL11);

	// if in a league, tell the guy
	if (Game.Data->InterBBS) {
		IBBS_LoginStats();
	}

	qputs(" |09The Clans |07" VERSION, 0, 16);

	sprintf(szString, " |07Enter login name.  Press [|09Enter|07] for |14%s", Config->szSysopName);
	qputs(szString, 0, 18);
	sprintf(szString, " |07Node |09%03d  |15> ", System.Node);
	qputs(szString, 0, 19);

	name[0] = 0;
	szString[0] = 0;
	gotoxy(14,20);
	Input(szString, 27);

	if (strlen(szString))
		strcpy(name, szString);
	else {
		if (strlen(Config->szSysopName))
			strcpy(name, Config->szSysopName);
		else
			strcpy(name, "Sysop");
	}

	zputs("|16");

	strcpy(od_control.user_name, name);
	/*    for (i = 0; i < (signed)strlen(name); i++)
	      name[i] = toupper(name[i]); */
	strcpy(od_control.user_location, Config->szBBSName);
	od_control.user_timelimit = 30;
	od_control.od_info_type = CUSTOM;
	od_control.user_ansi = TRUE;
	od_control.user_screen_length = 25;

	System.Local = TRUE;
#endif
}

// ------------------------------------------------------------------------- //

/*
 * Functions run before and after chat respectively.
 *
 */

void BeforeChat(void)
{
	rputs(ST_BEFORECHAT);
}

void AfterChat(void)
{
	rputs(ST_AFTERCHAT);
}

// ------------------------------------------------------------------------- //

void NoDoorFileHandler(void)
/*
 * If no dropfile was found, this function is run.
 */
{
	if (!System.Local) {
		/* show title ascii */

		zputs(ST_NODOORFILE);
		delay(2500);
		System_Close();
	}
}

// ------------------------------------------------------------------------- //
BOOL StatusOn = TRUE;

void NoStatus(void)
/*
 * If no status line is chosen, this is run?
 */
{
#ifndef __unix__
	if (StatusOn) {
		od_set_statusline(STATUS_NONE);
		StatusOn = FALSE;
	}
	else {
		StatusOn = TRUE;
		od_set_statusline(STATUS_NORMAL);

		if (Video_VideoType() == MONO) {
			ClearArea(0,23,  79,24, (7<<4));
			qputs("|23|00The C l a n s   " VERSION " copyright 1998 Allen Ussher    F1-F3 toggles status bar  ", 0, 24);
		}
		else {
			ClearArea(0,23,  79,24,  7|(1<<4));
			qputs("|15|17The C l a n s   |00" VERSION " |03copyright 1998 Allen Ussher    |00F1-F3 toggles status bar  |16", 0, 24);
		}
	}
#endif
}

void TwoLiner(unsigned char which)
/*
 * Two line status line function.
 */
{
#ifndef __unix__
	char szString[128];
	static char Status[2][155];

	switch (which) {
		case PEROP_INITIALIZE :
			od_control.key_status[0] = 0x3B00;    // F1
			od_control.key_status[1] = 0x3C00;    // F2
			od_control.key_status[2] = 0x3D00;    // F3
			od_control.key_status[3] = 0;    // F4
			od_control.key_status[4] = 0;    // F5
			od_control.key_status[5] = 0;    // F6
			od_control.key_status[6] = 0;    // F7
			od_control.key_status[7] = 0;    // F8

			od_control.od_hot_key[od_control.od_num_keys] = 0x4300;    // F9
			od_control.od_hot_function[od_control.od_num_keys] = NoStatus;
			od_control.od_num_keys++;

			od_control.key_chat = 0x2E00;     // Alt-C
			od_control.key_drop2bbs = 0x2000;   // Alt-D
			od_control.key_dosshell = 0x2400;   // Alt-J
			od_control.key_hangup = 0x2300;     // Alt-H
			od_control.key_keyboardoff = 0x2500;  // Alt-K
			od_control.key_lesstime = 0x5000;   // dn key
			od_control.key_moretime = 0x4800;   // up key

			if (Video_VideoType() == MONO) {
				ClearArea(0,23,  79,24, (7<<4));
				qputs("|23|00The C l a n s   " VERSION " copyright 1998 Allen Ussher    F1-F3 toggles status bar  ", 0, 24);
			}
			else {
				ClearArea(0,23,  79,24,  7|(1<<4));

				qputs("|15|17The C l a n s   |00" VERSION " |03copyright 1998 Allen Ussher    |00F1-F3 toggles status bar  |16", 0, 24);
			}
			break;
		case PEROP_DEINITIALIZE :
			break;
		case PEROP_DISPLAY1 :
			if (!StatusOn)  NoStatus();

			sprintf(Status[0], "%s of %-010s                     ", od_control.user_name, od_control.user_location);
			xputs(Status[0], 0, 23);
			sprintf(Status[1], " Baud: %5ld Time: %2d      %s    Node: %3d ", od_control.baud, od_control.user_timelimit - od_control.user_time_used, (od_control.user_ansi) ? "[ANSI] " : "[ASCII]", System.Node);
			xputs(Status[1], 31, 23);
			break;
		case PEROP_UPDATE1 :
			if (!StatusOn)  NoStatus();

			xputs(Status[0], 0, 23);

			sprintf(szString, "%-3d", od_control.user_timelimit - od_control.user_time_used);

			Status[1][19] = szString[0];
			Status[1][20] = szString[1];
			Status[1][21] = szString[2];
			Status[1][22] = szString[3];
			xputs(Status[1], 31, 23);
			break;
		case PEROP_DISPLAY2 :
		case PEROP_UPDATE2 :
			if (!StatusOn)  NoStatus();
			xputs("Alt+ [H]angup  [J]ump to DOS  [C]hat w/User  [D]rop to BBS                      ", 0, 23);
			break;
		case PEROP_DISPLAY3 :
		case PEROP_UPDATE3 :
			if (!StatusOn)  NoStatus();
			xputs("[F9] Toggle Status Line ON/OFF  [Up] Give Time to user  [Down] Take Time        ", 0, 23);
			break;
	}
#endif
}

// ------------------------------------------------------------------------- //

void Door_ToggleScreenPause(void)
{
	Door.AllowScreenPause = !Door.AllowScreenPause;
}

void Door_SetScreenPause(__BOOL State)
{
	Door.AllowScreenPause = State;
}

__BOOL Door_AllowScreenPause(void)
{
	return Door.AllowScreenPause;
}


// ------------------------------------------------------------------------- //

void door_pause(void)
/*
 * Displays <pause> prompt.
 */
{
	char *pc;

	rputs(ST_PAUSE);

	/* wait for user to hit key */
	while (od_get_key(TRUE) == FALSE)
		od_sleep(0);

	od_putch('\r');
	pc = ST_PAUSE;
	while (*pc) {
		od_putch(' ');
		pc++;
	}
	od_putch('\r');
}


// ------------------------------------------------------------------------- //

void rputs(char *string)
/*
 * Outputs the pipe or ` encoded string.
 */
{
	_INT16 i;
	char number[3]; // two digit number!
	char *pCurChar,*pStrChar;
	_INT16 attr;  // color
	static _INT16 o_fg = 7, o_bg = 0;
	static _INT16 old_fg = 7, old_bg = 0;
	char szString[128],
	*szQuality[4] = { "Average", "Good", "Very Good", "Excellent" };

	if (!(*string))
		return;

	pCurChar = string;

	while (*pCurChar) {
		for (pStrChar=pCurChar; *pStrChar!='|'&&*pStrChar!=0&&*pStrChar!=SPECIAL_CODE&&*pStrChar!='\n'&&*pStrChar!='`'&&*pStrChar!=SPECIAL_CODE; pStrChar++);
		od_disp(pCurChar, pStrChar-pCurChar, FALSE);
		pCurChar=pStrChar;
		if (!(*pCurChar))break;

		if ((*pCurChar) == '|') {
			if (isdigit(*(pCurChar + 1)) && isdigit(*(pCurChar +2))) {
				number[0] = *(pCurChar+1);
				number[1] = *(pCurChar+2);
				number[2] = 0;

				attr = atoi(number);
				if (attr > 15) {
					if (o_bg != (attr-16)) {
						o_bg = attr - 16;
						od_set_colour(o_fg, o_bg);
					}
				}
				else {
					if (o_fg != attr) {
						o_fg = attr;
						od_set_colour(o_fg, o_bg);
					}
				}
				pCurChar += 3;
			}
			else if (*(pCurChar+1) == '0' && isalpha(*(pCurChar+2))) {
				if (!(o_fg == Door.ColorScheme[ *(pCurChar+2) - 'A'] &&
						o_bg == 0)) {
					o_fg = Door.ColorScheme[ *(pCurChar+2) - 'A'];
					o_bg = 0;

					od_set_colour(o_fg, o_bg);
				}
				pCurChar += 3;
			}
			else if (*(pCurChar+1) == 'S') {
				old_fg = o_fg;
				old_bg = o_bg;
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'R') {
				o_fg = old_fg;
				o_bg = old_bg;

				od_set_colour(o_fg, o_bg);
				pCurChar += 2;
			}
			else {
				od_emulate(*pCurChar);
				pCurChar++;
			}

		}
		else if ((*pCurChar) == SPECIAL_CODE) {
			if (*(pCurChar+1) == 'P') {
				/* pause */
				pCurChar += 2;
				door_pause();
			}
			else if (*(pCurChar+1) == 'C') {
				/* cls */
				od_clr_scr();
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'B') {
				/* backspace */
				od_putch('\b');
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'V') {
				/* version */
				rputs(VERSION);
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'N') {
				Door_ToggleScreenPause();
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'T') {
				/* send 24 CR/LF to "clear" line*/
				for (i = 0; i < od_control.user_screen_length; i++) {
					od_disp_str("\n\r");
					delay(10);
				}
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'Y') {
				/* send 24 CR/LF to "clear" line*/
				for (i = 0; i < od_control.user_screen_length; i++)
					od_disp_str("\n\r");
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'Z') {
				/* end line */
				return;
			}
			else if (*(pCurChar+1) == 'D') {
				/* delay 100 */
				delay(100);
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'R') {
				/* \r */
				od_disp_str("\r");
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'L') {
				/* \r */
				od_disp_str("\n\r");
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'F') {
				/* fights remaining */
				sprintf(szString, "%d", PClan->FightsLeft);
				rputs(szString);
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'M') {
				/* mine level */
				sprintf(szString, "%d", PClan->MineLevel);
				rputs(szString);
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'S') {
				/* spellcasting options */
				switch (*(pCurChar+2)) {
					case 'S' : /* source of cast */
						rputs(Spells_szCastSource);
						pCurChar += 3;
						break;
					case 'D' : /* destination/target of cast */
						rputs(Spells_szCastDestination);
						pCurChar += 3;
						break;
					case 'V' : /* value of cast */
						sprintf(szString, "%d", Spells_CastValue);
						rputs(szString);
						pCurChar += 3;
						break;
					default :
						pCurChar++;
						break;
				}
			}
			else if (*(pCurChar+1) == '1') {
				/* pawn level */
				sprintf(szString, "%d", Village.Data->PawnLevel);
				rputs(szString);
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == '2') {
				/* wiz shop level */
				sprintf(szString, "%d", Village.Data->WizardLevel);
				rputs(szString);
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'X') {
				/* mine level */
				sprintf(szString, "%d", Village.Data->MarketLevel);
				rputs(szString);
				pCurChar += 2;
			}
			else if (*(pCurChar+1) == 'Q') {
				/* market quality level */

				sprintf(szString, "%s", szQuality[Village.Data->MarketQuality]);
				rputs(szString);
				pCurChar += 2;
			}
			else {
				// od_emulate( *pCurChar );
				od_putch(*pCurChar);
				pCurChar++;
			}
		}
		else if ((*pCurChar) == '\n') {
			od_putch('\n');
			od_putch('\r');
			pCurChar++;
		}
		else if ((*pCurChar) == '`' && iscodechar(*(pCurChar + 1))
				 && iscodechar(*(pCurChar +2))) {
			pCurChar++;

			// get background
			if (isdigit(*pCurChar))
				o_bg = *pCurChar - '0';
			else
				o_bg = *pCurChar - 'A' + 10;

			pCurChar++;

			// get foreground
			if (isdigit(*pCurChar))
				o_fg = *pCurChar - '0';
			else
				o_fg = *pCurChar - 'A' + 10;

			od_set_colour(o_fg, o_bg);

			pCurChar++;
		}
		else {
			//od_emulate( *pCurChar );
			od_putch(*pCurChar);
			pCurChar++;
		}
	}
}

// ------------------------------------------------------------------------- //

void Display(char *FileName)
/*
 * Displays the file given.
 */
{
	/* if file ends in .ASC, assume that it is pipe codes */
	/* if file ends in .ANS, assume it is ANSI */
	/* if ends in something else, assume pipe codes */

	_INT16 FileType; /* 0 == ASCII, 1 == ANSI */
	_INT16 CurLine = 0, NumLines, cTemp;
	long MaxBytes;
	char *Lines[30];
	struct FileHeader FileHeader;

	if ((toupper(FileName[ strlen(FileName) - 3 ]) == 'A') &&
			(toupper(FileName[ strlen(FileName) - 2 ]) == 'N') &&
			(toupper(FileName[ strlen(FileName) - 1 ]) == 'S'))
		FileType = ANSIFILE;
	else
		FileType = ASCIIFILE;

	/* now display it according to the type of file it is */
	// open the file
	MyOpen(FileName, "r", &FileHeader);

	if (!FileHeader.fp) {
		/* rputs("Display: File not found!\n"); */
		return;
	}

	/* init mem */
	for (cTemp = 0; cTemp < 30; cTemp++) {
		Lines[cTemp] = malloc(255);
		CheckMem(Lines[cTemp]);
	}

	for (;;) {
		// if at end of file, stop
		/* get od_control.user_screen_length-3 lines if possible */
		for (cTemp = 0; cTemp < (od_control.user_screen_length-3) && cTemp < 30; cTemp++) {
			MaxBytes = FileHeader.lEnd - ftell(FileHeader.fp) + EXTRABYTES;
			if (MaxBytes > 254)
				MaxBytes = 254;

			if (ftell(FileHeader.fp) >= FileHeader.lEnd || feof(FileHeader.fp)) {
				break;
			}

			if (!fgets(Lines[cTemp], (_INT16) MaxBytes, FileHeader.fp))
				break;

			Lines[cTemp][(_INT16)(MaxBytes - EXTRABYTES + 1)] = 0;
		}
		NumLines = cTemp;

		/* display them all */
		for (CurLine = 0; CurLine < NumLines; CurLine++) {
			if (FileType == ANSIFILE)
				od_disp_emu(Lines[CurLine], TRUE);
			else  /* assume ASCII pipe codes */
				rputs(Lines[CurLine]);
		}

		/* pause if od_control.user_screen_length-3 lines */
		if (CurLine == (od_control.user_screen_length-3) && Door.AllowScreenPause) {
			rputs(ST_MORE);
			od_sleep(0);
			if (toupper(od_get_key(TRUE)) == 'N') {
				rputs("\r                       \r");
				break;
			}
			rputs("\r                       \r");

			CurLine = 0;
		}
		else if (Door.AllowScreenPause == FALSE)
			CurLine = 0;

		/* see if end of file, if so, exit */
		if (ftell(FileHeader.fp) >= FileHeader.lEnd || feof(FileHeader.fp))
			break;

		/* see if key hit */
		if (od_get_key(FALSE)) break;
	}

	fclose(FileHeader.fp);

	/* de-init mem */
	for (cTemp = 0; cTemp < 30; cTemp++)
		free(Lines[cTemp]);
}

// ------------------------------------------------------------------------- //

_INT16 YesNo(char *Query)
{
	/* show query */
	DisplayStr(Query);

	/* show Yes/no */
	// rputs(" |01[|09Yes|01/no]: ");
	DisplayStr(STR_YESNO);

	/* get user input */
	if (od_get_answer("YN\r\n") == 'N') {
		/* user says NO */
		DisplayStr("No\n");
		return NO;
	}
	else { /* user says YES */
		DisplayStr("Yes\n");
		return YES;
	}
}

_INT16 NoYes(char *Query)
{
	/* show query */
	DisplayStr(Query);

	/* show Yes/no */
	// rputs(" |01[yes/|09No|01]: ");
	DisplayStr(STR_NOYES);

	/* get user input */
	if (od_get_answer("YN\r\n") == 'Y') {
		/* user says YES */
		DisplayStr("Yes\n");
		return (YES);
	}
	else { /* user says NO */
		DisplayStr("No\n");
		return (NO);
	}
}

// ------------------------------------------------------------------------- //
void Door_ShowTitle(void)
{
	char szFileName[20];

	sprintf(szFileName, "Title %d", (_INT16) RANDOM(3));

	Door_SetScreenPause(FALSE);
	Help(szFileName, ST_MENUSHLP);
	Door_SetScreenPause(TRUE);
	rputs("|16|07");
	od_get_key(TRUE);
	od_clr_scr();
}
// ------------------------------------------------------------------------- //

void Door_Init(__BOOL Local)
/*
 * Called to initialize OpenDoors specific info (od_init namely) and
 * load the dropfile or prompt the user for local login.
 */
{
	if (Verbose) {
		DisplayStr("> Door_Init()\n");
		delay(500);
	}

	/* force opendoors to NOT show copyright message */
	od_control.od_nocopyright = TRUE;
	od_control.od_node = System.Node;
	od_control.od_before_exit = System_Close;
	//    od_registration_key = YOUR_KEY;

	/* If local, intialize user data before od_init is run */
	if (Local) {
		od_control.od_info_type = NO_DOOR_FILE;
		od_control.od_force_local = TRUE;
		od_control.user_ansi = TRUE;
		od_control.od_always_clear = TRUE;
		od_control.od_disable |= DIS_NAME_PROMPT;
		od_control.baud = 0;

		LocalLoad();
	}

	/* initialize opendoors ------------------------------------------------*/
	od_control.od_mps = INCLUDE_MPS;

	strcpy(od_registered_to, "Your Name");

	od_add_personality("TWOLINER", 1, 23, TwoLiner);
	od_control.od_default_personality = TwoLiner;

	// 12/23/2001 [au] no longer use time-slicer
	//    od_control.od_ker_exec = TSlicer_Update;

	od_control.od_cbefore_chat = BeforeChat;
	od_control.od_cafter_chat = AfterChat;

	od_control.od_chat_colour1 = 14;
	od_control.od_chat_colour2 = 7;

	od_control.od_no_file_func = NoDoorFileHandler;

	od_init();

#ifdef _WIN32
	/* All need for the console has passed */
	FreeConsole();
#endif


	Door.Initialized = TRUE;

	od_control.od_before_chat = "";
	od_control.od_after_chat = "";

	/* see if local using dropfile */
	if (System.Local == FALSE) {
		if (od_carrier() == FALSE) {
			System.Local = TRUE;
		}
	}

	if (SomeoneOnline()) {
		//  rputs("\nSomeone is currently playing the game on another node.\nPlease return in a few minutes.\n%P");
		Door.UserBooted = TRUE;
		rputs(ST_MAIN4);
		System_Close();
	}

	// otherwise, create semaphor
	CreateSemaphor();
}

// ------------------------------------------------------------------------- //

void Door_Close(void)
/*
 * Called to destroy anything created by Door_Init.
 */
{
	if (Door.UserBooted == FALSE)
		RemoveSemaphor();
}
