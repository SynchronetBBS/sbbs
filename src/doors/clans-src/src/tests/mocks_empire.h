/*
 * mocks_empire.h
 *
 * Shared mocks for empire.h functionality.
 */

#ifndef MOCKS_EMPIRE_H
#define MOCKS_EMPIRE_H

const struct BuildingType BuildingType[NUM_BUILDINGTYPES] = {0};

int16_t ArmySpeed(struct Army *Army)
{
	(void)Army;
	return 0;
}

int64_t ArmyOffense(struct Army *Army)
{
	(void)Army;
	return 0;
}

int64_t ArmyDefense(struct Army *Army)
{
	(void)Army;
	return 0;
}

int64_t ArmyVitality(struct Army *Army)
{
	(void)Army;
	return 0;
}

void ProcessAttackPacket(struct AttackPacket *AttackPacket)
{
	(void)AttackPacket;
}

void ProcessResultPacket(struct AttackResult *Result)
{
	(void)Result;
}

void Empire_Stats(struct empire *Empire)
{
	(void)Empire;
}

void Empire_Maint(struct empire *Empire)
{
	(void)Empire;
}

void Empire_Create(struct empire *Empire, bool UserEmpire)
{
	(void)Empire; (void)UserEmpire;
}

void Empire_Manage(struct empire *Empire)
{
	(void)Empire;
}

void DonateToEmpire(struct empire *Empire)
{
	(void)Empire;
}

#endif /* MOCKS_EMPIRE_H */
