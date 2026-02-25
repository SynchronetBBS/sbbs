#ifndef THE_CLANS__IBBS___H
#define THE_CLANS__IBBS___H

#include "structs.h"

extern struct ibbs IBBS;
extern const char aszShortMonthName[12][4];

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
						   int16_t Goal, int16_t ExtentOfAttack, int16_t TargetType, int16_t ClanID[2], int16_t DestID);
void IBBS_EnqueueOutPacket(int16_t PacketType, size_t PacketLength, void *PacketData,
					 int16_t DestID);

void IBBS_SendSpy(struct empire *Empire, int16_t DestID);

void IBBS_SendQueuedResults(void);
void IBBS_HandleQueuedPackets(void);

void IBBS_ShowLeagueAscii(void);
void IBBS_LeagueInfo(void);

void IBBS_SendPacket(int16_t PacketType, size_t PacketLength, void *PacketData,
					 int16_t DestID);

void IBBS_UpdateReset(void);

void IBBS_UpdateRecon(void);

void IBBS_SeeVillages(bool Travel);

void IBBS_CurrentTravelInfo(void);

void IBBS_SendComeBack(int16_t BBSIdTo, struct clan *Clan);

bool IBBS_InList(char *szName, bool ClanName);

void RemoveFromUList(const int16_t ClanID[2]);

void IBBS_DistributeNDX(void);

void IBBS_SendReset(int16_t DestID);
void IBBS_SendRecon(int16_t DestID);

void LeagueKillUser(struct UserInfo *User);

void IBBS_PacketIn(void);
void IBBS_LeagueNewUser(struct UserInfo *User);

void KillAlliances(void);
const char *VillageName(int16_t BBSID);
const char *BBSName(int16_t BBSID);

#endif
