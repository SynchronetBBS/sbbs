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

#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#include "unix_wrappers.h"
#include <sys/param.h>
#else
#ifndef _WIN32
# include <alloc.h>
# include <dos.h>
#else
# include <malloc.h>
#endif
#include <share.h>
#endif

#include <time.h>
#include <string.h>
#include <ctype.h>

#include "structs.h"
#include "k_config.h"
#include "k_comman.h"
// #include "tasker.h"

#include <OpenDoor.h>
#include "parsing.h"
#include "ibbs.h"
#include "system.h"
// #include "tslicer.h"
#include "language.h"
#include "door.h"
#include "mstrings.h"
#include "maint.h"
#include "video.h"
#include "spells.h"
#include "village.h"
#include "class.h"
#include "game.h"
#include "clansini.h"
#include "tasker.h"
#include "items.h"
#include "quests.h"
#include "news.h"
#include "user.h"
#include "reg.h"
#include "scores.h"

struct config *Config;
struct system System;
extern struct Language *Language;
extern struct IniFile IniFile;
extern struct ibbs IBBS;
extern struct game Game;
BOOL Verbose = FALSE;

// ------------------------------------------------------------------------- //
BOOL System_LockedOut(void)
{
	FILE *fp;
	char szLine[128], *pcCurrentPos;

	// read in file
	fp = fopen("lockout.txt", "r");
	if (!fp)
		return FALSE;

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
			return TRUE;
		}
	}
	return FALSE;
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
	_INT16 iKeyWord, iCurrentNode = 1;

	if (Verbose) {
		DisplayStr("> Config_Init()\n");
		delay(500);
	}

	Config = malloc(sizeof(struct config));
	CheckMem(Config);

	// --- Set defaults
	strcpy(szConfigName, "clans.cfg");
	Config->szSysopName[0] = 0;
	Config->szBBSName[0] = 0;
	strcpy(Config->szScoreFile[0], "scores.asc");
	strcpy(Config->szScoreFile[1], "scores.ans");
	Config->szRegcode[0] = 0;
	Config->BBSID = 0;

	System.InterBBS = FALSE;
	Config->InterBBS = FALSE;

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
						strcpy(Config->szSysopName, pcCurrentPos);
						break;
					case 1 :  /* bbsname */
						strcpy(Config->szBBSName, pcCurrentPos);
						break;
					case 2 :  /* use log? */
						if (stricmp(pcCurrentPos, "Yes") == 0) {
							/* use log */
							od_control.od_logfile = INCLUDE_LOGFILE;
							sprintf(od_control.od_logfile_name, "clans%d.log", System.Node);
						}
						break;
					case 3 :  /* ansi file */
						strcpy(Config->szScoreFile[1], pcCurrentPos);
						break;
					case 4 :  /* ascii file */
						strcpy(Config->szScoreFile[0], pcCurrentPos);
						break;
					case 5 :  /* node = ? */
						iCurrentNode = atoi(pcCurrentPos);
						break;
					case 6 :  /* dropdirectory = ? */
						if (System.Node == iCurrentNode) {
							strcpy(od_control.info_path, pcCurrentPos);
						}
						break;
					case 7 :  /* usefossil */
						if (System.Node == iCurrentNode) {
							if (stricmp(pcCurrentPos, "No") == 0) {
								/* do not use fossil */
								od_control.od_no_fossil = TRUE;
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
						strcpy(Config->szNetmailDir, pcCurrentPos);

						/* remove '\' if last char is it */
						if (Config->szNetmailDir [ strlen(Config->szNetmailDir) - 1] == '\\' || Config->szNetmailDir [strlen(Config->szNetmailDir) - 1] == '/')
							Config->szNetmailDir [ strlen(Config->szNetmailDir) - 1] = 0;
						break;
					case 12 : /* inbound dir */
						strcpy(Config->szInboundDir, pcCurrentPos);

						/* add '\' if last char is not it */
						if (Config->szInboundDir [ strlen(Config->szInboundDir) - 1] != '\\' &&  Config->szInboundDir [strlen(Config->szInboundDir) - 1] != '/')
							strcat(Config->szInboundDir, "/");
						break;
					case 13 : /* mailer type */
						if (stricmp(pcCurrentPos, "BINKLEY") == 0)
							Config->MailerType = MAIL_BINKLEY;
						else
							Config->MailerType = MAIL_OTHER;
						break;
					case 14 : /* in a league? */
						System.InterBBS = TRUE;
						Config->InterBBS = TRUE;
						break;
					case 15 : /* regcode */
						if (*pcCurrentPos)
							strcpy(Config->szRegcode, pcCurrentPos);
						break;
				}
			}
		}
	}

	fclose(fpConfigFile);

}

void Config_Close(void)
/*
 * Shuts down config's mem.
 *
 */
{
	free(Config);
}

// ------------------------------------------------------------------------- //

void ShowHelp(void)
/*
 * Shows help screen for /? and /Help
 *
 */
{
	// help info using help parameter
	zputs(ST_HELPPARM);
}

#if defined(__unix__)
char * fullpath(char *target, const char *path, size_t size)
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
	strncat(target,path,size-1);

	/*  if(stat(target,&sb))
	        return(NULL);
	    if(sb.st_mode&S_IFDIR)
	        strcat(target,"/"); */

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

void PrimitiveCommandLine(void)
/*
 * Deals with basic command lines (i.e. those which do not depend on the
 * game being loaded yet).
 */
{
	_INT16 cTemp, iKeyWord;
	BOOL FoundMatch;
	char *szHelp = "|02Invalid parameter -- |07type |10CLANS /? |07for help\n";

	/* This function will parse the command line by looking for the  */
	/* flags and stuff!                        */

	for (cTemp = 1; cTemp < _argc; cTemp++) {
		if (_argv[cTemp][0] == '-' || _argv[cTemp][0] == '/') {
			FoundMatch = FALSE;

			/* Loop through list of keywords */
			for (iKeyWord = 0; iKeyWord < MAX_COMLINE_WORDS; ++iKeyWord) {
				/* If keyword matches */
				if (stricmp(&_argv[cTemp][1], papszComLineKeyWords[iKeyWord]) == 0 ||
						(_argv[cTemp][1] == 'D' && papszComLineKeyWords[iKeyWord][0] == 'D')) {
					FoundMatch = TRUE;

					/* Process token */
					switch (iKeyWord) {
						case 3 : /* ? */
						case 4 : /* Help */
							ShowHelp();
							delay(3000);
							exit(0);
							/*                System_Close(); */
							break;
						case 7 :  /* LIBBS */
							System.LocalIBBS = TRUE;
							break;
						case 11 : /* Recon */
							cTemp++;
							break;
						case 12 : /* SendReset */
							cTemp++;
							break;
						case 13 : /* Reset */
							System_Error("To reset the game, please run RESET.EXE.\n");
							break;
						case 14 : /* Verbose */
							DisplayStr("|07Verbose |14ON\n");
							Verbose = TRUE;
							break;
						case 15 :  /* D */
							strcpy(od_control.info_path, &_argv[cTemp][2]);
							break;

					}
				}
			}
			if (FoundMatch == FALSE) {
				if (toupper(_argv[cTemp][1]) - 'A' == 18 && stricmp(&_argv[cTemp][2], "lop") == 0) {
					Register();
					System_Close();
				}
				else if (toupper(_argv[cTemp][1]) == 'N' || toupper(_argv[cTemp][1]) == 'D')
					System.Node = atoi(&_argv[cTemp][2]);

				// 12/23/2001 [au] dropped time-slicer code
				/*
				    else if (toupper(_argv[cTemp][1]) == 'T')
				    {
				      TSlicer_Init();

				      if (_argv[cTemp][2])
				        TSlicer_SetPoll(atoi(&_argv[cTemp][2]));
				    }
				*/

				else {
					zputs(szHelp);
					System_Close();
				}
			}
		}
		else {
			zputs(szHelp);
			delay(3000);
			exit(0);
		}
	}

}

// ------------------------------------------------------------------------- //


/* Parses commands used to load game */
void ParseCommands(void)
/*
 * Deals with Command-Line Parms which DO require game to be loaded and
 * initialized.
 */
{
	_INT16 cTemp, iKeyWord;
	char szString[128];

	/* This function will parse the command line by looking for the  */
	/* flags and stuff!                        */

	for (cTemp = 1; cTemp < _argc; cTemp++) {
		if (_argv[cTemp][0] == '-' || _argv[cTemp][0] == '/') {
			/* Loop through list of keywords */
			for (iKeyWord = 0; iKeyWord < MAX_COMLINE_WORDS; ++iKeyWord) {
				/* If keyword matches */
				if (stricmp(&_argv[cTemp][1], papszComLineKeyWords[iKeyWord]) == 0) {
					/* Process token */
					switch (iKeyWord) {
						case 0  : /* L */
						case 1  : /* Local */
							System.Local = TRUE;
							break;
						case 2  : /* M */
							Maintenance();
							System_Close();
							break;
						case 7 :  /* LIBBS */
							if (Game.Data->InterBBS == FALSE)
								System_Error(ST_IBBSONLY);
							break;
						case 8 :  /* Users */
							User_List();
							System_Close();
							break;
						case 9 :  /* NewNDX */
							if (Game.Data->InterBBS) {
								if (IBBS.Data->BBSID != 1) {
									System_Error("Only the League Coordinator can update the world.ndx file.\n");
								}

								IBBS_DistributeNDX();
								System_Close();
							}
							else
								System_Error(ST_IBBSONLY);
							break;
						case 10 : /* I */
							// read in packets waiting
							IBBS_PacketIn();

							if (Game.Data->InterBBS == FALSE)
								System_Error(ST_IBBSONLY);
							else
								System_Close();
							break;
						case 11 : /* Recon */
							cTemp++;
							if (Game.Data->InterBBS) {
								if (IBBS.Data->BBSID != atoi(_argv[cTemp]) &&
										(atoi(_argv[cTemp]) > 0 && atoi(_argv[cTemp]) < MAX_IBBSNODES)) {
									sprintf(szString, "Sending recon to %d\n", atoi(_argv[cTemp]));
									DisplayStr(szString);
									IBBS_SendRecon(atoi(_argv[cTemp]));
								}
								System_Close();
							}
							else
								System_Error(ST_IBBSONLY);
							break;
						case 12 : /* SendReset */
							cTemp++;
							if (Game.Data->InterBBS) {
								if (IBBS.Data->BBSID != 1) {
									System_Error("Only the League Coordinator can send a reset.\n");
								}

								/* sprintf(szString, "trying to send reset to %d\n", atoi(_argv[cTemp]));
								DisplayStr(szString);
								*/

								if (IBBS.Data->BBSID != atoi(_argv[cTemp]) &&
										(atoi(_argv[cTemp]) > 0 && atoi(_argv[cTemp]) < MAX_IBBSNODES)) {
									sprintf(szString, "Sending reset to %d\n", atoi(_argv[cTemp]));
									DisplayStr(szString);
									IBBS_SendReset(atoi(_argv[cTemp]));
								}
								System_Close();
							}
							else
								System_Error(ST_IBBSONLY);
							break;
					}
				}
			}
		}
		else {
			zputs("|06Invalid parameter -- |07type |14CLANS /? |07for help\n");
			System_Close();
		}
	}
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
		System.Initialized = FALSE;

//#ifdef _DEBUG
//# define TRACEX(x) MessageBox (NULL, (x), TEXT("DEBUG"), MB_OK)
//#else
# define TRACEX(x)
//#endif

		TRACEX("DisplayScores");
		DisplayScores(TRUE);

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
			od_exit(0, FALSE);
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
#ifndef _WIN32
	struct date date;
#else
	SYSTEMTIME system_time;
#endif
	_INT16 iTemp;
#ifdef __unix__
	char *pszResolvedPath;
#endif

	System.Initialized = TRUE;
	System.LocalIBBS = FALSE;

	// Get directory from commandline
	strcpy(System.szMainDir, _argv[0]);
	for (iTemp = strlen(_argv[0]); iTemp > 0; iTemp--) {
		if (_argv[0][iTemp] == '\\' || _argv[0][iTemp] == '/')
			break;
	}
	++iTemp;
	System.szMainDir[iTemp] = 0;
#ifdef __unix__
	pszResolvedPath=fullpath(NULL,System.szMainDir,sizeof(System.szMainDir));
	strcpy(System.szMainDir,pszResolvedPath);
	free(pszResolvedPath);
#endif

#ifdef PRELAB
	printf("Clans Start -- mem left = %lu\n", farcoreleft());
#endif

	// 12/23/2001 [au] getting rid of time-slicer support
	// Time slicer init
	// get_os();

	// initialize date
#ifndef _WIN32
	getdate(&date);
	sprintf(System.szTodaysDate, "%02d/%02d/%4d", date.da_mon, date.da_day, date.da_year);
#else
	GetSystemTime(&system_time);
	sprintf(System.szTodaysDate, "%02d/%02d/%4d", system_time.wMonth, system_time.wDay, system_time.wYear);
#endif

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

	PrimitiveCommandLine();

	Config_Init();

	// init stuff
	System.Local = FALSE;

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
	if (_argc > 1)
		ParseCommands();

	Door_Init(System.Local);

	if (System_LockedOut()) {
		rputs("Sorry, you have been locked out of this door.\n%P");
		od_exit(0, FALSE);
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
