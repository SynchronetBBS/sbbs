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
 * Quests/Event ADT
 *
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include <OpenDoor.h>

#include "door.h"
#include "fight.h"
#include "help.h"
#include "input.h"
#include "items.h"
#include "k_quests.h"
#include "language.h"
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

#define CD_OR                   1
#define CD_AND                  2

struct Quest Quests[MAX_QUESTS];

char Quests_TFlags[8];          // temp. flags

static bool QuestsInitialized = false;

// ------------------------------------------------------------------------- //

void Quests_Init(void)
{
	/* read in quests file and allocate memory for quest names */
	int16_t iTemp, CurQuest;
	FILE *fp;
	char szLine[128], *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	int16_t iKeyWord;

	if (Verbose) {
		DisplayStr("> Quests_Init()\n");
		delay(500);
	}


	// init quest names
	for (iTemp = 0; iTemp < MAX_QUESTS; iTemp++) {
		Quests[iTemp].Active = false;
		Quests[iTemp].pszQuestName = NULL;
		Quests[iTemp].pszQuestIndex = NULL;
		Quests[iTemp].pszQuestFile = NULL;
		Quests[iTemp].Known = false;
	}

	fp = fopen("quests.ini", "r");
	if (!fp) {
		DisplayStr("NO QUESTS FILE!!\n");
		return;
	}

	/* read in all lines and get event names */
	CurQuest = -1;
	for (;;) {
		/* read in a line */
		if (fgets(szLine, 128, fp) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;

		ParseLine(pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;

		GetToken(pcCurrentPos, szToken);

		if (szToken[0] == '$')
			break;

		/* Loop through list of keywords */
		for (iKeyWord = 0; iKeyWord < MAX_QUEST_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (strcasecmp(szToken, papszQuestKeyWords[iKeyWord]) == 0) {
				/* Process config token */
				switch (iKeyWord) {
					case 0 :  /* name of quest */
						if (CurQuest == MAX_QUESTS)
							break;

						/* NO MORE REG
						              if (CurQuest == 10 &&
						                (IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) == NFALSE ||
						                IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) != NTRUE))
						              {
						                // can't load more than 10 in unreg
						                break;
						              }
						*/

						CurQuest++;

						Quests[CurQuest].Active = true;
						Quests[CurQuest].Known  = false;

						Quests[ CurQuest ].pszQuestName = DupeStr(pcCurrentPos);
						break;
					case 1 :  /* index of quest */
						/* make room for questname in mem */
						Quests[ CurQuest ].pszQuestIndex = DupeStr(pcCurrentPos);
						break;
					case 2 :  /* filename to use */
						/* make room for questname in mem */
						Quests[ CurQuest ].pszQuestFile = DupeStr(pcCurrentPos);
						break;
					case 3 :  /* known */
						Quests[CurQuest].Known = true;
						break;
				}
			}
		}
	}

	fclose(fp);

	QuestsInitialized = true;
}

void Quests_Close(void)
{
	int16_t iTemp;

	if (!QuestsInitialized) return;


	for (iTemp = 0; iTemp < MAX_QUESTS; iTemp++)
		if (Quests[iTemp].Active) {
			if (Quests[iTemp].pszQuestName) {
				free(Quests[iTemp].pszQuestName);
				Quests[iTemp].pszQuestName = NULL;
			}

			if (Quests[iTemp].pszQuestFile) {
				free(Quests[iTemp].pszQuestFile);
				Quests[iTemp].pszQuestFile = NULL;
			}

			if (Quests[iTemp].pszQuestIndex) {
				free(Quests[iTemp].pszQuestIndex);
				Quests[iTemp].pszQuestIndex = NULL;
			}
		}
}

// ------------------------------------------------------------------------- //

void ClearFlags(char *Flags)
{
	int16_t iTemp;

	for (iTemp = 0; iTemp < 8; iTemp++)
		Flags[iTemp] = 0;
}

static void SetFlag(char *Flags, int16_t WhichFlag)
{
	Flags[ WhichFlag/8 ] |= (1 << (WhichFlag % 8));
}

static void ClearFlag(char *Flags, int16_t WhichFlag)
{
	if (Flags[ WhichFlag/8 ]  & (1 << (WhichFlag % 8)))
		Flags[ WhichFlag/8 ] ^= (1 << (WhichFlag % 8));
}

static bool FlagSet(char *Flags, int16_t WhichFlag)
{
	if (Flags[ WhichFlag/8 ] & (1 << (WhichFlag % 8)))
		return true;
	else
		return false;
}

static bool legal(char *pszAcs, int16_t *iCharsRead)
{
	bool bCurrent = true;   // flag as to whether last pszAcs's were true
	bool UseCurrent = true;
	bool bSoFar = true;
	bool UseNot = false;    // needed to check if we need to switch/NOT
	// next ACS flag
	int16_t iCharsSkipped;  /* number of characters skipped when calling legal()
                 from within this function */
	int16_t iTemp;
	int16_t LastCondition = CD_AND;
	char szIndex[20];
	char *pcCurrentPos;
	char pszAcsCopy[50];
	int32_t GoldAmount;

	strlcpy(pszAcsCopy, pszAcs, sizeof(pszAcsCopy));

	pcCurrentPos = pszAcsCopy;

	while (*pcCurrentPos) {
		switch (*pcCurrentPos) {
			case ')' :  /* end bCurrent conditions */
				pcCurrentPos++;
				if (pcCurrentPos - pszAcs > INT16_MAX)
					System_Error("Unterminated string in legal()");
				if (iCharsRead)
					*iCharsRead = (int16_t)(pcCurrentPos - pszAcs);
				return bSoFar;
			case '(' :  /* start a new set of conditions */
				pcCurrentPos++;
				bCurrent = legal(pcCurrentPos, &iCharsSkipped);
				UseCurrent = true;
				pcCurrentPos += iCharsSkipped;
				break;
			case ' ' :
				pcCurrentPos++;
				UseCurrent = false;
				break;
			case '&' :
				LastCondition = CD_AND;
				pcCurrentPos++;
				UseCurrent = false;
				break;
			case '|' :
				LastCondition = CD_OR;
				pcCurrentPos++;
				UseCurrent = false;
				break;
			case '^' :
				bCurrent = true;
				UseCurrent = true;
				pcCurrentPos++;
				break;
			case '%' :
				bCurrent = false;
				UseCurrent = true;
				pcCurrentPos++;
				break;
			case '!' :
				UseNot = true;
				UseCurrent = false;
				pcCurrentPos++;
				break;
			case 'Q' :  // quest done?
				// get quest num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;
				iTemp = ato16(szIndex, "Quest Done", __func__) - 1;

				// if this quest done, it's true
				if (PClan->QuestsDone[ iTemp/8 ] & (char)(1 << (iTemp%8)))
					bCurrent = true;
				else
					bCurrent = false;
				break;
			case '$' :  // have this much gold?
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;
				GoldAmount = ato32(szIndex, "Gold", __func__);

				if (PClan->Empire.VaultGold >= GoldAmount)
					bCurrent = true;
				else
					bCurrent = false;
				break;
			case 'L' :  // level
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;
				iTemp = ato16(szIndex, "Level", __func__);

				if (PClan->MineLevel == iTemp)
					bCurrent = true;
				else
					bCurrent = false;
				break;
			case 'K' :  // level must be at least this
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;
				iTemp = ato16(szIndex, "MinLevel", __func__);

				if (PClan->MineLevel >= iTemp)
					bCurrent = true;
				else
					bCurrent = false;
				break;
			case 'R' :  // random
				// get random value which is highest
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;
				iTemp = ato16(szIndex, "Random", __func__);

				if (my_random(100) >= (100-iTemp))
					bCurrent = true;
				else
					bCurrent = false;
				break;
			case 'T' :  // TFlag
				// get tflag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;

				if (FlagSet(Quests_TFlags, ato16(szIndex, "TFlag", __func__)))
					bCurrent = true;
				else
					bCurrent = false;

				// od_printf("T%d is %s\n\r", atoi(szIndex), bCurrent ? "true" : "false");
				break;
			case 'P' :  // PFlag
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;

				if (FlagSet(PClan->PFlags, ato16(szIndex, "PFlag", __func__)))
					bCurrent = true;
				else
					bCurrent = false;
				break;
			case 'G' :  // GFlag
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;

				if (FlagSet(Village.Data.GFlags, ato16(szIndex, "GFlag", __func__)))
					bCurrent = true;
				else
					bCurrent = false;
				break;
			case 'D' :  // DFlag
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;

				if (FlagSet(PClan->DFlags, ato16(szIndex, "DFlag", __func__)))
					bCurrent = true;
				else
					bCurrent = false;
				break;
			case 'H' :  // HFlag
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = true;

				if (FlagSet(Village.Data.HFlags, ato16(szIndex, "HFlag", __func__)))
					bCurrent = true;
				else
					bCurrent = false;
				break;
			default:
				pcCurrentPos++;
		}

		if (UseCurrent) {
			if (UseNot) {
				UseNot = false;
				bCurrent = !bCurrent;
			}

			if (LastCondition == CD_OR) {
				if ((bCurrent == true) || (bSoFar == true))
					bSoFar = true;
				else
					bSoFar = false;
			}
			else if (LastCondition == CD_AND) {
				if ((bCurrent==true) && (bSoFar==true))
					bSoFar = true;
				else
					bSoFar = false;
			}
		}
	}

	if (iCharsRead) {
		if (pcCurrentPos - pszAcs > INT16_MAX)
			System_Error("Too many characters read!");
		*iCharsRead = (int16_t)(pcCurrentPos - pszAcs);
	}

	return bSoFar;
}



// ------------------------------------------------------------------------- //

static void JumpToEvent(const char *szLabel, struct FileHeader *FileHeader)
{
	struct EventHeader EventHeader;
	uint8_t hBuf[BUF_SIZE_EventHeader];

	fseek(FileHeader->fp, FileHeader->lStart, SEEK_SET);

	for (;;) {
		// if past end of file or at it, break
		if (ftell(FileHeader->fp) >= FileHeader->lEnd)
			System_Error("Couldn't find event!\n");

		// find event to run first
		fread(hBuf, sizeof(hBuf), 1, FileHeader->fp);
		s_EventHeader_d(hBuf, sizeof(hBuf), &EventHeader);

		if (strcasecmp(EventHeader.szName, szLabel) == 0)
			break;
		else {
			// see start of next event
			fseek(FileHeader->fp, EventHeader.EventSize, SEEK_CUR);
		}
	}
}

// ------------------------------------------------------------------------- //
static void TellQuest(char *pszQuestIndex)
{
	int16_t QuestNum, CurQuest;
	char szString[80];

	// scan for quest in questlist
	QuestNum = -1;
	for (CurQuest = 0; CurQuest < MAX_QUESTS; CurQuest++) {
		if (Quests[CurQuest].Active &&
			strcasecmp(pszQuestIndex, Quests[CurQuest].pszQuestIndex) == 0) {
			// found match
			QuestNum = CurQuest;
			break;
		}
	}
	if (QuestNum == -1) {
		snprintf(szString, sizeof(szString), "Couldn't find quest %s\n\r", pszQuestIndex);
		rputs(szString);
		return;
	}

	// set bit
	if (PClan->QuestsKnown[ QuestNum/8 ] & (1<<(QuestNum%8)))
		return;  // already knew about that quest

	// set it
	PClan->QuestsKnown[ QuestNum/8 ] |= (1<<(QuestNum%8));

	// tell user
	snprintf(szString, sizeof(szString), "\n|0CYour clan now knows of |0B%s|0C.\n",
			Quests[QuestNum].pszQuestName);
	rputs(szString);
}


// ------------------------------------------------------------------------- //

bool RunEvent(bool QuoteToggle, char *szEventFile, char *szEventName,
			  struct NPCInfo *NPCInfo, char *szNPCIndex)
{
	char szString[256], szLegal[155], szLabel[32];
	char szLabel1[32], szLabel2[32], szLabel3[32], szClanName[25];
	struct EventHeader EventHeader;
	struct FileHeader FileHeader;
	char szNPCFileName[25], *apszLabels[MAX_OPTIONS];
	char szText[255];
	unsigned char DataLength;
	char CommandType, OldCommType = 0, cInput, *pcCurrentPos;
	const char *pszOptionNames[MAX_OPTIONS];
	char szKeys[MAX_OPTIONS+1];      // extra space for '\0'
	char Buffer[256];               // not same as ecomp.c's buffer
	bool Done;
	int16_t iTemp, CurOption, WhichEnemy, EventsFound, ChosenEvent,
	NumOptions, WhichOption;
	long OldOffset = 0;
	bool QuestDone = false;
	struct clan EnemyClan = {0};
	bool EndedChat = false;
	int16_t CurMember, PercentGold, FightResult, EmptySlot;
	int32_t GoldAmount, XPAmount;
	struct NPCNdx NPCNdx;
	uint8_t hBuf[BUF_SIZE_EventHeader];

	// reset temp flags
	if (QuoteToggle == false) // clear flags for event files but not quotes
		ClearFlags(Quests_TFlags);

	rputs("|0C");


	/* initialize file */
	MyOpen(szEventFile, "rb", &FileHeader);
	if (FileHeader.fp == NULL) {
		snprintf(szString, sizeof(szString), "-> %s\n", szEventFile);
		rputs(szString);
		rputs("Error opening event file.\n%P");

		return false;
	}

	if (*szEventName) {
		JumpToEvent(szEventName, &FileHeader);
	}
	else {
		EventsFound = 0;

		// find out how many events in the file
		for (;;) {
			// if at end of file or past it, stop
			if (ftell(FileHeader.fp) >= FileHeader.lEnd)
				break;

			if (fread(hBuf, sizeof(hBuf), 1, FileHeader.fp) == 0)
				break;
			s_EventHeader_d(hBuf, sizeof(hBuf), &EventHeader);

			if (EventHeader.Event)
				EventsFound++;

			// seek next event
			fseek(FileHeader.fp, EventHeader.EventSize, SEEK_CUR);
		}

		// choose one randomly and seek it
		ChosenEvent = (int16_t)my_random(EventsFound);

		fseek(FileHeader.fp, FileHeader.lStart, SEEK_SET);

		EventsFound = 0;
		// find out how many events in the file
		for (;;) {
			// if at end...
			if (ftell(FileHeader.fp) >= FileHeader.lEnd)
				break;

			if (fread(hBuf, sizeof(hBuf), 1, FileHeader.fp) == 0)
				break;
			s_EventHeader_d(hBuf, sizeof(hBuf), &EventHeader);

			if (EventHeader.Event && EventsFound == ChosenEvent)
				break;

			if (EventHeader.Event)
				EventsFound++;

			// seek next event
			fseek(FileHeader.fp, EventHeader.EventSize, SEEK_CUR);
		}
	}

	/* initialize optionnames */
	// initialize labels
	for (iTemp = 0; iTemp < MAX_OPTIONS; iTemp++) {
		pszOptionNames[iTemp] = NULL;
		szKeys[iTemp] = 0;
		apszLabels[iTemp] = NULL;
	}

	/* initialize enemyclan to fight */
	NPC_ResetNPCClan(&EnemyClan);

	Done = false;
	while (!Done) {
		if (ftell(FileHeader.fp) >= FileHeader.lEnd) {
			DisplayStr("K-Error\n%P");
			break;
		}

		// read in command type
		fread(&CommandType, sizeof(char), 1, FileHeader.fp);

		// read in szLegal
		szLegal[0] = 0;
		fread(&DataLength, sizeof(char), 1, FileHeader.fp);
		fread(szLegal, DataLength, 1, FileHeader.fp);

		// if not legal, skip
		if (szLegal[0] && legal(szLegal, NULL) == false) {
			// skip it
			fread(&DataLength, sizeof(char), 1, FileHeader.fp);
			fseek(FileHeader.fp, DataLength, SEEK_CUR);
			continue;
		}

		switch (CommandType) {
				// Following commands take one parameter, a string
			case 2 :  // Text
			case 6 :  // Prompt
			case 10:  // AddNews
			case 8  : // TellQuest
			case 13 : // Chat
			case 14 : // SetFlag
			case 15 : // ClearFlag
			case 16 : // Jump
			case 17 : // Heal
			case 18 : // TakeGold
			case 19 : // GiveGold
			case 20 : // GiveXP
			case 21 : // GiveItem
			case 23 : // GiveFight
			case 24 : // GiveFollowers
			case 25 : // GivePoints
			case 28 : // TellTopic
			case 32 :   // Display
				// read in text
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);
				fread(szText, DataLength, 1, FileHeader.fp);

				switch (CommandType) {
					case 2 :  // Text
						if (szText[0])
							rputs(szText);

						rputs("\n");
						break;
					case 6 :  // Prompt
						rputs(szText);
						break;
					case 10:  // AddNews
						News_AddNews(szText);
						News_AddNews("\n\n");
						break;
					case 8  : // TellQuest
						TellQuest(szText);
						break;
					case 13 : // Chat
						NPC_ChatNPC(szText);
						break;
					case 14 : // SetFlag
						// first value is the type of flag
						pcCurrentPos = szText;
						switch (toupper(*pcCurrentPos)) {
							case 'D' :
								SetFlag(PClan->DFlags, ato16(pcCurrentPos+1, "Set DFlag", __func__));
								break;
							case 'P' :
								SetFlag(PClan->PFlags, ato16(pcCurrentPos+1, "Set PFlag", __func__));
								break;
							case 'T' :
								SetFlag(Quests_TFlags, ato16(pcCurrentPos+1, "Set TFlag", __func__));
								break;
							case 'G' :
								SetFlag(Village.Data.GFlags, ato16(pcCurrentPos+1, "Set GFlag", __func__));
								break;
							case 'H' :
								SetFlag(Village.Data.HFlags, ato16(pcCurrentPos+1, "Set HFlag", __func__));
								break;
						}
						break;
					case 15 : // ClearFlag
						pcCurrentPos = szText;
						switch (toupper(*pcCurrentPos)) {
							case 'D' :
								ClearFlag(PClan->DFlags, ato16(pcCurrentPos+1, "Clear DFlag", __func__));
								break;
							case 'P' :
								ClearFlag(PClan->PFlags, ato16(pcCurrentPos+1, "Clear PFlag", __func__));
								break;
							case 'T' :
								ClearFlag(Quests_TFlags, ato16(pcCurrentPos+1, "Clear TFlag", __func__));
								break;
							case 'G' :
								ClearFlag(Village.Data.GFlags, ato16(pcCurrentPos+1, "Clear GFlag", __func__));
								break;
							case 'H' :
								ClearFlag(Village.Data.HFlags, ato16(pcCurrentPos+1, "Clear HFlag", __func__));
								break;
						}
						break;
					case 16 : // Jump
						JumpToEvent(szText, &FileHeader);
						break;
					case 17 : // Heal
						for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
							if (PClan->Member[iTemp]) {
								if (szText[0] == 0) {
									if (PClan->Member[iTemp]->Status == Here)
										PClan->Member[iTemp]->HP =
											PClan->Member[iTemp]->MaxHP;
								}
								else if (strcasecmp(szText, "SP") == 0) {
									if (PClan->Member[iTemp]->Status == Here)
										PClan->Member[iTemp]->SP =
											PClan->Member[iTemp]->MaxSP;
								}
							}
						}
						break;
					case 18 : // TakeGold
						pcCurrentPos = szText;
						if (*pcCurrentPos == '%') {
							PercentGold = ato16(&pcCurrentPos[1], "Precent Gold", __func__);
							if (PercentGold < 0)
								PercentGold = 0;
							if (PercentGold > 100)
								PercentGold = 100;

							GoldAmount = (PClan->Empire.VaultGold*PercentGold)/100L;
						}
						else
							GoldAmount = ato32(pcCurrentPos, "Take Gold", __func__);

						PClan->Empire.VaultGold -= GoldAmount;
						break;
					case 19 : // GiveGold
						GoldAmount = ato32(szText, "Give Gold", __func__);
						PClan->Empire.VaultGold += GoldAmount;
						break;
					case 20 : // GiveXP
						XPAmount = ato32(szText, "Give XP", __func__);

						for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
							if (PClan->Member[iTemp] && PClan->Member[iTemp]->Status == Here)
								PClan->Member[iTemp]->Experience +=
									XPAmount;
						Fight_CheckLevelUp();
						break;
					case 21 : // GiveItem
						Items_GiveItem(szText);
						break;
					case 23 : // GiveFight
						PClan->FightsLeft += ato16(szText, "Give Fight", __func__);
						break;
					case 24 : // GiveFollowers
						PClan->Empire.Army.Followers += atol(szText);
						break;
					case 25 : // GivePoints
						PClan->Points += atol(szText);
						break;
					case 28 : // TellTopic
						for (iTemp = 0; iTemp < MAX_TOPICS; iTemp++) {
							if (NPCInfo->Topics[iTemp].Active == false)
								continue;

							if (strcasecmp(NPCInfo->Topics[iTemp].szFileName,
										szText) == 0) {
								// found quote, set it to known
								NPCInfo->Topics[iTemp].Known = true;
								break;
							}
						}
						break;
					case 32 :   // Display
						Display(szText);
						break;
				}
				break;
			case 3 :  // Option keyword
			case 33 : // GetKey keyword

				// get user input

				for (iTemp = 0; iTemp < MAX_OPTIONS; iTemp++) {
					if (pszOptionNames[iTemp]) {
						free((void*)pszOptionNames[iTemp]);
						pszOptionNames[iTemp] = NULL;
					}
					szKeys[iTemp] = 0;
				}


				// get first key allowed
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);
				fread(Buffer, DataLength, 1, FileHeader.fp);
				strlcpy(szLabel, &Buffer[1], sizeof(szLabel));

				pszOptionNames[0] = DupeStr(szLabel);

				szKeys[0] = Buffer[0];

				for (CurOption = 1; CurOption < MAX_OPTIONS;) {
					OldOffset = ftell(FileHeader.fp);

					OldCommType = CommandType;

					// read in command type
					fread(&CommandType, sizeof(char), 1, FileHeader.fp);

					// if not an Option command, skip back and quit
					if (CommandType != OldCommType) {
						break;
					}

					// read in szLegal
					szLegal[0] = 0;
					fread(&DataLength, sizeof(char), 1, FileHeader.fp);
					fread(szLegal, DataLength, 1, FileHeader.fp);

					// if szLegal do legal() thing later
					if (szLegal[0] && legal(szLegal, NULL) == false) {
						// skip it
						fread(&DataLength, sizeof(char), 1, FileHeader.fp);
						fseek(FileHeader.fp, DataLength, SEEK_CUR);
						continue;
					}
					else {
						// else is legal, allow as an option

						// get key and label
						fread(&DataLength, sizeof(char), 1, FileHeader.fp);
						fread(Buffer, DataLength, 1, FileHeader.fp);
						strlcpy(szLabel, &Buffer[1], sizeof(szLabel));
						szKeys[CurOption] = Buffer[0];

						/* get this optionname */
						pszOptionNames[CurOption] = DupeStr(szLabel);
					}

					CurOption++;
				}
				szKeys[CurOption] = 0;

				// get input from user

				cInput = toupper(od_get_answer(szKeys) & 0x7f) & 0x7f;

				if (OldCommType != 33) {
					snprintf(szString, sizeof(szString), "%c\n\r", cInput);
					rputs(szString);
				}

				/* see which key that was */
				for (CurOption = 0; CurOption < MAX_OPTIONS; CurOption++) {
					if (toupper(szKeys[CurOption]) == cInput) {
						/* found which key */
						break;
					}
				}

				/* if that option has nextline to "jump" to, keep
				   going in same block of data */
				if (strcasecmp(pszOptionNames[CurOption], "NextLine") == 0) {
					fseek(FileHeader.fp, OldOffset, SEEK_SET);
					break;
				}

				/* if that option has "STOP" to "jump" to, stop */
				else if (strcasecmp(pszOptionNames[CurOption], "Stop") == 0) {
					Done = true;
					break;
				}

				/* else, look for new spot to jump to */
				JumpToEvent(pszOptionNames[CurOption], &FileHeader);
				break;
			case 7  : // Fight
				// read in 3 labels

				// skip in datalength
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);

				fread(&DataLength, sizeof(char), 1, FileHeader.fp);
				fread(szLabel1, DataLength, 1, FileHeader.fp);

				fread(&DataLength, sizeof(char), 1, FileHeader.fp);
				fread(szLabel2, DataLength, 1, FileHeader.fp);

				fread(&DataLength, sizeof(char), 1, FileHeader.fp);
				fread(szLabel3, DataLength, 1, FileHeader.fp);

				if (strcasecmp(szLabel3, "NoRun") == 0)
					FightResult = Fight_Fight(PClan, &EnemyClan, false, false, false);
				else  // can run
					FightResult = Fight_Fight(PClan, &EnemyClan, false, true, false);

				/* set all spells to 0 */
				for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
					if (PClan->Member[CurMember] == NULL)
						continue;

					for (iTemp = 0; iTemp < 10; iTemp++)
						PClan->Member[CurMember]->SpellsInEffect[iTemp].SpellNum = -1;
				}


				/* free up memory used by undead */
				for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
					if (PClan->Member[iTemp] &&
							PClan->Member[iTemp]->Undead) {
						free(PClan->Member[iTemp]);
						PClan->Member[iTemp] = NULL;
					}
				}

				// free up enemies found
				FreeClanMembers(&EnemyClan);

				if (FightResult == FT_WON) {
					// won
					strlcpy(szString, szLabel1, sizeof(szString));
				}
				else if (FightResult == FT_LOST) {
					// lost
					strlcpy(szString, szLabel2, sizeof(szString));
				}
				else if (FightResult == FT_RAN) {
					// ran away

					if (strcasecmp(szLabel3, "NoRun") == 0)
						break;
					else
						strlcpy(szString, szLabel3, sizeof(szString));
				}

				// if label is "nextline", stay here
				if (strcasecmp(szString, "nextline") == 0)
					break;
				// if label is "stop", end scrpt
				else if (strcasecmp(szString, "Stop") == 0) {
					Done = true;
					break;
				}
				else
					JumpToEvent(szString, &FileHeader);
				break;
			case 4  : // End
			case 9  : // DoneQuest
			case 22 : // Pause
			case 29 : // JoinClan
			case 30 : // EndChat
				// skip in datalength (0)
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);

				if (CommandType == 4)
					Done = true;
				else if (CommandType == 9) {
					rputs("\n|14Quest successfully completed!\n");
					Done = true;
					QuestDone = true;
				}
				else if (CommandType == 22) {
					door_pause();
				}
				else if (CommandType == 29) {
					NPC_GetNPCNdx(NPCInfo, &NPCNdx);

					if (NPCNdx.InClan) {
						if (NPCNdx.ClanID[0] == PClan->ClanID[0] &&
								NPCNdx.ClanID[1] == PClan->ClanID[1]) {
							rputs("|0BI am in your clan already.\n");
							break;
						}
						if (NPCInfo->Loyalty == 10) {
							rputs("|0BSorry, I am already in a clan and cannot join yours.\n");
							break;
						}
						else {
							// add it on as a new topic
							NPCInfo->Topics[ NPCInfo->KnownTopics ].Active = true;
							NPCInfo->Topics[ NPCInfo->KnownTopics ].Known = true;
							NPCInfo->Topics[ NPCInfo->KnownTopics ].ClanInfo = true;

							GetClanNameID(szClanName, NPCNdx.ClanID);
							strlcpy(NPCInfo->Topics[ NPCInfo->KnownTopics ].szName, szClanName, sizeof(NPCInfo->Topics[ NPCInfo->KnownTopics ].szName));

							snprintf(szString, sizeof(szString), "I'm already in a clan called %s.\n\n", szClanName);
							rputs(szString);
						}
					}
					else {
						if (NumMembers(PClan, false) == 6) {
							rputs("Your clan already has the max. amount of members in it.\n\n");
							break;
						}

						// set NPC.DAT file so it says he is in a clan
						// right now and set which clan (this one)

						NPCNdx.InClan = true;
						NPCNdx.ClanID[0] = PClan->ClanID[0];
						NPCNdx.ClanID[1] = PClan->ClanID[1];

						NPC_AddNPCMember(szNPCIndex);

						snprintf(szString, sizeof(szString), "|0B%s |0Cjoins your clan!\n",
								NPCInfo->szName);
						rputs(szString);

						NPC_UpdateNPCNdx(szNPCIndex, &NPCNdx);
					}
				}
				else if (CommandType == 30) {
					Done = true;
					EndedChat = true;
				}
				break;
			case 11 : // AddEnemy
				// read in text
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);
				fread(Buffer, DataLength, 1, FileHeader.fp);
				WhichEnemy = Buffer[ DataLength-1 ];

				// find slot in enemy clan
				for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
					if (EnemyClan.Member[iTemp] == NULL) break;

				EmptySlot = iTemp;

				EnemyClan.Member[EmptySlot] = malloc(sizeof(struct pc));
				CheckMem(EnemyClan.Member[EmptySlot]);

				strlcpy(szNPCFileName, Buffer, sizeof(szNPCFileName));

				NPC_GetNPC(EnemyClan.Member[EmptySlot], szNPCFileName, WhichEnemy);
				EnemyClan.Member[EmptySlot]->MyClan = &EnemyClan;
				break;
			case 31 :  // Input keyword

				for (iTemp = 0; iTemp < MAX_OPTIONS; iTemp++) {
					if (pszOptionNames[iTemp]) {
						free((void*)pszOptionNames[iTemp]);
						pszOptionNames[iTemp] = NULL;
					}

					// free labels
					if (apszLabels[iTemp]) {
						free(apszLabels[iTemp]);
						apszLabels[iTemp] = NULL;
					}

				}

				// get first input
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);

				// this datalength represents length of label
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);

				apszLabels[0] = malloc(DataLength);
				CheckMem(apszLabels[0]);
				fread(apszLabels[0], DataLength, 1, FileHeader.fp);

				// this datalength represents length of option
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);

				pszOptionNames[0] = malloc(DataLength);
				CheckMem(pszOptionNames[0]);
				fread((char*)pszOptionNames[0], DataLength, 1, FileHeader.fp);

				for (CurOption = 1; CurOption < MAX_OPTIONS;) {
					OldOffset = ftell(FileHeader.fp);

					// read in command type
					fread(&CommandType, sizeof(char), 1, FileHeader.fp);

					// if not an Input command, skip back and quit
					if (CommandType != 31) {
						break;
					}

					// read in szLegal
					szLegal[0] = 0;
					fread(&DataLength, sizeof(char), 1, FileHeader.fp);
					fread(szLegal, DataLength, 1, FileHeader.fp);

					// see if legal, if not, skip
					if (szLegal[0] && legal(szLegal, NULL) == false) {
						// skip it
						fread(&DataLength, sizeof(char), 1, FileHeader.fp);
						fseek(FileHeader.fp, DataLength, SEEK_CUR);
						continue;
					}
					else {
						// else is legal, read in label + option name
						fread(&DataLength, sizeof(char), 1, FileHeader.fp);

						// this datalength represents length of label
						fread(&DataLength, sizeof(char), 1, FileHeader.fp);

						apszLabels[CurOption] = malloc(DataLength);
						CheckMem(apszLabels[CurOption]);
						fread(apszLabels[CurOption], DataLength, 1, FileHeader.fp);

						// this datalength represents length of option
						fread(&DataLength, sizeof(char), 1, FileHeader.fp);

						pszOptionNames[CurOption] = malloc(DataLength);
						CheckMem(pszOptionNames[CurOption]);
						fread((char*)pszOptionNames[CurOption], DataLength, 1, FileHeader.fp);

					}

					CurOption++;
				}
				NumOptions = CurOption;

				// get input from user
				GetStringChoice(pszOptionNames, NumOptions, "|0GChoose one\n|0E> |07", &WhichOption,
								true, DT_LONG, false);

				/* if that option has nextline to "jump" to, keep
				   going in same block of data */
				if (strcasecmp(apszLabels[WhichOption], "NextLine") == 0) {
					fseek(FileHeader.fp, OldOffset, SEEK_SET);
					break;
				}

				/* if that option has "STOP" to "jump" to, stop */
				else if (strcasecmp(apszLabels[WhichOption], "Stop") == 0) {
					Done = true;
					break;
				}

				if (WhichOption == -1) {
					fseek(FileHeader.fp, OldOffset, SEEK_SET);
					break;
				}

				/* else, look for new spot to jump to */
				JumpToEvent(apszLabels[WhichOption], &FileHeader);
				break;
			case 26 : // EndQ
				// no data, just end
				Done = true;
				break;
		}
	}

	/* free optionnames */
	/* free labels */
	for (iTemp = 0; iTemp < MAX_OPTIONS; iTemp++) {
		if (pszOptionNames[iTemp]) {
			free((void*)pszOptionNames[iTemp]);
			pszOptionNames[iTemp] = NULL;
		}

		if (apszLabels[iTemp]) {
			free(apszLabels[iTemp]);
			apszLabels[iTemp] = NULL;
		}
	}

	fclose(FileHeader.fp);

	// free up enemies found
	FreeClanMembers(&EnemyClan);

	// reset health

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (PClan->Member[iTemp] && PClan->Member[iTemp]->Status == Here) {
			PClan->Member[iTemp]->HP = PClan->Member[iTemp]->MaxHP;
			PClan->Member[iTemp]->MaxSP = PClan->Member[iTemp]->MaxSP;
		}
	}



	if (QuoteToggle) {
		if (EndedChat) {
			door_pause();
			return true;
		}
		else
			return false;
	}
	else
		return (QuestDone);
}

// ------------------------------------------------------------------------- //

void Quests_GoQuest(void)
{
	int16_t iTemp, TotalQuests = 0, WhichQuest, NumQuestsDone, QuestIndex[64];
	bool QuestDone, QuestKnown;
	char KnownBitSet, DoneBitSet;
	char szString[50], cInput;

	// tell him how many quests he's done right here for now since i'm too lazy
	for (iTemp = 0, NumQuestsDone = 0; iTemp < MAX_QUESTS; iTemp++) {
		if (PClan->QuestsDone[ iTemp/8 ] & (1 << (iTemp%8)))
			NumQuestsDone++;
	}

	for (;;) {
		snprintf(szString, sizeof(szString), " |0CQuests Completed:  |0B%d\n", NumQuestsDone);
		rputs(szString);
		rputs(ST_LONGLINE);

		/* List quests known and not completed */
		/* <pause> where needed */
		TotalQuests = 0;
		for (iTemp = 0; iTemp < MAX_QUESTS; iTemp++) {
			KnownBitSet =
				(PClan->QuestsKnown[ iTemp/8 ] & (1 << (iTemp%8)) ||
				 (Quests[iTemp].Known && Quests[iTemp].Active));
			DoneBitSet  = (char)(PClan->QuestsDone[ iTemp / 8 ] & (unsigned char)(1U << (iTemp % 8)));

			// quest known? AND not complete?
			if (KnownBitSet && !DoneBitSet && Quests[iTemp].pszQuestName) {
				// show quest then
				snprintf(szString, sizeof(szString), " |0A(|0B%c|0A) |0C%s\n", TotalQuests + 'A',
						Quests[iTemp].pszQuestName);
				rputs(szString);
				QuestIndex[TotalQuests] = iTemp;
				TotalQuests++;
			}
		}

		if (TotalQuests == 0) {
			rputs("\n|12You do not know of any quests.\n%P");
			return;
		}

		rputs(ST_LONGLINE);
		rputs(" |0GWhich quest? (Enter=abort)|0E> |0F");
		/* get choice from user on which quest to complete */
		for (;;) {
			cInput = toupper(od_get_key(true) & 0x7f) & 0x7f;

			if (cInput == '\r' || cInput == '\n') {
				rputs(ST_ABORTED);
				return;
			}

			/* run QUEST.EVT block here */
			WhichQuest = cInput - 'A';
			if (WhichQuest >= 0 && WhichQuest < TotalQuests) {
				break;
			}
		}

		if (Quests[ QuestIndex[WhichQuest] ].pszQuestIndex == NULL) {
			rputs("\n|12Quest not found.\n%P");
			return;
		}

		QuestKnown = PClan->QuestsKnown[ QuestIndex[WhichQuest]/8 ] & (char)(1 << (QuestIndex[WhichQuest]%8)) ||
					 (Quests[QuestIndex[WhichQuest]].Known && Quests[QuestIndex[WhichQuest]].Active);
		QuestDone  = PClan->QuestsDone[ QuestIndex[WhichQuest]/8 ] & (char)(1 << (QuestIndex[WhichQuest]%8));

		//od_printf("Comparing with %d\n", (1 << (QuestIndex[WhichQuest]%8)));

		if (QuestKnown == false || QuestDone || !Quests[QuestIndex[WhichQuest]].pszQuestName) {
			rputs("\n|12Quest not found.\n%P");
			return;
		}

		// display name of quest
		rputs(Quests[QuestIndex[WhichQuest]].pszQuestName);
		rputs("\n\n");

		// show help
		Help(Quests[ QuestIndex[WhichQuest] ].pszQuestName, "quests.hlp");
		if (YesNo("\n|0SGo on this quest?") == YES)
			break;

		rputs("\n");
	}

	PClan->QuestToday = true;

	/* if successful (returns true), set that quest bit to done */
	if (RunEvent(false,
				 Quests[QuestIndex[WhichQuest]].pszQuestFile, Quests[QuestIndex[WhichQuest]].pszQuestIndex,
				 NULL, NULL)) {
		// set bit since quest completed
		PClan->QuestsDone[ QuestIndex[WhichQuest]/8 ] |= (1<<(QuestIndex[WhichQuest]%8));
	}
	door_pause();
}
