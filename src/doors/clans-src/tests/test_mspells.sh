#!/bin/sh
#
# Integration tests for mspells.
# Input:  spell definition file (.txt)
# Output: compiled spell binary (.spl)

cd "$(dirname "$0")"
TOOL="${BINDIR}mspells${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/mspells"
T="${CLANS_TMPDIR}/mspells"
mkdir -p "$T"

# --- happy path ---
assert_succeeds  "valid input exits zero" \
	"$TOOL" "$F/valid.txt" "$T/out.spl"
assert_nonempty  "valid input produces .spl file" \
	"$T/out.spl"

# --- all flag types, all string fields, NoTarget, Friendly, MultiAffect, etc. ---
assert_succeeds  "allkinds input exits zero" \
	"$TOOL" "$F/allkinds.txt" "$T/allkinds.spl"
assert_nonempty  "allkinds input produces .spl file" \
	"$T/allkinds.spl"
assert_file_larger  "allkinds output larger than single-spell output" \
	"$T/allkinds.spl" "$T/out.spl"

# --- error: missing input file ---
assert_fails     "missing input exits nonzero" \
	"$TOOL" "$T/nonexistent.txt" "$T/err.spl"

finish
