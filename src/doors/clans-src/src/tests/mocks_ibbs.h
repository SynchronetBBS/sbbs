/*
 * mocks_ibbs.h
 *
 * Shared mocks for ibbs.h functionality.  Provides IBBS-related stubs
 * and the month name table used by 6+ test files.
 */

#ifndef MOCKS_IBBS_H
#define MOCKS_IBBS_H

struct ibbs IBBS;

const char aszShortMonthName[12][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

void IBBS_Init(void)
{
}

void IBBS_Close(void)
{
}

void IBBS_LoginStats(void)
{
}

void IBBS_Maint(void)
{
}

void IBBS_SendAttackPacket(struct empire *ae, struct Army *aa, int16_t g,
	int16_t e, int16_t tt, int16_t id[2], int16_t d)
{
	(void)ae; (void)aa; (void)g; (void)e; (void)tt; (void)id; (void)d;
}

void IBBS_EnqueueOutPacket(int16_t t, size_t l, void *d, int16_t dest)
{
	(void)t; (void)l; (void)d; (void)dest;
}

void IBBS_SendSpy(struct empire *e, int16_t d)
{
	(void)e; (void)d;
}

void IBBS_SendQueuedResults(void)
{
}

void IBBS_HandleQueuedPackets(void)
{
}

void IBBS_ShowLeagueAscii(void)
{
}

void IBBS_LeagueInfo(void)
{
}

void IBBS_SendPacket(int16_t t, size_t l, void *d, int16_t dest)
{
	(void)t; (void)l; (void)d; (void)dest;
}

void IBBS_UpdateReset(void)
{
}

void IBBS_UpdateRecon(void)
{
}

void IBBS_SeeVillages(bool tr)
{
	(void)tr;
}

void IBBS_CurrentTravelInfo(void)
{
}

void IBBS_SendComeBack(int16_t id, struct clan *c)
{
	(void)id; (void)c;
}

bool IBBS_InList(char *n, bool cn)
{
	(void)n; (void)cn;
	return false;
}

void RemoveFromUList(const int16_t id[2])
{
	(void)id;
}

void IBBS_DistributeNDX(void)
{
}

void IBBS_SendReset(int16_t d)
{
	(void)d;
}

void IBBS_SendRecon(int16_t d)
{
	(void)d;
}

void LeagueKillUser(struct UserInfo *u)
{
	(void)u;
}

void IBBS_PacketIn(void)
{
}

void IBBS_LeagueNewUser(struct UserInfo *u)
{
	(void)u;
}

const char *VillageName(int16_t id)
{
	(void)id;
	return "?";
}

const char *BBSName(int16_t id)
{
	(void)id;
	return "?";
}

#endif /* MOCKS_IBBS_H */
