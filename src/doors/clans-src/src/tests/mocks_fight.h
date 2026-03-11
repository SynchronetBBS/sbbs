/*
 * mocks_fight.h
 *
 * Shared mocks for fight.h functionality.  Provides fight-related stubs
 * used by 5+ test files.
 *
 * INTENTIONALLY OMITTED: FreeClanMembers.  This function is defined
 * differently in test_scores.c (calls free()) and is intentionally not
 * mocked here.  test_scores.c must define it locally.
 */

#ifndef MOCKS_FIGHT_H
#define MOCKS_FIGHT_H

void FreeClan(struct clan *c)
{
	(void)c;
}

void InitClan(struct clan *c)
{
	memset(c, 0, sizeof(*c));
}

void Fight_CheckLevelUp(void)
{
}

int16_t Fight_Fight(struct clan *a, struct clan *d, bool he, bool cr, bool af)
{
	(void)a; (void)d; (void)he; (void)cr; (void)af;
	return 0;
}

void Fight_Clan(void)
{
}

void Fight_Heal(struct clan *c)
{
	(void)c;
}

void Fight_Monster(int16_t l, char *f)
{
	(void)l; (void)f;
}

#endif /* MOCKS_FIGHT_H */
