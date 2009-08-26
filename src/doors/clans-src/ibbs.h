
void IBBS_Init(void);
/*
 * IBBS data is initialized.  MUST be called before any other IBBS_*
 * functions are called.
 */

void IBBS_Close(void);
/*
 * Closes down IBBS.
 */

void IBBS_LoginStats(void);
/*
 * Shows IBBS stats for local login.
 */

void IBBS_Maint(void);

void IBBS_SendAttackPacket(struct empire *AttackingEmpire, struct Army *AttackingArmy,
						   _INT16 Goal, _INT16 ExtentOfAttack, _INT16 TargetType, _INT16 ClanID[2], _INT16 DestID);

void IBBS_SendSpy(struct empire *Empire, _INT16 DestID);

void IBBS_ShowLeagueAscii(void);
void IBBS_LeagueInfo(void);

void IBBS_SendPacket(_INT16 PacketType, long PacketLength, void *PacketData,
					 _INT16 DestID);

void IBBS_UpdateReset(void);

void IBBS_UpdateRecon(void);

void IBBS_SendPacketFile(_INT16 DestID, char *pszSendFile);


void IBBS_SeeVillages(BOOL Travel);

void IBBS_TravelMaint(void);

void IBBS_CurrentTravelInfo(void);
void IBBS_BackupMaint(void);

void IBBS_SendComeBack(_INT16 BBSIdTo, struct clan *Clan);

BOOL IBBS_InList(char *szName, BOOL ClanName);

void RemoveFromUList(_INT16 ClanID[2]);

void IBBS_DistributeNDX(void);

void IBBS_SendReset(_INT16 DestID);
void IBBS_SendRecon(_INT16 DestID);

void LeagueKillUser(struct UserInfo *User);

void IBBS_PacketIn(void);
void IBBS_LeagueNewUser(struct UserInfo *User);
