/* test_statekey.c -- the snapshot staleness key. The lobby computes it to
 * decide which cartridges show as suspended, and hands it to the door on the
 * command line (`-state <key8>`) to name the file; the door never computes
 * one itself. If the two ever disagree the lobby marks games the door then
 * refuses to resume, and nothing reports an error -- so both halves pin the
 * same golden value here and in exec/tests/syncretro_state_test.js.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro_statekey.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_STR(got, want) \
		do { \
			const char *g_ = (got); \
			if (g_ == NULL || strcmp(g_, (want)) != 0) { \
				printf("FAIL %s:%d: got \"%s\", want \"%s\"\n", \
					   __FILE__, __LINE__, g_ ? g_ : "(null)", (want)); \
				failures++; \
			} \
		} while (0)

/* Fixed inputs. THE SAME THREE STRINGS AND THE SAME EXPECTED KEY appear in
 * exec/tests/syncretro_state_test.js -- change one, change both. */
#define CORE_MD5 "0123456789abcdef0123456789abcdef"
#define ROM_MD5  "fedcba9876543210fedcba9876543210"
#define OPTS     "mame2003-plus_skip_disclaimer=enabled\n" \
		"mame2003-plus_skip_warnings=enabled"

static void test_golden(void)
{
	char key[9];

	sr_state_key(key, CORE_MD5, ROM_MD5, OPTS);
	/* Golden: pins the CORE_MD5/ROM_MD5/OPTS recipe above to this exact
	 * output. exec/tests/syncretro_state_test.js asserts the identical
	 * constant against the JS implementation of the same recipe. Never adjust
	 * this value to match a changed implementation -- if it moves, the recipe
	 * changed and every existing snapshot on every install just became
	 * unreachable. */
	CHECK_STR(key, "38ed1f37");
}

/* Each input participates: change any one and the key must move. */
static void test_inputs_matter(void)
{
	char base[9], other[9];

	sr_state_key(base, CORE_MD5, ROM_MD5, OPTS);

	sr_state_key(other, "ffffffffffffffffffffffffffffffff", ROM_MD5, OPTS);
	if (strcmp(base, other) == 0) {
		printf("FAIL %s:%d: core hash does not affect the key\n",
		       __FILE__, __LINE__);
		failures++;
	}
	sr_state_key(other, CORE_MD5, "ffffffffffffffffffffffffffffffff", OPTS);
	if (strcmp(base, other) == 0) {
		printf("FAIL %s:%d: rom hash does not affect the key\n",
		       __FILE__, __LINE__);
		failures++;
	}
	sr_state_key(other, CORE_MD5, ROM_MD5, "");
	if (strcmp(base, other) == 0) {
		printf("FAIL %s:%d: options do not affect the key\n",
		       __FILE__, __LINE__);
		failures++;
	}
}

/* Shape: 8 lowercase hex digits, NUL-terminated. The filename depends on it. */
static void test_shape(void)
{
	char key[9];
	int  i;

	sr_state_key(key, CORE_MD5, ROM_MD5, "");
	if (strlen(key) != 8) {
		printf("FAIL %s:%d: length %u, want 8\n", __FILE__, __LINE__,
		       (unsigned)strlen(key));
		failures++;
	}
	for (i = 0; key[i] != '\0'; i++) {
		if (!((key[i] >= '0' && key[i] <= '9')
		      || (key[i] >= 'a' && key[i] <= 'f'))) {
			printf("FAIL %s:%d: '%c' is not lowercase hex\n",
			       __FILE__, __LINE__, key[i]);
			failures++;
			break;
		}
	}
}

int main(void)
{
	test_golden();
	test_inputs_matter();
	test_shape();
	printf("%s: %d failure(s)\n", failures ? "FAILED" : "ok", failures);
	return failures != 0;
}
