#!/bin/sh
#
# Integration tests for mclass.
# Input:  class or race definition file (.txt)
# Output: compiled class/race binary (.cls)
# mclass handles both classes.txt and races.txt; they share the same format.

cd "$(dirname "$0")"
TOOL="${BINDIR}mclass${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/mclass"
T="${CLANS_TMPDIR}/mclass"
mkdir -p "$T"

# --- happy path: classes ---
assert_succeeds  "valid class input exits zero" \
	"$TOOL" "$F/valid_class.txt" "$T/out_class.cls"
assert_nonempty  "valid class input produces .cls file" \
	"$T/out_class.cls"

# --- happy path: races ---
assert_succeeds  "valid race input exits zero" \
	"$TOOL" "$F/valid_race.txt" "$T/out_race.cls"
assert_nonempty  "valid race input produces .cls file" \
	"$T/out_race.cls"

# --- two classes: exercises flush-on-next-Name + MaxMP + multiple Spell entries ---
assert_succeeds  "withspells input exits zero" \
	"$TOOL" "$F/withspells.txt" "$T/withspells.cls"
assert_nonempty  "withspells input produces .cls file" \
	"$T/withspells.cls"
assert_file_larger  "withspells output larger than single-class output" \
	"$T/withspells.cls" "$T/out_class.cls"

# --- error: missing input file ---
assert_fails     "missing input exits nonzero" \
	"$TOOL" "$T/nonexistent.txt" "$T/err.cls"

finish
