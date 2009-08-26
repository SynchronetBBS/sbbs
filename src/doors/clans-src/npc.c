/*

The Clans BBS Door Game
Copyright (C) 1998-2002 Allen Ussher

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
 * NPCs ADT
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
#include "quests.h"
#include "myopen.h"
#include "video.h"
#include "language.h"
#include "mstrings.h"
#include "door.h"
#include "news.h"
#include "input.h"
#include "user.h"
#include "fight.h"


extern char Quests_TFlags[8];
extern struct IniFile IniFile;
extern struct clan *PClan;
extern struct Language *Language;
extern struct game Game;

// ------------------------------------------------------------------------- //

void NPC_GetNPCNdx(struct NPCInfo *NPCInfo, struct NPCNdx *NPCNdx)
{
	/* gets NPC stats from .NPX file if available, else goes to .NPC file */

	char *NPXFile = "clans.npx";
	FILE *fpNPX;
	BOOL Found = FALSE;

	fpNPX = fopen(NPXFile, "rb");
	if (fpNPX) {
		for (;;) {
			if (!fread(NPCNdx, sizeof(struct NPCNdx), 1, fpNPX))
				break;

			// found it!
			if (stricmp(NPCNdx->szIndex, NPCInfo->szIndex) == 0) {
				Found = TRUE;
				break;
			}
		}

		fclose(fpNPX);
	}

	if (!Found) {
		// couldn't find NDX in the NPX for that particular NPC

		strcpy(NPCNdx->szIndex, NPCInfo->szIndex);
		NPCNdx->InClan = FALSE;
		NPCNdx->ClanID[0] = -1;
		NPCNdx->ClanID[1] = -1;
		NPCNdx->Status = NPCS_NOTHERE;
		NPCNdx->WhereWander = NPCInfo->WhereWander;
	}
}

void NPC_UpdateNPCNdx(char *szIndex, struct NPCNdx *NPCNdx)
{
	/* Function to update status of an NPC to .NPX file */
	char *NPXFile = "clans.npx";
	FILE *fpNPX;
	long Offset;
	struct NPCNdx TmpNdx;

	fpNPX = fopen(NPXFile, "r+b");
	if (fpNPX) {
		for (;;) {
			Offset = ftell(fpNPX);

			if (!fread(&TmpNdx, sizeof(struct NPCNdx), 1, fpNPX))
				break;

			// found it!
			if (stricmp(TmpNdx.szIndex, szIndex) == 0) {
				fseek(fpNPX, Offset, SEEK_SET);
				break;
			}
		}
	}

	fwrite(NPCNdx, sizeof(struct NPCNdx), 1, fpNPX);

	fclose(fpNPX);
}

void NPC_Maint(void)
{
	FILE *fpNPX;
	_INT16 CurNPCFile;
	struct FileHeader FileHeader;
	struct NPCInfo *NPCInfo;
	struct NPCNdx NPCNdx;
	char *szClansNPX = "clans.npx";
	_INT16 iTemp;

	DisplayStr("* NPC Maint\n");

	NPCInfo = malloc(sizeof(struct NPCInfo));
	CheckMem(NPCInfo);

	// delete old CLANS.NPX file
	unlink(szClansNPX);

	fpNPX = fopen(szClansNPX, "wb");

	for (iTemp = 0; iTemp < MAX_NPCFILES; iTemp++) {
		if (IniFile.pszNPCFileName[iTemp] == NULL) break;

		MyOpen(IniFile.pszNPCFileName[iTemp], "rb", &FileHeader);

		if (!FileHeader.fp)
			continue;

		for (;;) {
			if (ftell(FileHeader.fp) >= FileHeader.lEnd)
				break;

			fread(NPCInfo, sizeof(struct NPCInfo), 1, FileHeader.fp);

			// is he a wandering type?
			if (NPCInfo->WhereWander == WN_NONE)
				continue;   // no, not wanderer, skip him

			// printf("Testing %s\n", NPCInfo->szName);

			// prepare to write to NPX file
			strcpy(NPCNdx.szIndex, NPCInfo->szIndex);
			NPCNdx.WhereWander = NPCInfo->WhereWander;
			NPCNdx.InClan = FALSE;
			NPCNdx.ClanID[0] = -1;
			NPCNdx.ClanID[1] = -1;
			NPCNdx.Status = NPCS_NOTHERE;

			// yes, he is a wanderer, see if appears in town today
			if (RANDOM(100) < NPCInfo->OddsOfSeeing) {
				// printf("%s is in town\n", NPCInfo->szName);

				// he appears in town today
				NPCNdx.Status = NPCS_HERE;

				// write to news
				if (NPCInfo->szHereNews[0]) {
					News_AddNews(NPCInfo->szHereNews);
					News_AddNews("\n\n");
				}
			}

			// write to NPX file
			fwrite(&NPCNdx, sizeof(struct NPCNdx), 1, fpNPX);
		}

		fclose(FileHeader.fp);
	}

	fclose(fpNPX);

	free(NPCInfo);
	(void)CurNPCFile;
}

void NPC_ChatNPC(char *szIndex)
{
	/* for now, this is the chatnpc function */

	/* for now, just use the knight as the NPC to chat with */
	struct NPCInfo *NPCInfo;
	_INT16 iTemp, NumTopicsKnown, WhichTopic, QuoteIndex[MAX_TOPICS],
	CurFile, CurNPC, UserInput;
	char *szString, szKeys[26 + 3], *pszTopics[MAX_TOPICS],
	szPrompt[128];
	struct clan *EnemyClan;
	struct FileHeader FileHeader;
	struct NPCNdx NPCNdx;
	long Offset;
	BOOL FoundNPC;

	szString = MakeStr(255);

	EnemyClan = NULL;

	NPCInfo = malloc(sizeof(struct NPCInfo));
	CheckMem(NPCInfo);

	FoundNPC = FALSE;
	for (CurFile = 0; CurFile < MAX_NPCS; CurFile++) {
		// if out of files, break
		if (IniFile.pszNPCFileName[CurFile] == NULL)
			break;

		MyOpen(IniFile.pszNPCFileName[CurFile], "rb", &FileHeader);

		if (!FileHeader.fp)  break;

		/* find him in the file */
		for (CurNPC = 0;; CurNPC++) {
			Offset = (long)CurNPC * (long)sizeof(struct NPCInfo) + FileHeader.lStart;

			// reach end
			if (Offset >= FileHeader.lEnd)
				break;

			if (fseek(FileHeader.fp, Offset, SEEK_SET))
				break;

			if (fread(NPCInfo, sizeof(struct NPCInfo), 1, FileHeader.fp) == 0)
				break;

			// one we want?
			if (stricmp(NPCInfo->szIndex, szIndex) == 0) {
				// yes
				FoundNPC = TRUE;
				break;
			}
		}
		fclose(FileHeader.fp);

		if (FoundNPC)  break;
	}

	if (FoundNPC == FALSE) {
		rputs("NPC not found\n\r");
		free(szString);
		free(NPCInfo);
		return;
	}

	// clear temp flags
	ClearFlags(Quests_TFlags);

	// run intro topic
	if (NPCInfo->IntroTopic.Active) {
		if (RunEvent(TRUE, NPCInfo->szQuoteFile,
					 NPCInfo->IntroTopic.szFileName, NPCInfo, szIndex) == TRUE) {
			free(NPCInfo);
			free(szString);
			return;
		}
		door_pause();
	}

	/* now the topic stuff */

	for (;;) {
		sprintf(szString, "\n |0AYou are chatting with |0B%s\n", NPCInfo->szName);
		rputs(szString);

		// get topics known
		NumTopicsKnown = 0;
		for (iTemp = 0; iTemp < MAX_TOPICS; iTemp++) {
			if (NPCInfo->Topics[iTemp].Active && NPCInfo->Topics[iTemp].Known) {
				pszTopics[ NumTopicsKnown ] = NPCInfo->Topics[iTemp].szName;
				QuoteIndex[ NumTopicsKnown ] = iTemp;
				NumTopicsKnown++;
			}
		}

		// see how many quotes left
		if (NPCInfo->MaxTopics != -1) {
			sprintf(szPrompt, "|0A You may discuss |0B%d |0Atopics.  ",
					NPCInfo->MaxTopics);
		}
		else szPrompt[0] = 0;

		strcat(szPrompt, "|0GEnter a Topic or press enter to quit\n|0E> |07");

		// chose one
		GetStringChoice(pszTopics, NumTopicsKnown, szPrompt, &UserInput,
						TRUE, DT_LONG, TRUE);

		if (UserInput == -1)
			break;

		if (NPCInfo->MaxTopics != -1)
			NPCInfo->MaxTopics--;

		// get topic's actual index
		WhichTopic = QuoteIndex[ UserInput ];

		// if quote is of type ClanInfo, tell user info
		if (NPCInfo->Topics[WhichTopic].ClanInfo) {
			rputs("|03Here's some info on the clan I'm in.\n%P");

			NPC_GetNPCNdx(NPCInfo, &NPCNdx);

			EnemyClan = malloc(sizeof(struct clan));
			CheckMem(EnemyClan);

			GetClan(NPCNdx.ClanID, EnemyClan);

			// Clan_Extract(EnemyClan, NPCNdx.ClanID);
			ClanStats(EnemyClan, FALSE);
			FreeClan(EnemyClan);

			continue;
		}

		// otherwise, "run" that quote in the NPCQUOTE.Q file
		if (RunEvent(TRUE, NPCInfo->szQuoteFile,
					 NPCInfo->Topics[WhichTopic].szFileName, NPCInfo, szIndex) == TRUE) {
			// player killed while fighting, quit
			break;
		}
		else door_pause();

		if (NPCInfo->MaxTopics == 0) {
			rputs("|0BYou have spent enough time chatting.  You end the discussion.\n%P");
			break;
		}
	}

	free(NPCInfo);
	free(szString);
	(void)szKeys;
}

void ChatVillagers(_INT16 WhichMenu)
{
	struct NPCInfo *NPCInfo;
	char *pszNPCIndex[MAX_NPCS], szString[80];
	char *pszNPCNames[MAX_NPCS], cInput, szKeys[MAX_NPCS + 4];
	_INT16 CurNPC, NPCsFound = 0, CurFile;
	long Offset;
	struct FileHeader FileHeader;
	struct NPCNdx NPCNdx;

	/* if all guys dead, tell guy can't fight */
	if (NumMembers(PClan, TRUE) == 0) {
		rputs(ST_FIGHT0);
		return;
	}

	// this function will allow the player to chat with NPCs which are in the
	// game or whatever

	// init names to NULL
	for (CurNPC = 0; CurNPC < MAX_NPCS; CurNPC++) {
		pszNPCNames[CurNPC] = NULL;
		pszNPCIndex[CurNPC] = NULL;
	}

	NPCInfo = malloc(sizeof(struct NPCInfo));
	CheckMem(NPCInfo);

	// go through each NPC file
	for (CurFile = 0; CurFile < MAX_NPCFILES; CurFile++) {
		if (IniFile.pszNPCFileName[CurFile] == NULL)
			break;

		// get NPCs from npcs.dat, create index table of index numbers
		MyOpen(IniFile.pszNPCFileName[CurFile], "rb", &FileHeader);
		if (!FileHeader.fp)  break;      // couldn't open file, break

		for (CurNPC = 0;; CurNPC++) {
			Offset = (long)CurNPC*sizeof(struct NPCInfo) + FileHeader.lStart;

			if (Offset >= FileHeader.lEnd)
				break;

			if (fseek(FileHeader.fp, Offset, SEEK_SET))
				break;  // no more NPCs to read

			if (fread(NPCInfo, sizeof(struct NPCInfo), 1, FileHeader.fp) == 0)
				break;  // no more NPCs in file

			// get his index
			NPC_GetNPCNdx(NPCInfo, &NPCNdx);

			// is he in town? AND in this menu?
			if (NPCNdx.Status != NPCS_HERE || NPCNdx.WhereWander != WhichMenu)
				continue; // no, not here

			// in future -- is this town same town as the NPC?

			// yes, he is in this menu, add to index
			pszNPCIndex[ NPCsFound ] = MakeStr(strlen(NPCInfo->szIndex) + 1);
			strcpy(pszNPCIndex[ NPCsFound ], NPCInfo->szIndex);
			pszNPCNames[ NPCsFound ] = MakeStr(strlen(NPCInfo->szName) + 1);
			strcpy(pszNPCNames[ NPCsFound ], NPCInfo->szName);
			NPCsFound++;
		}
		fclose(FileHeader.fp);

		if (NPCsFound == MAX_NPCS)
			break;
	}

	free(NPCInfo);

	if (NPCsFound == 0) {
		rputs("|07You found no villagers in the area.\n%P");
		return;
	}

	for (;;) {
		rputs("\n |0BYou scan the area for villagers . . .\n");
		rputs(ST_LONGDIVIDER);
		for (CurNPC = 0; CurNPC < NPCsFound; CurNPC++) {
			sprintf(szString, " |0A(|0B%c|0A) |0C%s\n", CurNPC + 'A', pszNPCNames[ CurNPC ]);
			rputs(szString);
			szKeys[CurNPC] = CurNPC + 'A';
		}
		szKeys[CurNPC] = 'Q';
		szKeys[CurNPC + 1] = '\r';
		szKeys[CurNPC + 2] = '\n';
		szKeys[CurNPC + 3] = 0;

		rputs(" |0A(|0BQ|0A) |0CQuit\n\n");
		sprintf(szString, " |0B%d |0Achat(s) left\n", MAX_CHATSPERDAY - PClan->ChatsToday);
		rputs(szString);
		rputs(ST_LONGDIVIDER);
		rputs(" |0GChoose one|0E> |0FQuit");

		// show them to player, allow him to choose one until [Q] Quit

		cInput = od_get_answer(szKeys);

		if (cInput == 'Q' || cInput == '\r' || cInput == '\n') {
			rputs("\n\n");
			break;  // done
		}

		sprintf(szString, "\b\b\b\b    \b\b\b\b%s\n\n", pszNPCNames[ cInput - 'A']);
		rputs(szString);

		// if not enough chats, don't let him chat
		if (PClan->ChatsToday == MAX_CHATSPERDAY) {
			rputs("\n|15You have chatted enough already today.\n%P");
			continue;
		}

		// chat with him using index value
		NPC_ChatNPC(pszNPCIndex[ cInput - 'A' ]);

		// decrease chats today
		PClan->ChatsToday++;
	}

	// free up name memory used
	for (CurNPC = 0; CurNPC < MAX_NPCS; CurNPC++) {
		if (pszNPCNames[CurNPC])
			free(pszNPCNames[CurNPC]);

		if (pszNPCIndex[CurNPC])
			free(pszNPCIndex[CurNPC]);
	}
}

void NPC_ResetNPCClan(struct clan *NPCClan)
{
	_INT16 iTemp;

	// free up enemies found
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		if (NPCClan->Member[iTemp]) {
			free(NPCClan->Member[iTemp]);
			NPCClan->Member[iTemp] = NULL;
		}

	NPCClan->szName[0] = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		NPCClan->Member[iTemp] = NULL;

	for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++)
		NPCClan->Items[iTemp].Available = FALSE;
}

void NPC_GetNPC(struct pc *NPC, char *szFileName, _INT16 WhichNPC)
{
	long Offset;
	struct FileHeader FileHeader;

	MyOpen(szFileName, "rb", &FileHeader);
	if (!FileHeader.fp) {
		DisplayStr("Error finding NPC file.\n");
		return;
	}
	Offset = (long)WhichNPC * (long)sizeof(struct pc) + (long)sizeof(_INT16)*MAX_MONSTERS +
			 FileHeader.lStart;
	fseek(FileHeader.fp, Offset, SEEK_SET);
	fread(NPC, sizeof(struct pc), 1, FileHeader.fp);
	fclose(FileHeader.fp);
}


void NPC_AddNPCMember(char *szIndex)
{
	struct pc *TmpPC;
	struct NPCInfo *NPCInfo;
	struct NPCNdx NPCNdx;
	_INT16 EmptySlot, CurFile, CurNPC, iTemp, WhichNPC;
	BOOL FoundNPC = FALSE;
	FILE *fpNPCDat, *fpNPCNdx;
	long Offset, SeekOffset;
	struct FileHeader FileHeader, PCFile;

	// This function ASSUMES the user has an empty slot already!!

	// find empty slot first
	for (iTemp = Game.Data->MaxPermanentMembers; iTemp < 6; iTemp++)
		if (PClan->Member[iTemp] == NULL)
			break;

	EmptySlot = iTemp;

	NPCInfo = malloc(sizeof(struct NPCInfo));
	CheckMem(NPCInfo);

	// go into NPC.DAT file, get npc index in the npc.pc file
	// set npc as a member of this clan
	// add npc to this clan's memberlist

	// first, find NPC in file somewhere
	for (CurFile = 0; CurFile < MAX_NPCFILES; CurFile++) {
		if (!IniFile.pszNPCFileName[CurFile]) break;

		MyOpen(IniFile.pszNPCFileName[CurFile], "rb", &FileHeader);
		if (!FileHeader.fp)  continue;

		/* find him in the file */
		for (CurNPC = 0; ; CurNPC++) {
			SeekOffset = (long)CurNPC * (long)sizeof(struct NPCInfo) +
						 FileHeader.lStart;

			if (SeekOffset >= FileHeader.lEnd)
				break;

			if (fseek(FileHeader.fp, SeekOffset, SEEK_SET))
				break;

			// save offset for later
			Offset = ftell(FileHeader.fp);

			fread(NPCInfo, sizeof(struct NPCInfo), 1, FileHeader.fp);

			// one we want?
			if (stricmp(NPCInfo->szIndex, szIndex) == 0) {
				// yes
				FoundNPC = TRUE;
				break;
			}
		}
		fclose(FileHeader.fp);

		if (FoundNPC)  break;
	}

	if (FoundNPC == FALSE) {
		rputs("NPC not found\n");
		free(NPCInfo);
		return;
	}

	NPC_GetNPCNdx(NPCInfo, &NPCNdx);

	NPCNdx.InClan = TRUE;
	NPCNdx.ClanID[0] = PClan->ClanID[0];
	NPCNdx.ClanID[1] = PClan->ClanID[1];

	NPC_UpdateNPCNdx(NPCInfo->szIndex, &NPCNdx);

	MyOpen(NPCInfo->szMonFile, "rb", &PCFile);
	if (!PCFile.fp) {
		//      rputs("Error finding NPC.\n");
		free(NPCInfo);
		return;
	}
	TmpPC = malloc(sizeof(struct pc));
	CheckMem(TmpPC);

	// seek to it but make sure you skip the monster index at start of pc file
	SeekOffset = (long)NPCInfo->NPCPCIndex * (long)sizeof(struct pc) + (long)sizeof(_INT16)*MAX_MONSTERS
				 + PCFile.lStart;
	fseek(PCFile.fp, SeekOffset, SEEK_SET);

	fread(TmpPC, sizeof(struct pc), 1, PCFile.fp);
	fclose(PCFile.fp);

	PClan->Member[EmptySlot] = malloc(sizeof(struct pc));
	CheckMem(PClan->Member[EmptySlot]);
	// CopyPC(PClan->Member[EmptySlot], TmpPC);
	*PClan->Member[EmptySlot] = *TmpPC;
	PClan->Member[EmptySlot]->MyClan = PClan;
	free(TmpPC);
	free(NPCInfo);
	(void)WhichNPC;
	(void)fpNPCDat;
	(void)fpNPCNdx;
}

