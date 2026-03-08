#!/bin/sh
#
# Integration tests for mcomp.
# Input:  monster definition file (.txt)
# Output: compiled monster binary (.mon)

cd "$(dirname "$0")"
TOOL="${BINDIR}mcomp${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/mcomp"
T="${CLANS_TMPDIR}/mcomp"
mkdir -p "$T"

# --- happy path ---
assert_succeeds  "valid input exits zero" \
	"$TOOL" "$F/valid.txt" "$T/out.mon"
assert_nonempty  "valid input produces .mon file" \
	"$T/out.mon"

# --- multi-monster: exercises flush-on-next-Name + SP/Spell/Shield/Armor/Undead ---
assert_succeeds  "multi-monster input exits zero" \
	"$TOOL" "$F/multi.txt" "$T/multi.mon"
assert_nonempty  "multi-monster input produces .mon file" \
	"$T/multi.mon"
assert_file_larger  "multi-monster output larger than single-monster output" \
	"$T/multi.mon" "$T/out.mon"

# --- error: missing input file ---
assert_fails     "missing input exits nonzero" \
	"$TOOL" "$T/nonexistent.txt" "$T/err.mon"

finish
