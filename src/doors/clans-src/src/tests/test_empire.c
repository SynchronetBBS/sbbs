/*
 * Unit tests for src/empire.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * Targets four pure Army computation functions (all simple arithmetic):
 *   ArmySpeed    – weighted-average travel speed of a mixed army
 *   ArmyOffense  – total offensive strength
 *   ArmyDefense  – total defensive strength
 *   ArmyVitality – total vitality (result is doubled)
 * and the static duplicate-packet detector:
 *   ValidateIndex – signed-wrap-safe "have we seen this attack index?" check
 *
 * The stat constants (SPD_*, OFF_*, DEF_*, VIT_*) are #define'd at the top
 * of empire.c and become visible in this translation unit via the
 * #include pattern.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"
#include "../system.h"
#include "../platform.h"
#include "../language.h"
#include "../alliance.h"
#include "../fight.h"
#include "../game.h"
#include "../help.h"
#include "../ibbs.h"
#include "../input.h"
#include "../mail.h"
#include "../mstrings.h"
#include "../myopen.h"
#include "../news.h"
#include "../readcfg.h"
#include "../user.h"
#include "../village.h"

#include "test_harness.h"
#include "mocks_od.h"
#include "mocks_system.h"
#include "mocks_video.h"
#include "mocks_language.h"
#include "mocks_door.h"
#include "mocks_fight.h"
#include "mocks_user.h"
#include "mocks_ibbs.h"
#include "mocks_game.h"
#include "mocks_village.h"
#include "mocks_readcfg.h"
#include "mocks_quests.h"

/* =========================================================================
 * user.h custom stubs (not mocked)
 * PClan is defined here; GetStat and NumMembers are real implementations
 * because they may be called from empire.c helper code paths.
 * ========================================================================= */

/* Already in mocks_user.h: struct clan PClan; */

int8_t GetStat(struct pc *p, char s)  { return p->Attributes[(int)s]; }

int16_t NumMembers(struct clan *c, bool h)
{
	int16_t n = 0;
	for (int i = 0; i < MAX_MEMBERS; i++)
		if (c->Member[i] && (!h || c->Member[i]->Status == Here))
			n++;
	return n;
}

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * platform.c brings unix_wrappers.c; random.c provides my_random (called
 * inside empire.c's combat functions); serialize/deserialize/myopen provide
 * the file I/O layer.  empire.c is last.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../empire.c"

/* -------------------------------------------------------------------------
 * Tests: ArmySpeed
 *
 * SPD_FOOTMEN=5, SPD_AXEMEN=3, SPD_KNIGHTS=7
 * Speed = (F*5 + A*3 + K*7) / (F+A+K), or 0 if no troops.
 * ------------------------------------------------------------------------- */
static void test_speed_empty(void)
{
	struct Army a = {0};
	ASSERT_EQ(ArmySpeed(&a), 0);
}

static void test_speed_footmen_only(void)
{
	struct Army a = {0};
	a.Footmen = 100;
	/* (100*5) / 100 = 5 */
	ASSERT_EQ(ArmySpeed(&a), 5);
}

static void test_speed_axemen_only(void)
{
	struct Army a = {0};
	a.Axemen = 100;
	/* (100*3) / 100 = 3 */
	ASSERT_EQ(ArmySpeed(&a), 3);
}

static void test_speed_knights_only(void)
{
	struct Army a = {0};
	a.Knights = 100;
	/* (100*7) / 100 = 7 */
	ASSERT_EQ(ArmySpeed(&a), 7);
}

static void test_speed_mixed(void)
{
	struct Army a = {0};
	a.Footmen = 60; a.Axemen = 30; a.Knights = 10;
	/* (60*5 + 30*3 + 10*7) / 100 = (300+90+70)/100 = 460/100 = 4 */
	ASSERT_EQ(ArmySpeed(&a), 4);
}

/* -------------------------------------------------------------------------
 * Tests: ArmyOffense
 *
 * OFF_FOOTMEN=6, OFF_AXEMEN=8, OFF_KNIGHTS=9
 * Offense = F*6 + A*8 + K*9
 * ------------------------------------------------------------------------- */
static void test_offense_empty(void)
{
	struct Army a = {0};
	ASSERT_EQ(ArmyOffense(&a), 0);
}

static void test_offense_footmen(void)
{
	struct Army a = {0};
	a.Footmen = 10;
	ASSERT_EQ(ArmyOffense(&a), 60); /* 10*6 */
}

static void test_offense_axemen(void)
{
	struct Army a = {0};
	a.Axemen = 10;
	ASSERT_EQ(ArmyOffense(&a), 80); /* 10*8 */
}

static void test_offense_knights(void)
{
	struct Army a = {0};
	a.Knights = 10;
	ASSERT_EQ(ArmyOffense(&a), 90); /* 10*9 */
}

static void test_offense_mixed(void)
{
	struct Army a = {0};
	a.Footmen = 1; a.Axemen = 2; a.Knights = 3;
	/* 1*6 + 2*8 + 3*9 = 6+16+27 = 49 */
	ASSERT_EQ(ArmyOffense(&a), 49);
}

/* -------------------------------------------------------------------------
 * Tests: ArmyDefense
 *
 * DEF_FOOTMEN=2, DEF_AXEMEN=4, DEF_KNIGHTS=1
 * Defense = F*2 + A*4 + K*1
 * ------------------------------------------------------------------------- */
static void test_defense_empty(void)
{
	struct Army a = {0};
	ASSERT_EQ(ArmyDefense(&a), 0);
}

static void test_defense_footmen(void)
{
	struct Army a = {0};
	a.Footmen = 10;
	ASSERT_EQ(ArmyDefense(&a), 20); /* 10*2 */
}

static void test_defense_axemen(void)
{
	struct Army a = {0};
	a.Axemen = 10;
	ASSERT_EQ(ArmyDefense(&a), 40); /* 10*4 */
}

static void test_defense_knights(void)
{
	struct Army a = {0};
	a.Knights = 10;
	ASSERT_EQ(ArmyDefense(&a), 10); /* 10*1 */
}

static void test_defense_mixed(void)
{
	struct Army a = {0};
	a.Footmen = 1; a.Axemen = 2; a.Knights = 3;
	/* 1*2 + 2*4 + 3*1 = 2+8+3 = 13 */
	ASSERT_EQ(ArmyDefense(&a), 13);
}

/* -------------------------------------------------------------------------
 * Tests: ArmyVitality
 *
 * VIT_FOOTMEN=8, VIT_AXEMEN=7, VIT_KNIGHTS=6
 * Vitality = 2 * (F*8 + A*7 + K*6)
 * ------------------------------------------------------------------------- */
static void test_vitality_empty(void)
{
	struct Army a = {0};
	ASSERT_EQ(ArmyVitality(&a), 0);
}

static void test_vitality_footmen(void)
{
	struct Army a = {0};
	a.Footmen = 5;
	ASSERT_EQ(ArmyVitality(&a), 80); /* 2*(5*8) = 80 */
}

static void test_vitality_axemen(void)
{
	struct Army a = {0};
	a.Axemen = 5;
	ASSERT_EQ(ArmyVitality(&a), 70); /* 2*(5*7) = 70 */
}

static void test_vitality_mixed(void)
{
	struct Army a = {0};
	a.Footmen = 1; a.Axemen = 2; a.Knights = 3;
	/* 2*(1*8 + 2*7 + 3*6) = 2*(8+14+18) = 2*40 = 80 */
	ASSERT_EQ(ArmyVitality(&a), 80);
}

/* -------------------------------------------------------------------------
 * Tests: ValidateIndex  (static — accessible via #include pattern)
 *
 * ValidateIndex(fid, iidx) rejects replayed attack packets by comparing
 * iidx to the stored ReceiveIndex for BBS node fid.  On the first call
 * with ReceiveIndex=0, only iidx=0 is "old" (ridx==iidx branch).
 * A fresh node has ReceiveIndex=0 by zero-initialisation.
 * ------------------------------------------------------------------------- */
static void test_validateindex_fresh_new(void)
{
	/* Node 1: ReceiveIndex=0; incoming index 1 is new → returns true */
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(ValidateIndex(1, 1), 1);
	/* ReceiveIndex must be updated to 1 */
	ASSERT_EQ(IBBS.Data.Nodes[0].Attack.ReceiveIndex, 1);
}

static void test_validateindex_duplicate(void)
{
	/* Same index as ReceiveIndex → duplicate, returns false */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Attack.ReceiveIndex = 5;
	ASSERT_EQ(ValidateIndex(1, 5), 0);
	/* ReceiveIndex must remain unchanged */
	ASSERT_EQ(IBBS.Data.Nodes[0].Attack.ReceiveIndex, 5);
}

static void test_validateindex_old_within_window(void)
{
	/* ridx=300, iidx=80: iidx <= ridx and iidx >= ridx-255=45 → old */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Attack.ReceiveIndex = 300;
	ASSERT_EQ(ValidateIndex(1, 80), 0);
}

static void test_validateindex_valid_advance(void)
{
	/* iidx well above ReceiveIndex → valid, ReceiveIndex updated */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Attack.ReceiveIndex = 100;
	ASSERT_EQ(ValidateIndex(1, 200), 1);
	ASSERT_EQ(IBBS.Data.Nodes[0].Attack.ReceiveIndex, 200);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(speed_empty);
	RUN(speed_footmen_only);
	RUN(speed_axemen_only);
	RUN(speed_knights_only);
	RUN(speed_mixed);
	RUN(offense_empty);
	RUN(offense_footmen);
	RUN(offense_axemen);
	RUN(offense_knights);
	RUN(offense_mixed);
	RUN(defense_empty);
	RUN(defense_footmen);
	RUN(defense_axemen);
	RUN(defense_knights);
	RUN(defense_mixed);
	RUN(vitality_empty);
	RUN(vitality_footmen);
	RUN(vitality_axemen);
	RUN(vitality_mixed);
	RUN(validateindex_fresh_new);
	RUN(validateindex_duplicate);
	RUN(validateindex_old_within_window);
	RUN(validateindex_valid_advance);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
