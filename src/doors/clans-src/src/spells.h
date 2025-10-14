
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
void Spells_CastSpell(struct pc *PC, struct clan *EnemyClan, int16_t Target, int16_t SpellNum);
