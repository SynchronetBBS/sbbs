
void Empire_Stats(struct empire *Empire);

void Empire_Maint(struct empire *Empire);

void Empire_Create(struct empire *Empire, BOOL UserEmpire);

void Empire_Manage(struct empire *Empire);

void DonateToEmpire(struct empire *Empire);


_INT16 ArmySpeed(struct Army *Army);
long ArmyOffense(struct Army *Army);
long ArmyDefense(struct Army *Army);
long ArmyVitality(struct Army *Army);

void ProcessAttackResult(struct AttackResult *AttackResult);
void ProcessAttackPacket(struct AttackPacket *AttackPacket);
void ProcessResultPacket(struct AttackResult *Result);
