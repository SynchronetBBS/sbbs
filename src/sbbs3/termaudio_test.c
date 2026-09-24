/* Standalone unit tests for termaudio.c
 *
 * Build and run:
 *   gcc -Wall -Wextra -o /tmp/termaudio_test src/sbbs3/termaudio_test.c -lm
 *   /tmp/termaudio_test
 *
 * The implementation is pulled in as a translation-unit include, the same way
 * boolsrch_test.c does it, so this builds as a single source file with no link
 * against the rest of the tree. Do NOT also pass termaudio.c on the command
 * line, and do NOT add this file to objects.mk or the MSVC projects.
 */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 ****************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "termaudio.c"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, fmt, ...)                                                 \
		do {                                                                      \
			tests_run++;                                                          \
			if (!(cond)) {                                                        \
				tests_failed++;                                                   \
				fprintf(stderr, "FAIL: " fmt "\n", ## __VA_ARGS__);               \
			}                                                                     \
		} while (0)

static bool near(float a, float b)
{
	return fabsf(a - b) < 0.05f;
}

int main(void)
{
	char  path[512];
	char  name[TERMAUDIO_MAX_CACHE_NAME];
	float db;

	/* --- percent curve --- */
	CHECK(near(termaudio_db_from_pct(100), 0.0f), "100 pct is unity");
	CHECK(near(termaudio_db_from_pct(50), -6.02f), "50 pct is about -6 dB");
	CHECK(near(termaudio_db_from_pct(0), TERMAUDIO_DB_MUTE), "0 pct is mute");
	CHECK(near(termaudio_db_from_pct(-5), TERMAUDIO_DB_MUTE), "negative is mute");

	/* --- volume parsing, good input --- */
	CHECK(termaudio_parse_volume("50", &db) && near(db, -6.02f), "bare 50");
	CHECK(termaudio_parse_volume("100", &db) && near(db, 0.0f), "bare 100");
	CHECK(termaudio_parse_volume("-6dB", &db) && near(db, -6.0f), "-6dB");
	CHECK(termaudio_parse_volume("-6db", &db) && near(db, -6.0f), "lowercase db");
	CHECK(termaudio_parse_volume("+3dB", &db) && near(db, 3.0f), "boost above unity");

	/* --- Review Focus 3: malformed volume --- */
	CHECK(!termaudio_parse_volume("abc", &db), "non-numeric rejected");
	CHECK(!termaudio_parse_volume("", &db), "empty rejected");
	CHECK(!termaudio_parse_volume(NULL, &db), "NULL rejected");
	CHECK(!termaudio_parse_volume("dB", &db), "suffix with no number rejected");
	CHECK(termaudio_parse_volume("-999dB", &db) && near(db, TERMAUDIO_DB_MUTE),
	      "absurdly low dB clamps to mute, not -999");
	CHECK(termaudio_parse_volume("999dB", &db) && near(db, TERMAUDIO_DB_MAX),
	      "absurdly high dB clamps, not +999");
	CHECK(termaudio_parse_volume("99999", &db) && near(db, 0.0f),
	      "percent above 100 clamps to unity");
	CHECK(!termaudio_parse_volume("1e400", &db), "overflow rejected, not inf");
	CHECK(!termaudio_parse_volume("12abc", &db), "trailing garbage rejected");
	db = 1234.0f;
	(void)termaudio_parse_volume("abc", &db);
	CHECK(near(db, 1234.0f), "rejected parse leaves *db untouched");

	/* --- path resolution, good input --- */
	CHECK(termaudio_resolve_path("/sbbs/text/", "beep.wav", false, path, sizeof(path))
	      && strcmp(path, "/sbbs/text/beep.wav") == 0, "bare filename");
	CHECK(termaudio_resolve_path("/sbbs/text/", "sound/theme.ogg", true, path, sizeof(path))
	      && strcmp(path, "/sbbs/text/sound/theme.ogg") == 0, "subdir allowed");

	/* --- Review Focus 4: escaping paths --- */
	CHECK(!termaudio_resolve_path("/sbbs/text/", "../../ctrl/sbbs.ini", true, path, sizeof(path)),
	      "dot-dot rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "/etc/passwd", true, path, sizeof(path)),
	      "absolute rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "sound\\..\\x", true, path, sizeof(path)),
	      "backslash rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "sound/x.wav", false, path, sizeof(path)),
	      "subdir rejected when not allowed");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "", true, path, sizeof(path)),
	      "empty rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "a/../b.wav", true, path, sizeof(path)),
	      "embedded dot-dot rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "/x.wav", true, path, sizeof(path)),
	      "leading slash rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "a//b.wav", true, path, sizeof(path)),
	      "doubled separator rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "a/", true, path, sizeof(path)),
	      "trailing separator rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "c:x.wav", true, path, sizeof(path)),
	      "drive letter rejected");
	CHECK(!termaudio_resolve_path("/sbbs/text/", "x.wav", true, path, 8),
	      "too-small output buffer rejected, not truncated");

	/* --- cache naming --- */
	{
		uint8_t md5[16];
		int     i;
		for (i = 0; i < 16; i++)
			md5[i] = (uint8_t)i;
		termaudio_cache_name(md5, "wav", name, sizeof(name));
		CHECK(strcmp(name, "sbbs_000102030405060708090a0b0c0d0e0f.wav") == 0,
		      "hex name, got '%s'", name);
		termaudio_cache_name(md5, NULL, name, sizeof(name));
		CHECK(strcmp(name, "sbbs_000102030405060708090a0b0c0d0e0f") == 0,
		      "NULL ext omits the dot, got '%s'", name);
		termaudio_cache_name(md5, "", name, sizeof(name));
		CHECK(strcmp(name, "sbbs_000102030405060708090a0b0c0d0e0f") == 0,
		      "empty ext omits the dot, got '%s'", name);
		termaudio_cache_name(md5, "verylongextension", name, sizeof(name));
		CHECK(strlen(name) < TERMAUDIO_MAX_CACHE_NAME, "long ext truncated");
		/* Different content must give a different name: that property is what
		   makes an edited file re-upload instead of playing a stale copy. */
		{
			char other[TERMAUDIO_MAX_CACHE_NAME];
			md5[0] = 0xff;
			termaudio_cache_name(md5, "wav", other, sizeof(other));
			CHECK(strcmp(other, "sbbs_000102030405060708090a0b0c0d0e0f.wav") != 0,
			      "different digest gives a different name");
		}
	}

	/* --- audio state token --- */
	CHECK(termaudio_parse_audio_state("[=7;100;1n") == 1, "libsndfile present");
	CHECK(termaudio_parse_audio_state("[=7;100;0n") == 0, "libsndfile absent");
	CHECK(termaudio_parse_audio_state("[=67;84;101;114;109;1;331c") == -1,
	      "CTerm DA is not an audio report");
	CHECK(termaudio_parse_audio_state("[24;80R") == -1, "CPR is not an audio report");
	CHECK(termaudio_parse_audio_state("") == -1, "empty token");
	CHECK(termaudio_parse_audio_state(NULL) == -1, "NULL token");
	CHECK(termaudio_parse_audio_state("[=7;100;1") == -1, "missing final byte");
	CHECK(termaudio_parse_audio_state("[=7;101;1n") == -1, "wrong feature id");
	CHECK(termaudio_parse_audio_state("[=8;100;1n") == -1, "wrong report id");

	/* --- Review Focus 5: C;L reply parsing --- */
	{
		char   names[4][TERMAUDIO_MAX_CACHE_NAME];
		size_t n;

		n = termaudio_parse_file_list("SyncTERM:C;L\nsbbs_aa.wav\tdeadbeef\n"
		                              "sbbs_bb.ogg\tcafebabe\n", names, 4);
		CHECK(n == 2, "two names parsed, got %u", (unsigned)n);
		CHECK(strcmp(names[0], "sbbs_aa.wav") == 0, "first name");
		CHECK(strcmp(names[1], "sbbs_bb.ogg") == 0, "second name");

		n = termaudio_parse_file_list("SyncTERM:C;L\n", names, 4);
		CHECK(n == 0, "header-only body yields nothing");
		n = termaudio_parse_file_list("", names, 4);
		CHECK(n == 0, "empty body yields nothing");
		n = termaudio_parse_file_list(NULL, names, 4);
		CHECK(n == 0, "NULL body yields nothing");
		n = termaudio_parse_file_list("SyncTERM:C;L\nnotabhere\n", names, 4);
		CHECK(n == 0, "line with no tab is skipped, not accepted");
		n = termaudio_parse_file_list("SyncTERM:C;L\n\tnoname\n", names, 4);
		CHECK(n == 0, "empty name before the tab is skipped");
		n = termaudio_parse_file_list("SyncTERM:C;L\nsbbs_a.wav\tdead", names, 4);
		CHECK(n == 1, "final line without newline still parsed");
		n = termaudio_parse_file_list("SyncTERM:C;L\na\t1\nb\t2\nc\t3\nd\t4\ne\t5\n",
		                              names, 4);
		CHECK(n == 4, "stops at maxnames without overrunning, got %u", (unsigned)n);
	}

	printf("%d tests run, %d failed\n", tests_run, tests_failed);
	return tests_failed == 0 ? 0 : 1;
}
