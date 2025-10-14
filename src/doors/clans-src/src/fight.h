
void Fight_Monster(int16_t Level, char *szFileName);

void FreeClan(struct clan *Clan);
struct clan *MallocClan(void);

void Fight_CheckLevelUp(void);

int16_t Fight_Fight(struct clan *Attacker, struct clan *Defender,
				   bool HumanEnemy, bool CanRun, bool AutoFight);

void Fight_Clan(void);

void Fight_Heal(struct clan *Clan);
