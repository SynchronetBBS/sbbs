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
#include "unix_wrappers.h"

#include <OpenDoor.h>

#include "door.h"
#include "fight.h"
#include "game.h"
#include "input.h"
#include "language.h"
#include "mstrings.h"
#include "myopen.h"
#include "news.h"
#include "quests.h"
#include "structs.h"
#include "user.h"
#include "video.h"

extern char Quests_TFlags[8];
extern struct IniFile IniFile;
extern struct clan *PClan;

// ------------------------------------------------------------------------- //

void NPC_GetNPCNdx(struct NPCInfo *NPCInfo, struct NPCNdx *NPCNdx)
{
	/* gets NPC stats from .NPX file if available, else goes to .NPC file */

	char *NPXFile = "clans.npx";
	FILE *fpNPX;
	bool Found = false;
	uint8_t iBuf[BUF_SIZE_NPCNdx];

	fpNPX = fopen(NPXFile, "rb");
	if (fpNPX) {
		for (;;) {
			if (!fread(iBuf, sizeof(iBuf), 1, fpNPX))
				break;
			s_NPCNdx_d(iBuf, sizeof(iBuf), NPCNdx);

			// found it!
			if (stricmp(NPCNdx->szIndex, NPCInfo->szIndex) == 0) {
				Found = true;
				break;
			}
		}

		fclose(fpNPX);
	}

	if (!Found) {
		// couldn't find NDX in the NPX for that particular NPC

		strcpy(NPCNdx->szIndex, NPCInfo->szIndex);
		NPCNdx->InClan = false;
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
	uint8_t iBuf[BUF_SIZE_NPCNdx];

	fpNPX = fopen(NPXFile, "r+b");
	if (fpNPX) {
		for (;;) {
			Offset = ftell(fpNPX);

			if (!fread(iBuf, sizeof(iBuf), 1, fpNPX))
				break;
			s_NPCNdx_d(iBuf, sizeof(iBuf), &TmpNdx);

			// found it!
			if (stricmp(TmpNdx.szIndex, szIndex) == 0) {
				fseek(fpNPX, Offset, SEEK_SET);
				break;
			}
		}
	}

	s_NPCNdx_s(NPCNdx, iBuf, sizeof(iBuf));
	fwrite(iBuf, sizeof(iBuf), 1, fpNPX);

	fclose(fpNPX);
}

void NPC_Maint(void)
{
	FILE *fpNPX;
	int16_t CurNPCFile;
	struct FileHeader FileHeader;
	struct NPCInfo *NPCInfo;
	struct NPCNdx NPCNdx;
	char *szClansNPX = "clans.npx";
	int16_t iTemp;
	uint8_t nfoBuf[BUF_SIZE_NPCInfo];
	uint8_t idxBuf[BUF_SIZE_NPCNdx];
	

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

			fread(nfoBuf, sizeof(nfoBuf), 1, FileHeader.fp);
			s_NPCInfo_d(nfoBuf, sizeof(nfoBuf), NPCInfo);

			// is he a wandering type?
			if (NPCInfo->WhereWander == WN_NONE)
				continue;   // no, not wanderer, skip him

			// printf("Testing %s\n", NPCInfo->szName);

			// prepare to write to NPX file
			strcpy(NPCNdx.szIndex, NPCInfo->szIndex);
			NPCNdx.WhereWander = NPCInfo->WhereWander;
			NPCNdx.InClan = false;
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
			s_NPCNdx_s(&NPCNdx, idxBuf, sizeof(idxBuf));
			fwrite(idxBuf, sizeof(idxBuf), 1, fpNPX);
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
	int16_t iTemp, NumTopicsKnown, WhichTopic, QuoteIndex[MAX_TOPICS],
	CurFile, CurNPC, UserInput;
	char *szString, szKeys[26 + 3], *pszTopics[MAX_TOPICS],
	szPrompt[128];
	struct clan *EnemyClan;
	struct FileHeader FileHeader;
	struct NPCNdx NPCNdx;
	long Offset;
	bool FoundNPC;
	uint8_t nBuf[BUF_SIZE_NPCInfo];

	szString = MakeStr(255);

	EnemyClan = NULL;

	NPCInfo = malloc(sizeof(struct NPCInfo));
	CheckMem(NPCInfo);

	FoundNPC = false;
	for (CurFile = 0; CurFile < MAX_NPCS; CurFile++) {
		// if out of files, break
		if (IniFile.pszNPCFileName[CurFile] == NULL)
			break;

		MyOpen(IniFile.pszNPCFileName[CurFile], "rb", &FileHeader);

		if (!FileHeader.fp)  break;

		/* find him in the file */
		for (CurNPC = 0;; CurNPC++) {
			Offset = (long)CurNPC * BUF_SIZE_NPCInfo + FileHeader.lStart;

			// reach end
			if (Offset >= FileHeader.lEnd)
				break;

			if (fseek(FileHeader.fp, Offset, SEEK_SET))
				break;

			if (fread(nBuf, sizeof(nBuf), 1, FileHeader.fp) == 0)
				break;
			s_NPCInfo_d(nBuf, sizeof(nBuf), NPCInfo);

			// one we want?
			if (stricmp(NPCInfo->szIndex, szIndex) == 0) {
				// yes
				FoundNPC = true;
				break;
			}
		}
		fclose(FileHeader.fp);

		if (FoundNPC)  break;
	}

	if (FoundNPC == false) {
		rputs("NPC not found\n\r");
		free(szString);
		free(NPCInfo);
		return;
	}

	// clear temp flags
	ClearFlags(Quests_TFlags);

	// run intro topic
	if (NPCInfo->IntroTopic.Active) {
		if (RunEvent(true, NPCInfo->szQuoteFile,
					 NPCInfo->IntroTopic.szFileName, NPCInfo, szIndex) == true) {
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
						true, DT_LONG, true);

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
			ClanStats(EnemyClan, false);
			FreeClan(EnemyClan);

			continue;
		}

		// otherwise, "run" that quote in the NPCQUOTE.Q file
		if (RunEvent(true, NPCInfo->szQuoteFile,
					 NPCInfo->Topics[WhichTopic].szFileName, NPCInfo, szIndex) == true) {
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

void ChatVillagers(int16_t WhichMenu)
{
	struct NPCInfo *NPCInfo;
	char *pszNPCIndex[MAX_NPCS], szString[80];
	char *pszNPCNames[MAX_NPCS], cInput, szKeys[MAX_NPCS + 4];
	int16_t CurNPC, NPCsFound = 0, CurFile;
	long Offset;
	struct FileHeader FileHeader;
	struct NPCNdx NPCNdx;
	uint8_t nBuf[BUF_SIZE_NPCInfo];

	/* if all guys dead, tell guy can't fight */
	if (NumMembers(PClan, true) == 0) {
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
			Offset = (long)CurNPC * BUF_SIZE_NPCInfo + FileHeader.lStart;

			if (Offset >= FileHeader.lEnd)
				break;

			if (fseek(FileHeader.fp, Offset, SEEK_SET))
				break;  // no more NPCs to read

			if (fread(nBuf, sizeof(nBuf), 1, FileHeader.fp) == 0)
				break;  // no more NPCs in file
			s_NPCInfo_d(nBuf, sizeof(nBuf), NPCInfo);

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
	int16_t iTemp;

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
		NPCClan->Items[iTemp].Available = false;
}

void NPC_GetNPC(struct pc *NPC, char *szFileName, int16_t WhichNPC)
{
	long Offset;
	struct FileHeader FileHeader;
	uint8_t pcBuf[BUF_SIZE_pc];

	MyOpen(szFileName, "rb", &FileHeader);
	if (!FileHeader.fp) {
		DisplayStr("Error finding NPC file.\n");
		return;
	}
	Offset = (long)WhichNPC * BUF_SIZE_pc + sizeof(int16_t) * MAX_MONSTERS +
			 FileHeader.lStart;
	fseek(FileHeader.fp, Offset, SEEK_SET);
	fread(pcBuf, sizeof(pcBuf), 1, FileHeader.fp);
	s_pc_d(pcBuf, sizeof(pcBuf), NPC);
	fclose(FileHeader.fp);
}


void NPC_AddNPCMember(char *szIndex)
{
	struct pc *TmpPC;
	struct NPCInfo *NPCInfo;
	struct NPCNdx NPCNdx;
	int16_t EmptySlot, CurFile, CurNPC, iTemp, WhichNPC;
	bool FoundNPC = false;
	FILE *fpNPCDat, *fpNPCNdx;
	long SeekOffset;
	struct FileHeader FileHeader, PCFile;
	uint8_t nBuf[BUF_SIZE_NPCInfo];
	uint8_t pBuf[BUF_SIZE_pc];

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
			SeekOffset = (long)CurNPC * BUF_SIZE_NPCInfo + FileHeader.lStart;

			if (SeekOffset >= FileHeader.lEnd)
				break;

			if (fseek(FileHeader.fp, SeekOffset, SEEK_SET))
				break;

			fread(nBuf, sizeof(nBuf), 1, FileHeader.fp);
			s_NPCInfo_d(nBuf, sizeof(nBuf), NPCInfo);

			// one we want?
			if (stricmp(NPCInfo->szIndex, szIndex) == 0) {
				// yes
				FoundNPC = true;
				break;
			}
		}
		fclose(FileHeader.fp);

		if (FoundNPC)  break;
	}

	if (FoundNPC == false) {
		rputs("NPC not found\n");
		free(NPCInfo);
		return;
	}

	NPC_GetNPCNdx(NPCInfo, &NPCNdx);

	NPCNdx.InClan = true;
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
	SeekOffset = (long)NPCInfo->NPCPCIndex * BUF_SIZE_pc + sizeof(int16_t) * MAX_MONSTERS
				 + PCFile.lStart;
	fseek(PCFile.fp, SeekOffset, SEEK_SET);

	fread(pBuf, sizeof(pBuf), 1, PCFile.fp);
	fclose(PCFile.fp);
	s_pc_d(pBuf, sizeof(pBuf), TmpPC);

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

