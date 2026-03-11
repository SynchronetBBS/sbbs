/*
 * Unit tests for src/u8cp437.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * u8cp437.c uses FileName() from the platform layer, so we include
 * platform.c first (it pulls in unix_wrappers.c which defines FileName()).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    /* mkdtemp, chdir, rmdir, unlink */

#include "../defines.h"
#include "../platform.h"

#include "test_harness.h"

#include "../platform.c"

/* External variable definitions required by defines.h */
int             _argc           = 0;
static char    *_argv_buf[]     = {NULL};
char          **_argv           = _argv_buf;

#include "../u8cp437.c"

/* -------------------------------------------------------------------------
 * Test temp directory for u8_fopen() probe tests
 * ------------------------------------------------------------------------- */
static char g_tmpdir[256];
static char g_origdir[256];

static void setup_tmpdir(void)
{
	if (!getcwd(g_origdir, sizeof(g_origdir))) {
		perror("getcwd");
		exit(1);
	}
	strlcpy(g_tmpdir, "/tmp/test_u8_XXXXXX", sizeof(g_tmpdir));
	if (!mkdtemp(g_tmpdir)) {
		perror("mkdtemp");
		exit(1);
	}
	if (chdir(g_tmpdir) != 0) {
		perror("chdir");
		exit(1);
	}
}

static void teardown_tmpdir(void)
{
	if (chdir(g_origdir) != 0)
		perror("chdir back");

	/* Clean up any files we created */
	char path[512];
	snprintf(path, sizeof(path), "%s/test.txt", g_tmpdir);
	unlink(path);
	snprintf(path, sizeof(path), "%s/test.u8.txt", g_tmpdir);
	unlink(path);
	snprintf(path, sizeof(path), "%s/noext", g_tmpdir);
	unlink(path);
	rmdir(g_tmpdir);
}

/* -------------------------------------------------------------------------
 * Helper: create a file with given binary content
 * ------------------------------------------------------------------------- */
static void create_file(const char *name, const unsigned char *data, size_t len)
{
	FILE *fp = fopen(name, "wb");
	if (!fp) { perror(name); exit(1); }
	fwrite(data, 1, len, fp);
	fclose(fp);
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- ASCII passthrough
 * ------------------------------------------------------------------------- */
static void test_ascii_passthrough(void)
{
	unsigned char buf[] = "Hello, world!\n";
	unsigned char expected[] = "Hello, world!\n";
	size_t len = strlen((char *)buf);

	ASSERT_EQ(u8_to_cp437(buf, len, "test", 1), 0);
	ASSERT_EQ(memcmp(buf, expected, len), 0);
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- high-byte canonical mappings
 * ------------------------------------------------------------------------- */
static void test_highbyte_box_drawing(void)
{
	/* UTF-8 for U+2500 (BOX DRAWINGS LIGHT HORIZONTAL) = E2 94 80
	 * Should map to CP437 0xC4 */
	unsigned char buf[] = { 0xE2, 0x94, 0x80, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0xC4);
	ASSERT_EQ(buf[1], '\0');
}

static void test_highbyte_full_block(void)
{
	/* UTF-8 for U+2588 (FULL BLOCK) = E2 96 88
	 * Should map to CP437 0xDB */
	unsigned char buf[] = { 0xE2, 0x96, 0x88, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0xDB);
}

static void test_highbyte_accented(void)
{
	/* UTF-8 for U+00E9 (LATIN SMALL LETTER E WITH ACUTE) = C3 A9
	 * Should map to CP437 0x82 */
	unsigned char buf[] = { 0xC3, 0xA9, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 2, "test", 1), 0);
	ASSERT_EQ(buf[0], 0x82);
}

static void test_highbyte_nobreak_space(void)
{
	/* UTF-8 for U+00A0 (NO-BREAK SPACE) = C2 A0
	 * Should map to CP437 0xFF */
	unsigned char buf[] = { 0xC2, 0xA0, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 2, "test", 1), 0);
	ASSERT_EQ(buf[0], 0xFF);
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- control-character glyphs
 * ------------------------------------------------------------------------- */
static void test_ctrl_smiley(void)
{
	/* UTF-8 for U+263A (WHITE SMILING FACE) = E2 98 BA
	 * Should map to CP437 0x01 */
	unsigned char buf[] = { 0xE2, 0x98, 0xBA, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0x01);
}

static void test_ctrl_heart(void)
{
	/* UTF-8 for U+2665 (BLACK HEART SUIT) = E2 99 A5
	 * Should map to CP437 0x03 */
	unsigned char buf[] = { 0xE2, 0x99, 0xA5, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0x03);
}

static void test_ctrl_house(void)
{
	/* UTF-8 for U+2302 (HOUSE) = E2 8C 82
	 * Should map to CP437 0x7F */
	unsigned char buf[] = { 0xE2, 0x8C, 0x82, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0x7F);
}

static void test_ctrl_music(void)
{
	/* UTF-8 for U+266A (EIGHTH NOTE) = E2 99 AA
	 * Should map to CP437 0x0D */
	unsigned char buf[] = { 0xE2, 0x99, 0xAA, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0x0D);
}

static void test_ctrl_arrow(void)
{
	/* UTF-8 for U+2192 (RIGHTWARDS ARROW) = E2 86 92
	 * Should map to CP437 0x1A */
	unsigned char buf[] = { 0xE2, 0x86, 0x92, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0x1A);
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- alternative codepoints
 * ------------------------------------------------------------------------- */
static void test_alt_beta(void)
{
	/* UTF-8 for U+03B2 (GREEK SMALL LETTER BETA) = CE B2
	 * Should map to CP437 0xE1 (same as sharp-s) */
	unsigned char buf[] = { 0xCE, 0xB2, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 2, "test", 1), 0);
	ASSERT_EQ(buf[0], 0xE1);
}

static void test_alt_mu(void)
{
	/* UTF-8 for U+03BC (GREEK SMALL LETTER MU) = CE BC
	 * Should map to CP437 0xE6 (same as micro sign) */
	unsigned char buf[] = { 0xCE, 0xBC, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 2, "test", 1), 0);
	ASSERT_EQ(buf[0], 0xE6);
}

static void test_alt_ohm(void)
{
	/* UTF-8 for U+2126 (OHM SIGN) = E2 84 A6
	 * Should map to CP437 0xEA (same as omega) */
	unsigned char buf[] = { 0xE2, 0x84, 0xA6, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0xEA);
}

static void test_alt_empty_set(void)
{
	/* UTF-8 for U+2205 (EMPTY SET) = E2 88 85
	 * Should map to CP437 0xED (same as phi) */
	unsigned char buf[] = { 0xE2, 0x88, 0x85, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), 0);
	ASSERT_EQ(buf[0], 0xED);
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- blocked codepoints
 * ------------------------------------------------------------------------- */
static void test_blocked_smart_quote_left(void)
{
	/* UTF-8 for U+2018 (LEFT SINGLE QUOTATION MARK) = E2 80 98 */
	unsigned char buf[] = { 0xE2, 0x80, 0x98, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), -1);
}

static void test_blocked_smart_quote_right(void)
{
	/* UTF-8 for U+2019 (RIGHT SINGLE QUOTATION MARK) = E2 80 99 */
	unsigned char buf[] = { 0xE2, 0x80, 0x99, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), -1);
}

static void test_blocked_em_dash(void)
{
	/* UTF-8 for U+2014 (EM DASH) = E2 80 94 */
	unsigned char buf[] = { 0xE2, 0x80, 0x94, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), -1);
}

static void test_blocked_en_dash(void)
{
	/* UTF-8 for U+2013 (EN DASH) = E2 80 93 */
	unsigned char buf[] = { 0xE2, 0x80, 0x93, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), -1);
}

static void test_blocked_double_quote_left(void)
{
	/* UTF-8 for U+201C (LEFT DOUBLE QUOTATION MARK) = E2 80 9C */
	unsigned char buf[] = { 0xE2, 0x80, 0x9C, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), -1);
}

static void test_blocked_double_quote_right(void)
{
	/* UTF-8 for U+201D (RIGHT DOUBLE QUOTATION MARK) = E2 80 9D */
	unsigned char buf[] = { 0xE2, 0x80, 0x9D, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), -1);
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- unmapped codepoints
 * ------------------------------------------------------------------------- */
static void test_unmapped_codepoint(void)
{
	/* UTF-8 for U+4E16 (CJK UNIFIED IDEOGRAPH) = E4 B8 96 */
	unsigned char buf[] = { 0xE4, 0xB8, 0x96, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 3, "test", 1), -1);
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- invalid UTF-8
 * ------------------------------------------------------------------------- */
static void test_invalid_continuation(void)
{
	/* 0x80 is a bare continuation byte */
	unsigned char buf[] = { 0x80, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 1, "test", 1), -1);
}

static void test_invalid_truncated(void)
{
	/* E2 without continuation bytes */
	unsigned char buf[] = { 0xE2, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 1, "test", 1), -1);
}

static void test_invalid_overlong(void)
{
	/* Overlong encoding of '/' (U+002F): C0 AF */
	unsigned char buf[] = { 0xC0, 0xAF, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 2, "test", 1), -1);
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- mixed content
 * ------------------------------------------------------------------------- */
static void test_mixed_ascii_and_utf8(void)
{
	/* "A" + U+2500 (box horiz) + "B" */
	unsigned char buf[] = { 'A', 0xE2, 0x94, 0x80, 'B', 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 5, "test", 1), 0);
	ASSERT_EQ(buf[0], 'A');
	ASSERT_EQ(buf[1], 0xC4);
	ASSERT_EQ(buf[2], 'B');
	ASSERT_EQ(buf[3], '\0');
}

static void test_multiple_utf8_sequences(void)
{
	/* U+2554 (double down-right) + U+2550 (double horiz) + U+2557 (double down-left) */
	unsigned char buf[] = {
		0xE2, 0x95, 0x94,   /* U+2554 -> 0xC9 */
		0xE2, 0x95, 0x90,   /* U+2550 -> 0xCD */
		0xE2, 0x95, 0x97,   /* U+2557 -> 0xBB */
		0x00
	};
	ASSERT_EQ(u8_to_cp437(buf, 9, "test", 1), 0);
	ASSERT_EQ(buf[0], 0xC9);
	ASSERT_EQ(buf[1], 0xCD);
	ASSERT_EQ(buf[2], 0xBB);
	ASSERT_EQ(buf[3], '\0');
}

/* -------------------------------------------------------------------------
 * Tests: u8_to_cp437 -- raw control bytes pass through
 * ------------------------------------------------------------------------- */
static void test_raw_control_passthrough(void)
{
	/* Raw 0x09 (tab) and 0x0A (newline) should pass through */
	unsigned char buf[] = { 'A', 0x09, 'B', 0x0A, 0x00 };
	unsigned char expected[] = { 'A', 0x09, 'B', 0x0A, 0x00 };
	ASSERT_EQ(u8_to_cp437(buf, 4, "test", 1), 0);
	ASSERT_EQ(memcmp(buf, expected, 5), 0);
}

/* -------------------------------------------------------------------------
 * Tests: u8_fgets -- non-UTF8 mode (passthrough)
 * ------------------------------------------------------------------------- */
static void test_fgets_passthrough(void)
{
	/* Create a file with CP437 content */
	unsigned char data[] = { 'H', 'i', 0xC4, 0xC4, '\n', 0x00 };
	create_file("test.txt", data, 5);

	FILE *fp = fopen("test.txt", "r");
	char buf[256];
	int lineno = 0;
	char *result = u8_fgets(buf, sizeof(buf), fp, 0, "test.txt", &lineno);

	ASSERT_EQ(result != NULL, 1);
	ASSERT_EQ(lineno, 1);
	ASSERT_EQ((unsigned char)buf[0], 'H');
	ASSERT_EQ((unsigned char)buf[1], 'i');
	ASSERT_EQ((unsigned char)buf[2], 0xC4);
	ASSERT_EQ((unsigned char)buf[3], 0xC4);
	fclose(fp);
}

/* -------------------------------------------------------------------------
 * Tests: u8_fgets -- UTF8 mode (conversion)
 * ------------------------------------------------------------------------- */
static void test_fgets_utf8_conversion(void)
{
	/* UTF-8: "Hi" + U+2500 (E2 94 80) + U+2500 (E2 94 80) + "\n" */
	unsigned char data[] = { 'H', 'i', 0xE2, 0x94, 0x80,
	                         0xE2, 0x94, 0x80, '\n' };
	create_file("test.txt", data, sizeof(data));

	FILE *fp = fopen("test.txt", "r");
	char buf[256];
	int lineno = 0;
	char *result = u8_fgets(buf, sizeof(buf), fp, 1, "test.txt", &lineno);

	ASSERT_EQ(result != NULL, 1);
	ASSERT_EQ(lineno, 1);
	ASSERT_EQ((unsigned char)buf[0], 'H');
	ASSERT_EQ((unsigned char)buf[1], 'i');
	ASSERT_EQ((unsigned char)buf[2], 0xC4);
	ASSERT_EQ((unsigned char)buf[3], 0xC4);
	ASSERT_EQ(buf[4], '\n');
	fclose(fp);
}

/* -------------------------------------------------------------------------
 * Tests: u8_fgets -- UTF8 mode error
 * ------------------------------------------------------------------------- */
static void test_fgets_utf8_error(void)
{
	/* UTF-8 for U+2018 (blocked smart quote) */
	unsigned char data[] = { 'A', 0xE2, 0x80, 0x98, '\n' };
	create_file("test.txt", data, sizeof(data));

	FILE *fp = fopen("test.txt", "r");
	char buf[256];
	int lineno = 0;
	char *result = u8_fgets(buf, sizeof(buf), fp, 1, "test.txt", &lineno);

	ASSERT_EQ(result == NULL, 1);
	fclose(fp);
}

/* -------------------------------------------------------------------------
 * Tests: u8_fopen -- probe logic
 * ------------------------------------------------------------------------- */
static void test_fopen_plain(void)
{
	/* Plain .txt name -> CP437 mode */
	unsigned char data[] = { 'H', 'i', '\n' };
	create_file("test.txt", data, sizeof(data));

	int is_utf8 = -1;
	FILE *fp = u8_fopen("test.txt", "r", &is_utf8);
	ASSERT_EQ(fp != NULL, 1);
	ASSERT_EQ(is_utf8, 0);
	fclose(fp);
	unlink("test.txt");
}

static void test_fopen_u8_name(void)
{
	/* .u8.txt name -> UTF-8 mode; read + convert */
	unsigned char data_utf8[] = { 0xE2, 0x94, 0x80, '\n' };
	create_file("test.u8.txt", data_utf8, sizeof(data_utf8));

	int is_utf8 = -1;
	FILE *fp = u8_fopen("test.u8.txt", "r", &is_utf8);
	ASSERT_EQ(fp != NULL, 1);
	ASSERT_EQ(is_utf8, 1);

	/* Read and convert -- should get 0xC4 */
	char buf[256];
	int lineno = 0;
	u8_fgets(buf, sizeof(buf), fp, is_utf8, "test.u8.txt", &lineno);
	ASSERT_EQ((unsigned char)buf[0], 0xC4);
	fclose(fp);
	unlink("test.u8.txt");
}

static void test_fopen_no_extension(void)
{
	/* File with no extension -- CP437 mode */
	unsigned char data[] = { 'X', '\n' };
	create_file("noext", data, sizeof(data));

	int is_utf8 = -1;
	FILE *fp = u8_fopen("noext", "r", &is_utf8);
	ASSERT_EQ(fp != NULL, 1);
	ASSERT_EQ(is_utf8, 0);
	fclose(fp);
	unlink("noext");
}

static void test_fopen_nonexistent(void)
{
	/* File does not exist */
	int is_utf8 = -1;
	FILE *fp = u8_fopen("nonexistent.txt", "r", &is_utf8);
	ASSERT_EQ(fp == NULL, 1);
	ASSERT_EQ(is_utf8, 0);
}

static void test_fopen_u8_nonexistent(void)
{
	/* .u8. file does not exist -- is_utf8 set but fopen returns NULL */
	int is_utf8 = -1;
	FILE *fp = u8_fopen("nonexistent.u8.txt", "r", &is_utf8);
	ASSERT_EQ(fp == NULL, 1);
	ASSERT_EQ(is_utf8, 1);
}

static void test_fopen_u8_in_directory(void)
{
	/* .u8. in a directory component must NOT trigger UTF-8 mode */
	int is_utf8 = -1;
	FILE *fp = u8_fopen("attack_dir.u8./barefilename", "r", &is_utf8);
	ASSERT_EQ(fp == NULL, 1);  /* file doesn't exist, that's fine */
	ASSERT_EQ(is_utf8, 0);

	/* Same with an extension on the basename */
	is_utf8 = -1;
	fp = u8_fopen("attack_dir.u8./file.txt", "r", &is_utf8);
	ASSERT_EQ(fp == NULL, 1);
	ASSERT_EQ(is_utf8, 0);
}

/* -------------------------------------------------------------------------
 * Tests: reverse table completeness -- all 128 high bytes covered
 * ------------------------------------------------------------------------- */
static void test_all_highbytes_mapped(void)
{
	/* For each CP437 byte 0x80-0xFF, verify at least one table entry
	 * maps to it.  We check by scanning the table. */
	int found[256] = {0};
	for (size_t i = 0; i < REVERSE_TABLE_SIZE; i++)
		found[reverse_table[i].cp437] = 1;

	int missing = 0;
	for (int b = 0x80; b <= 0xFF; b++) {
		if (!found[b]) {
			fprintf(stderr, "  Missing CP437 0x%02X in reverse table\n", b);
			missing++;
		}
	}
	ASSERT_EQ(missing, 0);
}

/* -------------------------------------------------------------------------
 * Tests: reverse table completeness -- all 32 ctrl glyphs covered
 * ------------------------------------------------------------------------- */
static void test_all_ctrl_glyphs_mapped(void)
{
	int found[256] = {0};
	for (size_t i = 0; i < REVERSE_TABLE_SIZE; i++)
		found[reverse_table[i].cp437] = 1;

	int missing = 0;
	for (int b = 0x01; b <= 0x1F; b++) {
		if (!found[b]) {
			fprintf(stderr, "  Missing CP437 0x%02X in reverse table\n", b);
			missing++;
		}
	}
	/* 0x7F (DEL/house) */
	if (!found[0x7F]) {
		fprintf(stderr, "  Missing CP437 0x7F in reverse table\n");
		missing++;
	}
	ASSERT_EQ(missing, 0);
}

/* -------------------------------------------------------------------------
 * Tests: reverse table is sorted (binary search correctness)
 * ------------------------------------------------------------------------- */
static void test_table_sorted(void)
{
	for (size_t i = 1; i < REVERSE_TABLE_SIZE; i++) {
		if (reverse_table[i].codepoint <= reverse_table[i-1].codepoint) {
			fprintf(stderr, "  Table not sorted at index %zu: "
			        "U+%04X <= U+%04X\n", i,
			        reverse_table[i].codepoint,
			        reverse_table[i-1].codepoint);
			ASSERT_EQ(0, 1);  /* force fail */
			return;
		}
	}
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	/* Redirect stderr to /dev/null during tests (error messages expected) */
	FILE *devnull = fopen("/dev/null", "w");
	if (devnull)
		stderr = devnull;

	/* ASCII passthrough */
	RUN(ascii_passthrough);

	/* High-byte canonical */
	RUN(highbyte_box_drawing);
	RUN(highbyte_full_block);
	RUN(highbyte_accented);
	RUN(highbyte_nobreak_space);

	/* Control-character glyphs */
	RUN(ctrl_smiley);
	RUN(ctrl_heart);
	RUN(ctrl_house);
	RUN(ctrl_music);
	RUN(ctrl_arrow);

	/* Alternative codepoints */
	RUN(alt_beta);
	RUN(alt_mu);
	RUN(alt_ohm);
	RUN(alt_empty_set);

	/* Blocked codepoints */
	RUN(blocked_smart_quote_left);
	RUN(blocked_smart_quote_right);
	RUN(blocked_em_dash);
	RUN(blocked_en_dash);
	RUN(blocked_double_quote_left);
	RUN(blocked_double_quote_right);

	/* Unmapped / invalid */
	RUN(unmapped_codepoint);
	RUN(invalid_continuation);
	RUN(invalid_truncated);
	RUN(invalid_overlong);

	/* Mixed content */
	RUN(mixed_ascii_and_utf8);
	RUN(multiple_utf8_sequences);
	RUN(raw_control_passthrough);

	/* File I/O tests need tmpdir */
	setup_tmpdir();

	/* u8_fgets */
	RUN(fgets_passthrough);
	RUN(fgets_utf8_conversion);
	RUN(fgets_utf8_error);

	/* u8_fopen */
	RUN(fopen_plain);
	RUN(fopen_u8_name);
	RUN(fopen_no_extension);
	RUN(fopen_nonexistent);
	RUN(fopen_u8_nonexistent);
	RUN(fopen_u8_in_directory);

	/* Table completeness */
	RUN(all_highbytes_mapped);
	RUN(all_ctrl_glyphs_mapped);
	RUN(table_sorted);

	teardown_tmpdir();

	if (devnull) {
		stderr = __stderrp;
		fclose(devnull);
	}

	printf("\n%d/%d tests passed\n",
	       g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
