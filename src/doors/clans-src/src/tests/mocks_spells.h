/*
 * mocks_spells.h
 *
 * Shared mocks for spells.h functionality.
 */

#ifndef MOCKS_SPELLS_H
#define MOCKS_SPELLS_H

char Spells_szCastDestination[25];
char Spells_szCastSource[25];
int Spells_CastValue;
struct Spell *Spells[MAX_SPELLS];

void Spells_Init(void)
{
}

void Spells_Close(void)
{
}

void Spells_UpdatePCSpells(struct pc *PC)
{
	(void)PC;
}

void Spells_ClearSpells(struct clan *Clan)
{
	(void)Clan;
}

void Spells_CastSpell(struct pc *PC, struct clan *EnemyClan,
	int16_t Target, int16_t SpellNum)
{
	(void)PC; (void)EnemyClan; (void)Target; (void)SpellNum;
}

#endif /* MOCKS_SPELLS_H */
