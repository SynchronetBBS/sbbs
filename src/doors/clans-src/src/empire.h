#ifndef THE_CLANS__EMPIRE___H
#define THE_CLANS__EMPIRE___H

#include "structs.h"

void Empire_Stats(struct empire *Empire);

void Empire_Maint(struct empire *Empire);

void Empire_Create(struct empire *Empire, bool UserEmpire);

void Empire_Manage(struct empire *Empire);

void DonateToEmpire(struct empire *Empire);


int16_t ArmySpeed(struct Army *Army);
int32_t ArmyOffense(struct Army *Army);
int32_t ArmyDefense(struct Army *Army);
int32_t ArmyVitality(struct Army *Army);

void ProcessAttackResult(struct AttackResult *AttackResult);
void ProcessAttackPacket(struct AttackPacket *AttackPacket);
void ProcessResultPacket(struct AttackResult *Result);

#endif
