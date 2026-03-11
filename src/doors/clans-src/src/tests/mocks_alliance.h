/*
 * mocks_alliance.h
 *
 * Shared mocks for alliance.h functionality.
 */

#ifndef MOCKS_ALLIANCE_H
#define MOCKS_ALLIANCE_H

struct Alliance *Alliances[MAX_ALLIANCES];
int16_t NumAlliances = 0;

void DeleteAlliance(int Index)
{
	(void)Index;
}

void Alliances_Init(void)
{
}

void Alliances_Close(void)
{
}

void KillAlliances(void)
{
}

#endif /* MOCKS_ALLIANCE_H */
