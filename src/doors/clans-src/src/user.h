#ifndef USER_H
#define USER_H

extern struct clan *PClan;

void User_Maint(void);

bool User_Init(void);
/*
 * The current user online is initialized.  His stats are read in from
 * the .PC file.  If not found, a new clan is created.
 *
 * Returns false if user is new and didn't want to join the game.
 */

void User_FirstTimeToday(void);
/*
 * Run whenever user plays first time that day.
 *
 */

void User_Close(void);
/*
 * Must be called to close down the user data.  User data is written to
 * file if UpdateUser set (so that deleted users are not written to file
 * if they chose to delete themselves).
 */

int16_t NumMembers(struct clan *Clan, bool OnlyOnesHere);
/*
 * Returns the number of members in the clan.
 *
 * PRE: OnlyOnesHere is true if only the alive members are to be checked.
 */

void ClanStats(struct clan *Clan, bool AllowModify);
/*
 * Shows stats for given Clan with option to modify values.
 */

bool GetClanID(int16_t ID[2], bool OnlyLiving, bool IncludeSelf,
				 int16_t WhichAlliance, bool InAllianceOnly);

void ShowPlayerStats(struct pc *PC, bool AllowModify);
char GetStat(struct pc *PC, char Stat);

void ListItems(struct clan *Clan);

bool GetClanNameID(char *szName, int16_t ID[2]);

bool GetClan(int16_t ClanID[2], struct clan *TmpClan);

bool ClanExists(int16_t ClanID[2]);

void PC_Create(struct pc *PC, bool ClanLeader);

void Clan_Update(struct clan *Clan);

void ShowVillageStats(void);

void User_List(void);

void AddToDisband(void);

void DeleteClan(int16_t ClanID[2], char *szClanName, bool Eliminate);

void User_Destroy(void);

bool Disbanded(void);

void User_Write(void);

#endif
