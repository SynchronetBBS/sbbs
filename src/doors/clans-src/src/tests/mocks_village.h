/*
 * mocks_village.h
 *
 * Shared mocks for village.h functionality.  Provides Village struct and
 * village management stubs used by 7+ test files.
 */

#ifndef MOCKS_VILLAGE_H
#define MOCKS_VILLAGE_H

struct village Village;

void Village_Init(void)
{
}

void Village_Close(void)
{
}

void Village_Maint(void)
{
}

void Village_NewRuler(void)
{
}

int16_t OutsiderTownHallMenu(void)
{
	return 0;
}

int16_t TownHallMenu(void)
{
	return 0;
}

void Village_Reset(void)
{
}

#endif /* MOCKS_VILLAGE_H */
