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
 * Voting module
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <dos.h>
#endif
#include <errno.h>
#include <string.h>

#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "door.h"
#include "input.h"
#include "news.h"
#include "myopen.h"
#include "user.h"
#include "help.h"

extern struct clan *PClan;
extern struct Language *Language;
extern struct village Village;

_INT16 GetVotes(_INT16 TopCandidates[50][2], _INT16 TopVotes[50], BOOL UserOnline)
{
	FILE *fpPlayerFile;
	char szFileName[50];
	_INT16 CurClan, iTemp, CurVote, NumVotes, NumUndecided = 0,
			ClanID[2];
	struct clan *TmpClan;
	long Offset;

	strcpy(szFileName, ST_CLANSPCFILE);

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	fpPlayerFile = fopen(szFileName, "r+b");
	if (!fpPlayerFile) {
		TopCandidates[0][0] = -1;
		TopCandidates[0][1] = -1;
		free(TmpClan);
		return 0;  /* failed to find clan */
	}


	// initialize votes to -1
	for (CurVote = 0; CurVote < 50; CurVote++) {
		TopCandidates[CurVote][0] = -1;
		TopCandidates[CurVote][1] = -1;
		TopVotes[CurVote] = 0;
	}

	for (CurClan = 0;; CurClan++) {
		/* go through file till you find clan he wants */

		Offset = (long)CurClan * (sizeof(struct clan) + 6L*sizeof(struct pc));
		if (fseek(fpPlayerFile, Offset, SEEK_SET)) {
			break;  /* couldn't fseek, so exit */
		}

		if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER) == 0)
			break;  /* stop reading if no more players found */

		/* skip if deleted clan */
		if (TmpClan->ClanID[0] == -1)
			continue;

		// skip if it's this user and he's online
		if (UserOnline &&
				TmpClan->ClanID[0] == PClan->ClanID[0] &&
				TmpClan->ClanID[1] == PClan->ClanID[1])
			continue;

		// skip vote if undecided
		if (TmpClan->ClanRulerVote[0] == -1) {
			NumUndecided++;
			continue;
		}

		// see if who he voted for is in list, if not, add to it
		for (CurVote = 0; CurVote < 50; CurVote++) {
			if (TopCandidates[CurVote][0] == TmpClan->ClanRulerVote[0] &&
					TopCandidates[CurVote][1] == TmpClan->ClanRulerVote[1]) {
				// found match, increment this vote
				TopVotes[CurVote]++;
				break;
			}

			// if -1, set this vote as our user's choice and increment votes
			if (TopCandidates[CurVote][0] == -1) {
				TopCandidates[CurVote][0] = TmpClan->ClanRulerVote[0];
				TopCandidates[CurVote][1] = TmpClan->ClanRulerVote[1];
				TopVotes[CurVote] = 1;
				break;
			}
		}
	}
	fclose(fpPlayerFile);

	free(TmpClan);
	TmpClan = NULL;

	// now add on the current user's stats

	// see if who he voted for is in list, if not, add to it
	if (UserOnline && PClan->ClanRulerVote[0] != -1)
		for (CurVote = 0; CurVote < 50; CurVote++) {
			if (TopCandidates[CurVote][0] == PClan->ClanRulerVote[0] &&
					TopCandidates[CurVote][1] == PClan->ClanRulerVote[1]) {
				// found match, increment this vote
				TopVotes[CurVote]++;
				break;
			}

			// if -1, set this vote as our user's choice and increment votes
			if (TopCandidates[CurVote][0] == -1) {
				TopCandidates[CurVote][0] = PClan->ClanRulerVote[0];
				TopCandidates[CurVote][1] = PClan->ClanRulerVote[1];
				TopVotes[CurVote] = 1;
				break;
			}
		}
	else if (UserOnline && PClan->ClanRulerVote[0] == -1)
		NumUndecided++;


	// sort it out
	for (iTemp = 0; iTemp < 50; iTemp++)
		for (CurVote = 0; CurVote < 49; CurVote++) {
			if (TopVotes[CurVote] < TopVotes[CurVote+1]) {
				NumVotes = TopVotes[CurVote];
				TopVotes[CurVote] = TopVotes[CurVote+1];
				TopVotes[CurVote+1] = NumVotes;

				ClanID[0] = TopCandidates[CurVote][0];
				ClanID[1] = TopCandidates[CurVote][1];

				TopCandidates[CurVote][0] = TopCandidates[CurVote+1][0];
				TopCandidates[CurVote][1] = TopCandidates[CurVote+1][1];

				TopCandidates[CurVote+1][0] = ClanID[0];
				TopCandidates[CurVote+1][1] = ClanID[1];
			}
		}

	return NumUndecided;
}

void VotingBooth(void)
{
	_INT16 TopCandidates[50][2];
	_INT16 TopVotes[50], ClanID[2], iTemp, Undecided;
	char szName[25], szString[128];
	char *szTheOptions[4], *szTop10Names[10];
	BOOL Done = FALSE;

	if (!PClan->VoteHelp) {
		PClan->VoteHelp = TRUE;
		Help("Voting Booth", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	LoadStrings(1310, 4, szTheOptions);

	// allocate mem for top 10 names
	for (iTemp = 0; iTemp < 10; iTemp++)
		szTop10Names[iTemp] = MakeStr(25);

	while (!Done) {
		Undecided = GetVotes(TopCandidates, TopVotes, TRUE);

		// get top 10 names
		for (iTemp = 0; iTemp < 10; iTemp++) {
			if (TopCandidates[iTemp][0] == -1)
				break;

			GetClanNameID(szTop10Names[iTemp], TopCandidates[iTemp]);
		}

		// show top votes to user

		rputs(ST_LONGLINE);
		rputs(" |07Rank Clan              Votes\n");

		for (iTemp = 0; iTemp < 10; iTemp++) {
			if (TopCandidates[iTemp][0] == -1)
				break;

			sprintf(szString, "|0B%3d.  |0C%-20s %d\n", iTemp+1,
					szTop10Names[iTemp], TopVotes[iTemp]);
			rputs(szString);
		}
		sprintf(szString, "      |0C%-20s %d\n", "Undecided", Undecided);
		rputs(szString);

		if (PClan->ClanRulerVote[0] != -1) {
			GetClanNameID(szName, PClan->ClanRulerVote);
		}
		else
			strcpy(szName, "Undecided");

		sprintf(szString, "\n |0CYour Vote: |0B%s\n", szName);
		rputs(szString);

		switch (GetChoice("Voting Booth", ST_ENTEROPTION, szTheOptions, "Q?VC", 'Q', TRUE)) {
			case 'Q' :    /* Quit */
				Done = TRUE;
				break;
			case '?' :    /* redisplay options */
				break;
			case 'V' :    /* stats */
				ClanStats(PClan, TRUE);
				break;
			case 'C' :    /* change vote */
				rputs("|0CEnter a blank line to become undecided\n");
				if (GetClanID(ClanID, FALSE, FALSE, -1, -1) == FALSE) {
					rputs("You are now undecided.\n");
					PClan->ClanRulerVote[0] = -1;
					PClan->ClanRulerVote[1] = -1;
				}
				else {
					rputs("Vote changed.\n");
					PClan->ClanRulerVote[0] = ClanID[0];
					PClan->ClanRulerVote[1] = ClanID[1];
				}
				break;
		}
	}

	for (iTemp = 0; iTemp < 10; iTemp++)
		free(szTop10Names[iTemp]);
}

void ChooseNewLeader(void)
{
	_INT16 TopCandidates[50][2];
	_INT16 TopVotes[50], /*ClanID[2],*/ iTemp, MostVotes, NumTied;
	_INT16 NewRulerID[2];
	char szName[25], szString[128];

	// if voting not allowed, just write in news that the dictatorship
	// continues
	/*
	    if (Village.GovtSystem == GS_DICTATOR && Village.RulingClanId[0] != -1)
	    {
	      GetClanNameID(szName, Village.RulingClanId);

	      // sprintf(szString, ">> %s's reign as dictator continues.\n\n",
	      sprintf(szString, ST_NEWS0, szName);
	      AddNews(szString);
	      return;
	    }
	*/

	// get sorted votes
	GetVotes(TopCandidates, TopVotes, FALSE);

	// if no votes, do nothing
	if (TopCandidates[0][0] == -1)
		return;

	// find the top one

	MostVotes = TopVotes[0];

	// eliminate all those who have less than this (this is done because there
	// may be a tie)

	for (iTemp = 0; iTemp < 50; iTemp++)
		if (TopVotes[iTemp] < MostVotes)
			break;

	// where we stopped is the amount of votes tied for first place
	NumTied = iTemp-1;

	if (NumTied == 0) {
		// only one guy at the top, make him the new ruler
		NewRulerID[0] = TopCandidates[0][0];
		NewRulerID[1] = TopCandidates[0][1];
	}
	else {
		// choose one at random
		iTemp = RANDOM(NumTied);

		NewRulerID[0] = TopCandidates[iTemp][0];
		NewRulerID[1] = TopCandidates[iTemp][1];
	}

	// get his name, put it in the news
	GetClanNameID(szName, NewRulerID);

	// if same as yesterday's ruler, just tell people he was re-elected
	if (NewRulerID[0] == Village.Data->RulingClanId[0] &&
			NewRulerID[1] == Village.Data->RulingClanId[1]) {
		// sprintf(szString, ">> %s is re-elected as the leader of town!\n\n",
		sprintf(szString, ST_NEWS1, szName);
		News_AddNews(szString);
	}
	else {
		// sprintf(szString, ">> %s is elected as the new leader of town!\n\n",
		sprintf(szString, ST_NEWS2, szName);
		News_AddNews(szString);

		// make him the new ruler
		Village.Data->RulingClanId[0] = NewRulerID[0];
		Village.Data->RulingClanId[1] = NewRulerID[1];
		strcpy(Village.Data->szRulingClan, szName);
		Village.Data->RulingDays = 0;

		// make sure voting is allowed for the new ruler
		Village.Data->GovtSystem = GS_DEMOCRACY;

		// write message? -- no need, pretty obvious if it's first thing
		// in the news
	}
}
