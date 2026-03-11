#!/bin/sh
#
# Integration tests for langcomp.
# Input:  strings.u8.txt  (NNNN [ST_MACRO ]text lines)
# Output: strings.xl   (binary language file)
#         mstrings.h   (optional generated C header)

cd "$(dirname "$0")"
TOOL="${BINDIR}langcomp${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/langcomp"
T="${CLANS_TMPDIR}/langcomp"
mkdir -p "$T"

# --- happy path: two-output form ---
assert_succeeds  "valid input exits zero" \
	"$TOOL" "$F/valid.txt" "$T/out.xl"
assert_nonempty  "valid input produces .xl" \
	"$T/out.xl"

# --- happy path: three-arg form generates mstrings.h ---
assert_succeeds  "three-arg form exits zero" \
	"$TOOL" "$F/valid.txt" "$T/out2.xl" "$T/mstrings.h"
assert_nonempty  "three-arg form produces .xl" \
	"$T/out2.xl"
assert_nonempty  "three-arg form produces mstrings.h" \
	"$T/mstrings.h"
assert_contains  "mstrings.h contains ST_TEST_A" \
	"$T/mstrings.h" "ST_TEST_A"
assert_contains  "mstrings.h contains ST_TEST_B" \
	"$T/mstrings.h" "ST_TEST_B"

# --- comments, @@ embedded newlines, escape sequences ---
assert_succeeds  "comments_and_escape exits zero" \
	"$TOOL" "$F/comments_and_escape.txt" "$T/ce.xl" "$T/ce_mstrings.h"
assert_nonempty  "comments_and_escape produces .xl" \
	"$T/ce.xl"
assert_contains  "ce mstrings.h has ST_FIRST" \
	"$T/ce_mstrings.h" "ST_FIRST"
assert_contains  "ce mstrings.h has ST_BELL" \
	"$T/ce_mstrings.h" "ST_BELL"
assert_contains  "ce mstrings.h has ST_PLAIN" \
	"$T/ce_mstrings.h" "ST_PLAIN"

# --- ST_ macro name parsing edge cases ---
assert_succeeds  "st_edge_cases exits zero" \
	"$TOOL" "$F/st_edge_cases.txt" "$T/se.xl" "$T/se_mstrings.h"
assert_contains  "se mstrings.h has ST_VALID_1" \
	"$T/se_mstrings.h" "ST_VALID_1"
assert_contains  "se mstrings.h has ST_VALID_2" \
	"$T/se_mstrings.h" "ST_VALID_2"

# --- error: entry without a valid ST_ macro name ---
assert_fails     "missing ST_ macro exits nonzero" \
	"$TOOL" "$F/no_macro.txt" "$T/no_macro.xl"

# --- error: missing input file ---
assert_fails     "missing input exits nonzero" \
	"$TOOL" "$T/nonexistent.txt" "$T/err.xl"

finish
