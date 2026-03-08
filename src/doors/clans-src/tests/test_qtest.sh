#!/bin/sh
#
# Integration tests for qtest script mode.
#
# Tests qtest's -s (script) mode, covering:
#   - Argument validation (exit 1)
#   - Basic event execution (SetFlag, GiveGold, Choice, Fight)
#   - NPC chat (Topic= hook)
#   - State arguments (-G, -g, -m)
#   - Type mismatch detection (exit 2)
#   - Unconsumed script detection (exit 3)

cd "$(dirname "$0")"
TOOL="${BINDIR}qtest${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
ECOMP="${BINDIR}ecomp${EXEFILE}"
MAKENPC="${BINDIR}makenpc${EXEFILE}"
if [ ! -x "$ECOMP" ] || [ ! -x "$MAKENPC" ]; then
	printf "SKIP  devkit tools not built\n"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/qtest"
T="${CLANS_TMPDIR}/qtest"
S="${F}/scripts"
mkdir -p "$T"

# Locate the compiled language file (data/strings.xl, two dirs above tests/).
CLANSRC="$(cd "$FIXTURE_DIR/../.." && pwd)"
LANGFILE="${CLANSRC}/data/strings.xl"
if [ ! -f "$LANGFILE" ]; then
	printf "SKIP  language file not built: %s\n" "$LANGFILE"
	exit 0
fi

# =========================================================================
# Compile event fixtures
# =========================================================================
"$ECOMP" "$F/simple.evt"   "$T/simple.e"   >/dev/null 2>&1
"$ECOMP" "$F/choice.evt"   "$T/choice.e"   >/dev/null 2>&1
"$ECOMP" "$F/fight.evt"    "$T/fight.e"    >/dev/null 2>&1
"$ECOMP" "$F/npc_chat.evt" "$T/barkeep.q"  >/dev/null 2>&1
"$MAKENPC" "$F/npc.npc.txt" "$T/barkeep.npc" >/dev/null 2>&1

for _f in "$T/simple.e" "$T/choice.e" "$T/fight.e" \
          "$T/barkeep.q" "$T/barkeep.npc"; do
	if [ ! -s "$_f" ]; then
		printf "FAIL  fixture compile failed: %s\n" "$_f"
		exit 1
	fi
done

# =========================================================================
# Set up qtest working directory
# =========================================================================
# Symlink language file so clans.ini can reference it by relative path.
ln -sf "$LANGFILE" "$T/strings.xl"

# Generate clans.ini.  All paths are relative to $T (qtest's CWD).
# Paths that do not start with '/' bypass the PAK lookup in MyOpen.
cat > "$T/clans.ini" << 'INI'
Language   strings.xl
NpcFile    barkeep.npc
INI

# quests.ini is optional; omitting it causes Quests_Init to log and continue.

# Output capture files.
OUT="$T/out.txt"
STATE="$T/state.txt"

# run_qtest: execute qtest from $T, capturing stdout and stderr.
# Returns qtest's exit code.
run_qtest() {
	sh -c "cd '$T' && '$TOOL' $* >'$OUT' 2>'$STATE'"
}

# check_rc: pass/fail a test based on expected vs actual exit code.
check_rc() {
	_desc="$1"; _want="$2"; _got="$3"
	if [ "$_got" -eq "$_want" ]; then
		pass_test "$_desc"
	else
		fail_test "$_desc  (expected rc=$_want, got rc=$_got)"
	fi
}

# =========================================================================
# Argument validation (exit 1) -- run from tests/ CWD, no clans.ini needed
# =========================================================================

assert_fails "-s without -n or -e exits nonzero" \
	"$TOOL" -s "$S/end.script"

assert_fails "-n and -e mutually exclusive exits nonzero" \
	"$TOOL" -n Barkeep -e "$T/simple.e" SimpleEv -s "$S/end.script"

assert_fails "missing script file exits nonzero" \
	"$TOOL" -n Barkeep -s /nonexistent/__no_such_script__.txt

assert_fails "-n without index exits nonzero" \
	"$TOOL" -n

assert_fails "-e without label exits nonzero" \
	"$TOOL" -e "$T/simple.e"

assert_fails "unknown option exits nonzero" \
	"$TOOL" -Z

# =========================================================================
# Simple event (no prompts): SetFlag G5, GiveGold 500
# =========================================================================

_rc=0; run_qtest -e simple.e SimpleEv -s "$S/end.script" || _rc=$?
check_rc "simple event exits zero" 0 "$_rc"
assert_contains "simple event sets GFlags=5"    "$STATE" "GFlags=5"
assert_contains "simple event gives gold +500"  "$STATE" "Gold=5500"
assert_contains "simple event: MineLevel=1"     "$STATE" "MineLevel=1"
assert_contains "simple event: Member0 name"    "$STATE" "Member0.Name=Fighter"
assert_contains "simple event: Member0 MaxHP"   "$STATE" "Member0.MaxHP=80"

# =========================================================================
# Choice event: Path A sets GFlag 1, Path B sets GFlag 2
# =========================================================================

_rc=0; run_qtest -e choice.e ChoiceEv -s "$S/choice_a.script" || _rc=$?
check_rc "choice=A exits zero" 0 "$_rc"
assert_contains "choice=A sets GFlags=1"  "$STATE" "GFlags=1"

_rc=0; run_qtest -e choice.e ChoiceEv -s "$S/choice_b.script" || _rc=$?
check_rc "choice=B exits zero" 0 "$_rc"
assert_contains "choice=B sets GFlags=2"  "$STATE" "GFlags=2"

# =========================================================================
# Fight event: Fight=W -> win -> EndQ
# =========================================================================

_rc=0; run_qtest -e fight.e FightEv -s "$S/fight_win.script" || _rc=$?
check_rc "fight=W exits zero" 0 "$_rc"
assert_contains "fight: mock output in stdout" "$OUT" "[MOCK Fight] outcome=W"
# RunEvent resets HP to MaxHP after event; verify MaxHP is reported.
assert_contains "fight: Member0.MaxHP=80 in state" "$STATE" "Member0.MaxHP=80"

# =========================================================================
# NPC chat: Barkeep with one KnownTopic "Greetings"
# =========================================================================

_rc=0; run_qtest -n Barkeep -s "$S/npc_greet.script" || _rc=$?
check_rc "NPC chat exits zero" 0 "$_rc"
assert_contains "NPC chat: topic text in stdout" "$OUT" "barkeep"

# =========================================================================
# State arguments: -G (GFlags), -g (Gold), -m (MineLevel)
# =========================================================================

# -G0,5 pre-sets GFlags bits 0 and 5; SimpleEv also sets bit 5 (idempotent).
_rc=0; run_qtest -G0,5 -e simple.e SimpleEv -s "$S/end.script" || _rc=$?
check_rc "-G0,5 simple event exits zero" 0 "$_rc"
assert_contains "-G0,5 reflects in GFlags" "$STATE" "GFlags=0,5"

# -g sets initial gold; SimpleEv adds 500 on top.
_rc=0; run_qtest -g9999 -e simple.e SimpleEv -s "$S/end.script" || _rc=$?
check_rc "-g9999 simple event exits zero" 0 "$_rc"
assert_contains "-g9999+500=10499 in Gold" "$STATE" "Gold=10499"

# -m sets MineLevel (no event modification).
_rc=0; run_qtest -m3 -e simple.e SimpleEv -s "$S/end.script" || _rc=$?
check_rc "-m3 simple event exits zero" 0 "$_rc"
assert_contains "-m3 reflects in MineLevel" "$STATE" "MineLevel=3"

# =========================================================================
# Exit code 2: type mismatch
# choice.evt expects Choice=, but type_mismatch.script provides Fight=
# =========================================================================

_rc=0; run_qtest -e choice.e ChoiceEv -s "$S/type_mismatch.script" || _rc=$?
check_rc "type mismatch exits with code 2" 2 "$_rc"

# =========================================================================
# Exit code 3: unconsumed script
# simple.evt has no prompts; extra_line.script has a line before End
# =========================================================================

_rc=0; run_qtest -e simple.e SimpleEv -s "$S/extra_line.script" || _rc=$?
check_rc "extra script line exits with code 3" 3 "$_rc"

finish
