/*
 * Unit tests for src/config.c MailerTypeName function
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * Targeted function:
 *   MailerTypeName – returns a string for a mailer type constant (switch statement)
 *
 * The function has 4 branches:
 *   MAIL_BINKLEY → "Binkley"
 *   MAIL_OTHER   → "Attach"
 *   MAIL_NONE    → "None"
 *   default      → "Unknown"
 */

#include <stdio.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"
#include "../platform.h"

#include "test_harness.h"

/* config.c defines its own main(); rename it before including */
#define main config_main
#include "../config.c"
#undef main

/* -------------------------------------------------------------------------
 * Tests: MailerTypeName
 * Tests all four branches of the switch statement.
 * The function returns string literals, so strcmp is the appropriate check.
 * ------------------------------------------------------------------------- */
static void test_mailertypename_binkley(void)
{
	const char *result = MailerTypeName(MAIL_BINKLEY);
	ASSERT_EQ(strcmp(result, "Binkley"), 0);
}

static void test_mailertypename_other(void)
{
	const char *result = MailerTypeName(MAIL_OTHER);
	ASSERT_EQ(strcmp(result, "Attach"), 0);
}

static void test_mailertypename_none(void)
{
	const char *result = MailerTypeName(MAIL_NONE);
	ASSERT_EQ(strcmp(result, "None"), 0);
}

static void test_mailertypename_unknown(void)
{
	/* Any value not in the switch should return "Unknown" */
	const char *result = MailerTypeName(9999);
	ASSERT_EQ(strcmp(result, "Unknown"), 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(mailertypename_binkley);
	RUN(mailertypename_other);
	RUN(mailertypename_none);
	RUN(mailertypename_unknown);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
