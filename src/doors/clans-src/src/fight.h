
void Fight_Monster(_INT16 Level, char *szFileName);

void FreeClan(struct clan *Clan);
struct clan *MallocClan(void);

void Fight_CheckLevelUp(void);

_INT16 Fight_Fight(struct clan *Attacker, struct clan *Defender,
				   BOOL HumanEnemy, BOOL CanRun, BOOL AutoFight);

void Fight_Clan(void);

void Fight_Heal(struct clan *Clan);
