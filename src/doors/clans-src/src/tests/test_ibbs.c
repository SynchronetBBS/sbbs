/*
 * Unit tests for src/ibbs.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * NOTE: ibbs.c defines struct ibbs IBBS and aszShortMonthName at file scope,
 * as well as all IBBS_* functions, VillageName, and BBSName.  Do NOT define
 * or stub any of those here.
 *
 * Targeted functions (all accessible via the #include pattern):
 *   VillageName  – maps a BBS ID to its village name string
 *   BBSName      – maps a BBS ID to its BBS name string
 *   CheckID      – static; validates a BBS ID and checks Active flag
 *   (CheckSourceID and CheckDestinationID are thin wrappers; tested here too)
 */

#include <assert.h>
#include <setjmp.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"
#include "../system.h"
#include "../platform.h"
#include "../language.h"
#include "../alliance.h"
#include "../empire.h"
#include "../fight.h"
#include "../game.h"
#include "../help.h"
#include "../ibbs.h"
#include "../input.h"
#include "../mail.h"
#include "../misc.h"
#include "../mstrings.h"
#include "../myibbs.h"
#include "../myopen.h"
#include "../news.h"
#include "../parsing.h"
#include "../readcfg.h"
#include "../scores.h"
#include "../user.h"
#include "../village.h"
#include "../video.h"

#include "test_harness.h"

/*
 * System_Error and CheckMem — forward-declared as noreturn in system.h.
 */
static jmp_buf g_fatal_jmp;

noreturn void System_Error(char *szErrorMsg)
{
	(void)szErrorMsg;
	longjmp(g_fatal_jmp, 1);
}

void CheckMem(void *ptr)
{
	if (!ptr) longjmp(g_fatal_jmp, 1);
}

#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../misc.c"
#include "../parsing.c"
#include "../myopen.c"
#include "../myibbs.c"
#include "../ibbs.c"

#include "mocks_od.h"
#include "mocks_door.h"
#include "mocks_video.h"
#include "mocks_language.h"
#include "mocks_empire.h"
#include "mocks_fight.h"
#include "mocks_user.h"
#include "mocks_game.h"
#include "mocks_village.h"
#include "mocks_readcfg.h"
#include "mocks_alliance.h"
#include "mocks_spells.h"
#include "mocks_items.h"
#include "mocks_help.h"
#include "mocks_input.h"
#include "mocks_mail.h"
#include "mocks_news.h"
#include "mocks_quests.h"

/* -------------------------------------------------------------------------
 * Tests: VillageName
 *
 * VillageName(BBSID):
 *   BBSID < 1 || BBSID > MAX_IBBSNODES → "<invalid>"
 *   pszVillageName == NULL             → "<defunct>"
 *   otherwise                          → pszVillageName
 * ------------------------------------------------------------------------- */
static void test_villagename_invalid_low(void)
{
	/* BBSID=0 is below the valid range */
	ASSERT_EQ(strcmp(VillageName(0), "<invalid>"), 0);
}

static void test_villagename_invalid_high(void)
{
	/* BBSID=MAX_IBBSNODES+1 is above the valid range */
	ASSERT_EQ(strcmp(VillageName(MAX_IBBSNODES + 1), "<invalid>"), 0);
}

static void test_villagename_null(void)
{
	/* BBSID=1, pszVillageName=NULL (zero-initialised) → "<defunct>" */
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(strcmp(VillageName(1), "<defunct>"), 0);
}

static void test_villagename_named(void)
{
	/* BBSID=1 with a non-NULL name → returns that name */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Info.pszVillageName = "Stonehaven";
	ASSERT_EQ(strcmp(VillageName(1), "Stonehaven"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: BBSName
 *
 * Same sentinel logic as VillageName, applied to pszBBSName.
 * ------------------------------------------------------------------------- */
static void test_bbsname_invalid_low(void)
{
	ASSERT_EQ(strcmp(BBSName(0), "<invalid>"), 0);
}

static void test_bbsname_null(void)
{
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(strcmp(BBSName(2), "<defunct>"), 0);
}

static void test_bbsname_named(void)
{
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[1].Info.pszBBSName = "TestBBS";
	ASSERT_EQ(strcmp(BBSName(2), "TestBBS"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: CheckID (static, tested via CheckSourceID / CheckDestinationID)
 *
 * CheckID(ID, name):
 *   ID < 1 || ID > MAX_IBBSNODES → 0 (false)
 *   !IBBS.Data.Nodes[ID-1].Active → 0
 *   otherwise                     → (int16_t)ID
 * ------------------------------------------------------------------------- */
static void test_checkid_invalid_low(void)
{
	/* ID=0 is outside the valid BBS ID range */
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(CheckSourceID(0), 0);
}

static void test_checkid_invalid_high(void)
{
	/* ID=MAX_IBBSNODES+1 is outside the valid BBS ID range */
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(CheckSourceID(MAX_IBBSNODES + 1), 0);
}

static void test_checkid_inactive(void)
{
	/* Valid ID but node is not Active → 0 */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Active = false;
	ASSERT_EQ(CheckSourceID(1), 0);
}

static void test_checkid_active(void)
{
	/* Valid ID and Active=true → returns the ID */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Active = true;
	ASSERT_EQ(CheckSourceID(1), 1);
}

static void test_checkdestid_inactive(void)
{
	/* CheckDestinationID wraps CheckID with a different label string */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[3].Active = false;
	ASSERT_EQ(CheckDestinationID(4), 0);
}

static void test_checkdestid_active(void)
{
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[3].Active = true;
	ASSERT_EQ(CheckDestinationID(4), 4);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(villagename_invalid_low);
	RUN(villagename_invalid_high);
	RUN(villagename_null);
	RUN(villagename_named);
	RUN(bbsname_invalid_low);
	RUN(bbsname_null);
	RUN(bbsname_named);
	RUN(checkid_invalid_low);
	RUN(checkid_invalid_high);
	RUN(checkid_inactive);
	RUN(checkid_active);
	RUN(checkdestid_inactive);
	RUN(checkdestid_active);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
