/*
 * mocks_user.h
 *
 * Shared mocks for user.h functionality.  Provides PClan struct and
 * user management stubs used by 6+ test files.
 *
 * INTENTIONALLY OMITTED: GetStat and NumMembers.  These functions are
 * defined differently in test_empire.c, test_fight.c, and test_scores.c
 * and are intentionally not mocked here.  Each test file that needs these
 * must define them locally.
 */

#ifndef MOCKS_USER_H
#define MOCKS_USER_H

struct clan PClan;

void User_Maint(void)
{
}

bool User_Init(void)
{
	return false;
}

void User_FirstTimeToday(void)
{
}

void User_Close(void)
{
}

void ClanStats(struct clan *c, bool b)
{
	(void)c; (void)b;
}

bool GetClanID(int16_t id[2], bool a, bool b, int16_t c, bool d)
{
	(void)id; (void)a; (void)b; (void)c; (void)d;
	return false;
}

void ShowPlayerStats(struct pc *p, bool b)
{
	(void)p; (void)b;
}

void ListItems(struct clan *c)
{
	(void)c;
}

bool GetClanNameID(char *n, size_t s, int16_t id[2])
{
	(void)n; (void)s; (void)id;
	return false;
}

bool GetClan(int16_t id[2], struct clan *c)
{
	(void)id; (void)c;
	return false;
}

bool ClanExists(int16_t id[2])
{
	(void)id;
	return false;
}

void PC_Create(struct pc *p, bool b)
{
	(void)p; (void)b;
}

void Clan_Update(struct clan *c)
{
	(void)c;
}

void ShowVillageStats(void)
{
}

void User_List(void)
{
}

void User_ResetAllVotes(void)
{
}

void DeleteClan(int16_t id[2], char *n)
{
	(void)id; (void)n;
}

void User_Destroy(void)
{
}

void User_Write(void)
{
}

#endif /* MOCKS_USER_H */
