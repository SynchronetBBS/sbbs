/*
 * Unit tests for src/items.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * items.c includes <OpenDoor.h> directly.  The binary is compiled with
 * -I$(SRC)odoors/ so the real header is found; we define od_control (the
 * single extern global OpenDoors expects) and stub od_clr_scr().
 *
 * NOTE: items.c defines the static Items{} struct and all Items_* functions
 * at file scope.  Do NOT define or stub those here.
 *
 * Targeted functions (pure slot-search and attribute-check logic):
 *   GetOpenItemSlot   – first Available=false slot in clan inventory
 *   GetUnusedItem     – first available, unequipped item of given type (static)
 *   GetUsedItem       – first equipped item; ITEM_NONE_EQUIPPED if items exist
 *                       but none are equipped; ITEM_NO_MATCH if no items (static)
 *   GetFirstItem      – first available item of given type (static)
 *   ItemPenalty       – true when PC lacks a required item attribute
 */

#include <assert.h>
#include <setjmp.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OpenDoor.h is found via -I$(SRC)odoors/ in the makefile rule. */
#include <OpenDoor.h>

/* Provide the storage for the single extern OpenDoors global. */
tODControl od_control;

#include "../defines.h"
#include "../structs.h"
#include "../system.h"
#include "../platform.h"
#include "../clansini.h"
#include "../door.h"
#include "../game.h"
#include "../help.h"
#include "../input.h"
#include "../items.h"
#include "../language.h"
#include "../mstrings.h"
#include "../myopen.h"
#include "../random.h"
#include "../spells.h"
#include "../user.h"
#include "../village.h"

#include "test_harness.h"

/*
 * System stubs
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
#include "../myopen.c"
#include "../items.c"

/* Note: test_items.c defines its own System_Error and CheckMem above,
 * so it does NOT include mocks_system.h (which would cause redefinition).
 * Other required externs are defined below. */

#include "mocks_od.h"
#include "mocks_door.h"
#include "mocks_language.h"
#include "mocks_spells.h"
#include "mocks_village.h"
#include "mocks_user.h"
#include "mocks_game.h"
#include "mocks_help.h"
#include "mocks_input.h"
#include "mocks_quests.h"
#include "mocks_clansini.h"

/* -------------------------------------------------------------------------
 * Tests: GetOpenItemSlot
 *
 * An "open slot" is one where Available==false (no item currently stored).
 * The function returns the index of the first such slot, or ITEM_NO_MATCH.
 * ------------------------------------------------------------------------- */
static void test_getopenitemslot_empty_clan(void)
{
	struct clan c = {0};
	/* All Available=false — first slot is free */
	ASSERT_EQ(GetOpenItemSlot(&c), 0);
}

static void test_getopenitemslot_all_occupied(void)
{
	struct clan c = {0};
	for (int i = 0; i < MAX_ITEMS_HELD; i++)
		c.Items[i].Available = true;
	ASSERT_EQ(GetOpenItemSlot(&c), ITEM_NO_MATCH);
}

static void test_getopenitemslot_first_hole(void)
{
	struct clan c = {0};
	c.Items[0].Available = true;
	c.Items[1].Available = true;
	c.Items[2].Available = true;
	/* Slot 3 is the first Available=false slot */
	ASSERT_EQ(GetOpenItemSlot(&c), 3);
}

/* -------------------------------------------------------------------------
 * Tests: GetUnusedItem (static — accessible via #include pattern)
 *
 * Finds the first available item of a given type with UsedBy==0 (not equipped).
 * Returns ITEM_NO_MATCH if none found.
 * ------------------------------------------------------------------------- */
static void test_getunuseditem_none_available(void)
{
	struct clan c = {0};
	/* No Available items at all */
	ASSERT_EQ(GetUnusedItem(&c, I_WEAPON), ITEM_NO_MATCH);
}

static void test_getunuseditem_all_in_use(void)
{
	struct clan c = {0};
	for (int i = 0; i < 3; i++) {
		c.Items[i].Available = true;
		c.Items[i].cType     = I_WEAPON;
		c.Items[i].UsedBy    = 1;  /* equipped by member 1 */
	}
	ASSERT_EQ(GetUnusedItem(&c, I_WEAPON), ITEM_NO_MATCH);
}

static void test_getunuseditem_finds_unused(void)
{
	struct clan c = {0};
	/* Slots 0 and 1 equipped, slot 2 unequipped */
	c.Items[0].Available = true; c.Items[0].cType = I_WEAPON; c.Items[0].UsedBy = 1;
	c.Items[1].Available = true; c.Items[1].cType = I_WEAPON; c.Items[1].UsedBy = 2;
	c.Items[2].Available = true; c.Items[2].cType = I_WEAPON; c.Items[2].UsedBy = 0;
	ASSERT_EQ(GetUnusedItem(&c, I_WEAPON), 2);
}

static void test_getunuseditem_type_filter(void)
{
	struct clan c = {0};
	c.Items[0].Available = true; c.Items[0].cType = I_ARMOR;  c.Items[0].UsedBy = 0;
	c.Items[1].Available = true; c.Items[1].cType = I_WEAPON; c.Items[1].UsedBy = 0;
	/* Searching for I_WEAPON skips slot 0 (I_ARMOR) and returns 1 */
	ASSERT_EQ(GetUnusedItem(&c, I_WEAPON), 1);
}

/* -------------------------------------------------------------------------
 * Tests: GetUsedItem (static — accessible via #include pattern)
 *
 * Returns index of a currently-equipped item.  If items exist but none are
 * equipped: ITEM_NONE_EQUIPPED.  If no items: ITEM_NO_MATCH.
 * Books and scrolls are excluded (cannot be "equipped").
 * ------------------------------------------------------------------------- */
static void test_getuseditem_no_items(void)
{
	struct clan c = {0};
	ASSERT_EQ(GetUsedItem(&c, I_WEAPON), ITEM_NO_MATCH);
}

static void test_getuseditem_none_equipped(void)
{
	struct clan c = {0};
	c.Items[0].Available = true;
	c.Items[0].cType     = I_WEAPON;
	c.Items[0].UsedBy    = 0;   /* present but not equipped */
	ASSERT_EQ(GetUsedItem(&c, I_WEAPON), ITEM_NONE_EQUIPPED);
}

static void test_getuseditem_finds_equipped(void)
{
	struct clan c = {0};
	c.Items[1].Available = true;
	c.Items[1].cType     = I_WEAPON;
	c.Items[1].UsedBy    = 2;   /* equipped by member 2 */
	ASSERT_EQ(GetUsedItem(&c, I_WEAPON), 1);
}

static void test_getuseditem_skips_books_scrolls(void)
{
	struct clan c = {0};
	/* A book and a scroll with UsedBy set — should NOT count as "equippable" */
	c.Items[0].Available = true; c.Items[0].cType = I_BOOK;   c.Items[0].UsedBy = 1;
	c.Items[1].Available = true; c.Items[1].cType = I_SCROLL; c.Items[1].UsedBy = 1;
	/* No equippable items present → ITEM_NO_MATCH */
	ASSERT_EQ(GetUsedItem(&c, I_ITEM), ITEM_NO_MATCH);
}

/* -------------------------------------------------------------------------
 * Tests: GetFirstItem (static — accessible via #include pattern)
 *
 * Returns the first available item of the given type (I_ITEM matches all).
 * ------------------------------------------------------------------------- */
static void test_getfirstitem_none(void)
{
	struct clan c = {0};
	ASSERT_EQ(GetFirstItem(&c, I_WEAPON), ITEM_NO_MATCH);
}

static void test_getfirstitem_any_type(void)
{
	struct clan c = {0};
	c.Items[2].Available = true;
	c.Items[2].cType     = I_ARMOR;
	/* I_ITEM (0) matches all types */
	ASSERT_EQ(GetFirstItem(&c, I_ITEM), 2);
}

static void test_getfirstitem_specific_type(void)
{
	struct clan c = {0};
	c.Items[0].Available = true; c.Items[0].cType = I_ARMOR;
	c.Items[1].Available = true; c.Items[1].cType = I_WEAPON;
	/* Searching for I_WEAPON skips slot 0 */
	ASSERT_EQ(GetFirstItem(&c, I_WEAPON), 1);
}

/* -------------------------------------------------------------------------
 * Tests: ItemPenalty
 *
 * Returns true if any PC attribute is below the item's required attribute.
 * Zero requirements are ignored (the condition is `ReqAttributes[i] != 0`).
 * ------------------------------------------------------------------------- */
static void test_itempenalty_no_penalty(void)
{
	struct pc         pc   = {0};
	struct item_data  item = {0};
	pc.Attributes[0]      = 10;
	item.ReqAttributes[0] =  5;   /* PC meets the requirement */
	ASSERT_EQ(ItemPenalty(&pc, &item), false);
}

static void test_itempenalty_has_penalty(void)
{
	struct pc         pc   = {0};
	struct item_data  item = {0};
	pc.Attributes[1]      = 3;
	item.ReqAttributes[1] = 7;   /* PC is below the requirement */
	ASSERT_EQ(ItemPenalty(&pc, &item), true);
}

static void test_itempenalty_zero_requirements(void)
{
	struct pc         pc   = {0};
	struct item_data  item = {0};
	/* All ReqAttributes are 0: condition `ReqAttributes[i] != 0` is false */
	ASSERT_EQ(ItemPenalty(&pc, &item), false);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(getopenitemslot_empty_clan);
	RUN(getopenitemslot_all_occupied);
	RUN(getopenitemslot_first_hole);
	RUN(getunuseditem_none_available);
	RUN(getunuseditem_all_in_use);
	RUN(getunuseditem_finds_unused);
	RUN(getunuseditem_type_filter);
	RUN(getuseditem_no_items);
	RUN(getuseditem_none_equipped);
	RUN(getuseditem_finds_equipped);
	RUN(getuseditem_skips_books_scrolls);
	RUN(getfirstitem_none);
	RUN(getfirstitem_any_type);
	RUN(getfirstitem_specific_type);
	RUN(itempenalty_no_penalty);
	RUN(itempenalty_has_penalty);
	RUN(itempenalty_zero_requirements);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
