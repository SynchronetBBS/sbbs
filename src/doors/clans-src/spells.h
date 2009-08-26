
void Spells_Init(void);
/*
 * This function loads spells from file.
 */

void Spells_Close(void);
/*
 * This function frees any mem initialized by Spells_Init.
 */

void Spells_UpdatePCSpells(struct pc *PC);
void Spells_ClearSpells(struct clan *Clan);
void Spells_CastSpell(struct pc *PC, struct clan *EnemyClan, _INT16 Target, _INT16 SpellNum);
