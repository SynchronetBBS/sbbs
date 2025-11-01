#ifndef THE_CLANS__EMPIRE___H
#define THE_CLANS__EMPIRE___H

#include "structs.h"

extern struct BuildingType BuildingType[NUM_BUILDINGTYPES];

void Empire_Stats(struct empire *Empire);

void Empire_Maint(struct empire *Empire);

void Empire_Create(struct empire *Empire, bool UserEmpire);

void Empire_Manage(struct empire *Empire);

void DonateToEmpire(struct empire *Empire);


int16_t ArmySpeed(struct Army *Army);
int64_t ArmyOffense(struct Army *Army);
int64_t ArmyDefense(struct Army *Army);
int64_t ArmyVitality(struct Army *Army);

void ProcessAttackPacket(struct AttackPacket *AttackPacket);
void ProcessResultPacket(struct AttackResult *Result);

#endif
