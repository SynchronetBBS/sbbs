
void User_Maint(void);

__BOOL User_Init(void);
/*
 * The current user online is initialized.  His stats are read in from
 * the .PC file.  If not found, a new clan is created.
 *
 * Returns FALSE if user is new and didn't want to join the game.
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

_INT16 NumMembers(struct clan *Clan, __BOOL OnlyOnesHere);
/*
 * Returns the number of members in the clan.
 *
 * PRE: OnlyOnesHere is TRUE if only the alive members are to be checked.
 */

void ClanStats(struct clan *Clan, __BOOL AllowModify);
/*
 * Shows stats for given Clan with option to modify values.
 */

__BOOL GetClanID(_INT16 ID[2], __BOOL OnlyLiving, __BOOL IncludeSelf,
				 _INT16 WhichAlliance, __BOOL InAllianceOnly);

void ShowPlayerStats(struct pc *PC, __BOOL AllowModify);
char GetStat(struct pc *PC, char Stat);

void ListItems(struct clan *Clan);

__BOOL GetClanNameID(char *szName, _INT16 ID[2]);

__BOOL GetClan(_INT16 ClanID[2], struct clan *TmpClan);

__BOOL ClanExists(_INT16 ClanID[2]);

void PC_Create(struct pc *PC, __BOOL ClanLeader);

void Clan_Update(struct clan *Clan);

void ShowVillageStats(void);

void User_List(void);

void AddToDisband(void);

void DeleteClan(_INT16 ClanID[2], char *szClanName, __BOOL Eliminate);

void User_Destroy(void);

__BOOL Disbanded(void);

void User_Write(void);
