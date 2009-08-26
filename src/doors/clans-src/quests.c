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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef __unix__
#include "unix_wrappers.h"
#endif

#include <OpenDoor.h>
#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "parsing.h"
#include "help.h"
#include "reg.h"
#include "door.h"
#include "video.h"
#include "myopen.h"
#include "fight.h"
#include "input.h"
#include "items.h"
#include "user.h"
#include "news.h"
#include "npc.h"
#include "k_quests.h"
#include "quests.h"

#define CD_OR                   1
#define CD_AND                  2

struct PACKED EventHeader {
	char szName[30];
	long EventSize;
	__BOOL Event;           // 1 = Event, 0 = Result
} PACKED;

struct Quest Quests[MAX_QUESTS];

extern struct village Village;
char Quests_TFlags[8];          // temp. flags
extern struct config *Config;
extern struct clan *PClan;
extern struct Language *Language;
BOOL QuestsInitialized = FALSE;
extern __BOOL Verbose;

// ------------------------------------------------------------------------- //

void Quests_Init(void)
{
	/* read in quests file and allocate memory for quest names */
	_INT16 iTemp, CurQuest;
	FILE *fp;
	char szLine[128], *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	unsigned _INT16 uCount;
	_INT16 iKeyWord;

	if (Verbose) {
		DisplayStr("> Quests_Init()\n");
		delay(500);
	}


	// init quest names
	for (iTemp = 0; iTemp < MAX_QUESTS; iTemp++) {
		Quests[iTemp].Active = FALSE;
		Quests[iTemp].pszQuestName = NULL;
		Quests[iTemp].pszQuestIndex = NULL;
		Quests[iTemp].pszQuestFile = NULL;
		Quests[iTemp].Known = FALSE;
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
			if (stricmp(szToken, papszQuestKeyWords[iKeyWord]) == 0) {
				/* Process config token */
				switch (iKeyWord) {
					case 0 :  /* name of quest */
						if (CurQuest == MAX_QUESTS)
							break;

						/* NO MORE REG
						              if (CurQuest == 10 &&
						                (IsRegged(Config->szSysopName, Config->szBBSName, Config->szRegcode) == NFALSE ||
						                IsRegged(Config->szSysopName, Config->szBBSName, Config->szRegcode) != NTRUE))
						              {
						                // can't load more than 10 in unreg
						                break;
						              }
						*/

						CurQuest++;

						Quests[CurQuest].Active = TRUE;
						Quests[CurQuest].Known  = FALSE;

						Quests[ CurQuest ].pszQuestName =
							malloc(strlen(pcCurrentPos) + 1);
						CheckMem(Quests[ CurQuest ].pszQuestName);
						strcpy(Quests[ CurQuest ].pszQuestName, pcCurrentPos);
						break;
					case 1 :  /* index of quest */
						/* make room for questname in mem */
						Quests[ CurQuest ].pszQuestIndex =
							malloc(strlen(pcCurrentPos) + 1);
						CheckMem(Quests[ CurQuest ].pszQuestIndex);
						strcpy(Quests[ CurQuest ].pszQuestIndex, pcCurrentPos);
						break;
					case 2 :  /* filename to use */
						/* make room for questname in mem */
						Quests[ CurQuest ].pszQuestFile =
							malloc(strlen(pcCurrentPos) + 1);
						CheckMem(Quests[ CurQuest ].pszQuestFile);
						strcpy(Quests[ CurQuest ].pszQuestFile, pcCurrentPos);
						break;
					case 3 :  /* known */
						Quests[CurQuest].Known = TRUE;
						break;
				}
			}
		}
	}

	fclose(fp);

	QuestsInitialized = TRUE;
	(void)uCount;
}

void Quests_Close(void)
{
	_INT16 iTemp;

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
	_INT16 iTemp;

	for (iTemp = 0; iTemp < 8; iTemp++)
		Flags[iTemp] = 0;
}

void SetFlag(char *Flags, _INT16 WhichFlag)
{
	Flags[ WhichFlag/8 ] |= (1 << (WhichFlag % 8));
}

void ClearFlag(char *Flags, _INT16 WhichFlag)
{
	if (Flags[ WhichFlag/8 ]  & (1 << (WhichFlag % 8)))
		Flags[ WhichFlag/8 ] ^= (1 << (WhichFlag % 8));
}

BOOL FlagSet(char *Flags, _INT16 WhichFlag)
{
	if (Flags[ WhichFlag/8 ] & (1 << (WhichFlag % 8)))
		return TRUE;
	else
		return FALSE;
}

BOOL legal(char *pszAcs, _INT16 *iCharsRead)
{
	BOOL bCurrent = TRUE;   // flag as to whether last pszAcs's were TRUE
	BOOL UseCurrent = TRUE;
	BOOL bSoFar = TRUE;
	BOOL UseNot = FALSE;    // needed to check if we need to switch/NOT
	// next ACS flag
	_INT16 iCharsSkipped;  /* number of characters skipped when calling legal()
                 from within this function */
	_INT16 iTemp;
	_INT16 LastCondition = CD_AND;
	char szIndex[20];
	unsigned char c;
	char *pcCurrentPos;
	char *pszString;
	_INT16 iFirstValue, iSecondValue;
	char pszAcsCopy[50];
	long GoldAmount;

	strcpy(pszAcsCopy, pszAcs);

	pcCurrentPos = pszAcsCopy;

	while (*pcCurrentPos) {
		switch (*pcCurrentPos) {
			case ')' :  /* end bCurrent conditions */
				pcCurrentPos++;
				*iCharsRead = pcCurrentPos - pszAcs;
				return bSoFar;
			case '(' :  /* start a new set of conditions */
				pcCurrentPos++;
				bCurrent = legal(pcCurrentPos, &iCharsSkipped);
				UseCurrent = TRUE;
				pcCurrentPos += iCharsSkipped;
				break;
			case ' ' :
				pcCurrentPos++;
				UseCurrent = FALSE;
				break;
			case '&' :
				LastCondition = CD_AND;
				pcCurrentPos++;
				UseCurrent = FALSE;
				break;
			case '|' :
				LastCondition = CD_OR;
				pcCurrentPos++;
				UseCurrent = FALSE;
				break;
			case '^' :
				bCurrent = TRUE;
				UseCurrent = TRUE;
				pcCurrentPos++;
				break;
			case '%' :
				bCurrent = FALSE;
				UseCurrent = TRUE;
				pcCurrentPos++;
				break;
			case '!' :
				UseNot = TRUE;
				UseCurrent = FALSE;
				pcCurrentPos++;
				break;
			case 'Q' :  // quest done?
				// get quest num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;
				iTemp = atoi(szIndex) - 1;

				// if this quest done, it's true
				if (PClan->QuestsDone[ iTemp/8 ] & (char)(1 << (iTemp%8)))
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			case '$' :  // have this much gold?
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;
				GoldAmount = atol(szIndex);

				if (PClan->Empire.VaultGold >= GoldAmount)
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			case 'L' :  // level
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;
				iTemp = atoi(szIndex);

				if (PClan->MineLevel == iTemp)
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			case 'K' :  // level must be at least this
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;
				iTemp = atoi(szIndex);

				if (PClan->MineLevel >= iTemp)
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			case 'R' :  // random
				// get random value which is highest
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;
				iTemp = atoi(szIndex);

				if (RANDOM(100) >= (100-iTemp))
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			case 'T' :  // TFlag
				// get tflag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;

				if (FlagSet(Quests_TFlags, atoi(szIndex)))
					bCurrent = TRUE;
				else
					bCurrent = FALSE;

				// od_printf("T%d is %s\n\r", atoi(szIndex), bCurrent ? "true" : "false");
				break;
			case 'P' :  // PFlag
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;

				if (FlagSet(PClan->PFlags, atoi(szIndex)))
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			case 'G' :  // GFlag
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;

				if (FlagSet(Village.Data->GFlags, atoi(szIndex)))
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			case 'D' :  // DFlag
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;

				if (FlagSet(PClan->DFlags, atoi(szIndex)))
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			case 'H' :  // HFlag
				// get flag num
				pcCurrentPos++;
				iTemp = 0;
				while (isdigit(*pcCurrentPos))
					szIndex[iTemp++] = *(pcCurrentPos++);
				szIndex[iTemp] = 0;

				UseCurrent = TRUE;

				if (FlagSet(Village.Data->HFlags, atoi(szIndex)))
					bCurrent = TRUE;
				else
					bCurrent = FALSE;
				break;
			default:
				pcCurrentPos++;
		}

		if (UseCurrent) {
			if (UseNot) {
				UseNot = FALSE;
				bCurrent = !bCurrent;
			}

			if (LastCondition == CD_OR) {
				if ((bCurrent == TRUE) || (bSoFar == TRUE))
					bSoFar = TRUE;
				else
					bSoFar = FALSE;
			}
			else if (LastCondition == CD_AND) {
				if ((bCurrent==TRUE) && (bSoFar==TRUE))
					bSoFar = TRUE;
				else
					bSoFar = FALSE;
			}
		}
	}

	if (iCharsRead)
		*iCharsRead = pcCurrentPos - pszAcs;

	(void)iFirstValue;
	(void)c;
	(void)pszString;
	(void)iSecondValue;
	return bSoFar;
}



// ------------------------------------------------------------------------- //

void JumpToEvent(char *szLabel, struct FileHeader *FileHeader)
{
	struct EventHeader EventHeader;

	fseek(FileHeader->fp, FileHeader->lStart, SEEK_SET);

	for (;;) {
		// if past end of file or at it, break
		if (ftell(FileHeader->fp) >= FileHeader->lEnd) {
			printf("Couldn't find event!\n");
			od_exit(0, FALSE);
		}

		// find event to run first
		fread(&EventHeader, sizeof(struct EventHeader), 1, FileHeader->fp);

		if (stricmp(EventHeader.szName, szLabel) == 0)
			break;
		else {
			// see start of next event
			fseek(FileHeader->fp, EventHeader.EventSize, SEEK_CUR);
		}
	}
}

// ------------------------------------------------------------------------- //
void TellQuest(char *pszQuestIndex)
{
	_INT16 QuestNum, CurQuest, iTemp;
	char szString[80];

	// scan for quest in questlist
	QuestNum = -1;
	for (CurQuest = 0; CurQuest < MAX_QUESTS; CurQuest++) {
		if (Quests[CurQuest].Active &&
				stricmp(pszQuestIndex, Quests[CurQuest].pszQuestIndex) == 0) {
			// found match
			QuestNum = CurQuest;
			break;
		}
	}
	if (QuestNum == -1) {
		sprintf(szString, "Couldn't find quest %s\n\r", pszQuestIndex);
		rputs(szString);
		return;
	}

	// set bit
	if (PClan->QuestsKnown[ QuestNum/8 ] & (1<<(QuestNum%8)))
		return;  // already knew about that quest

	// set it
	PClan->QuestsKnown[ QuestNum/8 ] |= (1<<(QuestNum%8));

	// tell user
	sprintf(szString, "\n|0CYour clan now knows of |0B%s|0C.\n",
			Quests[QuestNum].pszQuestName);
	rputs(szString);
	(void)iTemp;
}


// ------------------------------------------------------------------------- //

BOOL RunEvent(BOOL QuoteToggle, char *szEventFile, char *szEventName,
			  struct NPCInfo *NPCInfo, char *szNPCIndex)
{
	char *szString, *pcBrace, *szLegal, *szLabel;
	char *szLabel1, *szLabel2, *szLabel3, szClanName[25];
	struct EventHeader EventHeader;
	struct FileHeader FileHeader;
	char szNPCFileName[25], *apszLabels[MAX_OPTIONS];
	char szText[255];
	char CommandType, OldCommType = 0, DataLength, cInput, *pcCurrentPos;
	char *pszOptionNames[MAX_OPTIONS];
	char szKeys[MAX_OPTIONS+1], Key;      // extra space for '\0'
	char *Buffer;               // not same as ecomp.c's buffer
	BOOL Done;
	_INT16 iTemp, CurOption, WhichEnemy, EventsFound, CurEvent, ChosenEvent,
	NumOptions, WhichOption;
	long OldOffset = 0;
	BOOL QuestDone = FALSE, FoundEvent;
	struct clan *EnemyClan;
	BOOL EndedChat = FALSE;
	_INT16 CurMember, PercentGold, FightResult, EmptySlot;
	long GoldAmount, XPAmount;
	struct NPCNdx NPCNdx;

	// reset temp flags
	if (QuoteToggle == FALSE) // clear flags for event files but not quotes
		ClearFlags(Quests_TFlags);

	rputs("|0C");


	// make strings
	szString  = MakeStr(255);
	szLegal   = MakeStr(155);
	Buffer    = MakeStr(255);
	szLabel   = MakeStr(31);
	szLabel1  = MakeStr(31);
	szLabel2  = MakeStr(31);
	szLabel3  = MakeStr(31);

	/* initialize file */
	MyOpen(szEventFile, "rb", &FileHeader);
	if (FileHeader.fp == NULL) {
		sprintf(szString, "-> %s\n", szEventFile);
		rputs(szString);
		rputs("Error opening event file.\n%P");

		// free strings
		free(szString);
		free(szLegal);
		free(Buffer);
		free(szLabel);
		free(szLabel1);
		free(szLabel2);
		free(szLabel3);

		return FALSE;
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

			if (fread(&EventHeader, sizeof(struct EventHeader), 1, FileHeader.fp) == 0)
				break;

			if (EventHeader.Event)
				EventsFound++;

			// seek next event
			fseek(FileHeader.fp, EventHeader.EventSize, SEEK_CUR);
		}

		// choose one randomly and seek it
		ChosenEvent = RANDOM(EventsFound);

		fseek(FileHeader.fp, FileHeader.lStart, SEEK_SET);

		EventsFound = 0;
		// find out how many events in the file
		for (;;) {
			// if at end...
			if (ftell(FileHeader.fp) >= FileHeader.lEnd)
				break;

			if (fread(&EventHeader, sizeof(struct EventHeader), 1, FileHeader.fp) == 0)
				break;

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
	EnemyClan = MallocClan();
	NPC_ResetNPCClan(EnemyClan);

	Done = FALSE;
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
		if (szLegal[0] && legal(szLegal, NULL) == FALSE) {
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
								SetFlag(PClan->DFlags, atoi(pcCurrentPos+1));
								break;
							case 'P' :
								SetFlag(PClan->PFlags, atoi(pcCurrentPos+1));
								break;
							case 'T' :
								SetFlag(Quests_TFlags, atoi(pcCurrentPos+1));
								break;
							case 'G' :
								SetFlag(Village.Data->GFlags, atoi(pcCurrentPos+1));
								break;
							case 'H' :
								SetFlag(Village.Data->HFlags, atoi(pcCurrentPos+1));
								break;
						}
						break;
					case 15 : // ClearFlag
						pcCurrentPos = szText;
						switch (toupper(*pcCurrentPos)) {
							case 'D' :
								ClearFlag(PClan->DFlags, atoi(pcCurrentPos+1));
								break;
							case 'P' :
								ClearFlag(PClan->PFlags, atoi(pcCurrentPos+1));
								break;
							case 'T' :
								ClearFlag(Quests_TFlags, atoi(pcCurrentPos+1));
								break;
							case 'G' :
								ClearFlag(Village.Data->GFlags, atoi(pcCurrentPos+1));
								break;
							case 'H' :
								ClearFlag(Village.Data->HFlags, atoi(pcCurrentPos+1));
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
								else if (stricmp(szText, "SP") == 0) {
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
							PercentGold = atoi(&pcCurrentPos[1]);
							if (PercentGold < 0)
								PercentGold = 0;
							if (PercentGold > 100)
								PercentGold = 100;

							GoldAmount = (PClan->Empire.VaultGold*PercentGold)/100L;
						}
						else
							GoldAmount = atol(pcCurrentPos);

						PClan->Empire.VaultGold -= GoldAmount;
						break;
					case 19 : // GiveGold
						GoldAmount = atol(szText);
						PClan->Empire.VaultGold += GoldAmount;
						break;
					case 20 : // GiveXP
						XPAmount = atol(szText);

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
						PClan->FightsLeft += atoi(szText);
						break;
					case 24 : // GiveFollowers
						PClan->Empire.Army.Followers += atol(szText);
						break;
					case 25 : // GivePoints
						PClan->Points += atol(szText);
						break;
					case 28 : // TellTopic
						for (iTemp = 0; iTemp < MAX_TOPICS; iTemp++) {
							if (NPCInfo->Topics[iTemp].Active == FALSE)
								continue;

							if (stricmp(NPCInfo->Topics[iTemp].szFileName,
										szText) == 0) {
								// found quote, set it to known
								NPCInfo->Topics[iTemp].Known = TRUE;
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
						free(pszOptionNames[iTemp]);
						pszOptionNames[iTemp] = NULL;
					}
					szKeys[iTemp] = 0;
				}


				// get first key allowed
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);
				fread(Buffer, DataLength, 1, FileHeader.fp);
				strcpy(szLabel, &Buffer[1]);

				pszOptionNames[0] = malloc(strlen(szLabel) + 1);
				CheckMem(pszOptionNames[0]);
				strcpy(pszOptionNames[0], szLabel);

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
					if (szLegal[0] && legal(szLegal, NULL) == FALSE) {
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
						strcpy(szLabel, &Buffer[1]);
						szKeys[CurOption] = Buffer[0];

						/* get this optionname */
						pszOptionNames[CurOption] = malloc(strlen(szLabel) + 1);
						CheckMem(pszOptionNames[CurOption]);
						strcpy(pszOptionNames[CurOption], szLabel);
					}

					CurOption++;
				}
				szKeys[CurOption] = 0;

				// get input from user

				cInput = toupper(od_get_answer(szKeys));

				if (OldCommType != 33) {
					sprintf(szString, "%c\n\r", cInput);
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
				if (stricmp(pszOptionNames[CurOption], "NextLine") == 0) {
					fseek(FileHeader.fp, OldOffset, SEEK_SET);
					break;
				}

				/* if that option has "STOP" to "jump" to, stop */
				else if (stricmp(pszOptionNames[CurOption], "Stop") == 0) {
					Done = TRUE;
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

				if (stricmp(szLabel3, "NoRun") == 0)
					FightResult = Fight_Fight(PClan, EnemyClan, FALSE, FALSE, FALSE);
				else  // can run
					FightResult = Fight_Fight(PClan, EnemyClan, FALSE, TRUE, FALSE);

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
				for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
					if (EnemyClan->Member[iTemp]) {
						free(EnemyClan->Member[iTemp]);
						EnemyClan->Member[iTemp] = NULL;
					}

				if (FightResult == FT_WON) {
					// won
					strcpy(szString, szLabel1);
				}
				else if (FightResult == FT_LOST) {
					// lost
					strcpy(szString, szLabel2);
				}
				else if (FightResult == FT_RAN) {
					// ran away

					if (stricmp(szLabel3, "NoRun") == 0)
						break;
					else
						strcpy(szString, szLabel3);
				}

				// if label is "nextline", stay here
				if (stricmp(szString, "nextline") == 0)
					break;
				// if label is "stop", end scrpt
				else if (stricmp(szString, "Stop") == 0) {
					Done = TRUE;
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
					Done = TRUE;
				else if (CommandType == 9) {
					rputs("\n|14Quest successfully completed!\n");
					Done = TRUE;
					QuestDone = TRUE;
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
							NPCInfo->Topics[ NPCInfo->KnownTopics ].Active = TRUE;
							NPCInfo->Topics[ NPCInfo->KnownTopics ].Known = TRUE;
							NPCInfo->Topics[ NPCInfo->KnownTopics ].ClanInfo = TRUE;

							GetClanNameID(szClanName, NPCNdx.ClanID);
							strcpy(NPCInfo->Topics[ NPCInfo->KnownTopics ].szName,
								   szClanName);

							sprintf(szString, "I'm already in a clan called %s.\n\n", szClanName);
							rputs(szString);
						}
					}
					else {
						if (NumMembers(PClan, FALSE) == 6) {
							rputs("Your clan already has the max. amount of members in it.\n\n");
							break;
						}

						// set NPC.DAT file so it says he is in a clan
						// right now and set which clan (this one)

						NPCNdx.InClan = TRUE;
						NPCNdx.ClanID[0] = PClan->ClanID[0];
						NPCNdx.ClanID[1] = PClan->ClanID[1];

						NPC_AddNPCMember(szNPCIndex);

						sprintf(szString, "|0B%s |0Cjoins your clan!\n",
								NPCInfo->szName);
						rputs(szString);

						NPC_UpdateNPCNdx(szNPCIndex, &NPCNdx);
					}
				}
				else if (CommandType == 30) {
					Done = TRUE;
					EndedChat = TRUE;
				}
				break;
			case 11 : // AddEnemy
				// read in text
				fread(&DataLength, sizeof(char), 1, FileHeader.fp);
				fread(Buffer, DataLength, 1, FileHeader.fp);
				WhichEnemy = Buffer[ DataLength-1 ];

				// find slot in enemy clan
				for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
					if (EnemyClan->Member[iTemp] == NULL) break;

				EmptySlot = iTemp;

				EnemyClan->Member[EmptySlot] = malloc(sizeof(struct pc));
				CheckMem(EnemyClan);

				strcpy(szNPCFileName, Buffer);

				NPC_GetNPC(EnemyClan->Member[EmptySlot], szNPCFileName, WhichEnemy);
				EnemyClan->Member[EmptySlot]->MyClan = EnemyClan;
				break;
			case 31 :  // Input keyword

				for (iTemp = 0; iTemp < MAX_OPTIONS; iTemp++) {
					if (pszOptionNames[iTemp]) {
						free(pszOptionNames[iTemp]);
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
				fread(pszOptionNames[0], DataLength, 1, FileHeader.fp);

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
					if (szLegal[0] && legal(szLegal, NULL) == FALSE) {
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
						fread(pszOptionNames[CurOption], DataLength, 1, FileHeader.fp);

					}

					CurOption++;
				}
				NumOptions = CurOption;

				// get input from user
				GetStringChoice(pszOptionNames, NumOptions, "|0GChoose one\n|0E> |07", &WhichOption,
								TRUE, DT_LONG, FALSE);

				/* if that option has nextline to "jump" to, keep
				   going in same block of data */
				if (stricmp(apszLabels[WhichOption], "NextLine") == 0) {
					fseek(FileHeader.fp, OldOffset, SEEK_SET);
					break;
				}

				/* if that option has "STOP" to "jump" to, stop */
				else if (stricmp(apszLabels[WhichOption], "Stop") == 0) {
					Done = TRUE;
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
				Done = TRUE;
				break;
		}
	}

	/* free optionnames */
	/* free labels */
	for (iTemp = 0; iTemp < MAX_OPTIONS; iTemp++) {
		if (pszOptionNames[iTemp]) {
			free(pszOptionNames[iTemp]);
			pszOptionNames[iTemp] = NULL;
		}

		if (apszLabels[iTemp]) {
			free(apszLabels[iTemp]);
			apszLabels[iTemp] = NULL;
		}
	}

	fclose(FileHeader.fp);

	// free strings
	free(szString);
	free(szLegal);
	free(Buffer);
	free(szLabel);
	free(szLabel1);
	free(szLabel2);
	free(szLabel3);

	// free up enemies found
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		if (EnemyClan->Member[iTemp]) {
			free(EnemyClan->Member[iTemp]);
			EnemyClan->Member[iTemp] = NULL;
		}

	if (EnemyClan) {
		free(EnemyClan);
		EnemyClan = NULL;
	}


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
			return TRUE;
		}
		else
			return FALSE;
	}
	else
		return (QuestDone);
	(void)CurEvent;
	(void)pcBrace;
	(void)Key;
	(void)FoundEvent;
}

// ------------------------------------------------------------------------- //

void Quests_GoQuest(void)
{
	_INT16 iTemp, TotalQuests = 0, WhichQuest, NumQuestsDone, QuestIndex[64];
	BOOL QuestDone, QuestKnown;
	char KnownBitSet, DoneBitSet;
	char szString[50], cInput;

	// tell him how many quests he's done right here for now since i'm too lazy
	for (iTemp = 0, NumQuestsDone = 0; iTemp < MAX_QUESTS; iTemp++) {
		if (PClan->QuestsDone[ iTemp/8 ] & (1 << (iTemp%8)))
			NumQuestsDone++;
	}

	for (;;) {
		sprintf(szString, " |0CQuests Completed:  |0B%d\n", NumQuestsDone);
		rputs(szString);
		rputs(ST_LONGLINE);

		/* List quests known and not completed */
		/* <pause> where needed */
		TotalQuests = 0;
		for (iTemp = 0; iTemp < MAX_QUESTS; iTemp++) {
			KnownBitSet =
				(PClan->QuestsKnown[ iTemp/8 ] & (1 << (iTemp%8)) ||
				 (Quests[iTemp].Known && Quests[iTemp].Active));
			DoneBitSet  = PClan->QuestsDone[ iTemp/8 ] & (1 << (iTemp%8));

			// quest known? AND not complete?
			if (KnownBitSet && !DoneBitSet && Quests[iTemp].pszQuestName) {
				// show quest then
				sprintf(szString, " |0A(|0B%c|0A) |0C%s\n", TotalQuests + 'A',
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
			cInput = toupper(od_get_key(TRUE));

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

		if (QuestKnown == FALSE || QuestDone || !Quests[QuestIndex[WhichQuest]].pszQuestName) {
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

	PClan->QuestToday = TRUE;

	/* if successful (returns TRUE), set that quest bit to done */
	if (RunEvent(FALSE,
				 Quests[QuestIndex[WhichQuest]].pszQuestFile, Quests[QuestIndex[WhichQuest]].pszQuestIndex,
				 NULL, NULL)) {
		// set bit since quest completed
		PClan->QuestsDone[ QuestIndex[WhichQuest]/8 ] |= (1<<(QuestIndex[WhichQuest]%8));
	}
	door_pause();
}
