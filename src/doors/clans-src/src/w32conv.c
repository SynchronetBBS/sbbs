/*

The Clans Win32 Datafile Conversion Utility
Copyright (C) 2002 Michael Dillon
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
/* w32conv.c was written by Michael Dillon.  It employs structures created by
   and under the authority of Allen Ussher. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "structs.h"

#ifndef _WIN32
# pragma message("This conversion program was intended for early Win32 versions of The Clans")
#endif

#define CLANS_PCFILE "CLANS.PC"
#define CLANS_PCFILE_NEW "NEW.PC"
#define CLANS_PCFILE_BACKUP "CLANS.PC.OLD"

struct old_clan {
	_INT16 ClanID[2];
	char szUserName[30];
	char szName[25];
	char Symbol[21];

	char QuestsDone[8], QuestsKnown[8];

	char PFlags[8], DFlags[8];
	char ChatsToday, TradesToday;

	_INT16 ClanRulerVote[2];        // who are you voting for as the ruler?

	_INT16 Alliances[MAX_ALLIES];       // alliances change from BBS to BBS, -1
	// means no alliance, 0 = first one, ID
	// that is...

	long Points;
	char szDateOfLastGame[11];

	_INT16 FightsLeft, ClanFights,
	MineLevel;

	_INT16 WorldStatus, DestinationBBS;

	char VaultWithdrawals;

	_INT16 PublicMsgIndex;

	_INT16 ClanCombatToday[MAX_CLANCOMBAT][2];
	_INT16 ClanWars;

	struct pc *Member[MAX_MEMBERS];
	struct item_data Items[MAX_ITEMS_HELD];

	char ResUncToday, ResDeadToday ;

	struct empire Empire;

	// Help
	char DefActionHelp : 1,
	CommHelp      : 1,
	MineHelp      : 1,
	MineLevelHelp : 1,
	CombatHelp    : 1,
	TrainHelp     : 1,
	MarketHelp    : 1,
	PawnHelp      : 1,
	WizardHelp    : 1,
	EmpireHelp    : 1,
	DevelopHelp   : 1,
	TownHallHelp  : 1,
	DestroyHelp   : 1,
	ChurchHelp    : 1,
	THallHelp     : 1,
	SpyHelp       : 1,
	AllyHelp      : 1,
	WarHelp       : 1,
	VoteHelp      : 1,
	TravelHelp    : 1,

	WasRulerToday : 1,
	MadeAlliance  : 1,
	Protection    : 4,
	FirstDay      : 1,
	Eliminated    : 1,
	QuestToday    : 1,
	AttendedMass  : 1,
	GotBlessing   : 1,
	Prayed        : 1;

	long CRC;
} PACKED;

void display_error(const char *, ...);
void check_pointer(void *);
void cipher(void *, size_t, unsigned char);
void convert_to_new(struct clan *, struct old_clan *);

int main(void)
{
	FILE *fp_old, *fp_new;
	struct clan *clan;
	struct pc *pc;
	struct old_clan *old_clan;
	int i;

	if (access(CLANS_PCFILE, 0) != 0) {
		display_error("Unable to open %s: %s\n", CLANS_PCFILE, strerror(errno));
		return 1;
	}

	clan = (struct clan *)malloc(sizeof(struct clan));
	old_clan = (struct old_clan *)malloc(sizeof(struct old_clan));
	pc = (struct pc *)malloc(sizeof(struct pc));

	fp_old = fopen(CLANS_PCFILE, "rb");
	if (!fp_old) {
		display_error("Failure to open %s: %s\n", CLANS_PCFILE, strerror(errno));
		exit(0);
	}

	fp_new = fopen(CLANS_PCFILE_NEW, "wb");
	if (!fp_new) {
		display_error("Failure to open %s: %s\n", CLANS_PCFILE_NEW, strerror(errno));
		exit(0);
	}

	do {
		if (fread(old_clan, sizeof(struct old_clan), 1, fp_old)) {
			cipher(old_clan, sizeof(struct old_clan), 9);
			printf("Found Clan: %s\n", old_clan->szName);
			convert_to_new(clan, old_clan);
			cipher(clan, sizeof(struct clan), 9);
			fwrite(clan, sizeof(struct clan), 1, fp_new);
		}
		else if (!feof(fp_old)) {
			display_error("clan: fread error");
			exit(0);
		}
		else
			break; /* last entry was read */
		for (i = 0; i < 6; i++) {
			if (fread(pc, sizeof(struct pc), 1, fp_old))
				fwrite(pc, sizeof(struct pc), 1, fp_new);
			else {
				display_error("pc: fread error");
				exit(0);
			}
		}
	}
	while (!feof(fp_old));

	fclose(fp_old);
	fflush(fp_new);
	fclose(fp_new);

	unlink(CLANS_PCFILE_BACKUP);
	rename(CLANS_PCFILE, CLANS_PCFILE_BACKUP);
	rename(CLANS_PCFILE_NEW, CLANS_PCFILE);

	free(pc);
	free(old_clan);
	free(clan);
	return 0;
}

void display_error(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fflush(stderr);
}

void check_pointer(void *ptr)
{
	if (!ptr)
		display_error("Pointer is invalid!");
	exit(0);
}

void cipher(void *obj, size_t obj_len, unsigned char xor_val)
{
	while (obj_len--) {
		*(char *)obj = *(char *)obj ^ xor_val;
		obj = (char *)obj + 1;
	}
}

void convert_to_new(struct clan *clan, struct old_clan *old_clan)
{
	int i;

	clan->ClanID[0] = old_clan->ClanID[0];
	clan->ClanID[1] = old_clan->ClanID[1];
	strncpy(clan->szUserName, old_clan->szUserName, 30);
	strncpy(clan->szName, old_clan->szName, 25);
	strncpy(clan->Symbol, old_clan->Symbol, 21);
	memcpy(clan->QuestsDone, old_clan->QuestsDone, 8);
	memcpy(clan->QuestsKnown, old_clan->QuestsKnown, 8);
	memcpy(clan->PFlags, old_clan->PFlags, 8);
	memcpy(clan->DFlags, old_clan->DFlags, 8);
	clan->ChatsToday = old_clan->ChatsToday;
	clan->TradesToday = old_clan->TradesToday;
	clan->ClanRulerVote[0] = old_clan->ClanRulerVote[0];
	clan->ClanRulerVote[1] = old_clan->ClanRulerVote[1];
	for (i = 0; i < MAX_ALLIES; i++)
		clan->Alliances[i] = old_clan->Alliances[i];
	clan->Points = old_clan->Points;
	strncpy(clan->szDateOfLastGame, old_clan->szDateOfLastGame, 11);
	clan->FightsLeft = old_clan->FightsLeft;
	clan->ClanFights = old_clan->ClanFights;
	clan->MineLevel = old_clan->MineLevel;
	clan->WorldStatus = old_clan->WorldStatus;
	clan->DestinationBBS = old_clan->DestinationBBS;
	clan->VaultWithdrawals = old_clan->VaultWithdrawals;
	clan->PublicMsgIndex = old_clan->PublicMsgIndex;
	for (i = 0; i < MAX_CLANCOMBAT; i++) {
		clan->ClanCombatToday[i][0] = old_clan->ClanCombatToday[i][0];
		clan->ClanCombatToday[i][1] = old_clan->ClanCombatToday[i][1];
	}
	clan->ClanWars = old_clan->ClanWars;
	for (i = 0; i < MAX_MEMBERS; i++)
		clan->Member[i] = old_clan->Member[i];
	for (i = 0; i < MAX_ITEMS_HELD; i++)
		memcpy(&clan->Items[i], &old_clan->Items[i], sizeof(struct item_data));
	clan->ResUncToday = old_clan->ResUncToday;
	clan->ResDeadToday = old_clan->ResDeadToday;
	memcpy(&clan->Empire, &old_clan->Empire, sizeof(struct empire));
	clan->DefActionHelp = old_clan->DefActionHelp;
	clan->CommHelp = old_clan->CommHelp;
	clan->MineHelp = old_clan->MineHelp;
	clan->MineLevelHelp = old_clan->MineLevelHelp;
	clan->CombatHelp = old_clan->CombatHelp;
	clan->TrainHelp = old_clan->TrainHelp;
	clan->MarketHelp = old_clan->MarketHelp;
	clan->PawnHelp = old_clan->PawnHelp;
	clan->WizardHelp = old_clan->WizardHelp;
	clan->EmpireHelp = old_clan->EmpireHelp;
	clan->DevelopHelp = old_clan->DevelopHelp;
	clan->TownHallHelp = old_clan->TownHallHelp;
	clan->DestroyHelp = old_clan->DestroyHelp;
	clan->ChurchHelp = old_clan->ChurchHelp;
	clan->THallHelp = old_clan->THallHelp;
	clan->SpyHelp = old_clan->SpyHelp;
	clan->AllyHelp = old_clan->AllyHelp;
	clan->WarHelp = old_clan->WarHelp;
	clan->VoteHelp = old_clan->VoteHelp;
	clan->TravelHelp = old_clan->TravelHelp;
	clan->WasRulerToday = old_clan->WasRulerToday;
	clan->MadeAlliance = old_clan->MadeAlliance;
	clan->Protection = old_clan->Protection;
	clan->FirstDay = old_clan->FirstDay;
	clan->Eliminated = old_clan->Eliminated;
	clan->QuestToday = old_clan->QuestToday;
	clan->AttendedMass = old_clan->AttendedMass;
	clan->GotBlessing = old_clan->GotBlessing;
	clan->Prayed = old_clan->Prayed;
	clan->CRC = old_clan->CRC;
}