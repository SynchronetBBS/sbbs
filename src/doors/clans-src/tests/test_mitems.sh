#!/bin/sh
#
# Integration tests for mitems.
# Input:  item definition file (.txt)
# Output: compiled item binary (.itm)

cd "$(dirname "$0")"
TOOL="${BINDIR}mitems${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/mitems"
T="${CLANS_TMPDIR}/mitems"
mkdir -p "$T"

# --- happy path ---
assert_succeeds  "valid input exits zero" \
	"$TOOL" "$F/valid.txt" "$T/out.itm"
assert_nonempty  "valid input produces .itm file" \
	"$T/out.itm"

# --- all item types: Armor, Shield, Scroll, Book, Other, Weapon;
#     Stats/Requirements/Penalties sections; HPAdd, SPAdd, Special, etc. ---
assert_succeeds  "allkinds input exits zero" \
	"$TOOL" "$F/allkinds.txt" "$T/allkinds.itm"
assert_nonempty  "allkinds input produces .itm file" \
	"$T/allkinds.itm"
assert_file_larger  "allkinds output larger than single-item output" \
	"$T/allkinds.itm" "$T/out.itm"

# --- error: missing input file ---
assert_fails     "missing input exits nonzero" \
	"$TOOL" "$T/nonexistent.txt" "$T/err.itm"

finish
