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
 * System functions
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <OpenDoor.h>
#include "platform.h"

#include "alliance.h"
#include "clansini.h"
#include "class.h"
#include "door.h"
#include "game.h"
#include "ibbs.h"
#include "items.h"
#include "language.h"
#include "maint.h"
#include "misc.h"
#include "mstrings.h"
#include "news.h"
#include "parsing.h"
#include "quests.h"
#include "readcfg.h"
#include "reg.h"
#include "scores.h"
#include "semfile.h"
#include "spells.h"
#include "structs.h"
#include "system.h"
#include "user.h"
#include "video.h"
#include "village.h"

struct system System = {0};
bool Verbose = false;

// ------------------------------------------------------------------------- //
static bool System_LockedOut(void)
{
	FILE *fp;
	char szLine[128], *pcCurrentPos;

	// read in file
	fp = fopen("lockout.txt", "r");
	if (!fp)
		return false;

	// while not end of file
	for (;;) {
		// read in line
		if (fgets(szLine, 128, fp) == NULL) {
			fclose(fp);
			break;
		}

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;

		ParseLine(pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;


		// else, compare names
		if (strcasecmp(pcCurrentPos, od_control.user_name) == 0) {
			fclose(fp);
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------------- //

noreturn void System_Error(char *szErrorMsg)
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */
{
#if defined(_WIN32)
	LogStr(szErrorMsg);
	char buffer[1000];
	_snprintf(buffer, 1000, "System Error: %s\n", szErrorMsg);
	RemovePipes(buffer, buffer);
	MessageBox(NULL, buffer, TEXT("System Error"), MB_OK |
			   MB_ICONERROR);
#else
	DisplayStr("|12System Error: |07");
	LogDisplayStr(szErrorMsg);
	delay(1000);
#endif
	System_Close();
}

// ------------------------------------------------------------------------- //

static void ShowHelp(void)
/*
 * Shows help screen for /? and /Help
 *
 */
{
	// help info using help parameter
	zputs(ST_HELPPARM);
}

static void ODCmdLineHandler(char *flag, char *val)
{
	char szString[128];

	bool primitive = !Config.Initialized;
	if (flag[0] == '-' || flag[0] == '/') {
		if (strcasecmp(&flag[1], "Recon") == 0) {
			if (!primitive) {
				if (Game.Data.InterBBS) {
					if (IBBS.Data.BBSID != ato16(val, "Recon", __func__) &&
							(ato16(val, "Recon", __func__) > 0 && ato16(val, "Recon", __func__) < MAX_IBBSNODES)) {
						snprintf(szString, sizeof(szString), "Sending recon to %d\n", atoi(val));
						LogDisplayStr(szString);
						IBBS_SendRecon(ato16(val, "Recon", __func__));
					}
					System_Close();
				}
				else
					System_Error(ST_IBBSONLY);
			}
			return;
		}
		else if (strcasecmp(&flag[1], "SendReset") == 0) {
			if (!primitive) {
				if (Game.Data.InterBBS) {
					if (IBBS.Data.BBSID != 1) {
						System_Error("Only the League Coordinator can send a reset.\n");
					}

					if (IBBS.Data.BBSID != atoi(val) &&
							(atoi(val) > 0 && atoi(val) < MAX_IBBSNODES)) {
						snprintf(szString, sizeof(szString), "Sending reset to %d\n", atoi(val));
						LogDisplayStr(szString);
						IBBS_SendReset(ato16(val, "Reset", __func__));
					}
					System_Close();
				}
				else
					System_Error(ST_IBBSONLY);
			}
			return;
		}
	}
	snprintf(szString, sizeof(szString), "|06Invalid parameter \"%s\" |07type |14CLANS /? |07for help\n", flag);
	zputs(szString);
	System_Close();
}

static BOOL ODCmdLineFlagHandler(const char *flag)
{
	char szString[128];
	bool primitive = !Config.Initialized;

	if (flag[0] == '-' || flag[0] == '/') {
		if (strcasecmp(&flag[1], "L") == 0 || strcasecmp(&flag[1], "Local") == 0) {
			if (!primitive)
				System.Local = true;
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "M") == 0) {
			if (!primitive) {
				Maintenance();
				System_Close();
			}
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "?") == 0 || strcasecmp(&flag[1], "Help") == 0) {
			ShowHelp();
			delay(3000);
			System_Close();
		}
		else if (strcasecmp(&flag[1], "T") == 0) {
			// No longer available, backward compatibility.
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "LIBBS") == 0) {
			if (primitive)
				System.LocalIBBS = true;
			else {
				if (Game.Data.InterBBS == false)
					System_Error(ST_IBBSONLY);
				if (Config.MailerType != MAIL_NONE)
					System_Error("Local IBBS with mailer type set");
			}
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "Users") == 0) {
			if (!primitive) {
				User_List();
				System_Close();
			}
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "NewNDX") == 0) {
			if (!primitive) {
				if (Game.Data.InterBBS) {
					if (IBBS.Data.BBSID != 1) {
						System_Error("Only the League Coordinator can update the world.ndx file.\n");
					}

					IBBS_DistributeNDX();
					System_Close();
				}
				else
					System_Error(ST_IBBSONLY);
			}
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "I") == 0) {
			if (!primitive) {
				// read in packets waiting
				IBBS_PacketIn();

				if (Game.Data.InterBBS == false)
					System_Error(ST_IBBSONLY);
				else
					System_Close();
			}
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "Recon") == 0) {
			// Requires an argument
			return FALSE;
		}
		else if (strcasecmp(&flag[1], "SendReset") == 0) {
			// Requires an argument
			return FALSE;
		}
		else if (strcasecmp(&flag[1], "Reset") == 0) {
			if (primitive)
				System_Error("To reset the game, please run RESET.EXE.\n");
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "Verbose") == 0) {
			if (primitive) {
				LogDisplayStr("|07Verbose |14ON\n");
				Verbose = true;
			}
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "Slop") == 0) {
			if (primitive) {
				Register();
				System_Close();
			}
			return TRUE;
		}
		else if (strcasecmp(&flag[1], "Exclusive") == 0) {
			if (primitive)
				RemoveSemaphor();
			return TRUE;
		}
		else if (flag[1] == 'S' || flag[1] == 's') {
			if (primitive) {
				od_control.od_use_socket = TRUE;
				od_control.od_open_handle = atoul(&flag[2], "Socket handle", __func__);
			}
			return TRUE;
		}
		else if (flag[1] == 'D' || flag[1] == 'd') {
			if (primitive)
				strlcpy(od_control.info_path, &flag[2], sizeof(od_control.info_path));
			return TRUE;
		}
		else if (flag[1] == 'N' || flag[1] == 'n') {
			if (primitive)
				System.Node = atou16(&flag[2], "Node", __func__);
			return TRUE;
		}
	}
	snprintf(szString, sizeof(szString), "|06Invalid parameter \"%s\" |07type |14CLANS /? |07for help\n", flag);
	zputs(szString);
	delay(3000);
	System_Close();
	// We shouldn't be able to get here...
	return true;
}

// ------------------------------------------------------------------------- //

void System_Maint(void)
{

	LogDisplayStr("* System_Maint()\n");

	// Update News
	unlink("yest.asc");
	rename("today.asc", "yest.asc");
	News_CreateTodayNews();

}

// ------------------------------------------------------------------------- //

/*
 * This is used to detect if something called from System_Close_AtExit()
 * has called exit().
 * 
 * System_Close_AtExit() toggles this bit, so the caller can clear the
 * bit and call System_Close_AtExit(). If the bit is cleared when
 * System_Close_AtExit() returns, it was called twice instead of just
 * once, and it can be assumed the second call was from the atexit()
 * handler.
 */
static bool RecurseCheck = false;
void System_Close_AtExit(void)
/*
 * purpose  Closes down the system, no matter WHERE it is called.
 *          Should be foolproof, but MUST NOT call exit()
 */
{
	RecurseCheck = !RecurseCheck;
	if (System.Initialized) {
		// This simply ensures this function is run ONLY ONCE!
		System.Initialized = false;

//#ifdef _DEBUG
//# define TRACEX(x) MessageBox (NULL, (x), TEXT("DEBUG"), MB_OK)
//#else
# define TRACEX(x)
//#endif

		TRACEX("DisplayScores");
		DisplayScores(true);

		TRACEX("Items_Close()");
		Items_Close();

		TRACEX("User_Close()");
		User_Close();

		TRACEX("Quests_Close()");
		Quests_Close();
		TRACEX("PClass_Close()");
		PClass_Close();
		TRACEX("Spells_Close()");
		Spells_Close();

		TRACEX("Door_Close()");
		Door_Close();

		TRACEX("Alliances_Close()");
		Alliances_Close();
		TRACEX("Village_Close()");
		Village_Close();
		TRACEX("Game_Close()");
		Game_Close();

		TRACEX("IBBS_Close()");
		if (Config.InterBBS)
			IBBS_Close();

		TRACEX("Config_Close()");
		Config_Close();

		TRACEX("ClansIni_Close()");
		ClansIni_Close();

		TRACEX("Language_Close()");
		Language_Close();

		TRACEX("Video_Close()");
		Video_Close();
	}
}

noreturn void System_Close(void)
/*
 * purpose  Closes down the system, no matter WHERE it is called.
 *          Should be foolproof.
 */
{
	if (System.Initialized) {
		RecurseCheck = false;
		System_Close_AtExit();

		if (RecurseCheck) {
			TRACEX("Door_Initialized()");
			if (Door_Initialized()) {
				od_exit(0, false);
			}
			else {
				exit(0);
			}
		}
		else {
			puts("exit() called from System_Close_AtExit()\r");
			abort();
		}
	}
	abort();
}

void System_Init(void)
/*
 * Initializes whole system.
 */
{
	time_t now = time(NULL);
	struct tm tm = *localtime(&now);
	size_t iTemp;
#ifdef __unix__
	char *pszResolvedPath;
#endif

	atexit(System_Close_AtExit);
	System.Initialized = true;
	System.LocalIBBS = false;

	// Get directory from commandline
#ifdef _WIN32
	GetModuleFileName(NULL, System.szMainDir, sizeof(System.szMainDir));
#else
	strlcpy(System.szMainDir, _argv[0], sizeof(System.szMainDir));
#endif
	for (iTemp = strlen(System.szMainDir); iTemp > 0; iTemp--) {
		if (System.szMainDir[iTemp] == '\\' || System.szMainDir[iTemp] == '/')
			break;
	}
	++iTemp;
	System.szMainDir[iTemp] = 0;
#ifdef __unix__
	pszResolvedPath=fullpath(NULL,System.szMainDir,sizeof(System.szMainDir));
	if (pszResolvedPath)
		strlcpy(System.szMainDir, pszResolvedPath, sizeof(System.szMainDir));
	else
		System.szMainDir[0] = 0;
	free(pszResolvedPath);
#endif

	// 12/23/2001 [au] getting rid of time-slicer support
	// Time slicer init
	// get_os();

	// initialize date
	if (tm.tm_mon > 11 || tm.tm_mon < 0)
		System_Error("Invalid Month");
	else if (tm.tm_mday > 31 || tm.tm_mday < 1)
		System_Error("Invalid Day");
	else if (tm.tm_year > 8099 || tm.tm_year < 125)
		System_Error("Invalid Year");
	snprintf(System.szTodaysDate, sizeof(System.szTodaysDate), "%02d/%02d/%4d", tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900);

	srand((unsigned)time(NULL));

	// initialize screen data
	Video_Init();

	DisplayStr("|14The Clans |15" VERSION " |07copyright 1997-1998 Allen Ussher\n\n");

	ClansIni_Init();

	Language_Init(IniFile.pszLanguage);

	od_control.od_cmd_line_flag_handler = ODCmdLineFlagHandler;
	od_control.od_cmd_line_handler = ODCmdLineHandler;
	od_control.od_cmd_line_help_func = ShowHelp;
#ifdef ODPLAT_WIN32
	od_parse_cmd_line(_lpCmdLine);
#else
	od_parse_cmd_line(_argc, _argv);
#endif
	if (od_control.od_force_local)
		System.Local = true;
	if (System.Node == 0 && od_control.od_node)
		System.Node = od_control.od_node;

	if (Verbose) {
		LogDisplayStr("> Config_Init()\n");
		delay(500);
	}

	if (!Config_Init(System.Node, NULL))
		System_Error("Config file not found\n");

	/* use log */
	if (Config.UseLog) {
		od_control.od_logfile = INCLUDE_LOGFILE;
		snprintf(od_control.od_logfile_name, sizeof(od_control.od_logfile_name), "clans%u.log", (unsigned)System.Node);
	}
	if (Config.pszInfoPath)
		strlcpy(od_control.info_path, Config.pszInfoPath, sizeof(od_control.info_path));
	od_control.od_no_fossil = Config.NoFossil;
	od_control.od_com_irq = Config.ComIRQ;
	if (System.LocalIBBS)
		Config.MailerType = MAIL_NONE;

	// init stuff
	System.Local = false;

	if (System.Node == 0)
		WaitSemaphor(System.Node);
	else {
		if (!CreateSemaphor(System.Node)) {
			Door.UserBooted = true;
			Door_Init(System.Local);
		}
	}

	// Game Specific data (village, game.dat)
	Game_Init();
	Village_Init();
	Alliances_Init();

	if (Game.Data.InterBBS) {
		IBBS_Init();

		// update recons + resets
		IBBS_UpdateRecon();

		if (IBBS.Data.BBSID == 1)
			IBBS_UpdateReset();
	}

	// parse command line here
#ifdef ODPLAT_WIN32
	od_parse_cmd_line(_lpCmdLine);
#else
	od_parse_cmd_line(_argc, _argv);
#endif
	if (od_control.od_force_local)
		System.Local = true;

	Door_Init(System.Local);

	if (System_LockedOut()) {
		rputs("Sorry, you have been locked out of this door.\n%P");
		System_Close();
	}


	// Clans specific data is loaded here (spells)
	Spells_Init();
	PClass_Init();

	Quests_Init();

	// read in packets waiting
	if (Config.InterBBS)
		IBBS_PacketIn();
}
