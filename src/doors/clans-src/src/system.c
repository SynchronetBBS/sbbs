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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __unix__
# include <sys/param.h>
#else
# ifndef _WIN32
#  include <alloc.h>
#  include <dos.h>
# endif
# include <share.h>
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include <OpenDoor.h>

#include "clansini.h"
#include "class.h"
#include "door.h"
#include "game.h"
#include "ibbs.h"
#include "items.h"
#include "k_config.h"
#include "language.h"
#include "maint.h"
#include "mstrings.h"
#include "news.h"
#include "parsing.h"
#include "quests.h"
#include "reg.h"
#include "scores.h"
#include "spells.h"
#include "structs.h"
#include "system.h"
#include "tasker.h"
#include "user.h"
#include "video.h"
#include "village.h"

struct config *Config;
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
		if (fgets(szLine, 128, fp) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;

		ParseLine(pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;


		// else, compare names
		if (stricmp(pcCurrentPos, od_control.user_name) == 0) {
			fclose(fp);
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------------- //

void System_Error(char *szErrorMsg)
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */
{
#ifdef __unix__
	printf("System Error: %s\n",szErrorMsg);
	delay(1000);
	System_Close();
#elif defined(_WIN32)
	char buffer[1000];
	_snprintf(buffer, 1000, "System Error: %s\n", szErrorMsg);
	MessageBox(NULL, buffer, TEXT("System Error"), MB_OK |
			   MB_ICONERROR);
	System_Close();
#else
	DisplayStr("|12System Error: |07");
	DisplayStr(szErrorMsg);
	delay(1000);
	System_Close();
#endif
}

// ------------------------------------------------------------------------- //

void Config_Init(void)
/*
 * Loads data from .CFG file into Config->
 *
 */
{
	FILE *fpConfigFile;
	char szConfigName[40], szConfigLine[255];
	char *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	int16_t iKeyWord, iCurrentNode = 1;

	if (Verbose) {
		DisplayStr("> Config_Init()\n");
		delay(500);
	}

	Config = calloc(1, sizeof(struct config));
	CheckMem(Config);

	// --- Set defaults
	strlcpy(szConfigName, "clans.cfg", sizeof(szConfigName));
	Config->szSysopName[0] = 0;
	Config->szBBSName[0] = 0;
	strlcpy(Config->szScoreFile[0], "scores.asc", sizeof(Config->szScoreFile[0]));
	strlcpy(Config->szScoreFile[1], "scores.ans", sizeof(Config->szScoreFile[1]));
	Config->szRegcode[0] = 0;
	Config->BBSID = 0;

	System.InterBBS = false;
	Config->InterBBS = false;

	fpConfigFile = _fsopen(szConfigName, "rt", SH_DENYWR);
	if (!fpConfigFile) {
		/* file not found! error */
		System_Error("Config file not found\n");
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
						strlcpy(Config->szSysopName, pcCurrentPos, sizeof(Config->szSysopName));
						break;
					case 1 :  /* bbsname */
						strlcpy(Config->szBBSName, pcCurrentPos, sizeof(Config->szBBSName));
						break;
					case 2 :  /* use log? */
						if (stricmp(pcCurrentPos, "Yes") == 0) {
							/* use log */
							od_control.od_logfile = INCLUDE_LOGFILE;
							snprintf(od_control.od_logfile_name, sizeof(od_control.od_logfile_name), "clans%d.log", System.Node);
						}
						break;
					case 3 :  /* ansi file */
						strlcpy(Config->szScoreFile[1], pcCurrentPos, sizeof(Config->szScoreFile[1]));
						break;
					case 4 :  /* ascii file */
						strlcpy(Config->szScoreFile[0], pcCurrentPos, sizeof(Config->szScoreFile[0]));
						break;
					case 5 :  /* node = ? */
						iCurrentNode = atoi(pcCurrentPos);
						break;
					case 6 :  /* dropdirectory = ? */
						if (System.Node == iCurrentNode) {
							strlcpy(od_control.info_path, pcCurrentPos, sizeof(od_control.info_path));
						}
						break;
					case 7 :  /* usefossil */
						if (System.Node == iCurrentNode) {
							if (stricmp(pcCurrentPos, "No") == 0) {
								/* do not use fossil */
								od_control.od_no_fossil = true;
							}
						}
						break;
					case 8 :  /* serial port addr */
						if (System.Node == iCurrentNode) {
							//printf("Not yet used\n");
						}
						break;
					case 9 :  /* serial port irq */
						if (System.Node == iCurrentNode) {
							if (stricmp(pcCurrentPos, "Default") != 0)
								od_control.od_com_irq = atoi(pcCurrentPos);
						}
						break;
					case 10 : /* BBS Id */
						Config->BBSID = atoi(pcCurrentPos);
						break;
					case 11 : /* netmail dir */
						strlcpy(Config->szNetmailDir, pcCurrentPos, sizeof(Config->szNetmailDir));

						/* remove '\' if last char is it */
						if (Config->szNetmailDir [ strlen(Config->szNetmailDir) - 1] == '\\' || Config->szNetmailDir [strlen(Config->szNetmailDir) - 1] == '/')
							Config->szNetmailDir [ strlen(Config->szNetmailDir) - 1] = 0;
						break;
					case 12 : /* inbound dir */
						strlcpy(Config->szInboundDir, pcCurrentPos, sizeof(Config->szInboundDir));

						/* add '\' if last char is not it */
						if (Config->szInboundDir [ strlen(Config->szInboundDir) - 1] != '\\' &&  Config->szInboundDir [strlen(Config->szInboundDir) - 1] != '/')
							strlcat(Config->szInboundDir, "/", sizeof(Config->szInboundDir));
						break;
					case 13 : /* mailer type */
						if (stricmp(pcCurrentPos, "BINKLEY") == 0)
							Config->MailerType = MAIL_BINKLEY;
						else
							Config->MailerType = MAIL_OTHER;
						break;
					case 14 : /* in a league? */
						System.InterBBS = true;
						Config->InterBBS = true;
						break;
					case 15 : /* regcode */
						if (*pcCurrentPos)
							strlcpy(Config->szRegcode, pcCurrentPos, sizeof(Config->szRegcode));
						break;
				}
			}
		}
	}

	fclose(fpConfigFile);

}

static void Config_Close(void)
/*
 * Shuts down config's mem.
 *
 */
{
	free(Config);
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

#if defined(__unix__)
static char * fullpath(char *target, const char *path, size_t size)
{
	char    *out;
	char    *p;

	if (target==NULL)  {
		if ((target=malloc(PATH_MAX+1))==NULL) {
			return(NULL);
		}
	}
	out=target;
	*out=0;

	if (*path != '/')  {
		p=getcwd(target,size);
		if (p==NULL || strlen(p)+strlen(path)>=size)
			return(NULL);
		out=strrchr(target,'\0');
		*(out++)='/';
		*out=0;
		out--;
	}
	strlcat(target, path, size);

	for (; *out; out++)  {
		while (*out=='/')  {
			if (*(out+1)=='/')
				memmove(out,out+1,strlen(out));
			else if (*(out+1)=='.' && (*(out+2)=='/' || *(out+2)==0))
				memmove(out,out+2,strlen(out)-1);
			else if (*(out+1)=='.' && *(out+2)=='.' && (*(out+3)=='/' || *(out+3)==0))  {
				*out=0;
				p=strrchr(target,'/');
				memmove(p,out+3,strlen(out+3)+1);
				out=p;
			}
			else  {
				out++;
			}
		}
	}
	return(target);
}
#endif

void ODCmdLineHandler(char *flag, char *val)
{
	char szString[128];

	bool primitive = Config == NULL;
	if (flag[0] == '-' || flag[0] == '/') {
		if (stricmp(&flag[1], "Recon") == 0) {
			if (!primitive) {
				if (Game.Data->InterBBS) {
					if (IBBS.Data->BBSID != atoi(val) &&
							(atoi(val) > 0 && atoi(val) < MAX_IBBSNODES)) {
						snprintf(szString, sizeof(szString), "Sending recon to %d\n", atoi(val));
						DisplayStr(szString);
						IBBS_SendRecon(atoi(val));
					}
					System_Close();
				}
				else
					System_Error(ST_IBBSONLY);
			}
			return;
		}
		else if (stricmp(&flag[1], "SendReset") == 0) {
			if (!primitive) {
				if (Game.Data->InterBBS) {
					if (IBBS.Data->BBSID != 1) {
						System_Error("Only the League Coordinator can send a reset.\n");
					}

					/* snprintf(szString, sizeof(szString), "trying to send reset to %d\n", atoi(val));
					DisplayStr(szString);
					*/

					if (IBBS.Data->BBSID != atoi(val) &&
							(atoi(val) > 0 && atoi(val) < MAX_IBBSNODES)) {
						snprintf(szString, sizeof(szString), "Sending reset to %d\n", atoi(val));
						DisplayStr(szString);
						IBBS_SendReset(atoi(val));
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

BOOL ODCmdLineFlagHandler(const char *flag)
{
	char szString[128];
	bool primitive = Config == NULL;

	if (flag[0] == '-' || flag[0] == '/') {
		if (stricmp(&flag[1], "L") == 0 || stricmp(&flag[1], "Local") == 0) {
			if (!primitive)
				System.Local = true;
			return TRUE;
		}
		else if (stricmp(&flag[1], "M") == 0) {
			if (!primitive) {
				Maintenance();
				System_Close();
			}
			return TRUE;
		}
		else if (stricmp(&flag[1], "?") == 0 || stricmp(&flag[1], "Help") == 0) {
			ShowHelp();
			delay(3000);
			exit(0);
		}
		else if (stricmp(&flag[1], "T") == 0) {
			// No longer available, backward compatibility.
			return TRUE;
		}
		else if (stricmp(&flag[1], "LIBBS") == 0) {
			if (primitive)
				System.LocalIBBS = true;
			else {
				if (Game.Data->InterBBS == false)
					System_Error(ST_IBBSONLY);
			}
			return TRUE;
		}
		else if (stricmp(&flag[1], "Users") == 0) {
			if (!primitive) {
				User_List();
				System_Close();
			}
			return TRUE;
		}
		else if (stricmp(&flag[1], "NewNDX") == 0) {
			if (!primitive) {
				if (Game.Data->InterBBS) {
					if (IBBS.Data->BBSID != 1) {
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
		else if (stricmp(&flag[1], "I") == 0) {
			if (!primitive) {
				// read in packets waiting
				IBBS_PacketIn();

				if (Game.Data->InterBBS == false)
					System_Error(ST_IBBSONLY);
				else
					System_Close();
			}
			return TRUE;
		}
		else if (stricmp(&flag[1], "Recon") == 0) {
			// Requires an argument
			return FALSE;
		}
		else if (stricmp(&flag[1], "SendReset") == 0) {
			// Requires an argument
			return FALSE;
		}
		else if (stricmp(&flag[1], "Reset") == 0) {
			if (primitive)
				System_Error("To reset the game, please run RESET.EXE.\n");
			return TRUE;
		}
		else if (stricmp(&flag[1], "Verbose") == 0) {
			if (primitive) {
				DisplayStr("|07Verbose |14ON\n");
				Verbose = true;
			}
			return TRUE;
		}
		else if (stricmp(&flag[1], "Slop") == 0) {
			if (primitive) {
				Register();
				System_Close();
			}
			return TRUE;
		}
		else if (flag[1] == 'S') {
			if (primitive) {
				od_control.od_use_socket = TRUE;
				od_control.od_open_handle = atoi(&flag[2]);
			}
			return TRUE;
		}
		else if (flag[1] == 'D') {
			if (primitive)
				strlcpy(od_control.info_path, &flag[2], sizeof(od_control.info_path));
			return TRUE;
		}
		else if (flag[1] == 'N') {
			if (primitive)
				System.Node = atoi(&flag[2]);
			return TRUE;
		}
	}
	snprintf(szString, sizeof(szString), "|06Invalid parameter \"%s\" |07type |14CLANS /? |07for help\n", flag);
	zputs(szString);
	delay(3000);
	exit(0);
}

// ------------------------------------------------------------------------- //

void System_Maint(void)
{

	DisplayStr("* System_Maint()\n");

	// Update News
	unlink("yest.asc");
	rename("today.asc", "yest.asc");
	News_CreateTodayNews();

}

// ------------------------------------------------------------------------- //

void System_Close(void)
/*
 * purpose  Closes down the system, no matter WHERE it is called.
 *          Should be foolproof.
 */
{
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

#ifdef PRELAB
		printf("post data -- mem left = %lu\n", farcoreleft());
#endif

		TRACEX("Door_Close()");
		Door_Close();

		TRACEX("Village_Close()");
		Village_Close();
		TRACEX("Game_Close()");
		Game_Close();

		TRACEX("IBBS_Close()");
		if (System.InterBBS)
			IBBS_Close();

		TRACEX("Config_Close()");
		Config_Close();

		TRACEX("ClansIni_Close()");
		ClansIni_Close();

		TRACEX("Language_Close()");
		Language_Close();

#ifdef PRELAB
		printf("Clans End   -- mem left = %lu\n", farcoreleft());
#endif

		TRACEX("Door_Initialized()");
		if (Door_Initialized()) {
			od_exit(0, false);
		}
		else {
			exit(0);
		}
	}
}

void System_Init(void)
/*
 * Initializes whole system.
 */
{
	SYSTEMTIME system_time;
	int16_t iTemp;
#ifdef __unix__
	char *pszResolvedPath;
#endif

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

#ifdef PRELAB
	printf("Clans Start -- mem left = %lu\n", farcoreleft());
#endif

	// 12/23/2001 [au] getting rid of time-slicer support
	// Time slicer init
	// get_os();

	// initialize date
	GetSystemTime(&system_time);
	snprintf(System.szTodaysDate, sizeof(System.szTodaysDate), "%02d/%02d/%4d", system_time.wMonth, system_time.wDay, system_time.wYear);

#ifndef _WIN32
	randomize();
#else
	srand((unsigned)time(NULL));
#endif

	// initialize screen data
	Video_Init();

	DisplayStr("|14The Clans |15" VERSION " |07copyright 1997-1998 Allen Ussher\n\n");

	ClansIni_Init();

	Language_Init(IniFile.pszLanguage);

	od_control.od_cmd_line_flag_handler = ODCmdLineFlagHandler;
	od_control.od_cmd_line_handler = ODCmdLineHandler;
#ifdef ODPLAT_WIN32
	od_parse_cmd_line(_lpCmdLine);
#else
	od_parse_cmd_line(_argc, _argv);
#endif
	if (System.Node == 0 && od_control.od_node)
		System.Node = od_control.od_node;

	Config_Init();

	// init stuff
	System.Local = false;

	// Game Specific data (village, game.dat)
	Game_Init();

	Village_Init();

	if (Game.Data->InterBBS) {
		IBBS_Init();

		// update recons + resets
		IBBS_UpdateRecon();

		if (IBBS.Data->BBSID == 1)
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
		od_exit(0, false);
	}


#ifdef PRELAB
	printf("pre data -- mem left = %lu\n", farcoreleft());
#endif

	// Clans specific data is loaded here (spells)
	Spells_Init();
	PClass_Init();

	Quests_Init();

	// read in packets waiting
	if (System.InterBBS)
		IBBS_PacketIn();
}
