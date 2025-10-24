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
 * IBBS ADT
 *
 * Portions of this code are taken from the IBBS samples provided
 * by Brian Pirie for the OpenDoors library.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utime.h>
#ifndef __unix__
# include <dos.h>
# include <share.h>
# ifndef _WIN32
#  include <dir.h>
# else
#  include <io.h>
# endif
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include <OpenDoor.h>

#include "alliance.h"
#include "door.h"
#include "empire.h"
#include "fight.h"
#include "game.h"
#include "help.h"
#include "ibbs.h"
#include "input.h"
#include "interbbs.h"
#include "k_ibbs.h"
#include "language.h"
#include "mail.h"
#include "misc.h"
#include "mstrings.h"
#include "myibbs.h"
#include "myopen.h"
#include "news.h"
#include "packet.h"
#include "parsing.h"
#include "readcfg.h"
#include "scores.h"
#include "structs.h"
#include "system.h"
#include "user.h"
#include "video.h"
#include "village.h"
#include "wb_fapnd.h"

#define MT_NORMAL       0
#define MT_CRASH        1
#define MT_HOLD         2

#define RECONDAYS       5

#ifdef _WIN32
struct ffblk {
	char ff_reserved[21];
	char ff_attrib;
	unsigned short ff_ftime;
	unsigned short ff_fdate;
	int32_t ff_fsize;
	char ff_name[13];
	HANDLE ff_findhandle; /* Added for Win32's API */
	char   ff_findattribute; /* Added for attribute matching on Win32 */
};

int findfirst(const char *pathname, struct ffblk *ffblk, int attribute);
int findnext(struct ffblk *);
static void convert_attribute(char *, uint32_t);
static int search_and_construct_ffblk(WIN32_FIND_DATA *, struct ffblk *, bool);

#ifndef _A_VOLID
# define _A_VOLID 0x08 /* not normally used in Win32 */
#endif

/* File Attributes */
#define FA_NORMAL _A_NORMAL /* 0x00 */
#define FA_RDONLY _A_RDONLY /* 0x01 */
#define FA_HIDDEN _A_HIDDEN /* 0x02 */
#define FA_SYSTEM _A_SYSTEM /* 0x04 */
#define FA_LABEL  _A_VOLID  /* 0x08 */
#define FA_DIREC  _A_SUBDIR /* 0x10 */
#define FA_ARCH   _A_ARCH   /* 0x20 */
#endif

struct ibbs IBBS;

static bool NoMSG[MAX_IBBSNODES];

static void IBBS_AddToGame(struct clan *Clan, bool WasLost);

// ------------------------------------------------------------------------- //
/* Function Procedure:
   Open file specified, copy entire contents and send as data in packet with
   the type specified in 'PacketType' to 'DestID'
*/
static void IBBS_SendFileInPacket(int16_t DestID, int16_t PacketType, char *szFileName)
{
	FILE *fp;
	char *cpBuffer;
	int32_t lFileSize;

	if (IBBS.Initialized == false) {
		System_Error("IBBS not initialized for call.\n");
	}

	fp = _fsopen(szFileName, "rb", SH_DENYRW);
	if (!fp)
		return;

	// find out length of file
	fseek(fp, 0L, SEEK_END);
	lFileSize = ftell(fp);

	// allocate memory for it
	cpBuffer = malloc(lFileSize);
	CheckMem(cpBuffer);

	// now read it in
	fseek(fp, 0L, SEEK_SET);
	fread(cpBuffer, lFileSize, sizeof(char), fp);

	fclose(fp);

	IBBS_SendPacket(PacketType, lFileSize, cpBuffer, DestID);

	free(cpBuffer);
}

// ------------------------------------------------------------------------- //
/* Function Procedure:
   Make sure destination ID is valid, send userlist.dat to that BBS
*/
static void IBBS_SendUserList(int16_t DestID)
{
	//printf("Sending userlist to %d\n", DestID);
	if (IBBS.Data.Nodes[DestID-1].Active)
		IBBS_SendFileInPacket(DestID, PT_ULIST, "userlist.dat");
}

// ------------------------------------------------------------------------- //
/* Function Procedure:
   Copy the entire world.ndx file.
   Cycle the BBS List and send the world.ndx to all active nodes.  Do not
   send it to this BBS
*/
void IBBS_DistributeNDX(void)
{
	FILE *fpNDX;
	char *cpBuffer;
	int16_t CurBBS;
	int32_t lFileSize;

	DisplayStr("Distributing new NDX file, please wait...\n");

	fpNDX = fopen("world.ndx", "rb");
	if (!fpNDX) {
		DisplayStr("world.ndx not found!\n");
		return;
	}

	// find out length of file
	fseek(fpNDX, 0L, SEEK_END);
	lFileSize = ftell(fpNDX);

	// allocate memory for it
	cpBuffer = malloc(lFileSize);
	CheckMem(cpBuffer);

	// now read it in
	fseek(fpNDX, 0L, SEEK_SET);
	fread(cpBuffer, lFileSize, sizeof(char), fpNDX);

	fclose(fpNDX);

	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active == false)
			continue;

		// skip if it's your own BBS
		if (CurBBS+1 == IBBS.Data.BBSID)
			continue;

		IBBS_SendPacket(PT_NEWNDX, lFileSize, cpBuffer, CurBBS+1);
	}

	free(cpBuffer);
}


// ------------------------------------------------------------------------- //
/* Function Procedure:
   Cycle the BBS List to find all active nodes.  If the BBSID of the current
   entry does not equal this BBS's ID, send a packet containing type of
   PT_SUBUSER and the passed UserInfo structure as the data.  Note: BBSIDs are
   1 based, not zero.
*/
void LeagueKillUser(struct UserInfo *User)
{
	// create packet for all BBSes saying "this user just committed suicide
	// on my board so remove him from your userlist"
	int16_t CurBBS;
	uint8_t pktBuf[BUF_SIZE_UserInfo];
	size_t ret = s_UserInfo_s(User, pktBuf, sizeof(pktBuf));
	assert(ret == sizeof(pktBuf));
	if (ret != sizeof(pktBuf))
		return;

	// for each BBS in the list, send him a packet
	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active == false)
			continue;

		// skip if it's your own BBS
		if (CurBBS+1 == IBBS.Data.BBSID)
			continue;

		IBBS_SendPacket(PT_SUBUSER, sizeof(pktBuf), pktBuf, CurBBS+1);
	}
}



// ------------------------------------------------------------------------- //
/* Function Procedure:
   [TargetType]
   - EO_VILLAGE:  Copy the village's name to the result target name
   - EO_CLAN   :  Verify Clan exists, if not set target as "-No longer exists-"
                  otherwise set target as name.
   Copy today's date to the result packet's date.
   Copy identifications from the attempt packet to the result packet
   This includes MasterID, ClanID and the BBSFrom/ToID.
   Do a random between the spy's intelligence level and the building's security
   level, if it's greater then set success and copy empire data.  Otherwise:
     If target was Village, send a message to ruler that a spy was caught.
     If target was clan, send a message to the clan that a spy was caught.
     Set the result packet success as false.
   Send an InterBBS packet as PT_SPYRESULT, SpyResultPacket as the data.
*/
static void IBBS_ProcessSpy(struct SpyAttemptPacket *Spy)
{
	struct SpyResultPacket SpyResult;
	struct empire *Empire = NULL;
	struct clan TmpClan = {0};
	bool NoTarget = false;
	char szMessage[600];
	int16_t Junk[2];
	uint8_t pktBuf[BUF_SIZE_SpyResultPacket];
	size_t res;

	// load up empire
	switch (Spy->TargetType) {
		case EO_VILLAGE :
			Empire = &Village.Data.Empire;
			strlcpy(SpyResult.szTargetName, Empire->szName, sizeof(SpyResult.szTargetName));
			break;
		case EO_CLAN :
			if (GetClan(Spy->ClanID, &TmpClan) == false) {
				NoTarget = true;
				strlcpy(SpyResult.szTargetName, "-No longer exists-", sizeof(SpyResult.szTargetName));
			}
			else
				strlcpy(SpyResult.szTargetName, TmpClan.Empire.szName, sizeof(SpyResult.szTargetName));
			Empire = &TmpClan.Empire;
			break;
	}

	strlcpy(SpyResult.szTheDate, System.szTodaysDate, sizeof(SpyResult.szTheDate));
	SpyResult.MasterID[0] = Spy->MasterID[0];
	SpyResult.MasterID[1] = Spy->MasterID[1];
	SpyResult.BBSFromID = Spy->BBSFromID;
	SpyResult.BBSToID = Spy->BBSToID;

	if (NoTarget == false && (Spy->IntelligenceLevel+RANDOM(5)) >
			(Empire->Buildings[B_SECURITY]+RANDOM(3))) {
		// success
		SpyResult.Success = true;
		SpyResult.Empire = *Empire;
	}
	else {
		// failure if no target OR otherwise
		SpyResult.Success = false;

		if (Spy->TargetType == EO_VILLAGE) {
			// snprintf(szMessage, sizeof(szMessage), " You caught a spy attempting to gain info on the village's empire.\n The spy was from %s.\n",
			snprintf(szMessage, sizeof(szMessage), ST_SPY4, Spy->szSpierName);
			GenericMessage(szMessage, Village.Data.RulingClanId, Junk, "", false);
		}
		else if (Spy->TargetType == EO_CLAN && NoTarget == false) {
			// snprintf(szMessage, sizeof(szMessage), " You caught a spy attempting to gain info on the village's empire.\n The spy was from %s.\n",
			snprintf(szMessage, sizeof(szMessage), ST_SPY6, Spy->szSpierName);
			GenericMessage(szMessage, TmpClan.ClanID, Junk, "", false);
		}
	}

	FreeClanMembers(&TmpClan);

	res = s_SpyResultPacket_s(&SpyResult, pktBuf, sizeof(pktBuf));
	assert(res == sizeof(pktBuf));
	if (res != sizeof(pktBuf))
		return;
	IBBS_SendPacket(PT_SPYRESULT, sizeof(pktBuf), pktBuf,
					Spy->BBSFromID);
}

/* Function Procedure:
   If the attempt was successful:
     Send a message to the spier with the following information:
     - Name of Target
     - VaultGold
     - Land
     - All Army Stats (Troops and Statistics)
     - All Building types and their names
   else
     Send failure message to spier
*/
static void IBBS_ProcessSpyResult(struct SpyResultPacket *SpyResult)
{
	char szMessage[600], szString[128];
	int16_t Junk[2], iTemp;

	if (SpyResult->Success) {
		// snprintf(szMessage, sizeof(szMessage), "Your spies sent to %s were successful and return with information:\n\nGold:  %ld\nLand:  %d\n\nTroops:  %ld footmen, %ld axemen, %ld knights\n\nBuildings:",
		snprintf(szMessage, sizeof(szMessage), ST_SPY7, SpyResult->Empire.szName,
				SpyResult->Empire.VaultGold,
				SpyResult->Empire.Land, SpyResult->Empire.Army.Footmen,
				SpyResult->Empire.Army.Axemen, SpyResult->Empire.Army.Knights,
				ArmySpeed(&SpyResult->Empire.Army),
				ArmyOffense(&SpyResult->Empire.Army),
				ArmyDefense(&SpyResult->Empire.Army),
				ArmyVitality(&SpyResult->Empire.Army));

		// buildings now

		for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
			if (SpyResult->Empire.Buildings[iTemp] == 0)
				continue;

			snprintf(szString, sizeof(szString), " %2d %s\n", SpyResult->Empire.Buildings[iTemp],
					BuildingType[iTemp].szName);
			strlcat(szMessage, szString, sizeof(szMessage));
		}

		GenericMessage(szMessage, SpyResult->MasterID, Junk, "", false);
	}
	else {
		snprintf(szMessage, sizeof(szMessage), "Your spy attempt on %s failed.\n", SpyResult->szTargetName);

		GenericMessage(szMessage, SpyResult->MasterID, Junk, "", false);
	}
}

// ------------------------------------------------------------------------- //
/* Function Procedure:
   If the BBS specified is active, sent the type:
   PT_DELUSER with UserInfo structure attached.  This tells the recipient BBS
   to remove the user with the ID specified.
*/
static void IBBS_SendDelUser(int16_t BBSID, struct UserInfo *User)
{
	uint8_t pktBuf[BUF_SIZE_UserInfo];
	size_t res;
	// run ONLY by main bbs
	// this will send a packet to a given bbs, telling the bbs to delete
	// the user with the clanid supplied

	res = s_UserInfo_s(User, pktBuf, sizeof(pktBuf));
	assert(res == sizeof(pktBuf));
	if (res != sizeof(pktBuf))
		return;
	if (IBBS.Data.Nodes[BBSID-1].Active)
		IBBS_SendPacket(PT_DELUSER, sizeof(pktBuf), pktBuf, BBSID);
}


// ------------------------------------------------------------------------- //
/* Function Procedure:
   Scan USERLIST.DAT for the specified ClanID, if found return true, else false
*/
static bool ClanIDInList(const int16_t ClanID[2])
{
	FILE *fpUList;
	bool Found = false;
	struct UserInfo User;

	fpUList = _fsopen("userlist.dat", "rb", SH_DENYWR);
	if (!fpUList) {
		// no user list found, assume not in list then
		return false;
	}

	// scan through file until end
	for (;;) {
		notEncryptRead_s(UserInfo, &User, fpUList, XOR_ULIST)
			break;

		// see if this user's name is same as one we're looking for
		if (User.ClanID[0] == ClanID[0] && User.ClanID[1] == ClanID[1]) {
			Found = true;
			break;
		}
	}

	fclose(fpUList);

	return Found;
}
/* Function Procedure:
  Verify the ClanID is on this system, otherwise ignore call
  Read all the users in USERLIST.DAT; find and remove the specified Clan.
*/
void RemoveFromUList(const int16_t ClanID[2])
{
	// open file for r+b
	// scan file for ClanID
	// set user as deleted
	// close file

	FILE *fpOldUList, *fpNewUList;
	struct UserInfo User;

	// if this guy is NOT in our list, then we don't need to do this
	if (ClanIDInList(ClanID) == false)
		return;

	// open user file
	fpOldUList = _fsopen("userlist.dat", "rb", SH_DENYWR);
	if (!fpOldUList)    // no user list at all
		return;

	fpNewUList = _fsopen("tmp.$$$", "wb", SH_DENYRW);
	// FIXME: assume file is opened

	for (;;) {
		notEncryptRead_s(UserInfo, &User, fpOldUList, XOR_ULIST)
			break;

//    printf("Read in %s\n", User.szName);

		// for each user in file, see if same as ClanID
		if (User.ClanID[0] == ClanID[0] && User.ClanID[1] == ClanID[1]) {
			//printf("skipping over %s\n", User.szName);
			// same, skip over him
			continue;
		}

		// otherwise, don't skip him, write him to new file
		EncryptWrite_s(UserInfo, &User, fpNewUList, XOR_ULIST);
	}

	// close file
	fclose(fpOldUList);
	fclose(fpNewUList);

	// rename file
	unlink("userlist.dat");
	rename("tmp.$$$", "userlist.dat");
}
/* Function Procedure:
   If the user is not already in the userlist, open USERLIST.DAT.
   Append the User to the end of the userlist.
*/
static void AddToUList(struct UserInfo *User)
{
	FILE *fpUList;

	// if this clan is in the list, don't bother
	if (ClanIDInList(User->ClanID))
		return;

	//printf("Adding %s to file\n", User->szName);

	// open user file
	fpUList = _fsopen("userlist.dat", "ab", SH_DENYWR);
	if (!fpUList)
		return;

	// append to file
	EncryptWrite_s(UserInfo, User, fpUList, XOR_ULIST);

	// close file
	fclose(fpUList);
}
/* Function Procedure:
   Cycle the BBS list, send the specified user to all active nodes with the
   packet type: PT_ADDUSER
*/
static void UpdateNodesOnNewUser(struct UserInfo *User)
{
	// run ONLY by main bbs
	// create packet for all BBSes saying "Here's a new user, add him to your
	// userlist"
	int16_t CurBBS;
	uint8_t pktBuf[BUF_SIZE_UserInfo];
	size_t res;

	res = s_UserInfo_s(User, pktBuf, sizeof(pktBuf));
	assert(res == sizeof(pktBuf));
	if (res != sizeof(pktBuf))
		return;

	// for each BBS in the list, send him a packet
	// must skip BBS #1 since that's this BBS (main BBS)
	for (CurBBS = 1; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active == false)
			continue;

		IBBS_SendPacket(PT_ADDUSER, sizeof(pktBuf), pktBuf, CurBBS+1);
	}
}

/* Function Procedure:
   If user is not on this board add them.
   If this BBS is not the main BBS, send the packet to the LC with type of PT_NEWUSER
   If this is the main BBS, call UpdateNodesOnNewUser, which tells all the BBSes about
   the user.
*/
void IBBS_LeagueNewUser(struct UserInfo *User)
{
	uint8_t pktBuf[BUF_SIZE_UserInfo];
	size_t res;

	// add this user to THIS bbs's user list now
	AddToUList(User);

	res = s_UserInfo_s(User, pktBuf, sizeof(pktBuf));
	assert(res == sizeof(pktBuf));
	if (res != sizeof(pktBuf))
		return;

	// if this is a node only
	if (IBBS.Data.BBSID != 1) {
		IBBS_SendPacket(PT_NEWUSER, sizeof(pktBuf), pktBuf, 1);
	}
	else {
		// if this is the main BBS, we DO NOT need to process him as the other
		// boards do since WE have the full list of players in the league already
		// assuming he got this far, his name was not yet on the list
		// and assuming WE got this far, then we already added him to our user
		// list, so we don't need to do that again, so all we need to do is send
		// a packet to all our other boards in the league saying "add this dude
		// to your user list"

		UpdateNodesOnNewUser(User);
	}
}



// ------------------------------------------------------------------------- //
/* Function Procedure:
   Check USERLIST.DAT for either ClanName or UserName that matches szName
   If found, return true else false
*/
bool IBBS_InList(char *szName, bool ClanName)
{
	// this and the other functions are ONLY used in InterBBS games.
	// they are NOT used in local games since they are redundant

	// returns true if user (either clan name or username) is already in
	// the list

	// if you have a user who's data isn't in this BBS's .PC file, run this
	// and if it returns true, it must mean the user is already playing on
	// another BBS

	// use this for new users who choose clan name, if already in use, tell
	// him after return true

	FILE *fpUList;
	bool Found = false;
	struct UserInfo User;

	fpUList = _fsopen("userlist.dat", "rb", SH_DENYWR);
	if (!fpUList) {
		// no user list found, assume not in list then
		return false;
	}

	// scan through file until end
	for (;;) {
		notEncryptRead_s(UserInfo, &User, fpUList, XOR_ULIST)
			break;

		// see if this user's name is same as one we're looking for
		if (ClanName) {
			if (stricmp(User.szName, szName) == 0) {
				Found = true;
				break;
			}
		}
		else {
			// compare with player's name
			if (stricmp(User.szMasterName, szName) == 0) {
				Found = true;
				break;
			}
		}
	}

	fclose(fpUList);

	return Found;
}


// ------------------------------------------------------------------------- //
/* Function Procedure:
   Create a packet that contains the ClanID and the BBSID for the data.
   Packet type will be PT_COMEBACK it is sent to the specified bbs id, it is
   sent from this bbs.  The packet is from an Active BBS, specifying today's
   date and this game's id.
*/
void IBBS_SendComeBack(int16_t BBSIdTo, struct clan *Clan)
{
	// write packet
	struct Packet Packet;
	FILE *fp;

	/* create packet header */
	Packet.Active = true;
	Packet.BBSIDFrom = IBBS.Data.BBSID;
	Packet.BBSIDTo = BBSIdTo;
	Packet.PacketType = PT_COMEBACK;
	strlcpy(Packet.szDate, System.szTodaysDate, sizeof(Packet.szDate));
	Packet.PacketLength = sizeof(int16_t)*3;
	strlcpy(Packet.GameID, Game.Data.GameID, sizeof(Packet.GameID));

	fp = _fsopen("tmp.$$$", "wb", SH_DENYWR);
	if (!fp) return;

	/* write packet */
	EncryptWrite_s(Packet, &Packet, fp, XOR_PACKET);

	// write info
	EncryptWrite16(&Clan->ClanID[0], fp, XOR_PACKET);
	EncryptWrite16(&Clan->ClanID[1], fp, XOR_PACKET);
	EncryptWrite16(&IBBS.Data.BBSID, fp, XOR_PACKET);

	fclose(fp);

	// send packet to BBS
	IBBS_SendPacketFile(Packet.BBSIDTo, "tmp.$$$");
	unlink("tmp.$$$");
}


// ------------------------------------------------------------------------- //
/* Function Procedure:
   Check the AttackPacket Type:
   If EO_VILLAGE:
      szAttackerName = "This village"
      Increase Footmen by Attacking army's Footmen
      Increase Axemen by Attacking army's Axemen
      Increase Knights by Attacking army's Knights
   If EO_CLAN:
      szAttackerName = "Your clan"
      If Clan found in database, increase Footmen, Axemen, and Knights of clan.
   Write message to attacker that their army has lost but returned.
*/
static void ReturnLostAttack(struct AttackPacket *AttackPacket)
{
	struct clan TmpClan;
	char szMessage[300], szAttackerName[20];
	int16_t WhichBBS, Junk[2];

	switch (AttackPacket->AttackingEmpire.OwnerType) {
		case EO_VILLAGE :
			strlcpy(szAttackerName, "This village", sizeof(szAttackerName));
			Village.Data.Empire.Army.Footmen += AttackPacket->AttackingArmy.Footmen;
			Village.Data.Empire.Army.Axemen  += AttackPacket->AttackingArmy.Axemen;
			Village.Data.Empire.Army.Knights += AttackPacket->AttackingArmy.Knights;
			break;
		case EO_CLAN :
			strlcpy(szAttackerName, "Your clan", sizeof(szAttackerName));
			if (GetClan(AttackPacket->AttackOriginatorID, &TmpClan)) {
				TmpClan.Empire.Army.Footmen += AttackPacket->AttackingArmy.Footmen;
				TmpClan.Empire.Army.Axemen  += AttackPacket->AttackingArmy.Axemen;
				TmpClan.Empire.Army.Knights += AttackPacket->AttackingArmy.Knights;
			}
			Clan_Update(&TmpClan);
			FreeClanMembers(&TmpClan);
			break;
	}

	// write message to AttackOriginator telling him the attack was lost
	// figure out which village it was
	WhichBBS = AttackPacket->BBSToID-1;

	snprintf(szMessage, sizeof(szMessage), "%s's attack on the village of %s was lost but returned.\nYou had sent the following: %" PRId32 " footmen, %" PRId32 " axemen, %" PRId32 " knights.",
			szAttackerName, IBBS.Data.Nodes[WhichBBS].Info.pszVillageName,
			AttackPacket->AttackingArmy.Footmen, AttackPacket->AttackingArmy.Axemen,
			AttackPacket->AttackingArmy.Knights);
	Junk[0] = Junk[1] = -1;
	GenericMessage(szMessage, AttackPacket->AttackOriginatorID, Junk, "", false);
}

/* Function Procedure:
   Read packets in the BACKUP.DAT file (if any).
   If the packet is marked as Active:
     If date of the packet is older than the days lost to the game process
     If Type == PT_CLANMOVE:
       Re-Add clan to game.
     If Type == PT_ATTACK:
       Return Lost troops to player due to backup-restore.
   If packet is newer than the days lost, resave it to the backup file.
   If packet is not marked as active, ignore it.
*/

static void IBBS_BackupMaint(void)
{
	/* read in old BACKUP.DAT, write only those which are Active */
	/* if packet which is older than 7 days is found, deal with it */

	FILE *fpOld;
	FILE *fpNew;
	struct Packet Packet;
	char *cpBuffer;
	int16_t iTemp;
	struct clan TmpClan;
	struct AttackPacket AttackPacket;
	struct pc TmpMember[6];

	fpOld = _fsopen("backup.dat", "rb", SH_DENYWR);
	if (!fpOld) {
		// DisplayStr("> No BACKUP.DAT to process.\n");
		return;
	}

	fpNew = _fsopen("backup.new", "wb", SH_DENYWR);
	if (!fpNew) {
		DisplayStr("> Error writing BACKUP.DAT\n");
		return;
	}

	for (;;) {
		notEncryptRead_s(Packet, &Packet, fpOld, XOR_PACKET)
			break;

		if (Packet.Active) {
			if (DaysBetween(Packet.szDate, System.szTodaysDate) > Game.Data.LostDays) {
				// see if it's also a travel packet type
				// if so do this
				if (Packet.PacketType == PT_CLANMOVE) {
					// player travel was screwed up, so restore him to .PC
					// file

					EncryptRead_s(clan, &TmpClan, fpOld, XOR_PACKET);

					// read members
					for (iTemp = 0; iTemp < 6; iTemp++) {
						TmpClan.Member[iTemp] = &TmpMember[iTemp];
						EncryptRead_s(pc, TmpClan.Member[iTemp], fpOld, XOR_PACKET);
					}

					IBBS_AddToGame(&TmpClan, true);
					FreeClanMembers(&TmpClan);

					// tell other board to comeback in case of problems
					// --> NO!!  SendComeBack(TmpClan->DestinationBBS, TmpClan);
					continue;
				}
				else if (Packet.PacketType == PT_ATTACK) {
					// an empire attack

					// get attackpacket
					EncryptRead_s(AttackPacket, &AttackPacket, fpOld, XOR_PACKET);

					// find clan who was doing the attacking
					// write him a message telling of the lost troops
					ReturnLostAttack(&AttackPacket);

					// update village's, alliance's OR clan's empire stats
					continue;
				}
			}

			//printf("Packet type is %d\n", Packet.PacketType);

			/* write it to file */
			EncryptWrite_s(Packet, &Packet, fpNew, XOR_PACKET);

			/* write buffer if any */
			if (Packet.PacketLength) {
				//printf("length found\n");
				cpBuffer = malloc(Packet.PacketLength);
				CheckMem(cpBuffer);
				EncryptRead(cpBuffer, Packet.PacketLength, fpOld, XOR_PACKET);
				EncryptWrite(cpBuffer, Packet.PacketLength, fpNew, XOR_PACKET);
				free(cpBuffer);
			}
		}
		else {
			if (Packet.PacketLength)
				fseek(fpOld, Packet.PacketLength, SEEK_CUR);
		}
	}

	/* done */

	fclose(fpNew);
	fclose(fpOld);

	unlink("backup.dat");
	rename("backup.new", "backup.dat");
}

// ------------------------------------------------------------------------- //
/* Function Procedure:
   Read LEAVING.DAT, find the clan's ID in the file.
   If found mark it as inactive and resave LEAVING.DAT.
   Reset the clan's WorldStatus to WS_STAYING and the DestinationBBS as -1
*/

static void AbortTrip(void)
{
	struct LeavingData LeavingData;
	FILE *fpLeavingDat;
	int16_t CurEntry = -1;

	/* open LEAVING.DAT */
	fpLeavingDat = _fsopen("leaving.dat", "r+b", SH_DENYRW);
	if (!fpLeavingDat) {
		return;
	}

	/* find that guy's entry which is active, make it inactive, write it to file */

	for (;;) {
		CurEntry++;
		notEncryptRead_s(LeavingData, &LeavingData, fpLeavingDat, XOR_TRAVEL) {
			//rputs("|04Couldn't find data in LEAVING.DAT file\n");
			break;
		}

		if (LeavingData.Active == false)    /* skip if inactive */
			continue;

		/* see if that's the one */
		if (LeavingData.ClanID[0] == PClan->ClanID[0] &&
				LeavingData.ClanID[1] == PClan->ClanID[1]) {
			/* found it! */
			fseek(fpLeavingDat, (long)CurEntry * BUF_SIZE_LeavingData, SEEK_SET);

			LeavingData.Active = false;

			EncryptWrite_s(LeavingData, &LeavingData, fpLeavingDat, XOR_TRAVEL);
			break;
		}
	}
	fclose(fpLeavingDat);

	/* set this guy's trip stuff to none */
	PClan->WorldStatus = WS_STAYING;
	PClan->DestinationBBS = -1;

	rputs("|04Trip aborted.\n");
}


/* Function Procedure:
   If the clan is set to leave (WorldStatus == WS_LEAVING) display the destination.
   Ask if user wants to abort trip.
   If no trip scheduled, tell user
*/
void IBBS_CurrentTravelInfo(void)
{
	char szString[90];

	/* see if travelling already */
	if (PClan->WorldStatus == WS_LEAVING) {
		snprintf(szString, sizeof(szString), "|0SYou are set to leave for %s.\n",
				IBBS.Data.Nodes[ PClan->DestinationBBS-1 ].Info.pszVillageName);
		rputs(szString);

		if (YesNo("|0SDo you wish to abort the trip?") == YES) {
			AbortTrip();
		}
	}
	else
		rputs("|0BNo trip is currently scheduled\n%P");
}

// ------------------------------------------------------------------------- //
/* Function Procedure:
   Set clan's worldstatus to WS_GONE.
   Add News entry for clan leaving.
   Fix the Leaving stats appropriately (Followers/Footmen/Axemen/Knights)
   Reduce the clan's current stats by the leaving stats (Followers/Footmen/Axemen/Knights)
   Update the clan's entry with these new values
   Set the clan to their leaving values (transfer all vault gold to the leaving group)
*/
static void IBBS_UpdateLeavingClan(struct clan *Clan, struct LeavingData LeavingData)
{
	char szString[128];
	int32_t Gold;

	/* update news */
	snprintf(szString, sizeof(szString), ST_NEWSCLANEXIT, Clan->szName);
	News_AddNews(szString);

	Clan->WorldStatus = WS_GONE;

	// fix up the stats for that clan's troops
	if (LeavingData.Followers > Clan->Empire.Army.Followers)
		LeavingData.Followers = Clan->Empire.Army.Followers;
	if (LeavingData.Footmen > Clan->Empire.Army.Footmen)
		LeavingData.Footmen = Clan->Empire.Army.Footmen;
	if (LeavingData.Axemen > Clan->Empire.Army.Axemen)
		LeavingData.Axemen = Clan->Empire.Army.Axemen;
	if (LeavingData.Knights > Clan->Empire.Army.Knights)
		LeavingData.Knights = Clan->Empire.Army.Knights;

	// reduce the stats on THIS bbs before writing to this BBS's data file
	Clan->Empire.Army.Followers -= LeavingData.Followers;
	Clan->Empire.Army.Footmen -= LeavingData.Footmen;
	Clan->Empire.Army.Axemen -= LeavingData.Axemen;
	Clan->Empire.Army.Knights -= LeavingData.Knights;

	Gold = Clan->Empire.VaultGold;
	Clan->Empire.VaultGold = 0;

	Clan_Update(Clan);

	// set the stats for the traveling clan now
	Clan->Empire.VaultGold = Gold;
	Clan->Empire.Army.Followers  = LeavingData.Followers;
	Clan->Empire.Army.Footmen = LeavingData.Footmen;
	Clan->Empire.Army.Axemen = LeavingData.Axemen;
	Clan->Empire.Army.Knights = LeavingData.Knights;
}


static void IBBS_TravelMaint(void)
{
	struct LeavingData LeavingData;
	struct Packet Packet;
	FILE *fpLeavingDat, *fpOutboundDat, *fpOld;
	struct clan TmpClan = {0};
	struct pc TmpPC = {0};
	int16_t iTemp;

	// DisplayStr("IBBS_TravelMaint()\n");

	fpLeavingDat = _fsopen("leaving.dat", "rb", SH_DENYWR);
	if (!fpLeavingDat) {
		return;
	}

	for (;;) {
		/* reading leavingdata structure */
		notEncryptRead_s(LeavingData, &LeavingData, fpLeavingDat, XOR_TRAVEL) {
			/* no more to read */
			break;
		}

		if (LeavingData.Active == false)  /* skip inactive ones */
			continue;

		/* get Clan struct from .PC file */
		if (GetClan(LeavingData.ClanID, &TmpClan) == false) {
			continue;
		}

		// if already left OR he's deleted, skip
		if (TmpClan.WorldStatus == WS_GONE) {
			FreeClanMembers(&TmpClan);
			InitClan(&TmpClan);
			continue;
		}

		IBBS_UpdateLeavingClan(&TmpClan, LeavingData);

		TmpClan.WorldStatus = WS_STAYING;

		/* write packet file */
		fpOutboundDat = _fsopen(ST_OUTBOUNDFILE, "wb", SH_DENYWR);
		if (!fpOutboundDat) {
			DisplayStr("|04Error writing temporary outbound.tmp file\n");
			fclose(fpLeavingDat);

			FreeClanMembers(&TmpClan);
			return;
		}

		fpOld = _fsopen("backup.dat", "ab", SH_DENYRW);
		if (!fpOld) {
			DisplayStr("|04Can't open BACKUP.DAT!!\n");
			FreeClanMembers(&TmpClan);
			fclose(fpLeavingDat);
			return;
		}

		/* BACKUP.DAT is checked daily.  When DataOk messages are sent from
		   BBSes, entries in BACKUP.DAT are set to "inactive" and are skipped
		   -- on next maintenance, inactive data in BACKUP.DAT is purged.
		   * if backup.dat is found which is a week old (meaning no DataOk
		     was received for a week), this BBS processes the user in as if
		     he were travelling here.  In the news, it'll say "Blah was lost
		     but returned to Blah"  --
		     FIXME:  then send packet saying "he aborted the trip, so make him
		     "comeback"
		*/

		/* create packet header */
		Packet.Active = true;
		Packet.BBSIDFrom = IBBS.Data.BBSID;
		Packet.BBSIDTo = LeavingData.DestID;
		Packet.PacketType = PT_CLANMOVE;
		strlcpy(Packet.szDate, System.szTodaysDate, sizeof(Packet.szDate));
		Packet.PacketLength = BUF_SIZE_clan + 6 * BUF_SIZE_pc;
		strlcpy(Packet.GameID, Game.Data.GameID, sizeof(Packet.GameID));

		/* write packet */
		EncryptWrite_s(Packet, &Packet, fpOutboundDat, XOR_PACKET);
		EncryptWrite_s(Packet, &Packet, fpOld, XOR_PACKET);

		EncryptWrite_s(clan, &TmpClan, fpOutboundDat, XOR_PACKET);
		EncryptWrite_s(clan, &TmpClan, fpOld, XOR_PACKET);

		/* write members to file */
		TmpPC.szName[0] = 0;
		TmpPC.Status = Dead;
		for (iTemp = 0; iTemp < 6; iTemp++) {
			if (TmpClan.Member[iTemp]) {
				EncryptWrite_s(pc, TmpClan.Member[iTemp], fpOutboundDat, XOR_PACKET);
				EncryptWrite_s(pc, TmpClan.Member[iTemp], fpOld, XOR_PACKET);
			}
			else {
				EncryptWrite_s(pc, &TmpPC, fpOutboundDat, XOR_PACKET);
				EncryptWrite_s(pc, &TmpPC, fpOld, XOR_PACKET);
			}
		}

		fclose(fpOld);
		fclose(fpOutboundDat);

		/* send this packet to the destination BBS */
		IBBS_SendPacketFile(LeavingData.DestID, ST_OUTBOUNDFILE);
		unlink(ST_OUTBOUNDFILE);

		// need to free up members used...
		FreeClanMembers(&TmpClan);
		InitClan(&TmpClan);
	}
	fclose(fpLeavingDat);
	unlink("leaving.dat");
}



// ------------------------------------------------------------------------- //
static void GetTroopsTraveling(struct LeavingData *LeavingData)
{
	char szString[128], cInput;

	rputs("|0CNow, you must choose the troops that will travel with the clan.\nThe rest will stay behind in this village.\n\n");

	for (;;) {
		rputs(" |0BTroops Traveling\n");
		rputs(ST_LONGLINE);

		// show troops currently going
		snprintf(szString, sizeof(szString), " |0A(|0BA|0A) |0CFollowers        |14%" PRId32 "\n", LeavingData->Followers);
		rputs(szString);
		snprintf(szString, sizeof(szString), " |0A(|0BB|0A) |0CFootmen          |14%" PRId32 "\n", LeavingData->Footmen);
		rputs(szString);
		snprintf(szString, sizeof(szString), " |0A(|0BC|0A) |0CAxemen           |14%" PRId32 "\n", LeavingData->Axemen);
		rputs(szString);
		snprintf(szString, sizeof(szString), " |0A(|0BD|0A) |0CKnights          |14%" PRId32 "\n", LeavingData->Knights);
		rputs(szString);
		rputs(" |0A(|0B0|0A) |0CDone\n");
		rputs(ST_LONGLINE);
		rputs(" ");
		rputs(ST_ENTEROPTION);
		rputs("Done");

		cInput = od_get_answer("ABCD0\r\n");

		rputs("\b\b\b\b    \b\b\b\b|15");

		if (cInput == '0' || cInput == '\r' || cInput == '\n')
			break;

		switch (cInput) {
			case 'A' :
				rputs("Followers\n\n");
				LeavingData->Followers = GetLong("|0SHow many followers?",
												 LeavingData->Followers, PClan->Empire.Army.Followers);
				break;
			case 'B' :
				rputs("Footmen\n\n");
				LeavingData->Footmen = GetLong("|0SHow many footmen?",
											   LeavingData->Footmen, PClan->Empire.Army.Footmen);
				break;
			case 'C' :
				rputs("Axemen\n\n");
				LeavingData->Axemen = GetLong("|0SHow many Axemen?",
											  LeavingData->Axemen , PClan->Empire.Army.Axemen);
				break;
			case 'D' :
				rputs("Knights\n\n");
				LeavingData->Knights = GetLong("|0SHow many Knights?",
											   LeavingData->Knights, PClan->Empire.Army.Knights);
				break;
		}
		rputs("\n");
	}
	rputs("Done\n\n");
}

static bool IBBS_TravelToBBS(int16_t DestID)
{
	char szString[128];
	struct LeavingData LeavingData;
	FILE *fpLeavingDat;

	/* see if it's this BBS */
	if (IBBS.Data.BBSID == DestID) {
		rputs("You're already here!\n");
		return false;
	}

	/* if no recon yet */
	if (IBBS.Data.Nodes[DestID-1].Recon.LastReceived == 0 ||
			(DaysSince1970(System.szTodaysDate) - IBBS.Data.Nodes[DestID-1].Recon.LastReceived) >= 10) {
		// if no recon yet don't allow travel
		// if days since last recon is >= 10 days, don't allow travel

		rputs("|12The path to that BBS is currently hazy.  Try again later.\n\n");
		return false;
	}

	/* see if travelling already */
	if (PClan->WorldStatus == WS_LEAVING) {
		snprintf(szString, sizeof(szString), "|0SYou are already set to leave for %s!\n",
				IBBS.Data.Nodes[ PClan->DestinationBBS-1 ].Info.pszVillageName);
		rputs(szString);

		if (YesNo("|0SDo you wish to abort the trip?") == YES) {
			AbortTrip();
		}
		else
			return false;
	}

	Help("Traveling", ST_VILLHLP);

	snprintf(szString, sizeof(szString), "|0SAre you sure you wish to travel to %s?",
			IBBS.Data.Nodes[ DestID-1 ].Info.pszVillageName);

	if (YesNo(szString) == YES) {
		/* append LEAVING.DAT file */
		LeavingData.DestID = DestID;
		LeavingData.Active = true;
		LeavingData.ClanID[0] = PClan->ClanID[0];
		LeavingData.ClanID[1] = PClan->ClanID[1];

		// ask user how many guys to bring along
		LeavingData.Followers = 0;
		LeavingData.Footmen = 0;
		LeavingData.Axemen = 0;
		LeavingData.Knights = 0;
		LeavingData.Catapults = 0;
		GetTroopsTraveling(&LeavingData);

		fpLeavingDat = _fsopen("leaving.dat", "ab", SH_DENYRW);
		if (!fpLeavingDat) {
			//rputs("|04Couldn't open LEAVING.DAT file\n");
			return false;
		}
		EncryptWrite_s(LeavingData, &LeavingData, fpLeavingDat, XOR_TRAVEL);
		fclose(fpLeavingDat);

		PClan->WorldStatus = WS_LEAVING;
		PClan->DestinationBBS = DestID;

		if (YesNo("|0SDo you wish to exit the game now?") == YES) {
			System_Close();
		}
	}
	else {
		rputs("|04Trip aborted.\n");
		return false;
	}

	rputs("|14Trip set!\n");
	return true;
}

void IBBS_SeeVillages(bool Travel)
{
	/* view other villages and their info */
	/* allow user to choose village as he would a help file */
	/* display info on that village using villinfo.txt file */
	/* display last stats for that village -- ruler, tax rate, etc. */
	/* if user enters blank line, quit */

	int16_t iTemp, NumBBSes, WhichBBS;
	char *pszBBSNames[MAX_IBBSNODES], szString[128], BBSIndex[MAX_IBBSNODES];
	bool ShowInitially;

	NumBBSes = 0;
	for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
		pszBBSNames[iTemp] = NULL;

		if (IBBS.Data.Nodes[iTemp].Active == false)
			continue;

		pszBBSNames [ NumBBSes ] = IBBS.Data.Nodes[iTemp].Info.pszVillageName;
		BBSIndex[NumBBSes] = iTemp;
		NumBBSes++;
	}

	ShowInitially = true;

	for (;;) {
		// choose which BBS
		GetStringChoice(pszBBSNames, NumBBSes, "|0SChoose which village.\n|0E> |0F",
						&WhichBBS, ShowInitially, DT_WIDE, true);

		ShowInitially = false;

		if (WhichBBS == -1)
			return;

		rputs("\n");
		Help(IBBS.Data.Nodes[0+(BBSIndex[WhichBBS])].Info.pszVillageName, "world.ndx");
		rputs("\n");

		if (Travel) {
			/* travel stuff here */
			snprintf(szString, sizeof(szString), "|0STravel to %s?", pszBBSNames[(BBSIndex[WhichBBS]+0)]);
			if (NoYes(szString) == YES) {
				/* travel stuff here */
				if (IBBS_TravelToBBS(BBSIndex[WhichBBS]+1))
					return;
			}
		}
	}
}



// ------------------------------------------------------------------------- //
static void IBBS_AddLCLog(char *szString)
{
	FILE *fpLCLogFile;

	/* open news file */

	/* add to it */

	fpLCLogFile = _fsopen("lc.log", "at", SH_DENYWR);

	fputs(szString, fpLCLogFile);

	fclose(fpLCLogFile);
}

// ------------------------------------------------------------------------- //

static void IBBS_Destroy(void)
{
	int16_t CurBBS;

	if (!IBBS.Initialized)
		return;

	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active) {
			if (IBBS.Data.Nodes[CurBBS].Info.pszBBSName)
				free(IBBS.Data.Nodes[CurBBS].Info.pszBBSName);
			if (IBBS.Data.Nodes[CurBBS].Info.pszVillageName)
				free(IBBS.Data.Nodes[CurBBS].Info.pszVillageName);
			if (IBBS.Data.Nodes[CurBBS].Info.pszAddress)
				free(IBBS.Data.Nodes[CurBBS].Info.pszAddress);
		}
	}

	IBBS.Initialized = false;
}

// ------------------------------------------------------------------------- //

void IBBS_LoginStats(void)
/*
 * Shows IBBS stats for local login.
 */
{
	char szString[128];

	snprintf(szString, sizeof(szString), "|02\xf0 |07This BBS is in the InterBBS league %s %s (GameID = %s)\n",
			Game.Data.LeagueID, Game.Data.szWorldName, Game.Data.GameID);
	zputs(szString);
}

// ------------------------------------------------------------------------- //

static void IBBS_Create(void)
{
	int16_t CurBBS;

	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active) {
			IBBS.Data.Nodes[CurBBS].Reset.Received = false;
			IBBS.Data.Nodes[CurBBS].Reset.LastSent = 0;

			IBBS.Data.Nodes[CurBBS].Recon.LastReceived = 0;
			IBBS.Data.Nodes[CurBBS].Recon.LastSent = 0;
			IBBS.Data.Nodes[CurBBS].Recon.PacketIndex = 'a';

			IBBS.Data.Nodes[CurBBS].Attack.ReceiveIndex = 0;
			IBBS.Data.Nodes[CurBBS].Attack.SendIndex = 0;
		}
	}
}

static void IBBS_Read(void)
{
	FILE *fp;
	int16_t CurBBS;

	fp = _fsopen("ibbs.dat", "rb", SH_DENYWR);
	if (!fp) {
		IBBS_Create();
		return;
	}

	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active) {
			EncryptRead_s(ibbs_node_recon, &IBBS.Data.Nodes[CurBBS].Recon, fp, XOR_IBBS);
			EncryptRead_s(ibbs_node_attack, &IBBS.Data.Nodes[CurBBS].Attack, fp, XOR_IBBS);
		}
		else {
			fseek(fp, BUF_SIZE_ibbs_node_attack + BUF_SIZE_ibbs_node_recon, SEEK_CUR);
		}
	}

	// now read in reset data
	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active) {
			notEncryptRead_s(ibbs_node_reset, &IBBS.Data.Nodes[CurBBS].Reset, fp, XOR_IBBS) {
				IBBS.Data.Nodes[CurBBS].Reset.Received = false;
				IBBS.Data.Nodes[CurBBS].Reset.LastSent = 0;
			}
		}
		else {
			fseek(fp, BUF_SIZE_ibbs_node_reset, SEEK_CUR);
		}
	}

	fclose(fp);
}

static void IBBS_Write(void)
{
	FILE *fp;
	int16_t CurBBS;
	struct ibbs_node_attack DummyAttack;
	struct ibbs_node_recon DummyRecon;
	struct ibbs_node_reset DummyReset;

	fp = _fsopen("ibbs.dat", "wb", SH_DENYRW);
	if (!fp) {
		return;
	}

	DummyReset.Received = false;
	DummyReset.LastSent = 0;

	DummyRecon.LastReceived = 0;
	DummyRecon.LastSent = 0;
	DummyRecon.PacketIndex = 'a';

	DummyAttack.ReceiveIndex = 0;
	DummyAttack.SendIndex = 0;

	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active) {
			EncryptWrite_s(ibbs_node_recon, &IBBS.Data.Nodes[CurBBS].Recon, fp, XOR_IBBS);
			EncryptWrite_s(ibbs_node_attack, &IBBS.Data.Nodes[CurBBS].Attack, fp, XOR_IBBS);
		}
		else {
			EncryptWrite_s(ibbs_node_recon, &DummyRecon, fp, XOR_IBBS);
			EncryptWrite_s(ibbs_node_attack, &DummyAttack, fp, XOR_IBBS);
		}
	}

	// now write reset data
	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active) {
			EncryptWrite_s(ibbs_node_reset, &IBBS.Data.Nodes[CurBBS].Reset, fp, XOR_IBBS);
		}
		else {
			EncryptWrite_s(ibbs_node_reset, &DummyReset, fp, XOR_IBBS);
		}
	}

	fclose(fp);
}

// ------------------------------------------------------------------------- //

static int16_t FoundPoint = false;
static int16_t Level = 0;
static int16_t StartingPoint;
static struct Point {
	char BackLink;
	char ForwardLinks[MAX_IBBSNODES];
} *Points[MAX_IBBSNODES];


static void FindPoint(int16_t Destination, int16_t Source, int16_t BasePoint)
{
	int16_t CurLink;

	Level++;

	/* see if we're there */
	if (Source == Destination) {
		if (StartingPoint == -1)
			StartingPoint = Source;

		//printf("Found %d.  Starting point was %d\n", Source+1, StartingPoint+1);
		FoundPoint = true;
		Level--;
		return;
	}

	/* try forward links from Source */
	for (CurLink = 0; CurLink < MAX_IBBSNODES; CurLink++) {
		if (Points[Source]->ForwardLinks[CurLink] != -1) {
			/* if link found is same as basepoint, don't go through it */
			if (Points[Source]->ForwardLinks[CurLink] == BasePoint)
				continue;

			if (Level == 1) {
				StartingPoint = Points[Source]->ForwardLinks[CurLink];
				//printf("Trying %d\n", StartingPoint);
			}

			//printf("Trying [%d][%d]\n", Source, CurLink);

			/* found a link, try it */
			FindPoint(Destination, Points[Source]->ForwardLinks[CurLink], Source);

			if (FoundPoint) {
				Level--;
				return;
			}
		}
		else break;         /* no more links */
	}

	/* tried all those points, now try backlinking */
	if (Points[Source]->BackLink != -1) {
		/* if backlink just returns you, return */
		if (Points[Source]->BackLink == BasePoint) {
			Level--;
			return;
		}

		if (Level == 1)
			StartingPoint = Points[Source]->BackLink;

		/* found a link, try it */
		FindPoint(Destination, Points[Source]->BackLink, Source);
	}

	Level--;
}


static void IBBS_LoadNDX(void)
/*
 * Loads up world.ndx file
 */
{
	FILE *fpWorld;
	char szLine[400], *pcCurrentPos, szString[400];
	char szToken[MAX_TOKEN_CHARS + 1];
	int16_t iKeyWord;
	int16_t CurBBS = 0;
	int16_t iTemp, CurSlot;
	bool UseHost = false;           // set if using host method of routing
	bool InDescription = false;

	/* make all bbs NULL pointers */
	for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++)
		IBBS.Data.Nodes[iTemp].Active = false;

	/* read in .NDX file */
	fpWorld = _fsopen("world.ndx", "r", SH_DENYWR);
	if (!fpWorld) {
		// DisplayStr("|12Could not find world.ndx file.  If you are not in an InterBBS league,\nturn off the InterBBS option in the CONFIG.EXE program.\n\n");
		IBBS_Destroy();
		System_Error(ST_NOWORLDNDX);
	}

	for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++)
		Points[iTemp] = NULL;

	for (;;) {
		/* read in a line */
		if (fgets(szLine, 400, fpWorld) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;
		ParseLine(pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;

		GetToken(pcCurrentPos, szToken);

		if (szToken[0] == '$')
			break;

		if (szToken[0] == '^') {
			InDescription = !InDescription;
			continue;
		}

		if (InDescription)
			continue;

		/* Loop through list of keywords */
		for (iKeyWord = 0; iKeyWord < MAX_NDX_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (stricmp(szToken, papszNdxKeyWords[iKeyWord]) == 0) {
				/* Process token */
				switch (iKeyWord) {
					case 5 :    /* num of BBS in list */
						/* see if out of items memory yet */
						CurBBS = atoi(pcCurrentPos) - 1;
						if (CurBBS < 0 || CurBBS >= MAX_IBBSNODES)
							System_Error("BBSId out of bounds!\n");

						if (Points[CurBBS] == NULL) {
							Points[CurBBS] = malloc(sizeof(struct Point));
							CheckMem(Points[CurBBS]);

							/* nullify all links since initialized it */
							for (CurSlot = 0; CurSlot < MAX_IBBSNODES; CurSlot++)
								Points[CurBBS]->ForwardLinks[CurSlot] = -1;

							Points[CurBBS]->BackLink = -1;
						}

						/* activate this BBS */

						IBBS.Data.Nodes[CurBBS].Active = true;
						IBBS.Data.Nodes[CurBBS].Info.pszBBSName = NULL;
						IBBS.Data.Nodes[CurBBS].Info.pszVillageName = NULL;
						IBBS.Data.Nodes[CurBBS].Info.pszAddress = NULL;
						IBBS.Data.Nodes[CurBBS].Info.RouteThrough = CurBBS+1;
						IBBS.Data.Nodes[CurBBS].Info.MailType = MT_NORMAL;

						IBBS.Data.Nodes[CurBBS].Attack.SendIndex = 0;
						IBBS.Data.Nodes[CurBBS].Attack.ReceiveIndex = 0;
						break;
					case 1 :    /* village name */
						IBBS.Data.Nodes[CurBBS].Info.pszVillageName = DupeStr(pcCurrentPos);
						break;
					case 2 :    /* Address */
						IBBS.Data.Nodes[CurBBS].Info.pszAddress = DupeStr(pcCurrentPos);
						break;
					case 0 :    /* BBS name */
						IBBS.Data.Nodes[CurBBS].Info.pszBBSName = DupeStr(pcCurrentPos);
						break;
					case 6 :    /* Status */
						//printf("status: currently unused\n");
						break;
					case 7 :    /* worldname */
						strlcpy(Game.Data.szWorldName, pcCurrentPos, sizeof(Game.Data.szWorldName));
						break;
					case 8 :    /* league ID */
						pcCurrentPos[2] = 0;    /* truncate it to two chars */
						strlcpy(Game.Data.LeagueID, pcCurrentPos, sizeof(Game.Data.LeagueID));
						break;
					case 9 :        /* Host */
						/* get tokens till no more */
						UseHost = true;

						for (;;) {
							GetToken(pcCurrentPos, szString);

							if (szString[0] == 0)
								break;

							iTemp = atoi(szString);

							// REP: delete later
							//printf("Linking %d through %d\n", iTemp, CurBBS+1);
							//getch();

							/* Node "iTemp" is a node of this host */

							if (Points[iTemp-1] == NULL) {
								//printf("Initializing node %d\n", iTemp);
								Points[iTemp-1] = (struct Point *) malloc(sizeof(struct Point));
								CheckMem(Points[iTemp-1]);

								/* nullify all links since initialized it */
								for (CurSlot = 0; CurSlot < MAX_IBBSNODES; CurSlot++)
									Points[iTemp-1]->ForwardLinks[CurSlot] = -1;

								Points[iTemp-1]->BackLink = -1;
							}


							Points[ iTemp-1 ]->BackLink = CurBBS;

							/* find open slot for this forward link */
							for (CurSlot = 0; CurSlot < MAX_IBBSNODES; CurSlot++)
								if (Points[CurBBS]->ForwardLinks[CurSlot] == -1)
									break;

							/* set up forwardlink */
							Points[CurBBS]->ForwardLinks[CurSlot] = iTemp-1;

						}
						break;
					case 10 : /* NoMSG */
						NoMSG[CurBBS] = true;
						break;
				}
				break;
			}
		}
	}

	// If keyword "Host" was found in the world.ndx file, Hosting method
	// is used, so figure out routing
	if (UseHost)
		for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
			if (IBBS.Data.Nodes[iTemp].Active == false)
				continue;

			//printf("searching for %d\n", iTemp+1);
			FoundPoint = false;
			StartingPoint = -1;
			FindPoint(iTemp, IBBS.Data.BBSID-1, IBBS.Data.BBSID-1);

			IBBS.Data.Nodes[iTemp].Info.RouteThrough = StartingPoint+1;
			//printf("%d routed through %d\n", iTemp+1, IBBS.Data.Nodes[iTemp].Info.RouteThrough);
		}

	/* get rid of Point memory */
	for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++)
		if (Points[iTemp]) {
			//printf("Removing point %d from memory\n", iTemp+1);
			free(Points[iTemp]);
		}

	fclose(fpWorld);
}

// ------------------------------------------------------------------------- //

static void IBBS_ProcessRouteConfig(void)
{
	FILE *fpRouteConfigFile;
	char szRouteConfigLine[255];
	char *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1], szFirstToken[20], szSecondToken[20];
	int16_t iKeyWord, iTemp;

	fpRouteConfigFile = _fsopen("route.cfg", "rt", SH_DENYWR);
	if (!fpRouteConfigFile) {
		return;
	}

	for (;;) {
		/* read in a line */
		if (fgets(szRouteConfigLine, 255, fpRouteConfigFile) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szRouteConfigLine;
		ParseLine(pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;

		GetToken(pcCurrentPos, szToken);

		if (szToken[0] == '$')
			break;

		/* Loop through list of keywords */
		for (iKeyWord = 0; iKeyWord < MAX_RT_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (stricmp(szToken, papszRouteKeyWords[iKeyWord]) == 0) {
				/* Process config token */
				switch (iKeyWord) {
					case 0 :    /* route */
						GetToken(pcCurrentPos, szFirstToken);
						GetToken(pcCurrentPos, szSecondToken);

						//printf("Token1 = [%s]\nToken2 = [%s]\n", szFirstToken, szSecondToken);
						if (stricmp(szFirstToken, "ALL") == 0) {
							//printf("all mail being routed through %d\n", atoi(szSecondToken));
							/* route all through somewhere else */
							for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
								if (IBBS.Data.Nodes[iTemp].Active == false)
									continue;

								IBBS.Data.Nodes[iTemp].Info.RouteThrough = atoi(szSecondToken);
							}
						}
						else {
							IBBS.Data.Nodes[ atoi(szFirstToken)-1 ].Info.RouteThrough = atoi(szSecondToken);
							//printf("Mail to %d being routed through %d\n", atoi(szFirstToken), atoi(szSecondToken));
						}
						break;
					case 1 :    /* crash */
						GetToken(pcCurrentPos, szFirstToken);

						//printf("Token1 = %s\n", szFirstToken);
						if (stricmp(szFirstToken, "ALL") == 0) {
							//printf("all mail being crashed\n");
							for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
								if (IBBS.Data.Nodes[iTemp].Active == false)
									continue;

								IBBS.Data.Nodes[iTemp].Info.MailType = MT_CRASH;
							}
						}
						else {
							IBBS.Data.Nodes[ atoi(szFirstToken)-1 ].Info.MailType = MT_CRASH;
							//printf("Mail to %d being crashed\n", atoi(szFirstToken));
						}
						break;
					case 2 :    /* hold */
						GetToken(pcCurrentPos, szFirstToken);

						//printf("Token1 = %s\n", szFirstToken);
						if (stricmp(szFirstToken, "ALL") == 0) {
							//printf("all mail being held\n");
							for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
								if (IBBS.Data.Nodes[iTemp].Active == false)
									continue;

								IBBS.Data.Nodes[iTemp].Info.MailType = MT_HOLD;
							}
						}
						else {
							IBBS.Data.Nodes[ atoi(szFirstToken)-1 ].Info.MailType = MT_HOLD;
							//printf("Mail to %d being held\n", atoi(szFirstToken));
						}
						break;
					case 3 :    /* normal */
						GetToken(pcCurrentPos, szFirstToken);

						//printf("Token1 = %s\n", szFirstToken);
						if (stricmp(szFirstToken, "ALL") == 0) {
							//printf("all mail set to normal\n");
							for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
								if (IBBS.Data.Nodes[iTemp].Active == false)
									continue;

								IBBS.Data.Nodes[iTemp].Info.MailType = MT_NORMAL;
							}
						}
						else {
							IBBS.Data.Nodes[ atoi(szFirstToken)-1 ].Info.MailType = MT_NORMAL;
							//printf("Mail to %d set to normal\n", atoi(szFirstToken));
						}
						break;
				}
			}
		}
	}

	fclose(fpRouteConfigFile);
}

// ------------------------------------------------------------------------- //

void IBBS_SendPacketFile(int16_t DestID, char *pszSendFile)
{
	char szFullFileName[PATH_SIZE], szPacketName[13]; /* 12345678.123 */
	char LastCounter, szString[580];
	FILE *fp;
	tIBInfo InterBBSInfo;

	if (IBBS.Initialized == false) {
		System_Error("IBBS not initialized for call.\n");
	}


	if (DestID <= 0 || DestID > MAX_IBBSNODES ||
			IBBS.Data.Nodes[DestID-1].Active == false) {
		DisplayStr("IBBS_SendPacketFile() aborted:  Invalid ID\n");
		return;
	}


	IBBS.Data.Nodes[DestID-1].Recon.LastSent = DaysSince1970(System.szTodaysDate);

//      snprintf(szString, sizeof(szString), "|03Sending Packet:  %s to %d\n", pszSendFile, DestinationId);
//      DisplayStr(szString);


	/* set up interbbsinfo */
	strlcpy(InterBBSInfo.szThisNodeAddress, IBBS.Data.Nodes[IBBS.Data.BBSID-1].Info.pszAddress, sizeof(InterBBSInfo.szThisNodeAddress));
	InterBBSInfo.szThisNodeAddress[NODE_ADDRESS_CHARS] = '\0';

	snprintf(szString, sizeof(szString), "The Clans League %s", Game.Data.LeagueID);
	strlcpy(InterBBSInfo.szProgName, szString, sizeof(InterBBSInfo.szProgName));
	InterBBSInfo.szProgName[PROG_NAME_CHARS] = '\0';

	strlcpy(InterBBSInfo.szNetmailDir, Config.szNetmailDir, sizeof(InterBBSInfo.szNetmailDir));
	InterBBSInfo.szNetmailDir[PATH_CHARS] = '\0';

	InterBBSInfo.bHold = false;
	InterBBSInfo.bCrash = false;

	if (IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Info.MailType == MT_CRASH)
		InterBBSInfo.bCrash = true;
	else if (IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Info.MailType == MT_HOLD)
		InterBBSInfo.bHold = true;

	InterBBSInfo.bEraseOnSend = true;
	InterBBSInfo.bEraseOnReceive = true;
	InterBBSInfo.nTotalSystems = 0;
	InterBBSInfo.paOtherSystem = NULL;

	/* change this later to the outbound directory of BBS */
	// strlcpy(szFullFileName, "C:\\PROG\\THECLANS\\OUTBOUND\\", sizeof(szFullFileName));
	// FIXME: ?!    Use outbound directory or don't care?

	if (System.LocalIBBS) {
		if (Config.NumInboundDirs)
			strlcpy(szFullFileName, Config.szInboundDirs[0], sizeof(szFullFileName));
	}
	else {
		strlcpy(szFullFileName, System.szMainDir, sizeof(szFullFileName));
		strlcat(szFullFileName, "outbound/", sizeof(szFullFileName));
	}

	/* format:  CLxxxyyy.idc */
	/* xxx = from BBS #
	     yyy = to BBS #
	     id = league ID
	     c = counter for that node */

	/* init the lastcounter */
	if (IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Recon.PacketIndex == 'a')
		LastCounter = 'z';
	else
		LastCounter = IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Recon.PacketIndex - 1;

	snprintf(szPacketName, sizeof(szPacketName),"cl%03d%03d.%-2s", IBBS.Data.BBSID,
			IBBS.Data.Nodes[DestID-1].Info.RouteThrough, Game.Data.LeagueID);

	strlcat(szFullFileName, szPacketName, sizeof(szFullFileName));


	/* see if can open that file */
	fp = _fsopen(szFullFileName, "rb", SH_DENYWR);
	if (fp) {
		/* if so, add onto it */
		//DisplayStr("Appending to packet.\n");
		fclose(fp);

		file_append(pszSendFile, szFullFileName);

		// DisplayStr("|15Appended|03\n");
	}
	else {
		// couldn't open file

		LastCounter = IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Recon.PacketIndex;

		if (System.LocalIBBS) {
			if (Config.NumInboundDirs)
				strlcpy(szFullFileName, Config.szInboundDirs[0], sizeof(szFullFileName));
		}
		else {
			strlcpy(szFullFileName, System.szMainDir, sizeof(szFullFileName));
			strlcat(szFullFileName, "outbound/", sizeof(szFullFileName));
		}

		szPacketName[11] = LastCounter;
		szPacketName[12] = 0;
		strlcat(szFullFileName, szPacketName, sizeof(szFullFileName));

		/* else, make a new packet */
		// DisplayStr("Making new packet.\n");

		/* increment for next time */
		IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Recon.PacketIndex++;
		if (IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Recon.PacketIndex == 'z' + 1)
			IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Recon.PacketIndex = 'a';

		/* copy file to outbound dir */
		//printf("Copying %s to %s\n", pszSendFile, szFullFileName);
		file_copy(pszSendFile, szFullFileName);

		//  snprintf(szString, sizeof(szString), "Sendfile is called %s\n", szFullFileName);
		//  DisplayStr(szString);

		/* send that file! */
//      snprintf(szString, sizeof(szString), "Packet for BBS #%d being sent through %s\n", DestinationId, BBS[ BBS[DestinationId-1].RouteThrough-1 ].Info.pszAddress);
//      DisplayStr(szString);

		//snprintf("Filenameis %s\n", sizeof("Filenameis %s\n"), szFullFileName);
		if (!NoMSG[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ])
			IBSendFileAttach(&InterBBSInfo, IBBS.Data.Nodes[ IBBS.Data.Nodes[DestID-1].Info.RouteThrough-1 ].Info.pszAddress, szFullFileName);
	}
	IBBS.Data.PacketSent = true;
}


// ------------------------------------------------------------------------- //
void IBBS_SendSpy(struct empire *Empire, int16_t DestID)
{
	struct SpyAttemptPacket Spy;
	struct Packet Packet;
	FILE *fp;

	switch (Empire->OwnerType) {
		case EO_VILLAGE :
			snprintf(Spy.szSpierName, sizeof(Spy.szSpierName), "the village of %s", Village.Data.szName);
			break;
		case EO_CLAN :
			snprintf(Spy.szSpierName, sizeof(Spy.szSpierName), "the clan of %s from %s", Empire->szName, Village.Data.szName);
			break;
	}

	Spy.IntelligenceLevel = Empire->Buildings[B_AGENCY];

	Spy.BBSFromID = IBBS.Data.BBSID;
	Spy.BBSToID     = DestID;

	// for now
	Spy.ClanID[0] = -1;
	Spy.ClanID[1] = -1;
	Spy.MasterID[0] = PClan->ClanID[0];
	Spy.MasterID[1] = PClan->ClanID[1];
	Spy.TargetType = EO_VILLAGE;

	/* create packet header */
	Packet.Active = true;
	Packet.BBSIDTo = DestID;
	Packet.BBSIDFrom = IBBS.Data.BBSID;
	Packet.PacketType = PT_SPY;
	strlcpy(Packet.GameID, Game.Data.GameID, sizeof(Packet.GameID));
	strlcpy(Packet.szDate, System.szTodaysDate, sizeof(Packet.szDate));
	Packet.PacketLength = BUF_SIZE_SpyAttemptPacket;

	fp = _fsopen(ST_OUTBOUNDFILE, "wb", SH_DENYWR);
	if (!fp)    return;

	// write packet header
	EncryptWrite_s(Packet, &Packet, fp, XOR_PACKET);

	EncryptWrite_s(SpyAttemptPacket, &Spy, fp, XOR_PACKET);

	fclose(fp);

	// send packet to BBS
	IBBS_SendPacketFile(Packet.BBSIDTo, ST_OUTBOUNDFILE);
	unlink(ST_OUTBOUNDFILE);

	rputs("|0BYour spy has been sent!\n");
}

// ------------------------------------------------------------------------- //

void IBBS_SendAttackPacket(struct empire *AttackingEmpire, struct Army *AttackingArmy,
						   int16_t Goal, int16_t ExtentOfAttack, int16_t TargetType, int16_t ClanID[2], int16_t DestID)
{
	struct AttackPacket AttackPacket;
	uint8_t pktBuf[BUF_SIZE_AttackPacket];
	size_t res;

	// write packet data
	AttackPacket.BBSFromID = IBBS.Data.BBSID;
	AttackPacket.BBSToID = DestID;
	AttackPacket.AttackingEmpire = *AttackingEmpire;
	AttackPacket.AttackingArmy = *AttackingArmy;
	AttackPacket.Goal = Goal;
	AttackPacket.ExtentOfAttack = ExtentOfAttack;
	AttackPacket.TargetType = TargetType;
	AttackPacket.ClanID[0] = ClanID[0];
	AttackPacket.ClanID[1] = ClanID[1];

	AttackPacket.AttackOriginatorID[0] = PClan->ClanID[0];
	AttackPacket.AttackOriginatorID[1] = PClan->ClanID[1];

	AttackPacket.AttackIndex = IBBS.Data.Nodes[DestID-1].Attack.SendIndex+1;
	IBBS.Data.Nodes[DestID-1].Attack.SendIndex++;

	res = s_AttackPacket_s(&AttackPacket, pktBuf, sizeof(pktBuf));
	assert(res == sizeof(pktBuf));
	if (res != sizeof(pktBuf))
		return;

	IBBS_SendPacket(PT_ATTACK, sizeof(pktBuf), pktBuf,
					DestID);
}


// ------------------------------------------------------------------------- //

void IBBS_ShowLeagueAscii(void)
{
	// shows ascii definied in world.ndx file

	FILE *fp;
	char szString[255], szToken[255];

	fp = _fsopen("world.ndx", "r", SH_DENYWR);
	if (!fp) {
		return;
	}

	for (;;) {
		if (fgets(szString, 255, fp) == 0)
			break;

		strlcpy(szToken, szString, sizeof(szToken));
		szToken[5] = 0;

		if (stricmp(szToken, "Ascii") == 0) {
			// point out to screen
			if (szString[6])
				rputs(&szString[6]);
			else
				rputs("\n");
		}
	}

	fclose(fp);

	// snprintf(szString, sizeof(szString), "|02You are entering the village of |10%s |02in the world of |14%s|02.\n",
	snprintf(szString, sizeof(szString), ST_MAIN2,
			IBBS.Data.Nodes[IBBS.Data.BBSID-1].Info.pszVillageName, Game.Data.szWorldName);
	rputs(szString);
}

// ------------------------------------------------------------------------- //

void IBBS_SendPacket(int16_t PacketType, int32_t PacketLength, void *PacketData,
					 int16_t DestID)
{
	struct Packet Packet;
	FILE *fpOutboundDat, *fpBackupDat;

	if (DestID <= 0 || DestID > MAX_IBBSNODES ||
			IBBS.Data.Nodes[ DestID - 1].Active == false) {
		DisplayStr("IBBS_SendPacket() aborted:  Invalid ID\n");
		return;
	}

	Packet.Active = true;

	strlcpy(Packet.GameID, Game.Data.GameID, sizeof(Packet.GameID));
	strlcpy(Packet.szDate, System.szTodaysDate, sizeof(Packet.szDate));
	Packet.BBSIDFrom = IBBS.Data.BBSID;
	Packet.BBSIDTo = DestID;

	Packet.PacketType = PacketType;
	Packet.PacketLength = PacketLength;

	fpOutboundDat = _fsopen(ST_OUTBOUNDFILE, "wb", SH_DENYWR);
	if (!fpOutboundDat) {
		DisplayStr("|04Error writing temporary outbound.tmp file\n");
		return;
	}

	/* write packet */
	EncryptWrite_s(Packet, &Packet, fpOutboundDat, XOR_PACKET);

	if (PacketLength)
		EncryptWrite(PacketData, PacketLength, fpOutboundDat, XOR_PACKET);

	fclose(fpOutboundDat);

	// printf("Sending packet to %d\n", DestID);

	IBBS_SendPacketFile(DestID, ST_OUTBOUNDFILE);
	unlink(ST_OUTBOUNDFILE);


	// write to backup.dat
	if (PacketType == PT_ATTACK) {
		fpBackupDat = _fsopen("backup.dat", "ab", SH_DENYWR);
		if (!fpBackupDat)  return;

		// write to backup packet
		EncryptWrite_s(Packet, &Packet, fpBackupDat, XOR_PACKET);

		if (PacketLength)
			EncryptWrite(PacketData, PacketLength, fpOutboundDat, XOR_PACKET);

		fclose(fpBackupDat);
	}
}

// ------------------------------------------------------------------------- //

void IBBS_SendRecon(int16_t DestID)
{
	if (IBBS.Data.Nodes[ DestID - 1].Active)
		IBBS_SendPacket(PT_RECON, 0, 0, DestID);
}

void IBBS_SendReset(int16_t DestID)
{
	uint8_t pktBuf[BUF_SIZE_game_data];
	size_t res;

	res = s_game_data_s(&Game.Data, pktBuf, sizeof(pktBuf));
	assert(res == sizeof(pktBuf));
	if (res != sizeof(pktBuf))
		return;
	if (IBBS.Data.Nodes[ DestID - 1].Active)
		IBBS_SendPacket(PT_RESET, sizeof(pktBuf), pktBuf, DestID);
}
// ------------------------------------------------------------------------- //

void IBBS_UpdateRecon(void)
{
	int16_t CurBBS;

	// go through all Nodes,
	// if Node is active AND last recon sent is 0, send recon and update
	// lastreconsent

	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active && (CurBBS+1) != IBBS.Data.BBSID) {
			if (IBBS.Data.Nodes[CurBBS].Recon.LastSent == 0 &&
					IBBS.Data.Nodes[CurBBS].Recon.LastReceived == 0) {
				IBBS_SendRecon(CurBBS+1);
				IBBS.Data.Nodes[CurBBS].Recon.LastSent = DaysSince1970(System.szTodaysDate);

				// REP: add new BBS to news?
				if (IBBS.Data.BBSID == 1)
					IBBS_SendUserList(CurBBS+1);
			}
			else if (((DaysSince1970(System.szTodaysDate) -
					   IBBS.Data.Nodes[CurBBS].Recon.LastReceived) >= RECONDAYS) &&
					 ((DaysSince1970(System.szTodaysDate) -
					   IBBS.Data.Nodes[CurBBS].Recon.LastSent) != 0)) {
				// send a recon there
				IBBS_SendRecon(CurBBS+1);
				IBBS.Data.Nodes[CurBBS].Recon.LastSent = DaysSince1970(System.szTodaysDate);
			}
		}
	}
}

void IBBS_UpdateReset(void)
{
	int16_t CurBBS;

	// go through all Nodes,
	// if Node hasn't yet received a reset AND a reset hasn't been sent yet
	// today, send one today

	// can ONLY be run by LC!!!
	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active && (CurBBS+1) != IBBS.Data.BBSID) {
			if (IBBS.Data.Nodes[CurBBS].Reset.Received == false &&
					IBBS.Data.Nodes[CurBBS].Reset.LastSent < DaysSince1970(System.szTodaysDate)) {
				//printf("Sending reset to BBS #%d\n", CurBBS+1);
				IBBS_SendReset(CurBBS+1);
				IBBS.Data.Nodes[CurBBS].Reset.LastSent = DaysSince1970(System.szTodaysDate);
			}
		}
	}
}

// ------------------------------------------------------------------------- //
void IBBS_LeagueInfo(void)
{
	char szString[128];
	int16_t CurBBS, NumBBSes;

	rputs("\n\n|14League Info|06\n\n");

	snprintf(szString, sizeof(szString), "League Id        |07%s|06\n", Game.Data.LeagueID);
	rputs(szString);
	snprintf(szString, sizeof(szString), "World Name       |07%s|06\n", Game.Data.szWorldName);
	rputs(szString);
	snprintf(szString, sizeof(szString), "GameID           |07%s|06\n", Game.Data.GameID);
	rputs(szString);

	if (Game.Data.GameState == 0) {
		rputs("A game is currently in progress.\n");
		snprintf(szString, sizeof(szString), "Days in progress |07%" PRId32 "|06\n", DaysBetween(Game.Data.szDateGameStart, System.szTodaysDate));
		rputs(szString);
		/*
		            snprintf(szString, sizeof(szString), "Elimination mode is |14%s|06\n",
		                World.EliminationMode ? "ON" : "OFF");
		            rputs(szString);
		*/
	}
	else if (Game.Data.GameState == 2) {
		rputs("The game is waiting for the main BBS's OK.\n");
	}
	else if (Game.Data.GameState == 1) {
		snprintf(szString, sizeof(szString), "The game will begin on |07%s|06\n",
				Game.Data.szDateGameStart);
		rputs(szString);
	}
	rputs("\n");

	// list other boards in league
	rputs("|07Id BBS Name             Village Name         Address    Recon\n");
	rputs("|08-- -------------------- -------------------- ---------- -------\n");
	for (CurBBS = 0, NumBBSes = 1; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active) {
			if (CurBBS+1 != IBBS.Data.BBSID) {
				snprintf(szString, sizeof(szString), "|06%-2d |14%-20s |06%-20s |07%-10s (%2" PRId32 "/%2" PRId32 ")\n",
						CurBBS+1, IBBS.Data.Nodes[CurBBS].Info.pszBBSName,
						IBBS.Data.Nodes[CurBBS].Info.pszVillageName, IBBS.Data.Nodes[CurBBS].Info.pszAddress,
						DaysSince1970(System.szTodaysDate) - IBBS.Data.Nodes[CurBBS].Recon.LastReceived,
						DaysSince1970(System.szTodaysDate) - IBBS.Data.Nodes[CurBBS].Recon.LastSent);
			}
			else {
				snprintf(szString, sizeof(szString), "|06%-2d |14%-20s |06%-20s |07%-10s ( N/A )\n",
						CurBBS+1, IBBS.Data.Nodes[CurBBS].Info.pszBBSName,
						IBBS.Data.Nodes[CurBBS].Info.pszVillageName, IBBS.Data.Nodes[CurBBS].Info.pszAddress);
			}

			rputs(szString);

			if (NumBBSes%15 == 0)
				door_pause();
			NumBBSes++;
		}
	}
	if (!(NumBBSes%15==0))
		door_pause();
}

// ------------------------------------------------------------------------- //

void KillAlliances(void)
{
	struct Alliance *Alliances[MAX_ALLIANCES];
	char szFileName[13];
	int16_t iTemp;

	GetAlliances(Alliances);

	// delete files
	// free up mem used by alliances
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		if (Alliances[iTemp]) {
			snprintf(szFileName, sizeof(szFileName), "hall%02d.txt", Alliances[iTemp]->ID);
			unlink(szFileName);

			free(Alliances[iTemp]);
			Alliances[iTemp] = NULL;
		}

	// free up mem used by alliances
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		if (Alliances[iTemp])
			free(Alliances[iTemp]);

	// called to destroy ALLY.DAT and remove those pesky HALLxxyy.TXT files
	unlink("ally.dat");
}

static void Reset(void)
{
	// Delete unwanted files here
	unlink("clans.msj");
	unlink("clans.pc");
	unlink("disband.dat");
	unlink("trades.dat");
	unlink("public.dat");
	unlink("pawn.dat");
	unlink("clans.npx");

	KillAlliances();

	unlink("ally.dat");

	// delete IBBS files
	unlink("ibbs.dat");
	unlink("ipscores.dat");
	unlink("userlist.dat");
	unlink("backup.dat");
	unlink("leaving.dat");

	// create a new IBBS.DAT file and then send recons
	IBBS_Create();
	IBBS_UpdateRecon();

	// create new village data BASED on old village data
	Village_Reset();

	// update news
	unlink("yest.asc");
	rename("today.asc", "yest.asc");
	News_CreateTodayNews();
	News_AddNews("|0A \xaf\xaf\xaf |0CReset received from LC!\n\n");

}

static void IBBS_Reset(struct game_data *GameData)
/*
 * Run if a reset packet was received.
 *
 */
{
	// if reset is same as last game's, don't run this
	if (stricmp(GameData->GameID, Game.Data.GameID) == 0) {
		DisplayStr("|08* |07dupe reset skipped.\n");
		return;
	}

	// otherwise, it's a legit reset request
	Game.Data = *GameData;

	strlcpy(Game.Data.szTodaysDate, System.szTodaysDate, sizeof(Game.Data.szTodaysDate));
	Game.Data.NextClanID = 0;
	Game.Data.NextAllianceID = 0;
	Game.Data.GameState = 1;

	// if today is game's start date
	if (DaysBetween(Game.Data.szDateGameStart, System.szTodaysDate) >= 0) {
		// today is the start of new game
		Game_Start();
	}

	Reset();
}

// ------------------------------------------------------------------------- //
// This removes a ClanMOVE packet from backup.dat and only that type!
static void IBBS_RemoveFromBackup(int16_t ID[2])
{
	FILE *fpBackupDat;
	struct Packet Packet;
	struct clan TmpClan = {0};
	long OldFileOffset; /* records start of packet so we can seek back to it */

	/* find leaving packet in Backup.dat and set as inactive */

	fpBackupDat = _fsopen("backup.dat", "r+b", SH_DENYRW);
	if (!fpBackupDat) {
		/* no need for dataOk?!  Report as error for now */
		DisplayStr("|12Error with backup.dat in RemoveFromBackup()\n");
		return;
	}

	/* seek through file, looking for ID[] which is same as this guy */
	for (;;) {
		/* save this file position */
		OldFileOffset = ftell(fpBackupDat);

		/* read packet */
		notEncryptRead_s(Packet, &Packet, fpBackupDat, XOR_PACKET)
			break;

		/* if packet has ID of this dude, mark it as inactive */
		if (Packet.PacketType == PT_CLANMOVE && Packet.Active) {
			notEncryptRead_s(clan, &TmpClan, fpBackupDat, XOR_PACKET)
				break;

			if (TmpClan.ClanID[0] == ID[0] && TmpClan.ClanID[1] == ID[1])
				Packet.Active = false;
		}
		/* if packet doesn't have same ID OR is not clanmove type, skip */
		else {
			if (Packet.PacketLength)
				fseek(fpBackupDat, Packet.PacketLength, SEEK_CUR);
			continue;
		}

		/* fseek back to position */
		fseek(fpBackupDat, OldFileOffset, SEEK_SET);

		/* write updated packet to file */
		EncryptWrite_s(Packet, &Packet, fpBackupDat, XOR_PACKET);

		if (Packet.PacketLength)
			fseek(fpBackupDat, Packet.PacketLength, SEEK_CUR);
	}

	fclose(fpBackupDat);
}



// ------------------------------------------------------------------------- //
static void IBBS_AddToGame(struct clan *Clan, bool WasLost)
{
	char szString[128];
	FILE *fpPC;
	int16_t iTemp, CurClan;
	bool FoundMatch = false;
	struct clan TmpClan = {0};

	/* update news */
	if (WasLost)
		snprintf(szString, sizeof(szString), ">> %s was lost but returns to town\n\n", Clan->szName);
	else
		snprintf(szString, sizeof(szString), ST_NEWSCLANENTER, Clan->szName);
	News_AddNews(szString);

	Clan->WorldStatus = WS_STAYING;
	Clan->DestinationBBS = -1;

	fpPC = _fsopen(ST_CLANSPCFILE, "r+b", SH_DENYWR);
	if (!fpPC) {
		/* no file yet, append 'em onto end */
		fpPC = _fsopen(ST_CLANSPCFILE, "ab", SH_DENYRW);

		// fix alliances since they are not the same on each BBS
		for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
			Clan->Alliances[iTemp] = -1;
		Clan->PublicMsgIndex = 0;
		Clan->MadeAlliance = false;

		// make it so no quests known or done
		for (iTemp = 0; iTemp < MAX_QUESTS/8; iTemp++) {
			Clan->QuestsDone[iTemp] = 0;
			Clan->QuestsKnown[iTemp] = 0;
		}

		EncryptWrite_s(clan, Clan, fpPC, XOR_USER);

		/* write members */
		for (iTemp = 0; iTemp < 6; iTemp++)
			EncryptWrite_s(pc, Clan->Member[iTemp], fpPC, XOR_PC);

		fclose(fpPC);

		/* done */
		return;
	}

	CurClan = 0;
	for (;;) {
		/* seek to current clan */
		fseek(fpPC, (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc), SEEK_SET);

		/* read in tmp clan */
		notEncryptRead_s(clan, &TmpClan, fpPC, XOR_USER)
			break;

		/* see if ID matches */
		if (TmpClan.ClanID[0] == Clan->ClanID[0] &&
				TmpClan.ClanID[1] == Clan->ClanID[1]) {
			/* found match */

			// fix alliances since they are not the same on each BBS
			for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
				Clan->Alliances[iTemp] = TmpClan.Alliances[iTemp];
			Clan->Empire.VaultGold      += TmpClan.Empire.VaultGold;
			Clan->Empire.Army.Followers += TmpClan.Empire.Army.Followers;
			Clan->Empire.Army.Footmen   += TmpClan.Empire.Army.Footmen;
			Clan->Empire.Army.Axemen    += TmpClan.Empire.Army.Axemen;
			Clan->Empire.Army.Knights   += TmpClan.Empire.Army.Knights;
			Clan->PublicMsgIndex         = TmpClan.PublicMsgIndex;
			Clan->MadeAlliance           = TmpClan.MadeAlliance;
			Clan->Eliminated             = false;

			// make it so no quests known are same
			for (iTemp = 0; iTemp < MAX_QUESTS/8; iTemp++) {
				Clan->QuestsDone[iTemp] = TmpClan.QuestsDone[iTemp];
				Clan->QuestsKnown[iTemp] = TmpClan.QuestsKnown[iTemp];
			}

			/* seek to that spot and write the merged info to file */
			fseek(fpPC, (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc), SEEK_SET);

			EncryptWrite_s(clan, Clan, fpPC, XOR_USER);

			/* write members */
			for (iTemp = 0; iTemp < 6; iTemp++)
				EncryptWrite_s(pc, Clan->Member[iTemp], fpPC, XOR_USER);

			FoundMatch = true;
			break;
		}
		CurClan++;
	}
	fclose(fpPC);

	/* when found, write data to file, write it ALL for now.  later only *some*
	   will be written */

	/* if not there, close file, open for ab and then append on end */
	if (FoundMatch == false) {
		// fix alliances since they are not the same on each BBS
		for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
			Clan->Alliances[iTemp] = -1;
		Clan->PublicMsgIndex = 0;
		Clan->MadeAlliance = false;

		fpPC = _fsopen(ST_CLANSPCFILE, "ab", SH_DENYRW);
		EncryptWrite_s(clan, Clan, fpPC, XOR_USER);

		/* write members */
		for (iTemp = 0; iTemp < 6; iTemp++)
			EncryptWrite_s(pc, Clan->Member[iTemp], fpPC, XOR_PC);

		fclose(fpPC);
	}

	/* done */
}

static void ComeBack(int16_t ClanID[2], int16_t BBSID)
{
	struct LeavingData LeavingData;
	FILE *fpLeavingDat;
	struct clan TmpClan = {0};

	// get Clan from file
	if (GetClan(ClanID, &TmpClan) == false) {
		FreeClanMembers(&TmpClan);
		return; // not even here!
	}

	// if that clan ain't here, return
	if (TmpClan.WorldStatus != WS_STAYING) {
		FreeClanMembers(&TmpClan);
		return;
	}

	/* append leaving.dat file */
	LeavingData.DestID = BBSID;
	LeavingData.Active = true;
	LeavingData.ClanID[0] = TmpClan.ClanID[0];
	LeavingData.ClanID[1] = TmpClan.ClanID[1];

	// send all troops back home
	LeavingData.Followers = TmpClan.Empire.Army.Followers;
	LeavingData.Footmen =   TmpClan.Empire.Army.Footmen;
	LeavingData.Axemen  =   TmpClan.Empire.Army.Axemen;
	LeavingData.Knights =   TmpClan.Empire.Army.Knights;

	fpLeavingDat = _fsopen("leaving.dat", "ab", SH_DENYRW);
	if (!fpLeavingDat) {
		FreeClanMembers(&TmpClan);
		return;
	}
	EncryptWrite_s(LeavingData, &LeavingData, fpLeavingDat, XOR_TRAVEL);
	fclose(fpLeavingDat);

	TmpClan.WorldStatus = WS_LEAVING;
	TmpClan.DestinationBBS = BBSID;

	Clan_Update(&TmpClan);
	FreeClanMembers(&TmpClan);
}


static int16_t IBBS_ProcessPacket(char *szFileName)
{
	struct SpyResultPacket SpyResult;
	struct SpyAttemptPacket Spy;
	FILE *fp, *fpNewFile, *fpScores, *fpWorldNDX, *fpUserList;
	struct Packet Packet;
	struct clan *TmpClan;
	int16_t iTemp, NumScores;
	char *pcBuffer, szString[128],
	ScoreDate[11];
	int16_t ClanID[2], BBSID;
	struct game_data GameData;
	struct Message Message;
	struct UserScore **UserScores;
	struct UserInfo User;
	struct AttackPacket *AttackPacket;
	struct AttackResult *AttackResult;

	/* open it */
	fp = _fsopen(szFileName, "rb", SH_DENYWR);
	if (!fp) {
		snprintf(szString, sizeof(szString), "Can't read in %s\n", szFileName);
		DisplayStr(szString);
		return 0;
	}

	// show processing it
	snprintf(szString, sizeof(szString), "|07> |15Processing |14%s\n", szFileName);
	DisplayStr(szString);

	/* read it in until end of file */
	for (;;) {
		notEncryptRead_s(Packet, &Packet, fp, XOR_PACKET)
			break;

		// if packet is old, ignore it
		if (DaysBetween(Packet.szDate, System.szTodaysDate) > MAX_PACKETAGE) {
			fseek(fp, Packet.PacketLength, SEEK_CUR);
			continue;
		}

		// if gameId is of the current game, update recon stuff
		if (stricmp(Packet.GameID, Game.Data.GameID) == 0) {
			IBBS.Data.Nodes[ Packet.BBSIDFrom - 1 ].Recon.LastReceived =
				DaysSince1970(Game.Data.szTodaysDate);
		}
		else {
			// GameIDs differ AND this isn't a reset packet, so we must ignore
			// this packet completely
			if (Packet.PacketType != PT_RESET) {
				fseek(fp, Packet.PacketLength, SEEK_CUR);
				continue;
			}
		}

		/* see if this packet is for you.  if not, reroute it to where it
		    belongs */

		if (Packet.BBSIDTo != IBBS.Data.BBSID) {
			/* get packet info and write it to file */
			if (Packet.PacketLength) {
				pcBuffer = malloc(Packet.PacketLength);
				CheckMem(pcBuffer);
				EncryptRead(pcBuffer, Packet.PacketLength, fp, XOR_PACKET);
			}
			else
				pcBuffer = NULL;


			fpNewFile = _fsopen(ST_OUTBOUNDFILE, "wb", SH_DENYWR);
			if (!fpNewFile) {
				if (pcBuffer)
					free(pcBuffer);

				fclose(fp);
				DisplayStr("Can't write outbound.tmp\n");
				return 0;
			}
			EncryptWrite_s(Packet, &Packet, fpNewFile, XOR_PACKET);
			if (Packet.PacketLength)
				EncryptWrite(pcBuffer, Packet.PacketLength, fpNewFile, XOR_PACKET);

			fclose(fpNewFile);

			if (pcBuffer)
				free(pcBuffer);

			DisplayStr("|08* |07Routing packet\n");
			IBBS_SendPacketFile(Packet.BBSIDTo, ST_OUTBOUNDFILE);
			unlink(ST_OUTBOUNDFILE);

			/* move onto next packet in file */
			continue;
		}
		else if (Packet.PacketType == PT_RESET) {
			/* data ok for specific char */
			DisplayStr("|08- |07received reset\n");

			EncryptRead_s(game_data, &GameData, fp, XOR_PACKET);

			IBBS_Reset(&GameData);

			// send back a gotreset
			IBBS_SendPacket(PT_GOTRESET, 0, 0, Packet.BBSIDFrom);
		}
		else if (Packet.PacketType == PT_GOTRESET) {
			snprintf(szString, sizeof(szString), "%s - Received GotReset from %s\n\n",
					System.szTodaysDate, IBBS.Data.Nodes[ Packet.BBSIDFrom - 1 ].Info.pszBBSName);
			IBBS_AddLCLog(szString);

			DisplayStr(szString);

			IBBS.Data.Nodes[ Packet.BBSIDFrom - 1].Reset.Received = true;
		}
		else if (Packet.PacketType == PT_RECON) {
			// send back a GOTRECON
			DisplayStr("|08* |07sending gotrecon\n");
			IBBS_SendPacket(PT_GOTRECON, 0, 0, Packet.BBSIDFrom);
		}
		else if (Packet.PacketType == PT_GOTRECON) {
			// do nothing
			DisplayStr("|08- |07got gotrecon\n");
		}
		else if (Packet.PacketType == PT_MSJ) {
			DisplayStr("|08- |07msj found\n");

			// read in message from file
			EncryptRead_s(Message, &Message, fp, XOR_PACKET);

			// allocate mem for text
			Message.Data.MsgTxt = malloc(Message.Data.Length);
			CheckMem(Message.Data.MsgTxt);

			// load message text
			EncryptRead(Message.Data.MsgTxt, Message.Data.Length, fp, XOR_PACKET);

			// write message
			PostMsj(&Message);

			// deallocate mem
			free(Message.Data.MsgTxt);
		}
		else if (Packet.PacketType == PT_SCOREDATA) {
			DisplayStr("|08- |07score data\n");

			EncryptRead16(&NumScores, fp, XOR_PACKET);

			// read the scores in
			UserScores = malloc(sizeof(struct UserScore *) * MAX_USERS);
			CheckMem(UserScores);
			for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
				UserScores[iTemp] = NULL;

			for (iTemp = 0; iTemp < NumScores; iTemp++) {
				UserScores[iTemp] = malloc(sizeof(struct UserScore));
				CheckMem(UserScores[iTemp]);
				EncryptRead_s(UserScore, UserScores[iTemp], fp, XOR_PACKET);
			}

			// process it
			ProcessScoreData(UserScores);

			// free up mem used
			for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
				if (UserScores[iTemp])
					free(UserScores[iTemp]);
			}
			free(UserScores);
		}
		else if (Packet.PacketType == PT_SCORELIST) {
			DisplayStr("|08- |07score list\n");

			EncryptRead16(&NumScores, fp, XOR_PACKET);

			// read in date
			EncryptRead(ScoreDate, 11, fp, XOR_PACKET);

			// read the scores in
			UserScores = malloc(sizeof(struct UserScore *) * MAX_USERS);
			CheckMem(UserScores);
			for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
				UserScores[iTemp] = NULL;

			for (iTemp = 0; iTemp < NumScores; iTemp++) {
				UserScores[iTemp] = malloc(sizeof(struct UserScore));
				CheckMem(UserScores[iTemp]);
				EncryptRead_s(UserScore, UserScores[iTemp], fp, XOR_PACKET);
			}

			// write it all to the IPSCORES.DAT file
			fpScores = _fsopen("ipscores.dat", "wb", SH_DENYWR);
			if (fpScores) {
				EncryptWrite(ScoreDate, 11, fpScores, XOR_IPS);

				for (iTemp = 0; iTemp < NumScores; iTemp++) {
					EncryptWrite_s(UserScore, UserScores[iTemp], fpScores, XOR_IPS);
				}

				fclose(fpScores);
			}

			// free up mem used
			for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
				if (UserScores[iTemp])
					free(UserScores[iTemp]);
			}
			free(UserScores);
		}
		else if (Packet.PacketType == PT_CLANMOVE) {
			TmpClan = malloc(sizeof(struct clan));
			CheckMem(TmpClan);
			EncryptRead_s(clan, TmpClan, fp, XOR_PACKET);

			for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
				TmpClan->Member[iTemp] = NULL;

			// only do if in a game already
			if (Game.Data.GameState == 0) {
				snprintf(szString, sizeof(szString), "|08- |07%s found.  Destination: |09%d\n", TmpClan->szName, Packet.BBSIDTo);
				DisplayStr(szString);
			}

			/* read in each PC */
			for (iTemp = 0; iTemp < 6; iTemp++) {
				TmpClan->Member[iTemp] = malloc(sizeof(struct pc));
				CheckMem(TmpClan->Member[iTemp]);
				EncryptRead_s(pc, TmpClan->Member[iTemp], fp, XOR_PACKET);
			}

			/* update .PC file with this guys stats */
			// only do if in a game already
			if (Game.Data.GameState == 0) {
				IBBS_AddToGame(TmpClan, false);
				TmpClan->ClanID[0] = SWAP16(TmpClan->ClanID[0]);
				TmpClan->ClanID[1] = SWAP16(TmpClan->ClanID[1]);
				IBBS_SendPacket(PT_DATAOK, sizeof(int16_t)*2, TmpClan->ClanID, Packet.BBSIDFrom);
				TmpClan->ClanID[0] = SWAP16(TmpClan->ClanID[0]);
				TmpClan->ClanID[1] = SWAP16(TmpClan->ClanID[1]);
			}

			FreeClan(TmpClan);
		}
		else if (Packet.PacketType == PT_DATAOK) {
			if (Game.Data.GameState != 0)
				continue;

			/* data ok for specific char */
			DisplayStr("|08- |07Received DataOk\n");

			EncryptRead16(&ClanID[0], fp, XOR_PACKET);
			EncryptRead16(&ClanID[1], fp, XOR_PACKET);

			IBBS_RemoveFromBackup(ClanID);
		}
		else if (Packet.PacketType == PT_COMEBACK) {
			// user is returning to the board specified FROM this board
			DisplayStr("|08- |07comeback found\n");

			// read in clanID + BBSId of destination
			EncryptRead16(&ClanID[0], fp, XOR_PACKET);
			EncryptRead16(&ClanID[1], fp, XOR_PACKET);
			EncryptRead16(&BBSID, fp, XOR_PACKET);

			ComeBack(ClanID, BBSID);
		}
		else if (Packet.PacketType == PT_NEWUSER) {
			DisplayStr("|08- |07newuser found\n");
			// new user has logged into a BBS in the league, we will
			// now see if he is a dupe user or a valid one

			// read in user from file
			EncryptRead_s(UserInfo, &User, fp, XOR_PACKET);

			// see if user is already in list
			if (IBBS_InList(User.szMasterName, false)) {
				// if so, return packet saying "Delete that guy!"
				IBBS_SendDelUser(Packet.BBSIDFrom, &User);
			}
			else {
				// if not already in list, send packet to everybody
				// in league saying
				//   "here is a new user, add him"
				// add the user to your own user list too

				UpdateNodesOnNewUser(&User);

				AddToUList(&User);
			}
		}
		else if (Packet.PacketType == PT_ADDUSER) {
			// new user update from main BBS
			DisplayStr("|08- |07adduser found\n");

			// read in user from file
			EncryptRead_s(UserInfo, &User, fp, XOR_PACKET);

			// add to THIS bbs's list
			AddToUList(&User);
		}
		else if (Packet.PacketType == PT_DELUSER) {
			DisplayStr("|08- |07deluser found\n");

			// he is a dupe user
			// got word from head BBS to delete a certain user, do it then :)
			EncryptRead_s(UserInfo, &User, fp, XOR_PACKET);

			snprintf(szString, sizeof(szString), "deleting %s\n", User.szName);
			DisplayStr(szString);

			DeleteClan(User.ClanID, User.szName, false);
		}
		else if (Packet.PacketType == PT_SUBUSER) {
			// user is being deleted from all boards
			DisplayStr("|08- |07subuser found\n");

			// read in user from file
			EncryptRead_s(UserInfo, &User, fp, XOR_PACKET);

			snprintf(szString, sizeof(szString), "deleting %s\n", User.szName);
			DisplayStr(szString);

			// remove him from clan data AND userlist (done in DeleteClan)
			DeleteClan(User.ClanID, User.szName, false);
		}
		else if (Packet.PacketType == PT_ATTACK) {
			DisplayStr("|08- |07attack found\n");

			// read attack packet
			AttackPacket = malloc(sizeof(struct AttackPacket));
			CheckMem(AttackPacket);
			EncryptRead_s(AttackPacket, AttackPacket, fp, XOR_PACKET);

			// process attack packet
			ProcessAttackPacket(AttackPacket);
			free(AttackPacket);
		}
		else if (Packet.PacketType == PT_ATTACKRESULT) {
			DisplayStr("|08- |07attackresult found\n");

			// read attack packet
			AttackResult = malloc(sizeof(struct AttackResult));
			CheckMem(AttackResult);
			EncryptRead_s(AttackResult, AttackResult, fp, XOR_PACKET);

			// process attack packet
			ProcessResultPacket(AttackResult);
			free(AttackResult);
		}
		else if (Packet.PacketType == PT_SPY) {
			DisplayStr("|08- |07spy\n");

			EncryptRead_s(SpyAttemptPacket, &Spy, fp, XOR_PACKET);

			// process attack packet
			IBBS_ProcessSpy(&Spy);
		}
		else if (Packet.PacketType == PT_SPYRESULT) {
			DisplayStr("|08- |07spy result\n");

			EncryptRead_s(SpyResultPacket, &SpyResult, fp, XOR_PACKET);

			// process attack packet
			IBBS_ProcessSpyResult(&SpyResult);
		}
		else if (Packet.PacketType == PT_NEWNDX) {
			DisplayStr("|08- |07ndx found\n");

			// allocate mem for world.ndx file
			pcBuffer = malloc(Packet.PacketLength);
			CheckMem(pcBuffer);

			// load world.ndx
			EncryptRead(pcBuffer, Packet.PacketLength, fp, XOR_PACKET);

			// write to file
			fpWorldNDX = _fsopen("world.ndx", "wb", SH_DENYRW);
			if (fpWorldNDX) {
				fwrite(pcBuffer, Packet.PacketLength, 1, fpWorldNDX);
				fclose(fpWorldNDX);
			}

			// deallocate mem
			free(pcBuffer);
		}
		else if (Packet.PacketType == PT_ULIST) {
			DisplayStr("|08- |07userlist found\n");

			// allocate mem for world.ndx file
			pcBuffer = malloc(Packet.PacketLength);
			CheckMem(pcBuffer);

			// load file in
			EncryptRead(pcBuffer, Packet.PacketLength, fp, XOR_PACKET);

			// write to file
			fpUserList = _fsopen("userlist.dat", "wb", SH_DENYRW);
			if (fpUserList) {
				fwrite(pcBuffer, Packet.PacketLength, 1, fpUserList);
				fclose(fpUserList);
			}

			// deallocate mem
			free(pcBuffer);
		}
		else {
			DisplayStr("|04x |12Unknown packet type!  ABORTING!!\n");
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	/* if unsuccessfully dealt with, return 0 */
	return 1;
}


// ------------------------------------------------------------------------- //

// returns true (I think) if no more files
static bool GetNextFile(char *szWildcard, char *szFileName, size_t sz)
{
	struct ffblk ffblks;
	uint16_t date = 65534, time = 65534;
	bool Done, NoMoreFiles = true;

	Done = findfirst(szWildcard, &ffblks, 0);


	// go through a file, record date, timestamps

	while (!Done) {
		//printf("trying out %s\n", ffblks.ff_name);
		// is date of file less than last file read in?
		if (ffblks.ff_fdate <= date) {
			// if same date, and this file's time is greater than
			// other time, this ain't the file we want.
			if ((ffblks.ff_fdate == date && ffblks.ff_ftime < time) ||
					(ffblks.ff_fdate < date)) {
				//printf("found file %s\n", ffblks.ff_name);
				date = ffblks.ff_fdate;
				time = ffblks.ff_ftime;
				strlcpy(szFileName, ffblks.ff_name, sz);
				NoMoreFiles = false;
			}
		}
		Done = findnext(&ffblks);
	}

	return NoMoreFiles;
}


/*
 * Iterates over the message files, calling skip() to see if one can
 * be skipped and calling process() for each match
 */
static void
MessageFileIterate(const char *pszFileName, tIBInfo *InterBBSInfo,
    bool(*skip)(void *),
    void(*process)(const char *, const char *, void *), void *cbdata)
{
	char wildcard[PATH_SIZE];
	struct ffblk ffblks;
	bool Done;
	tFidoNode ThisNode;

	if (Config.MailerType == MAIL_NONE)
		return;
	ConvertStringToAddress(&ThisNode, InterBBSInfo->szThisNodeAddress);

	// Go through all *.msg files in the netmail directory.
#ifdef __unix__
	snprintf(wildcard, sizeof(wildcard), "%s/*.[Mm][Ss][Gg]", InterBBSInfo->szNetmailDir);
#else
	snprintf(wildcard, sizeof(wildcard), "%s/*.msg", InterBBSInfo->szNetmailDir);
#endif

	Done = findfirst(wildcard, &ffblks, 0);
	while (!Done) {
		char hdrbuf[BUF_SIZE_MessageHeader];
		FILE *f;

		// We need to get to the last iteration or we'll leak resources
		if (!skip(cbdata)) {
			bool Found = false;
			char fname[PATH_SIZE];

			snprintf(fname, sizeof(fname), "%s/%s", InterBBSInfo->szNetmailDir, ffblks.ff_name);
			f = fopen(fname, "rb");

			if (f) {
				if (fread(hdrbuf, sizeof(hdrbuf), 1, f) == 1) {
					struct MessageHeader hdr;
					// Check if it's to us and from us
					s_MessageHeader_d(hdrbuf, sizeof(hdrbuf), &hdr);
					// TODO: Check the source address too
					if (ThisNode.wZone == hdr.wDestZone
					    && ThisNode.wNet == hdr.wDestNet
					    && ThisNode.wNode == hdr.wDestNode
					    && ThisNode.wPoint == hdr.wDestPoint
					    && (strcmp(InterBBSInfo->szProgName, hdr.szToUserName) == 0)
					    && (strcmp(InterBBSInfo->szProgName, hdr.szFromUserName) == 0)) {
						// Then see if the file is attached
						const char *pktfname = hdr.szSubject;
						switch (*pktfname) {
							case '#':
							case '^':
							case '-':
							case '~':
							case '!':
							case '@':
								pktfname++;
						}
						if (strcmp(pktfname, pszFileName) == 0)
							Found = true;
					}
				}
				fclose(f);
			}
			if (Found)
				process(fname, ffblks.ff_name, cbdata);
		}

		Done = findnext(&ffblks);
	}
	return;
}

static bool SkipAfterFound(void *cbdata)
{
	bool ret = *(bool *)cbdata;
	return ret;
}

static void SetSkip(const char *fname, const char *sname, void *cbdata)
{
	bool *skip = cbdata;
	*skip = true;
}

static bool CheckMessageFile(const char *pszFileName, tIBInfo *InterBBSInfo)
{
	bool Found = false;

	MessageFileIterate(pszFileName, InterBBSInfo, SkipAfterFound, SetSkip, &Found);
	return Found;
}

static bool NeverSkip(void *cbdata)
{
	return false;
}

static void DeleteFound(const char *fname, const char *sname, void *cbdata)
{
	char msg[256];
	snprintf(msg, sizeof(msg), "|08* |07Deleting %s from netmail directory", sname);
	DisplayStr(msg);
	unlink(fname);
}

static void DeleteMessageWithFile(const char *pszFileName, tIBInfo *InterBBSInfo)
{
	MessageFileIterate(pszFileName, InterBBSInfo, NeverSkip, DeleteFound, NULL);
}

void IBBS_PacketIn(void)
{
	tIBInfo InterBBSInfo;
	/*      struct ffblk ffblk;*/
	char szFileName[PATH_SIZE], szPacketName[PATH_SIZE], szString[580], szFileName2[PATH_SIZE];
	bool Done;
	FILE *fp;
	int16_t nInbound;

	if (Game.Data.InterBBS == false) {
		DisplayStr("|12* This game is not set up for InterBBS!\n");
		return;
	}
	DisplayStr("|09* IBBS_PacketIn()\n");

	/* set up interbbsinfo */
	strlcpy(InterBBSInfo.szThisNodeAddress, IBBS.Data.Nodes[IBBS.Data.BBSID-1].Info.pszAddress, sizeof(InterBBSInfo.szThisNodeAddress));
	InterBBSInfo.szThisNodeAddress[NODE_ADDRESS_CHARS] = '\0';

	snprintf(szString, sizeof(szString), "The Clans League %s", Game.Data.LeagueID);
	strlcpy(InterBBSInfo.szProgName, szString, sizeof(InterBBSInfo.szProgName));
	InterBBSInfo.szProgName[PROG_NAME_CHARS] = '\0';

	strlcpy(InterBBSInfo.szNetmailDir, Config.szNetmailDir, sizeof(InterBBSInfo.szNetmailDir));
	InterBBSInfo.szNetmailDir[PATH_CHARS] = '\0';

	InterBBSInfo.bHold = false;
	InterBBSInfo.bCrash = false;
	InterBBSInfo.bEraseOnSend = true;
	InterBBSInfo.bEraseOnReceive = true;
	InterBBSInfo.nTotalSystems = 0;
	InterBBSInfo.paOtherSystem = NULL;

	// process files with this wildcard spec ONLY:  CLxxx*.IDy
	//
	// xxx is THIS BBS's Id.

	for (nInbound = 0; nInbound < Config.NumInboundDirs; nInbound++) {
		// create filename to search for
#ifdef __unix__
		strlcpy(szPacketName, Config.szInboundDirs[nInbound], sizeof(szPacketName));
		snprintf(szFileName, sizeof(szFileName),"[Cc][Ll]???%03d.%-2s?",IBBS.Data.BBSID,Game.Data.LeagueID);
#else
		snprintf(szFileName, sizeof(szFileName),"CL???%03d.%-2s?",IBBS.Data.BBSID,Game.Data.LeagueID);
#endif
		// now copy over to the full filename
		strlcat(szPacketName, szFileName, sizeof(szPacketName));

		// printf("Filespec to search is %s\n", szPacketName);

		//Done = findfirst(szPacketName, &ffblk, 0);
		Done = GetNextFile(szPacketName, szFileName2, sizeof(szFileName2));

		/* keep calling till no more messages to read */
		while (!Done) {
			szFileName[0] = 0;

			strlcpy(szFileName, Config.szInboundDirs[nInbound], sizeof(szFileName));
			//strlcat(szFileName, ffblk.ff_name, sizeof(szFileName));
			strlcat(szFileName, szFileName2, sizeof(szFileName));

			/* process file */
			if (Config.StrictMsgFile && !CheckMessageFile(szFileName, &InterBBSInfo)) {
				snprintf(szString, sizeof(szString), "No valid msg file for packet %s\n", szFileName);
				DisplayStr(szString);
			}
			else {
				if (IBBS_ProcessPacket(szFileName) == 0) {
					snprintf(szString, sizeof(szString), "Error dealing with packet %s\n", szFileName);
					DisplayStr(szString);
				}
				else {
					/* delete it */
					unlink(szFileName);
					/* And try to delete *.msg files that had it attached */
					DeleteMessageWithFile(szFileName, &InterBBSInfo);
				}
			}

			// Done = findnext(&ffblk);
			Done = GetNextFile(szPacketName, szFileName2, sizeof(szFileName2));
		}
	}

	DisplayStr("\n");

	// look for new nodelist
	// look for world.ndx first
	// if not found, go for WORLD.ID
	if (System.LocalIBBS == false) {
		for (nInbound = 0; nInbound < Config.NumInboundDirs; nInbound++) {
			// try world.ID
#ifdef __unix__
			snprintf(szFileName2, sizeof(szFileName2), "[Ww][Oo][Rr][Ll][Dd].%-2s", Game.Data.LeagueID);
#else
			snprintf(szFileName2, sizeof(szFileName2), "WORLD.%-2s", Game.Data.LeagueID);
#endif

			strlcpy(szFileName, Config.szInboundDirs[nInbound], sizeof(szFileName));
			strlcat(szFileName, szFileName2, sizeof(szFileName));

			while (!GetNextFile(szFileName,szFileName2, sizeof(szFileName2)))  {
				if (Config.StrictMsgFile && !CheckMessageFile(szFileName2, &InterBBSInfo)) {
					snprintf(szString, sizeof(szString), "No valid msg file for packet %s\n", szFileName2);
					DisplayStr(szString);
				}
				else {
					fp = _fsopen(szFileName2, "r", SH_DENYWR);
					if (fp) {
						DisplayStr("|08- |07new world.ndx found.\n");
						// found it
						fclose(fp);
						file_copy(szFileName2, "world.ndx");
						unlink(szFileName2);
					}
				}
			}
		}
	}
}

// ------------------------------------------------------------------------- //


void IBBS_Init(void)
/*
 * IBBS data is initialized.    MUST be called before any other IBBS_*
 * functions are called.
 */
{
	if (Verbose) {
		DisplayStr("> IBBS_Init()\n");
		delay(500);
	}

#ifdef PRELAB
	printf("IBBS Initializing. -- %lu\n", farcoreleft());
#endif

	IBBS.Initialized = true;

	IBBS.Data.BBSID = Config.BBSID;

	IBBS_LoadNDX();
	IBBS_ProcessRouteConfig();

	IBBS_Read();
}

// ------------------------------------------------------------------------- //

void IBBS_Close(void)
/*
 * Closes down IBBS.
 */
{
	if (!IBBS.Initialized) return;

#ifdef PRELAB
	printf("IBBS Closing. -- %lu\n", farcoreleft());
#endif

	IBBS_Write();
	IBBS_Destroy();
	if (Config.szOutputSem[0]) {
		// Touch the semaphore
		FILE *f;

		if (utime(Config.szOutputSem, NULL)) {
			if ((f = fopen(Config.szOutputSem, "ab")))
				fclose(f);
		}
	}
}

// ------------------------------------------------------------------------- //

void IBBS_Maint(void)
{
	int16_t CurBBS;

	if (Game.Data.InterBBS == false)
		return;

	DisplayStr("* IBBS_Maint()\n");

	IBBS_PacketIn();

	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data.Nodes[CurBBS].Active) {
			if (IBBS.Data.Nodes[CurBBS].Recon.PacketIndex < 'z')
				IBBS.Data.Nodes[CurBBS].Recon.PacketIndex++;
			else
				IBBS.Data.Nodes[CurBBS].Recon.PacketIndex = 'a';
		}
	}

	// scores for the league
	if (IBBS.Data.BBSID != 1)
		CreateScoreData(true);

	CreateScoreData(false);

	if (IBBS.Data.BBSID == 1)
		SendScoreList();

	if (IBBS.Data.BBSID == 1) {
		/* send userlist to every BBS */
		if ((DaysSince1970(System.szTodaysDate) % 4) == 0) {
			for (CurBBS = 1; CurBBS < MAX_IBBSNODES; CurBBS++) {
				if (IBBS.Data.Nodes[CurBBS].Active) {
					IBBS_SendUserList(CurBBS+1);
				}
			}
		}
	}

	IBBS_TravelMaint();
	IBBS_BackupMaint();
}

#ifdef _WIN32
#if defined(_UNICODE) || defined(UNICODE)
# error "findfirst/findnext clones will not compile under UNICODE"
#endif

int findfirst(const char *pathname, struct ffblk *ffblks, int attribute)
{
	WIN32_FIND_DATA w32_finddata;

	if (!ffblks) {
		errno = ENOENT;
		return -1;
	}

	ffblks->ff_findhandle = FindFirstFile(pathname, &w32_finddata);
	if (ffblks->ff_findhandle == INVALID_HANDLE_VALUE) {
		errno = ENOENT;
		return -1;
	}

	ffblks->ff_findattribute = attribute;
	convert_attribute(&ffblks->ff_attrib, w32_finddata.dwFileAttributes);
	/* Search for matching attribute */
	return (search_and_construct_ffblk(&w32_finddata, ffblks, true));
}

int findnext(struct ffblk *ffblks)
{
	WIN32_FIND_DATA w32_finddata;

	if (!ffblks) {
		errno = ENOENT;
		return -1;
	}

	return (search_and_construct_ffblk(&w32_finddata, ffblks, false));
}

static int search_and_construct_ffblk(WIN32_FIND_DATA *w32_finddata, struct ffblk *ffblks, bool bhave_file)
{
	SYSTEMTIME system_time;

	if ((bhave_file == true && !(ffblks->ff_attrib & ffblks->ff_findattribute)) ||
			(bhave_file == false)) {
		do {
			/* Check for FA_NORMAL and FA_ARCH  */
			/* Make sure these IFs only run AFTER a file has been found for FindNextFile */
			if (bhave_file) {
				if (ffblks->ff_attrib == FA_NORMAL && ffblks->ff_findattribute == FA_NORMAL)
					break;
				if (ffblks->ff_attrib == FA_ARCH && ffblks->ff_findattribute == FA_NORMAL)
					break;
			}
			else
				bhave_file = true;
			if (!FindNextFile(ffblks->ff_findhandle, w32_finddata)) {
				FindClose(ffblks->ff_findhandle);
				errno = ENOENT;
				return -1;
			}
			else
				convert_attribute(&ffblks->ff_attrib, w32_finddata->dwFileAttributes);
		}
		while (!(ffblks->ff_attrib & ffblks->ff_findattribute));
	}

	if (strlen(w32_finddata->cAlternateFileName))
		strlcpy(ffblks->ff_name, w32_finddata->cAlternateFileName, sizeof(ffblks->ff_name));
	else
		strlcpy(ffblks->ff_name, w32_finddata->cFileName, sizeof(ffblks->ff_name));

	FileTimeToSystemTime(&w32_finddata->ftLastWriteTime, &system_time);

	ffblks->ff_fsize = (int32_t)w32_finddata->nFileSizeLow;
	/* Packed Date Format:
	   Year [7 bits/0xFE00]
	   Month [4 bits/0x01E0]
	   Day [5 bits/0x001F]
	   Packed Time Format:
	   Hours [5 bits/0xF800]
	   Minutes [6 bits/0x07E0]
	   Seconds/2 [5 bits/0x001F]
	*/
	ffblks->ff_fdate  = ((system_time.wYear - 1980) & 0x7F) << 9;
	ffblks->ff_fdate |= ((system_time.wMonth - 1) & 0x0F) << 5;
	ffblks->ff_fdate |= ((system_time.wDay) & 0x1F);
	ffblks->ff_ftime  = ((system_time.wHour) & 0x1F) << 11;
	ffblks->ff_ftime |= ((system_time.wMinute) & 0x2F) << 5;
	ffblks->ff_ftime |= ((system_time.wSecond / 2) & 0x1F);
	return 0;
}

static void convert_attribute(char *attrib_out, uint32_t attrib_in)
{
	*attrib_out = FA_NORMAL;
	if (attrib_in & FILE_ATTRIBUTE_READONLY)
		*attrib_out |= FA_RDONLY;
	if (attrib_in & FILE_ATTRIBUTE_HIDDEN)
		*attrib_out |= FA_HIDDEN;
	if (attrib_in & FILE_ATTRIBUTE_SYSTEM)
		*attrib_out |= FA_SYSTEM;
	if (attrib_in & FILE_ATTRIBUTE_DIRECTORY)
		*attrib_out |= FA_DIREC;
	if (attrib_in & FILE_ATTRIBUTE_ARCHIVE)
		*attrib_out |= FA_ARCH;
}
#endif
