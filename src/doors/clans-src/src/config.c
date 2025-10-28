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
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "console.h"
#include "defines.h"
#include "parsing.h"
#include "readcfg.h"
#include "structs.h"
#include "video.h"

#define MAX_OPTION      14

/* reset types */
#define RESET_LOCAL     1   // normal reset -- local, NO ibbs junk
#define RESET_JOINIBBS  2   // joining an IBBS league game
#define RESET_LC        3   // leaguewide reset by LC

#define FIGHTSPERDAY    12
#define CLANCOMBATADAY  5


static void ConfigMenu(void);

static void EditOption(int16_t WhichOption);
static void EditNodeOption(int16_t WhichOption);

static struct NodeData *setCurrentNode(int node);

void System_Error(char *szErrorMsg);

static void UpdateOption(char Option);
static void UpdateNodeOption(char Option);

struct NodeData *nodes;
struct NodeData *currNode;
static bool ConfigUseLog;

typedef void * SCREENSTATE;

int main(void)
{
	Video_Init();
	if (!Config_Init(0, setCurrentNode)) {
		strcpy(Config.szNetmailDir, "/bbs/netmail/");
		Config.BBSID = 1;
	}
	if (!Config.NumInboundDirs)
		AddInboundDir("/bbs/inbound");
	if (Config.NumInboundDirs < 2)
		AddInboundDir("");

	// load up config
	ConfigMenu();
	Video_Close();
	return (0);
}

static const char *
MailerTypeName(int16_t t)
{
	switch (t) {
		case MAIL_BINKLEY:
			return "Binkley";
		case MAIL_OTHER:
			return "Attach";
		case MAIL_NONE:
			return "None";
	}
	return "Unknown";
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
	fputs("\n", f);
	fputs("# [InterBBSData] ------------------------------------------------------------\n", f);
	fprintf(f, "%sInterBBS\n", Config.InterBBS ? "" : "#");
	fprintf(f, "%sBBSID           %d\n", Config.InterBBS ? "" : "#", Config.BBSID);
	fprintf(f, "%sNetmailDir      %s\n", Config.InterBBS ? "" : "#", Config.szNetmailDir);
	for (int i = 0; i < Config.NumInboundDirs; i++) {
		if (Config.szInboundDirs[i][0])
			fprintf(f, "%sInboundDir      %s\n", Config.InterBBS ? "" : "#", Config.szInboundDirs[i]);
	}
	fprintf(f, "%sMailerType      %s\n", Config.InterBBS ? "" : "#", MailerTypeName(Config.MailerType));
	fprintf(f, "%sOutputSemaphore %s\n", Config.InterBBS ? "" : "#", Config.szOutputSem);
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
	char cInput;
	char OldOption = -1;
	bool Quit = false, DidReset = false;

	/* clear all */
	clrscr();
	ShowTextCursor(false);

	/* show options */
	qputs(" |03Config for The Clans " VERSION, 0, 0);
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
	xputs(" Alternative Inbound Directory", 0, 11);
	xputs(" Mailer Type", 0, 12);
	xputs(" Output Semaphore Filename", 0, 13);
	xputs(" Configure Node", 0, 14);
	xputs(" Save Config", 0, 15);
	xputs(" Abort Config", 0, 16);
	qputs("|01--------------------------------------------------------------------------- |09-",0,17);

	qputs("|09 Press the up and down keys to navigate.", 0, 19);

	/* init defaults */

	ColorArea(0,2+MAX_OPTION, 39, 2+MAX_OPTION, 7);
	gotoxy(2, CurOption+3);

	/* dehilight all options */
	ColorArea(39, 2, 76, 16, 15);    // choices on the right side

	ColorArea(0, 2, 39, MAX_OPTION+2, 7);

	/* dehilight options which can't be activated */
	if (!Config.InterBBS) {
		ColorArea(0, 8, 76, 13,  8);
	}

	textattr(15);
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
	UpdateOption(11);
	UpdateOption(12);

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
		if (cInput == 0 || cInput == (char)0xE0 || strchr("wkahsjdl", cInput)) {
			if (cInput == 0 || cInput == (char)0xE0)
				cInput = getch();
			switch (cInput) {
				case 'w':
				case 'k':
				case 'a':
				case 'h':
				case K_UP   :
				case K_LEFT :
					if (CurOption == 0)
						CurOption = MAX_OPTION;
					else
						CurOption--;
					if ((CurOption == (MAX_OPTION-3)) && (!Config.InterBBS))
						CurOption -= 6;
					break;
				case 's':
				case 'j':
				case 'd':
				case 'l':
				case K_DOWN :
				case K_RIGHT:
					if (CurOption == MAX_OPTION)
						CurOption = 0;
					else
						CurOption++;
					if ((CurOption == (MAX_OPTION-8)) && (!Config.InterBBS))
						CurOption += 6;
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
			if (Config.InterBBS == false && CurOption >= (MAX_OPTION-8) &&
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

	textattr(7);
	clrscr();

	if (DidReset) {
		qputs("|09> |07Your game has been configured.", 0,0);
		qputs("|09> |03You may now type |11RESET |03to complete the setup.", 0,1);
	}
	else
		qputs("|09> |07The game was not configured.  No changes were made.", 0,0);
	textattr(7);
	ShowTextCursor(true);
	gotoxy(0,3);
}

static void NodeMenu(void)
{
	char CurOption = 0;
	char  cInput;
	char OldOption = -1;
	bool Quit = false;
	char str[80];
	const int last = 4;

	/* clear all */
	clrscr();

	/* show options */
	snprintf(str, sizeof(str), " |03Node %d Config for The Clans " VERSION, currNode->number);
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

	textattr(15);
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
		if (cInput == 0 || cInput == (char)0xE0 || strchr("wkahsjdl", cInput)) {
			if (cInput == 0 || cInput == (char)0xE0)
				cInput = getch();
			switch (cInput) {
				case 'w':
				case 'k':
				case 'a':
				case 'h':
				case K_UP   :
				case K_LEFT :
					if (CurOption == 0)
						CurOption = last;
					else
						CurOption--;
					break;
				case 's':
				case 'j':
				case 'd':
				case 'l':
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

static void EditOption(int16_t WhichOption)
{
	/* save screen */
	SCREENSTATE screen_state;
	screen_state = save_screen();
	char szString[PATH_SIZE];

	textattr(15);
	switch (WhichOption) {
		case 0 :    /* sysop name started */
			gotoxy(40, 2);
			DosGetStr(Config.szSysopName, sizeof(Config.szSysopName) - 1, false);
			break;
		case 1 :    /* BBS name */
			gotoxy(40, 3);
			DosGetStr(Config.szBBSName, sizeof(Config.szBBSName) - 1, false);
			break;
		case 2 :    /* Use Log */
			ConfigUseLog = !ConfigUseLog;
			break;
		case 3 :    /* ANSI Score File */
			gotoxy(40, 5);
			DosGetStr(Config.szScoreFile[1], 39, false);
			break;
		case 4 :    /* ASCII Score File */
			gotoxy(40, 6);
			DosGetStr(Config.szScoreFile[0], 39, false);
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
			DosGetStr(Config.szNetmailDir, 39, false);
			break;
		case 8 :    /* Inbound Dir */
			gotoxy(40, 10);
			strlcpy(szString, Config.szInboundDirs[0], sizeof(szString));
			DosGetStr(szString, 39, false);
			free(Config.szInboundDirs[0]);
			Config.szInboundDirs[0] = strdup(szString);
			CheckMem(Config.szInboundDirs[0]);
			break;
		case 9 :    /* Alt. Inbound Dir */
			gotoxy(40, 11);
			strlcpy(szString, Config.szInboundDirs[1], sizeof(szString));
			DosGetStr(szString, 39, false);
			free(Config.szInboundDirs[1]);
			Config.szInboundDirs[1] = strdup(szString);
			CheckMem(Config.szInboundDirs[1]);
			break;
		case 10 :    /* Mailer type */
			switch (Config.MailerType) {
				case MAIL_BINKLEY:
					Config.MailerType = MAIL_OTHER;
					break;
				case MAIL_OTHER:
					Config.MailerType = MAIL_NONE;
					break;
				case MAIL_NONE:
				default:
					Config.MailerType = MAIL_BINKLEY;
					break;
			}
			break;
		case 11 :   /* Outbound Semaphore */
			gotoxy(40, 13);
			DosGetStr(Config.szOutputSem, 39, false);
			break;
		case 12:    /* Configure node */
			gotoxy(40, 14);
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
	textattr(15);
	switch (WhichOption) {
		case 0 :    /* dropfile directory */
			gotoxy(40, 2);
			DosGetStr(currNode->dropDir, 39, false);
			break;
		case 1 :    /* use fossil */
			currNode->fossil = !currNode->fossil;
			break;
		case 2 :    /* Address */
			gotoxy(40, 4);
			if (currNode->addr != UINTPTR_MAX)
				snprintf(addrstr, sizeof(addrstr), "0x%" PRIXPTR, currNode->addr);
			else
				strlcpy(addrstr, "Default", sizeof(addrstr));
			DosGetStr(addrstr, sizeof(addrstr) - 1, false);
			currNode->addr = strtoull(addrstr, NULL, 0);
			break;
		case 3 :    /* IRQ */
			gotoxy(40, 5);
			currNode->irq = DosGetLong("", currNode->irq, 2048);
			break;
	}
}

static struct NodeData *
setCurrentNode(int node)
{
	size_t nodeCount = 0;

	currNode = NULL;
	if (node < 1)
		return NULL;
	for (struct NodeData *nd = nodes; nd && nd->number > 0; nd++) {
		nodeCount++;
		if (nd->number == node) {
			currNode = nd;
			return currNode;
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
	return currNode;
}

static void UpdateOption(char Option)
{
	char szString[50];
	switch (Option) {
		case 0:
			textattr(15);
			xputs("                                        ", 40, 2);
			xputs(Config.szSysopName, 40, 2);
			break;
		case 1:
			textattr(15);
			xputs("                                        ", 40, 3);
			xputs(Config.szBBSName, 40, 3);
			break;
		case 2:
			textattr(15);
			xputs(ConfigUseLog ? "Yes" : "No ", 40, 4);
			break;
		case 3:
			textattr(15);
			xputs("                                        ", 40, 5);
			xputs(Config.szScoreFile[1], 40, 5);
			break;
		case 4:
			textattr(15);
			xputs("                                        ", 40, 6);
			xputs(Config.szScoreFile[0], 40, 6);
			break;
		case 5:
			textattr(15);
			xputs(Config.InterBBS ? "Yes" : "No ", 40, 7);
			ColorArea(0, 8, 39, 13,  Config.InterBBS ? 7 : 8);
			ColorArea(40, 8, 79, 13,  Config.InterBBS ? 15 : 8);
			break;
		case 6:
			snprintf(szString, sizeof(szString), "%d\n", Config.BBSID);
			textattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 8);
			xputs(szString, 40, 8);
			break;
		case 7:
			textattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 9);
			xputs(Config.szNetmailDir, 40, 9);
			break;
		case 8:
			textattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 10);
			xputs(Config.szInboundDirs[0], 40, 10);
			break;
		case 9:
			textattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 11);
			xputs(Config.szInboundDirs[1], 40, 11);
			break;
		case 10:
			textattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 12);
			xputs(MailerTypeName(Config.MailerType), 40, 12);
			break;
		case 11:
			textattr(Config.InterBBS ? 15 : 8);
			xputs("                                        ", 40, 13);
			xputs(Config.szOutputSem, 40, 13);
			break;
	}
}

static void UpdateNodeOption(char Option)
{
	char szString[50];
	switch (Option) {
		case 0:
			textattr(15);
			xputs("                                        ", 40, 2);
			xputs(currNode->dropDir, 40, 2);
			break;
		case 1:
			textattr(15);
			xputs(currNode->fossil ? "Yes" : "No ", 40, 3);
			break;
		case 2:
			textattr(15);
			xputs("                                        ", 40, 4);
			if (currNode->addr == UINTPTR_MAX)
				strlcpy(szString, "Default", sizeof(szString));
			else
				snprintf(szString, sizeof(szString), "0x%" PRIXPTR, currNode->addr);
			xputs(szString, 40, 4);
			break;
		case 3:
			textattr(15);
			xputs("                                        ", 40, 5);
			if (currNode->irq < 0)
				strlcpy(szString, "Default", sizeof(szString));
			else
				snprintf(szString, sizeof(szString), "%d", currNode->irq);
			xputs(szString, 40, 5);
			break;
	}
}
