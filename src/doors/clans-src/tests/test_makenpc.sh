#!/bin/sh
#
# Integration tests for makenpc.
# Input:  NPC definition file (.txt) — same format as monsters.txt
# Output: compiled NPC binary (.npc)

cd "$(dirname "$0")"
TOOL="${BINDIR}makenpc${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/makenpc"
T="${CLANS_TMPDIR}/makenpc"
mkdir -p "$T"

# --- happy path ---
assert_succeeds  "valid input exits zero" \
	"$TOOL" "$F/valid.txt" "$T/out.npc"
assert_nonempty  "valid input produces .npc file" \
	"$T/out.npc"

# --- two NPCs: exercises flush-on-next-Index + all NPC fields ---
assert_succeeds  "full NPC input exits zero" \
	"$TOOL" "$F/full.txt" "$T/full.npc"
assert_nonempty  "full NPC input produces .npc file" \
	"$T/full.npc"
assert_file_larger  "full NPC output larger than single-NPC output" \
	"$T/full.npc" "$T/out.npc"

# --- error: wrong arg count ---
assert_fails     "wrong argc exits nonzero" \
	"$TOOL" "$F/valid.txt"

# --- error: missing input file ---
assert_fails     "missing input exits nonzero" \
	"$TOOL" "$T/nonexistent.txt" "$T/err.npc"

finish
