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
 * Village ADT
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __unix__
#include <share.h>
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include <OpenDoor.h>

#include "door.h"
#include "empire.h"
#include "game.h"
#include "help.h"
#include "ibbs.h"
#include "input.h"
#include "language.h"
#include "mail.h"
#include "menus.h"
#include "misc.h"
#include "mstrings.h"
#include "myopen.h"
#include "news.h"
#include "npc.h"
#include "parsing.h"
#include "quests.h"
#include "reg.h"
#include "structs.h"
#include "system.h"
#include "user.h"
#include "video.h"
#include "village.h"
#include "voting.h"

struct village Village;

struct  Scheme {
	char szName[20];
	char ColorScheme[23];
};

// ------------------------------------------------------------------------- //

int16_t OutsiderTownHallMenu(void)
{
	char *szTheOptions[11];
	char szString[128];
	int16_t ClanId[2];
	long lTemp;

	LoadStrings(380, 10, szTheOptions);
	szTheOptions[10] = "View Village Empire Stats";

	/* get a choice */
	for (;;) {
		rputs("\n\n");

		/* show 'menu' */
		if (Game.Data.InterBBS)
			snprintf(szString, sizeof(szString), ST_VSTATHEADER, VillageName(IBBS.Data.BBSID));
		else
			snprintf(szString, sizeof(szString), ST_VSTATHEADER, Village.Data.szName);
		rputs(szString);

		rputs(ST_LONGLINE);

		if (Village.Data.RulingClanId[0] == -1)
			rputs(ST_T2MENUSTAT0);
		else {
			snprintf(szString, sizeof(szString), ST_T2MENUSTAT1, Village.Data.szRulingClan, (int)Village.Data.RulingDays);
			rputs(szString);
		}

		snprintf(szString, sizeof(szString), ST_T2MENUSTAT2, (int)Village.Data.TaxRate);
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_T2MENUSTAT3, (int)Village.Data.InterestRate);
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_T2MENUSTAT4, (int)Village.Data.GST);
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_T2MENUSTAT6, (long)Village.Data.Empire.VaultGold);
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_TMENUSTAT8, (int)Village.Data.ConscriptionRate);
		rputs(szString);

		switch (GetChoice("Town2", ST_ENTEROPTION, szTheOptions, "DWBVPH?Q/ES", 'Q', true)) {
			case 'S' :  // village empire stats
				Empire_Stats(&Village.Data.Empire);
				break;
			case 'E' :  // donate to empire
				DonateToEmpire(&Village.Data.Empire);
				break;
			case 'B' :  // voting booth
				VotingBooth();
				break;
			case 'D' :  /* deposit into vault */
				Help("Deposit into Vault", ST_CITIZENHLP);
				lTemp = GetLong(ST_T2MENU1Q, 0, PClan->Empire.VaultGold);

				if (lTemp) {
					Village.Data.Empire.VaultGold += lTemp;
					PClan->Empire.VaultGold -= lTemp;

					snprintf(szString, sizeof(szString), ST_T2MENU1, (long)lTemp);
					rputs(szString);
				}
				break;
			case 'P' :  /* public discussion */
				Menus_ChatRoom("public.txt");
				break;
			case 'W' :      /* Write to ruler */
				if (Village.Data.RulingClanId[0] == -1) {
					// no ruler to write to!
					rputs(ST_T2MENU3);
					break;
				}
				ClanId[0] = Village.Data.RulingClanId[0];
				ClanId[1] = Village.Data.RulingClanId[1];
				MyWriteMessage2(ClanId, false, false, -1, "", false, -1);
				break;
			case 'H' :  /* citizen help */
				GeneralHelp(ST_CITIZENHLP);
				break;
			case 'V' :  /* view clan stats */
				ClanStats(PClan, true);
				break;
			case 'Q' :      /* return to previous menu */
				return 0;
			case '?' :      /* redisplay options */
				break;
			case '/' :  // chat villagers
				ChatVillagers(WN_TOWNHALL);
				break;
		}
	}
}


// ------------------------------------------------------------------------- //

static void ChangeFlagScheme(void)
{
	bool Quit = false;
	char cInput;
	int16_t WhichColour;
	char OldFlag[3];
	bool ChangesMade = false;
	char szString[255];

	/* save current flag */
	OldFlag[0] = Village.Data.ColorScheme[23];
	OldFlag[1] = Village.Data.ColorScheme[24];
	OldFlag[2] = Village.Data.ColorScheme[25];

	Help("Flag", ST_RULERHLP);
	rputs("\n%P");

	while (!Quit) {
		/* show current scheme */
		rputs(" |0CCurrent Flag:  |0X\xdb|0Y\xdb|0Z\xdb\n\n");

		rputs("|0A(|0B1|0A) |0CLeft bar\n");
		rputs("|0A(|0B2|0A) |0CMiddle bar\n");
		rputs("|0A(|0B3|0A) |0CRight bar\n");
		rputs("|0A(|0BQ|0A) |0CQuit\n\n");

		rputs("|0GChoose one|0E> |15");

		/* get option */
		cInput = od_get_answer("123Q\r\n");

		switch (cInput) {
			case '1' :
			case '2' :
			case '3' :
				snprintf(szString, sizeof(szString), "%c\n\n", cInput);
				rputs(szString);

				ChangesMade = true;

				WhichColour = (cInput - '1') + 23;

				Help("Individual Colours", ST_CLANSHLP);

				Village.Data.ColorScheme[ WhichColour ] = (char)GetLong("|0SPlease enter the colour to use (0 to 15) ", Village.Data.ColorScheme[ WhichColour ], 15);
				Door_SetColorScheme(Village.Data.ColorScheme);
				break;
			case 'Q' :
			case '\n' :
			case '\r' :
				rputs("Quit\n\n");
				Quit = true;
				break;
		}

		/* act on option */
	}

	if (ChangesMade && NoYes("|0SAre you sure you wish to use this flag?") == NO) {
		Village.Data.ColorScheme[23] = OldFlag[0];
		Village.Data.ColorScheme[24] = OldFlag[1];
		Village.Data.ColorScheme[25] = OldFlag[2];
	}
	else if (ChangesMade) {
		rputs("|15Flag changes have been saved.\n\n");

		snprintf(szString, sizeof(szString), ST_NEWSNEWFLAG, Village.Data.szRulingClan, Village.Data.ColorScheme[23], Village.Data.ColorScheme[24], Village.Data.ColorScheme[25]);
		News_AddNews(szString);
	}
}

// ------------------------------------------------------------------------- //

static void GetNums(char *Array, int16_t NumVars, char *string)
{
	int16_t iTemp, CurChar = 0;

	// get next number
	while ((string[CurChar] == ',' || string[CurChar] == ' ') && string[CurChar])
		CurChar++;

	for (iTemp = 0;  iTemp < NumVars; iTemp++) {
		Array[iTemp] = atoc(&string[CurChar], "Number?", __func__);

		while (isdigit(string[CurChar]))
			CurChar++;

		// get next number
		while ((string[CurChar] == ',' || string[CurChar] == ' ') && string[CurChar])
			CurChar++;

		if (string[CurChar] == 0)
			break;
	}
}

static void LoadSchemes(struct Scheme *Scheme[128])
{
	int16_t iTemp, CurScheme;
	char szLine[128], *pcCurrentPos, szName[20];
	struct FileHeader FileHeader;

	for (iTemp = 0; iTemp < 128; iTemp++)
		Scheme[iTemp] = NULL;

	MyOpen("/d/Schemes", "r", &FileHeader);

	if (!FileHeader.fp) return;

	for (CurScheme = 0;;) {
		if (ftell(FileHeader.fp) >= FileHeader.lEnd)
			break;

		if (fgets(szLine, 128, FileHeader.fp) == NULL)
			break;

		Scheme[CurScheme] = malloc(sizeof(struct Scheme));
		CheckMem(Scheme[CurScheme]);

		pcCurrentPos = szLine;

		/* get name */
		GetToken(pcCurrentPos, szName);
		strlcpy(Scheme[CurScheme]->szName, szName, sizeof(Scheme[CurScheme]->szName));

		/* get colors */
		GetNums(Scheme[CurScheme]->ColorScheme, 23, pcCurrentPos);

		CurScheme++;
	}

	fclose(FileHeader.fp);
}

static void AddScheme(void)
{
	FILE *fp;
	int16_t iTemp;

	fp = fopen("schemes.txt", "a");

	if (fp) {
		fprintf(fp, "\nScheme ");
		for (iTemp = 0; iTemp < 23; iTemp++)
			fprintf(fp, "%d ", Village.Data.ColorScheme[iTemp]);

		fclose(fp);
	}
	else {
		rputs("\n|04Failed to open schemes.txt\n");
	}
}


// ------------------------------------------------------------------------- //

static void ChangeColourScheme(void)
{
	bool Quit = false;
	char cInput, szKeys[26], szString[128];
	unsigned char cTemp;
	int16_t iTemp, WhichColour, Choice;
	struct Scheme *Scheme[128];
	char TableColor[128];

	/* set up tables which "point" to which color */
	/* this is a tricky way of fixing a problem. ;) */
	for (cTemp = 0; cTemp <= 128; cTemp++)
		TableColor[cTemp] = (char)cTemp;
	/* now for the abnormal ones */
	TableColor[ '5' ] = '7';
	TableColor[ '6' ] = '5';
	TableColor[ '7' ] = '6';

	Help("Color Scheme", ST_RULERHLP);
	rputs("\n%P");


	LoadSchemes(Scheme);

	while (!Quit) {
		/* show current scheme */
		rputs(ST_SCHEMETITLE);

		rputs(ST_SCHEME1);
		rputs(ST_SCHEME2);
		rputs(ST_SCHEME3);
		rputs(ST_SCHEME4);
		rputs(ST_SCHEME5);

		rputs(ST_SCHEME6);
		rputs(ST_SCHEME7);
		rputs(ST_SCHEME8);
		rputs(ST_SCHEME9);
		rputs(ST_SCHEME10);

		rputs(ST_COLOR03);
		rputs(ST_SCHEMEMENU1);
		rputs(ST_SCHEMEMENU2);
		rputs(ST_SCHEMEMENU3);
		rputs(ST_SCHEMEMENU4);

		rputs(ST_SCHEMEMENU5);
		rputs(ST_SCHEMEMENU6);
		rputs(ST_SCHEMEMENU7);
		rputs(ST_SCHEMEMENU8);

		/* prompt */
		rputs(ST_SCHEMEMENU9);

		/* get option */
		cInput = od_get_answer("123456789ABCDEFGHIJMNQ\r\n!Z");

		switch (cInput) {
			case 'Z' :  /* choose scheme */
				rputs("Choose scheme\n\n");

				strlcpy(szKeys, "ABCDEFGHIJKLMNOP", sizeof(szKeys));
				for (iTemp = 0; iTemp < 128; iTemp++) {
					if (Scheme[iTemp] == NULL)
						break;

					snprintf(szString, sizeof(szString), "|01(|09%c|01) |07%s\n", iTemp + 'A', Scheme[iTemp]->szName);
					rputs(szString);
				}
				szKeys[iTemp] = 0;
				strlcat(szKeys, "Q\r\n", sizeof(szKeys));
				rputs("|01(|09Q|01) |07Quit\n\n|07Choose one|03> |11");
				cInput = od_get_answer(szKeys);

				if (cInput == '\r' || cInput == 'Q' || cInput == '\n') {
					rputs("Quit\n");
					break;
				}
				else
					snprintf(szString, sizeof(szString), "%s\n", Scheme[cInput - 'A']->szName);
				rputs(szString);

				Choice = cInput - 'A';

				for (iTemp = 0; iTemp < 23; iTemp++)
					Village.Data.ColorScheme[iTemp] = Scheme[Choice]->ColorScheme[iTemp];
				Door_SetColorScheme(Village.Data.ColorScheme);
				break;
			case '!' :
				for (iTemp = 0; iTemp < 26; iTemp++) {
					snprintf(szString, sizeof(szString), "%d ", Village.Data.ColorScheme[iTemp]);
					rputs(szString);
				}
				rputs("\n");
				AddScheme();
				door_pause();
				break;
			case 'Q' :
			case '\r':
			case '\n':
				rputs("Quit\n\n");
				Quit = true;
				break;
			default :

				snprintf(szString, sizeof(szString), "%c\n\n", cInput);
				rputs(szString);

				cInput = TableColor[(cInput + 0)];

				if (isdigit(cInput))
					WhichColour = cInput - '1';
				else
					WhichColour = (cInput - 'A') + 9;

				Help("Individual Colours", ST_CLANSHLP);

				iTemp = (int16_t)GetLong("|0SPlease enter the colour to use (1 to 15) ", (int32_t)Village.Data.ColorScheme[ WhichColour ], 15);

				if (iTemp)
					Village.Data.ColorScheme[ WhichColour ] = (char)iTemp;
				Door_SetColorScheme(Village.Data.ColorScheme);
				break;
		}

		/* act on option */
	}

	for (iTemp = 0; iTemp < 128; iTemp++)
		if (Scheme[iTemp])
			free(Scheme[iTemp]);
}


// ------------------------------------------------------------------------- //
static void BuildMenu(void)
{
	char *szTheOptions[9];
	char szString[255];
	int32_t THallBuildCosts[MAX_THALLLEVEL] = { 15500L, 50000L, 100000L, 250000L };
	int32_t ChurchBuildCosts[MAX_CHURCHLEVEL] = { 8000L, 25000L, 30000L, 50000L, 70000L };
	int32_t PawnBuildCosts[MAX_PAWNLEVEL] = { 10000L, 25000L, 30000L, 50000L, 70000L };
	int32_t WizardBuildCosts[MAX_WIZARDLEVEL] = { 10000L, 25000L, 35000L, 40000L, 70000L };
	int32_t TotalCost;

	LoadStrings(350, 9, szTheOptions);

	if (!PClan->TownHallHelp) {
		PClan->TownHallHelp = true;
		Help("Town Hall", ST_NEWBIEHLP);
	}

	/* get a choice */
	for (;;) {
		rputs("\n\n");

		/* show 'menu' */
		if (Game.Data.InterBBS)
			snprintf(szString, sizeof(szString), ST_VSTATHEADER, VillageName(IBBS.Data.BBSID));
		else
			snprintf(szString, sizeof(szString), ST_VSTATHEADER, Village.Data.szName);
		rputs(szString);

		rputs(ST_LONGLINE);

		snprintf(szString, sizeof(szString), ST_TMENUSTAT5, (long)Village.Data.Empire.VaultGold);
		rputs(szString);

		/* church,
		   training hall,
		  + more on the way */

		if (Village.Data.ChurchLevel == 0)
			rputs(ST_BMENU3);
		else if (Village.Data.ChurchLevel < MAX_CHURCHLEVEL) {
			snprintf(szString, sizeof(szString), ST_BMENU4, (int)(Village.Data.ChurchLevel + 1));
			rputs(szString);
		}
		else {
			snprintf(szString, sizeof(szString), ST_BMENU5, (int)MAX_CHURCHLEVEL);
			rputs(szString);
		}

		if (Village.Data.TrainingHallLevel == 0)
			rputs(ST_BMENU6);
		else if (Village.Data.TrainingHallLevel < MAX_THALLLEVEL) {
			snprintf(szString, sizeof(szString), ST_BMENU7, (int)(Village.Data.TrainingHallLevel + 1));
			rputs(szString);
		}
		else {
			snprintf(szString, sizeof(szString), ST_BMENU8, (int)MAX_THALLLEVEL);
			rputs(szString);
		}

		if (Village.Data.PawnLevel == 0)
			rputs(ST_BMENU30);
		else if (Village.Data.PawnLevel < MAX_PAWNLEVEL) {
			snprintf(szString, sizeof(szString), ST_BMENU31, (int)(Village.Data.PawnLevel + 1));
			rputs(szString);
		}
		else {
			snprintf(szString, sizeof(szString), ST_BMENU32, (int)MAX_PAWNLEVEL);
			rputs(szString);
		}
		if (Village.Data.WizardLevel == 0)
			rputs(ST_BMENU33);
		else if (Village.Data.WizardLevel < MAX_WIZARDLEVEL) {
			snprintf(szString, sizeof(szString), ST_BMENU34, (int)(Village.Data.WizardLevel + 1));
			rputs(szString);
		}
		else {
			snprintf(szString, sizeof(szString), ST_BMENU35, (int)MAX_WIZARDLEVEL);
			rputs(szString);
		}

		if (Village.Data.MarketLevel < MAX_MARKETLEVEL) {
			snprintf(szString, sizeof(szString), ST_BMENU36, (int)(Village.Data.MarketLevel + 1));
			rputs(szString);
		}
		else {
			snprintf(szString, sizeof(szString), "     |0CSmithy Level                |0B%d\n", (int)Village.Data.MarketLevel);
			rputs(szString);
		}

		/* more options */
		rputs(ST_BMENU9);
		rputs(ST_BMENU10);
		rputs(ST_LONGLINE);

		switch (GetChoice("", ST_ENTEROPTION, szTheOptions, "PZFCTH?QS", 'Q', true)) {
			case 'P' :  // pawn shop
				Help("Pawn Shop", ST_RULERHLP);

				if (Village.Data.PawnLevel == MAX_PAWNLEVEL) {
					rputs(ST_BMENU37);
					break;
				}
				if (Village.Data.PawnLevel == 0) {
					snprintf(szString, sizeof(szString), ST_BMENU38,
							(long)PawnBuildCosts[0], (long)Village.Data.Empire.VaultGold);
					rputs(szString);

					if (Village.Data.Empire.VaultGold < PawnBuildCosts[0]) {
						rputs(ST_VILLNOAFFORD);
						break;
					}

					if (YesNo(ST_BMENU39) == YES) {
						Village.Data.Empire.VaultGold -= PawnBuildCosts[0];
						Village.Data.PawnLevel = 1;

						rputs(ST_BMENU40);
						snprintf(szString, sizeof(szString), ST_BMENU41, Village.Data.szRulingClan);
						News_AddNews(szString);
						break;
					}
				}
				/* NO MORE REG
				          else if (Village.Data.PawnLevel == 3 && (IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) == NFALSE ||
				            IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) != NTRUE))
				          {
				            rputs(ST_NEEDREGISTER);
				          }
				*/
				else if (Village.Data.PawnLevel < MAX_PAWNLEVEL) {
					snprintf(szString, sizeof(szString), ST_BMENU42,
							(long)PawnBuildCosts[Village.Data.PawnLevel], (int)(Village.Data.PawnLevel + 1), (long)Village.Data.Empire.VaultGold);
					rputs(szString);

					if (Village.Data.Empire.VaultGold < PawnBuildCosts[ Village.Data.PawnLevel ]) {
						rputs(ST_VILLNOAFFORD);
						break;
					}

					if (YesNo(ST_BMENU43) == YES) {
						Village.Data.Empire.VaultGold -= PawnBuildCosts[ Village.Data.PawnLevel ];
						Village.Data.PawnLevel++;

						snprintf(szString, sizeof(szString), ST_BMENU44, (int)Village.Data.PawnLevel);
						rputs(szString);

						snprintf(szString, sizeof(szString), ST_BMENU45,
								Village.Data.szRulingClan, (int)Village.Data.PawnLevel);
						News_AddNews(szString);
						break;
					}
				}
				break;
			case 'Z' :  // wizard's shop
				Help("Wizard's Shop", ST_RULERHLP);

				if (Village.Data.WizardLevel == MAX_WIZARDLEVEL) {
					rputs(ST_BMENU46);
					break;
				}
				if (Village.Data.WizardLevel == 0) {
					snprintf(szString, sizeof(szString), ST_BMENU47,
							(long)WizardBuildCosts[0], (long)Village.Data.Empire.VaultGold);
					rputs(szString);

					if (Village.Data.Empire.VaultGold < WizardBuildCosts[0]) {
						rputs(ST_VILLNOAFFORD);
						break;
					}

					if (YesNo(ST_BMENU48) == YES) {
						Village.Data.Empire.VaultGold -= WizardBuildCosts[0];
						Village.Data.WizardLevel = 1;

						rputs(ST_BMENU49);
						snprintf(szString, sizeof(szString),
								ST_BMENU50, Village.Data.szRulingClan);
						News_AddNews(szString);
						break;
					}
				}
				/* NO MORE REG
				          else if (Village.Data.WizardLevel == 3 && (IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) == NFALSE ||
				            IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) != NTRUE))
				          {
				            rputs(ST_NEEDREGISTER);
				          }
				*/
				else if (Village.Data.WizardLevel < MAX_WIZARDLEVEL) {
					snprintf(szString, sizeof(szString), ST_BMENU51,
							(long)WizardBuildCosts[Village.Data.WizardLevel], (int)(Village.Data.WizardLevel + 1), (long)Village.Data.Empire.VaultGold);
					rputs(szString);

					if (Village.Data.Empire.VaultGold < WizardBuildCosts[ Village.Data.WizardLevel ]) {
						rputs(ST_VILLNOAFFORD);
						break;
					}

					if (YesNo(ST_BMENU52) == YES) {
						Village.Data.Empire.VaultGold -= WizardBuildCosts[ Village.Data.WizardLevel ];
						Village.Data.WizardLevel++;

						snprintf(szString, sizeof(szString), ST_BMENU53, (int)Village.Data.WizardLevel);
						rputs(szString);

						snprintf(szString, sizeof(szString), ST_BMENU54,
								Village.Data.szRulingClan, (int)Village.Data.WizardLevel);
						News_AddNews(szString);
						break;
					}
				}
				break;
			case 'C' :  /* church */
				Help("Church", ST_RULERHLP);

				if (Village.Data.ChurchLevel == MAX_CHURCHLEVEL) {
					rputs(ST_BMENU11);
					break;
				}
				if (Village.Data.ChurchLevel == 0) {
					// what will it cost
					snprintf(szString, sizeof(szString), ST_BMENU12,
							(long)ChurchBuildCosts[0], (long)Village.Data.Empire.VaultGold);
					rputs(szString);

					if (Village.Data.Empire.VaultGold < ChurchBuildCosts[0]) {
						rputs(ST_VILLNOAFFORD);
						break;
					}

					if (YesNo(ST_BMENU13) == YES) {
						Village.Data.Empire.VaultGold -= ChurchBuildCosts[0];
						Village.Data.ChurchLevel = 1;

						rputs(ST_BMENU14);
						snprintf(szString, sizeof(szString), ST_NEWSNEWCHURCH, Village.Data.szRulingClan);
						News_AddNews(szString);
						break;
					}
				}
				/* NO MORE REG
				          else if (Village.Data.ChurchLevel == 3 && (IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) == NFALSE ||
				            IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) != NTRUE))
				          {
				            rputs(ST_NEEDREGISTER);
				          }
				*/
				else if (Village.Data.ChurchLevel < MAX_CHURCHLEVEL) {
					snprintf(szString, sizeof(szString), ST_BMENU15, (long)ChurchBuildCosts[Village.Data.ChurchLevel], (int)Village.Data.ChurchLevel+1, (long)Village.Data.Empire.VaultGold);
					rputs(szString);

					if (Village.Data.Empire.VaultGold < ChurchBuildCosts[ Village.Data.ChurchLevel ]) {
						rputs(ST_VILLNOAFFORD);
						break;
					}

					if (YesNo(ST_BMENU16) == YES) {
						Village.Data.Empire.VaultGold -= ChurchBuildCosts[ Village.Data.ChurchLevel ];
						Village.Data.ChurchLevel++;

						snprintf(szString, sizeof(szString), ST_BMENU17, (int)Village.Data.ChurchLevel);
						rputs(szString);

						snprintf(szString, sizeof(szString), ST_NEWSUPGRADECHURCH, Village.Data.szRulingClan, (int)Village.Data.ChurchLevel);
						News_AddNews(szString);
						break;
					}
				}
				break;
			case 'T' :  /* training hall */
				Help("Training Hall", ST_RULERHLP);

				if (Village.Data.TrainingHallLevel == MAX_THALLLEVEL) {
					rputs(ST_BMENU18);
					break;
				}
				if (Village.Data.TrainingHallLevel == 0) {
					snprintf(szString, sizeof(szString), ST_BMENU19, (long)THallBuildCosts[0], (long)Village.Data.Empire.VaultGold);
					rputs(szString);

					if (Village.Data.Empire.VaultGold < THallBuildCosts[0]) {
						rputs(ST_VILLNOAFFORD);
						break;
					}

					if (YesNo(ST_BMENU20) == YES) {
						Village.Data.Empire.VaultGold -= THallBuildCosts[0];
						Village.Data.TrainingHallLevel = 1;

						rputs(ST_BMENU21);
						snprintf(szString, sizeof(szString), ST_NEWSNEWTHALL, Village.Data.szRulingClan);
						News_AddNews(szString);
						break;
					}
				}
				/* NO MORE REG
				          else if (Village.Data.TrainingHallLevel == 3 && (IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) == NFALSE ||
				            IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) != NTRUE))
				          {
				            rputs(ST_NEEDREGISTER);
				          }
				*/
				else if (Village.Data.TrainingHallLevel < MAX_THALLLEVEL) {
					snprintf(szString, sizeof(szString), ST_BMENU22, (long)THallBuildCosts[Village.Data.TrainingHallLevel], (int)(Village.Data.TrainingHallLevel + 1), (long)Village.Data.Empire.VaultGold);
					rputs(szString);

					if (Village.Data.Empire.VaultGold < THallBuildCosts[ Village.Data.TrainingHallLevel ]) {
						rputs(ST_VILLNOAFFORD);
						break;
					}

					if (YesNo(ST_BMENU23) == YES) {
						Village.Data.Empire.VaultGold -= THallBuildCosts[ Village.Data.TrainingHallLevel ];
						Village.Data.TrainingHallLevel++;

						snprintf(szString, sizeof(szString), ST_BMENU24, (int)Village.Data.TrainingHallLevel);
						rputs(szString);

						snprintf(szString, sizeof(szString), ST_NEWSUPGRADETHALL, Village.Data.szRulingClan, (int)Village.Data.TrainingHallLevel);
						News_AddNews(szString);
						break;
					}
				}
				break;
			case 'H' :  /* ruling help */
				GeneralHelp(ST_RULERHLP);
				break;
			case 'Q' :    /* return to previous menu */
				return;
			case '?' :    /* redisplay options */
				break;
			case 'S' :  /* smithy */
				Help("Smithy", ST_RULERHLP);

				if (Village.Data.MarketLevel == MAX_MARKETLEVEL) {
					rputs("The smithy is at its highest level already\n%P");
					break;
				}
				/* NO MORE REG
				          else if (Village.Data.MarketLevel == 3 && (IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) == NFALSE ||
				            IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) != NTRUE))
				          {
				            rputs(ST_NEEDREGISTER);
				            break;
				          }
				*/

				TotalCost = ((int32_t)(Village.Data.MarketLevel+1)*(Village.Data.MarketLevel+1)*1250L) + 15000L;

				snprintf(szString, sizeof(szString), ST_TOWN3, (long)TotalCost, (int)(Village.Data.MarketLevel + 1), (long)Village.Data.Empire.VaultGold);
				rputs(szString);

				if (Village.Data.Empire.VaultGold < TotalCost) {
					rputs(ST_VILLNOAFFORD);
					break;
				}

				if (YesNo(ST_TOWN4) == YES) {
					Village.Data.Empire.VaultGold -= TotalCost;
					Village.Data.MarketLevel++;

					snprintf(szString, sizeof(szString), ST_TOWN5, (int)Village.Data.MarketLevel);
					rputs(szString);

					snprintf(szString, sizeof(szString), ST_NEWSSMITHY, Village.Data.szRulingClan, (int)Village.Data.MarketLevel);
					News_AddNews(szString);
				}
				break;
		}
	}
}



// ------------------------------------------------------------------------- //

static void EconomicsMenu(void)
{
	char *szTheOptions[7];
	char szString[128];
	int16_t iTemp, OldTax;
	long lTemp;

	LoadStrings(390, 7, szTheOptions);

	if (!PClan->TownHallHelp) {
		PClan->TownHallHelp = true;
		Help("Town Hall", ST_NEWBIEHLP);
	}

	/* get a choice */
	for (;;) {
		rputs("\n\n");

		/* show 'menu' */
		if (Game.Data.InterBBS)
			snprintf(szString, sizeof(szString), ST_VSTATHEADER, VillageName(IBBS.Data.BBSID));
		else
			snprintf(szString, sizeof(szString), ST_VSTATHEADER, Village.Data.szName);
		rputs(szString);

		rputs(ST_LONGLINE);

		snprintf(szString, sizeof(szString), ST_TMENUSTAT5, (long)Village.Data.Empire.VaultGold);
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_EMENU1, (int)Village.Data.TaxRate);
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_EMENU3, (int)Village.Data.GST);
		rputs(szString);

		rputs(ST_EMENU7);

		rputs(ST_EMENU8);
		rputs(ST_EMENU9);
		rputs(ST_LONGLINE);

		switch (GetChoice("", ST_ENTEROPTION, szTheOptions, "TGWDH?Q", 'Q', true)) {
			case 'T' :  /* Tax rate */
				Help("Tax Rate", ST_RULERHLP);

				/* if already set tax today, don't allow it again */
				if (Village.Data.SetTaxToday)
					rputs(ST_EMENU10);  // tell him he can't set it more than once per day
				else {
					/* figure out lowest tax rate and highest tax rate */
					iTemp = (int16_t)GetLong(ST_EMENU11, Village.Data.TaxRate, 50);

					/* if no change, do nothing */
					if (iTemp == Village.Data.TaxRate)
						break;

					Village.Data.SetTaxToday = true;

					/* increasing the tax rate decreases/increases public approval */

					/* going from 5% tax rate to an 8% rate means
					   8 - 5 = 3.  approval drops by 1 (3/3)
					   and momentum drops down by 3 */

					OldTax = Village.Data.TaxRate;
					Village.Data.TaxRate = iTemp;

					snprintf(szString, sizeof(szString), ST_NEWSTAX1,
							Village.Data.szRulingClan, Village.Data.TaxRate > OldTax ? "raised" : "lowered",
							(int)OldTax, (int)Village.Data.TaxRate);
					News_AddNews(szString);
				}
				break;
			case 'G' :  /* GST */
				Help("GST", ST_RULERHLP);

				/* if already set today, don't allow it again */
				if (Village.Data.SetGSTToday)
					rputs(ST_EMENU15);
				else {
					iTemp = (int16_t)GetLong(ST_EMENU16, Village.Data.GST, 50);

					/* if no change, do nothing */
					if (iTemp == Village.Data.GST)
						break;

					Village.Data.SetGSTToday = true;

					/* increasing the tax rate decreases/increases public approval */

					/* going from 1% tax rate to an 7% rate means
					   7 - 1 = 6.  approval drops by 2 (6/3)
					   and momentum drops down by 6 */

					OldTax = Village.Data.GST;
					Village.Data.GST = iTemp;

					snprintf(szString, sizeof(szString), ST_NEWSTAX3,
							Village.Data.szRulingClan,
							Village.Data.GST > OldTax ? "raised" : "lowered",
							(int)OldTax, (int)Village.Data.GST);
					News_AddNews(szString);
				}
				break;
			case 'D' :  /* deposit in vault */
				Help("Deposit into Vault", ST_RULERHLP);
				lTemp = GetLong(ST_EMENU26, 0, PClan->Empire.VaultGold);

				if (lTemp) {
					Village.Data.Empire.VaultGold += lTemp;
					PClan->Empire.VaultGold -= lTemp;

					snprintf(szString, sizeof(szString), ST_EMENU27, (long)lTemp);
					rputs(szString);

					snprintf(szString, sizeof(szString), ST_NEWSDONATED, PClan->szName, (long)lTemp);
					News_AddNews(szString);
				}
				break;
			case 'W' :  /* withdraw from vault */
				Help("Withdraw from Vault", ST_RULERHLP);

				if (PClan->VaultWithdrawals == 3) {
					/* can only do 3 withdrawals a day */
					rputs(ST_EMENU28);
					break;
				}
				PClan->VaultWithdrawals++;

				lTemp = GetLong(ST_EMENU29, 0, Village.Data.Empire.VaultGold);

				if (lTemp) {
					Village.Data.Empire.VaultGold -= lTemp;
					PClan->Empire.VaultGold += lTemp;

					snprintf(szString, sizeof(szString), ST_EMENU30, (long)lTemp);
					rputs(szString);

					snprintf(szString, sizeof(szString), ST_NEWSEMBEZZLE, PClan->szName, (long)lTemp);
					News_AddNews(szString);
				}
				break;
			case 'H' :  /* ruling help */
				GeneralHelp(ST_RULERHLP);
				break;
			case 'Q' :    /* return to previous menu */
				return;
			case '?' :    /* redisplay options */
				break;
		}
	}
}


// ------------------------------------------------------------------------- //

int16_t TownHallMenu(void)
{
	char *szTheOptions[17];
	char szString[255], szSpeech[128];
	int16_t iTemp, OldRate;

	LoadStrings(335, 15, szTheOptions);
	szTheOptions[15] = "Conscription Rate";
	szTheOptions[16] = "Toggle Empire Stats";

	/* get a choice */
	for (;;) {
		rputs("\n\n");

		/* show 'menu' */
		if (Game.Data.InterBBS)
			snprintf(szString, sizeof(szString), ST_VSTATHEADER, VillageName(IBBS.Data.BBSID));
		else
			snprintf(szString, sizeof(szString), ST_VSTATHEADER, Village.Data.szName);
		rputs(szString);

		rputs(ST_LONGLINE);
		snprintf(szString, sizeof(szString), ST_TMENUSTAT0, (int)Village.Data.RulingDays);
		rputs(szString);
		snprintf(szString, sizeof(szString), ST_TMENUSTAT7, (long)Village.Data.Empire.VaultGold);
		rputs(szString);
		snprintf(szString, sizeof(szString), ST_TMENUSTAT8, (int)Village.Data.ConscriptionRate);
		rputs(szString);

		rputs(ST_LONGLINE);

		switch (GetChoice("Town1", ST_ENTEROPTION, szTheOptions, "SECLHMD!V?Q/BGPRT", 'Q', true)) {
				/*      case 'T' :
				          ToggleEmpireStats();
				          break;
				*/
			case 'R' :  // conscription rate
				Help("Conscription Rate", ST_RULERHLP);

				/* if already set rate today, don't allow it again */
				if (Village.Data.SetConToday)
					// rputs("Can't set conscription rate more than once a day.\n%P");  // tell him he can't set it more than once per day
					rputs(ST_TMENU19);  // tell him he can't set it more than once per day
				else {
					/* figure out lowest rate rate and highest rate */
					iTemp = (int16_t)GetLong(ST_TMENU20, Village.Data.ConscriptionRate, 20);

					/* if no change, do nothing */
					if (iTemp == Village.Data.ConscriptionRate)
						break;

					Village.Data.SetConToday = true;

					OldRate = Village.Data.ConscriptionRate;
					Village.Data.ConscriptionRate = iTemp;

					snprintf(szString, sizeof(szString), ST_NEWS4,
							Village.Data.szRulingClan, Village.Data.ConscriptionRate > OldRate ? "raised" : "lowered",
							(int)OldRate, (int)Village.Data.ConscriptionRate);
					News_AddNews(szString);
				}
				break;
			case 'P' :  // manage empire
				Empire_Manage(&Village.Data.Empire);
				break;
				/*      case 'G' :  // system of gov't
				          ChangeGovtSystem();
				          break;
				*/
			case 'B' :  // voting booth
				/*        if (Village.Data.GovtSystem == GS_DICTATOR)
				          {
				            // rputs("This town is under dictatorial rule.  Voting is disabled.\n");
				            rputs(ST_TMENU21);
				          }
				          else
				            VotingBooth();
				*/
				VotingBooth();
				break;
			case 'E' :  /* economics */
				EconomicsMenu();
				break;
			case 'V' :  /* view clan stats */
				ClanStats(PClan, true);
				break;
			case 'L' :    /* flag scheme */
				ChangeFlagScheme();
				break;
			case 'C' :    /* change colour scheme */
				ChangeColourScheme();
				break;
			case 'M' :  /* make announcement */
				/* enter announcement */
				rputs(ST_TMENU11);
				szSpeech[0] = 0;
				GetStr(szSpeech, 70, false);

				if (szSpeech[0] == 0) {
					rputs(ST_ABORTED);
					break;
				}

				snprintf(szString, sizeof(szString), ST_NEWSRULERSPEECH, Village.Data.szRulingClan);
				News_AddNews(szString);
				News_AddNews(szSpeech);
				News_AddNews("|16\n\n");
				break;
			case 'D' :  /* public discussion */
				Menus_ChatRoom("public.txt");
				break;
			case '!' :  /* abdicate */
				/* ask if he wants to abdicate for sure */
				if (NoYes(ST_TMENU15Q) == YES) {
					/* reset Village stats */
					Village.Data.RulingClanId[0] = -1;
					Village.Data.RulingClanId[1] = -1;
					Village.Data.szRulingClan[0] = 0;

					Village.Data.RulingDays = 0;
					Village.Data.GovtSystem = GS_DEMOCRACY;

					/* reduce score */
					/* reset user stats */
					PClan->WasRulerToday = true;
					PClan->Points -= 100;

					snprintf(szString, sizeof(szString), ST_NEWSABDICATED, PClan->szName);
					News_AddNews(szString);

					return 0;
				}
				break;
			case 'H' :  /* ruling help */
				GeneralHelp(ST_RULERHLP);
				break;
			case 'S' :  /* structures menu */
				BuildMenu();
				break;
			case 'Q' :  /* return to previous menu */
				return 0;
			case '?' :  /* redisplay options */
				break;
			case '/' :  // chat villagers
				ChatVillagers(WN_TOWNHALL);
				break;
		}
	}
}




// ------------------------------------------------------------------------- //

void Village_NewRuler(void)
{
	char szString[128];

	/* if they WERE the rulers today, can't become rulers again */
	if (PClan->WasRulerToday) {
		rputs("|15Your clan cannot claim rule again until tomorrow.\n%P");
		return;
	}

	Village.Data.RulingClanId[0] = PClan->ClanID[0];
	Village.Data.RulingClanId[1] = PClan->ClanID[1];
	Village.Data.RulingDays = 0;

	strlcpy(Village.Data.szRulingClan, PClan->szName, sizeof(Village.Data.szRulingClan));

	snprintf(szString, sizeof(szString), ST_NEWSNEWRULER, Village.Data.szRulingClan);
	News_AddNews(szString);

	/* give him points for becoming ruler */
	PClan->Points += 20L;

	rputs("|15Your clan now rules the village!\n");
	door_pause();
}


// ------------------------------------------------------------------------- //

static void Village_Destroy(void)
{
	Village.Initialized = false;
}

static bool Village_Read(void)
/*
 * This function reads in the village.dat data and places it in Village.
 *
 */
{
	FILE *fp;

	fp = _fsopen(ST_VILLAGEDATFILE, "rb", _SH_DENYWR);
	if (!fp)  return false;

	notEncryptRead_s(village_data, &Village.Data, fp, XOR_VILLAGE)
		return false;
	fclose(fp);
	return true;
}

static void Village_Write(void)
/*
 * This function writes the Village data to village.dat.
 *
 */
{
	FILE *fp;

	if (Village.Initialized == false) {
		Village_Destroy();
		System_Error("Village not initialized!\n");
	}

	fp = _fsopen(ST_VILLAGEDATFILE, "wb", _SH_DENYRW);
	if (fp) {
		EncryptWrite_s(village_data, &Village.Data, fp, XOR_VILLAGE);
		fclose(fp);
	}
}

// ------------------------------------------------------------------------- //

void Village_Reset(void)
{
	Village.Data.PublicMsgIndex = 1;

	Village.Data.TownType = -1;      /* unknown for now */
	Village.Data.TaxRate = 0;        /* no taxes */
	Village.Data.InterestRate = 0;   /* 0% interest rate */
	Village.Data.GST = 0;        /* no gst */
	Village.Data.Empire.VaultGold = 45000L; /* some money to begin with */
	Village.Data.ConscriptionRate = 10;

	Village.Data.RulingClanId[0] = -1; /* no ruling clan yet */
	Village.Data.RulingClanId[1] = -1;

	Village.Data.szRulingClan[0] = 0;
	Village.Data.RulingDays = 0;
	Village.Data.GovtSystem = GS_DEMOCRACY;


	Village.Data.MarketLevel = 0;      /* weaponshop level */
	Village.Data.TrainingHallLevel = 0;
	Village.Data.ChurchLevel = 0;
	Village.Data.PawnLevel = 0;
	Village.Data.WizardLevel = 0;

	Village.Data.SetTaxToday = false;
	Village.Data.SetInterestToday = false;
	Village.Data.SetGSTToday = false;
	Village.Data.SetConToday = false;

	Village.Data.UpMarketToday = false;
	Village.Data.UpTHallToday = false;
	Village.Data.UpChurchToday = false;
	Village.Data.UpPawnToday = false;
	Village.Data.UpWizToday = false;
	Village.Data.ShowEmpireStats = false;

	ClearFlags(Village.Data.HFlags);
	ClearFlags(Village.Data.GFlags);

	Empire_Create(&Village.Data.Empire, false);
	strlcpy(Village.Data.Empire.szName, Village.Data.szName, sizeof(Village.Data.Empire.szName));
	Village.Data.Empire.Land = 500;      // village starts off with this
	Village.Data.Empire.Buildings[B_BARRACKS] = 10;
	Village.Data.Empire.Buildings[B_WALL] = 10;
	Village.Data.Empire.Buildings[B_TOWER] = 5;
	Village.Data.Empire.Buildings[B_STEELMILL] = 5;
	Village.Data.Empire.Buildings[B_BUSINESS] = 10;
	Village.Data.Empire.Army.Footmen = 100;
	Village.Data.Empire.Army.Axemen = 25;

	Village.Data.CostFluctuation = (int16_t)(5 - my_random(11));
	Village.Data.MarketQuality = MQ_AVERAGE;
}

// ------------------------------------------------------------------------- //

void Village_Maint(void)
{
	int16_t MarketIndex;
	char *szQuality[4] = { "Average", "Good", "Very Good", "Excellent" },
						 szString[255];

	DisplayStr("* Village_Maint()\n");

	if (Village.Data.Empire.VaultGold < 0)
		Village.Data.Empire.VaultGold = 0;

	Village.Data.SetTaxToday       = false;
	Village.Data.SetInterestToday  = false;
	Village.Data.SetGSTToday       = false;
	Village.Data.SetConToday       = false;

	Village.Data.UpMarketToday     = false;
	Village.Data.UpTHallToday      = false;
	Village.Data.UpChurchToday     = false;
	Village.Data.UpPawnToday       = false;
	Village.Data.UpWizToday        = false;

	if (Village.Data.ConscriptionRate > 20)
		Village.Data.ConscriptionRate = 20;

	ClearFlags(Village.Data.HFlags);

	if (Village.Data.RulingClanId[0] != -1)
		Village.Data.RulingDays++;

	// choose a new ruler if possible
	ChooseNewLeader();

	// figure out village fluctuation costs
	MarketIndex = (int16_t)my_random(15);

	if (MarketIndex < 7) {
		Village.Data.CostFluctuation = (int16_t)(5 - my_random(11));
		Village.Data.MarketQuality = MQ_AVERAGE;
	}
	else if (MarketIndex < 11) {
		Village.Data.CostFluctuation = (int16_t)my_random(10);
		Village.Data.MarketQuality = MQ_GOOD;
	}
	else if (MarketIndex < 13) {
		Village.Data.CostFluctuation = (int16_t)(10 + my_random(10));
		Village.Data.MarketQuality = MQ_VERYGOOD;
	}
	else {
		Village.Data.CostFluctuation = (int16_t)(20 + my_random(10));
		Village.Data.MarketQuality = MQ_EXCELLENT;
	}

	snprintf(szString, sizeof(szString), "|0A \xaf\xaf\xaf |0CWeapon quality today is |0B%s\n\n",
			szQuality[Village.Data.MarketQuality]);
	News_AddNews(szString);

	Empire_Maint(&Village.Data.Empire);


	Village_Write();
}

// ------------------------------------------------------------------------- //

void Village_Init(void)
/*
 * This function initializes Village data.
 *
 */
{
	if (Verbose) {
		DisplayStr("> Village_Init()\n");
		delay(500);
	}

	Village.Initialized = true;

	if (!Village_Read()) {
		Village_Destroy();
		System_Error("village.dat not found!\n");
	}

	// ensure CRC is correct
	if (!Village.Data.CRC) {
		Village_Destroy();
		System_Error("Village data corrupt!\n");
	}

	Door_SetColorScheme(Village.Data.ColorScheme);

	if (Verbose) {
		DisplayStr("> Village_Init done()\n");
		delay(500);
	}

}

// ------------------------------------------------------------------------- //

void Village_Close(void)
/*
 * This function initializes Village data.
 *
 */
{
	if (Village.Initialized == false) return;

	Village_Write();
	Village_Destroy();
}

