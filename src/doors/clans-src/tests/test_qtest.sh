#!/bin/sh
#
# Integration tests for qtest script mode.
#
# Tests qtest's -s (script) mode, covering:
#   - Argument validation (exit 1)
#   - Basic event execution (SetFlag, GiveGold, Choice, Fight)
#   - NPC chat (Topic= hook)
#   - State arguments (-G, -g, -m, -q)
#   - Type mismatch detection (exit 2)
#   - Premature End detection (exit 2)
#   - Invalid/empty Choice= detection (exit 2)
#   - Malformed script line detection (exit 2)
#   - Topic-not-found and ambiguous-topic detection (exit 2)
#   - Unconsumed script detection (exit 3)
#   - EOF-instead-of-End detection (exit 3)
#   - All SetFlag/ClearFlag flag types (G, H, P, D, T)
#   - Reward commands (GiveGold, TakeGold, GiveXP, GiveFight, GivePoints,
#       GiveFollowers, GiveItem)
#   - Heal SP, Pause
#   - All ACS condition types (^, %, G, H, P, D, T, Q, $, L, K, R, C, &, |, ())
#   - Jump, Display, AddNews
#   - Multiple options (3-way), GetKey, Input, DoneQuest
#   - Fight outcomes: Lose and Run
#   - Multi-topic NPC, TellTopic
#   - TellQuest
#   - State args: -H, -P, -D

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
"$ECOMP" "$F/simple.evt"        "$T/simple.e"        >/dev/null 2>&1
"$ECOMP" "$F/choice.evt"        "$T/choice.e"        >/dev/null 2>&1
"$ECOMP" "$F/fight.evt"         "$T/fight.e"         >/dev/null 2>&1
"$ECOMP" "$F/npc_chat.evt"      "$T/barkeep.q"       >/dev/null 2>&1
"$MAKENPC" "$F/npc.npc.txt"     "$T/barkeep.npc"     >/dev/null 2>&1
"$ECOMP" "$F/flags.evt"         "$T/flags.e"         >/dev/null 2>&1
"$ECOMP" "$F/rewards.evt"       "$T/rewards.e"       >/dev/null 2>&1
"$ECOMP" "$F/heal.evt"          "$T/heal.e"          >/dev/null 2>&1
"$ECOMP" "$F/acs.evt"           "$T/acs.e"           >/dev/null 2>&1
"$ECOMP" "$F/jump.evt"          "$T/jump.e"          >/dev/null 2>&1
"$ECOMP" "$F/display.evt"       "$T/display.e"       >/dev/null 2>&1
"$ECOMP" "$F/news.evt"          "$T/news.e"          >/dev/null 2>&1
"$ECOMP" "$F/multi_option.evt"  "$T/multi_option.e"  >/dev/null 2>&1
"$ECOMP" "$F/getkey.evt"        "$T/getkey.e"        >/dev/null 2>&1
"$ECOMP" "$F/donequest.evt"     "$T/donequest.e"     >/dev/null 2>&1
"$ECOMP" "$F/input.evt"         "$T/input.e"         >/dev/null 2>&1
"$ECOMP" "$F/tellquest.evt"     "$T/tellquest.e"     >/dev/null 2>&1
"$ECOMP" "$F/misc.evt"          "$T/misc.e"          >/dev/null 2>&1
"$ECOMP" "$F/npc_multi.evt"     "$T/wizard.q"        >/dev/null 2>&1
"$ECOMP" "$F/npc_telltopic.evt" "$T/merchant.q"      >/dev/null 2>&1
"$MAKENPC" "$F/npc_multi.npc.txt"     "$T/wizard.npc"   >/dev/null 2>&1
"$MAKENPC" "$F/npc_telltopic.npc.txt" "$T/merchant.npc" >/dev/null 2>&1

for _f in \
	"$T/simple.e"       "$T/choice.e"       "$T/fight.e" \
	"$T/barkeep.q"      "$T/barkeep.npc" \
	"$T/flags.e"        "$T/rewards.e"      "$T/heal.e" \
	"$T/acs.e"          "$T/jump.e"         "$T/display.e" \
	"$T/news.e"         "$T/multi_option.e" "$T/getkey.e" \
	"$T/donequest.e"    "$T/input.e"        "$T/tellquest.e" \
	"$T/misc.e" \
	"$T/wizard.q"       "$T/wizard.npc" \
	"$T/merchant.q"     "$T/merchant.npc"
do
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
NpcFile    wizard.npc
NpcFile    merchant.npc
INI

# Copy quests.ini for TellQuest tests.  With one hidden quest (TestQ1),
# no quests start known, so existing tests are unaffected.
cp "$F/quests.ini" "$T/quests.ini"

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

assert_fails "bare non-option argument exits nonzero" \
	"$TOOL" plainword

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
# Exit code 2: premature End (script has End when hook expected)
# choice.evt expects Choice=; end.script is just "End"
# =========================================================================

_rc=0; run_qtest -e choice.e ChoiceEv -s "$S/end.script" || _rc=$?
check_rc "premature End exits with code 2" 2 "$_rc"

# =========================================================================
# Exit code 2: invalid Choice char (not in allowed set)
# choice.evt allows A or B; invalid_choice.script supplies Z
# =========================================================================

_rc=0; run_qtest -e choice.e ChoiceEv -s "$S/invalid_choice.script" || _rc=$?
check_rc "invalid Choice char exits with code 2" 2 "$_rc"

# =========================================================================
# Exit code 2: empty Choice= value
# =========================================================================

_rc=0; run_qtest -e choice.e ChoiceEv -s "$S/empty_choice.script" || _rc=$?
check_rc "empty Choice= exits with code 2" 2 "$_rc"

# =========================================================================
# Exit code 2: malformed script line (no '=' sign)
# =========================================================================

_rc=0; run_qtest -e choice.e ChoiceEv -s "$S/malformed_line.script" || _rc=$?
check_rc "malformed script line exits with code 2" 2 "$_rc"

# =========================================================================
# Exit code 2: Topic= not found in option list
# input.evt has "Choice Alpha" and "Choice Beta"; bad_topic.script uses NoSuchOption
# =========================================================================

_rc=0; run_qtest -e input.e InputEv -s "$S/bad_topic.script" || _rc=$?
check_rc "topic not found exits with code 2" 2 "$_rc"

# =========================================================================
# Exit code 2: ambiguous Topic= prefix
# input.evt has "Choice Alpha" and "Choice Beta"; "Choice" matches both
# =========================================================================

_rc=0; run_qtest -e input.e InputEv -s "$S/ambiguous_topic.script" || _rc=$?
check_rc "ambiguous topic exits with code 2" 2 "$_rc"

# =========================================================================
# Exit code 3: unconsumed script
# simple.evt has no prompts; extra_line.script has a line before End
# =========================================================================

_rc=0; run_qtest -e simple.e SimpleEv -s "$S/extra_line.script" || _rc=$?
check_rc "extra script line exits with code 3" 3 "$_rc"

# =========================================================================
# Exit code 3: EOF reached instead of End marker
# simple.evt has no prompts; eof_no_end.script is empty (no End line)
# =========================================================================

_rc=0; run_qtest -e simple.e SimpleEv -s "$S/eof_no_end.script" || _rc=$?
check_rc "EOF without End exits with code 3" 3 "$_rc"

# =========================================================================
# SetFlag: all five flag types
# =========================================================================

_rc=0; run_qtest -e flags.e SetGFlag3 -s "$S/end.script" || _rc=$?
check_rc "SetFlag G3 exits zero" 0 "$_rc"
assert_contains "SetFlag G3: GFlags=3" "$STATE" "GFlags=3"

_rc=0; run_qtest -e flags.e SetHFlag7 -s "$S/end.script" || _rc=$?
check_rc "SetFlag H7 exits zero" 0 "$_rc"
assert_contains "SetFlag H7: HFlags=7" "$STATE" "HFlags=7"

_rc=0; run_qtest -e flags.e SetPFlag2 -s "$S/end.script" || _rc=$?
check_rc "SetFlag P2 exits zero" 0 "$_rc"
assert_contains "SetFlag P2: PFlags=2" "$STATE" "PFlags=2"

_rc=0; run_qtest -e flags.e SetDFlag15 -s "$S/end.script" || _rc=$?
check_rc "SetFlag D15 exits zero" 0 "$_rc"
assert_contains "SetFlag D15: DFlags=15" "$STATE" "DFlags=15"

_rc=0; run_qtest -e flags.e SetTFlag0 -s "$S/end.script" || _rc=$?
check_rc "SetFlag T0 exits zero" 0 "$_rc"
assert_contains "SetFlag T0: TFlags=0" "$STATE" "TFlags=0"

# =========================================================================
# ClearFlag: verify bit is cleared after being pre-set via state arg
# =========================================================================

_rc=0; run_qtest -G0 -e flags.e ClearGFlag0 -s "$S/end.script" || _rc=$?
check_rc "ClearFlag G0 exits zero" 0 "$_rc"
assert_contains "ClearFlag G0: GFlags empty" "$STATE" "GFlags="
# Confirm the bit is gone (GFlags= not followed by a digit on the same line)
assert_not_contains "ClearFlag G0: no bit 0 in GFlags" "$STATE" "GFlags=0"

_rc=0; run_qtest -P2 -e flags.e ClearPFlag2 -s "$S/end.script" || _rc=$?
check_rc "ClearFlag P2 exits zero" 0 "$_rc"
assert_not_contains "ClearFlag P2: PFlags no longer has 2" "$STATE" "PFlags=2"

# SetFlag G10 + ClearFlag G0 (G0 pre-set): result is only G10
_rc=0; run_qtest -G0 -e flags.e SetClearCombo -s "$S/end.script" || _rc=$?
check_rc "SetFlag G10 + ClearFlag G0 exits zero" 0 "$_rc"
assert_contains "combo: GFlags=10" "$STATE" "GFlags=10"
assert_not_contains "combo: G0 cleared" "$STATE" "GFlags=0,"

# =========================================================================
# Reward commands
# =========================================================================

_rc=0; run_qtest -e rewards.e GiveGoldEv -s "$S/givegold.script" || _rc=$?
check_rc "GiveGold 1000 exits zero" 0 "$_rc"
assert_contains "GiveGold: Gold=6000" "$STATE" "Gold=6000"

_rc=0; run_qtest -e rewards.e TakeGoldEv -s "$S/takegold_flat.script" || _rc=$?
check_rc "TakeGold 200 exits zero" 0 "$_rc"
assert_contains "TakeGold flat: Gold=4800" "$STATE" "Gold=4800"

# TakeGold %10 of 5000 = 500 taken; 5000-500=4500
_rc=0; run_qtest -e rewards.e TakeGoldPctEv -s "$S/takegold_pct.script" || _rc=$?
check_rc "TakeGold %10 exits zero" 0 "$_rc"
assert_contains "TakeGold pct: Gold=4500" "$STATE" "Gold=4500"

# GiveXP 100: all 4 members present get 100 XP each
_rc=0; run_qtest -e rewards.e GiveXPEv -s "$S/givexp.script" || _rc=$?
check_rc "GiveXP 100 exits zero" 0 "$_rc"
assert_contains "GiveXP: Member0.XP=100" "$STATE" "Member0.XP=100"
assert_contains "GiveXP: Member3.XP=100" "$STATE" "Member3.XP=100"

_rc=0; run_qtest -e rewards.e GivePointsEv -s "$S/givepoints.script" || _rc=$?
check_rc "GivePoints 50 exits zero" 0 "$_rc"
assert_contains "GivePoints: Points=50" "$STATE" "Points=50"

# GiveFight adds to FightsLeft (initial=10, +5=15)
_rc=0; run_qtest -e rewards.e GiveFightEv -s "$S/givefight.script" || _rc=$?
check_rc "GiveFight 5 exits zero" 0 "$_rc"
assert_contains "GiveFight: FightsLeft=15" "$STATE" "FightsLeft=15"

_rc=0; run_qtest -e rewards.e GiveFollowersEv -s "$S/givefollowers.script" || _rc=$?
check_rc "GiveFollowers 10 exits zero" 0 "$_rc"
assert_contains "GiveFollowers: Followers=10" "$STATE" "Followers=10"

# =========================================================================
# Heal SP
# Members start at MaxSP/2 (Fighter: 5/10).  RunEvent does NOT reset SP.
# Members start at MaxHP/2 and MaxSP/2 (Fighter: HP=40/80, SP=5/10).
# reset_hp=false in script mode so both Heal variants are observable.
# =========================================================================

_rc=0; run_qtest -e heal.e HealSPEv -s "$S/heal_sp.script" || _rc=$?
check_rc "Heal SP exits zero" 0 "$_rc"
assert_contains "Heal SP: Member0.SP=10" "$STATE" "Member0.SP=10"
assert_contains "Heal SP: Member0.MaxSP=10" "$STATE" "Member0.MaxSP=10"

_rc=0; run_qtest -e heal.e HealHPEv -s "$S/heal_hp.script" || _rc=$?
check_rc "Heal HP exits zero" 0 "$_rc"
assert_contains "Heal HP: Member0.HP=80" "$STATE" "Member0.HP=80"
assert_contains "Heal HP: Member0.MaxHP=80" "$STATE" "Member0.MaxHP=80"

# Fight reduces SP by 1 (Fighter: 5->4); Heal SP restores to MaxSP=10
_rc=0; run_qtest -e heal.e HealAfterFightEv -s "$S/heal_after_fight.script" || _rc=$?
check_rc "Heal SP after fight exits zero" 0 "$_rc"
assert_contains "Heal SP after fight: Member0.SP=10" "$STATE" "Member0.SP=10"

# =========================================================================
# ACS conditions
# =========================================================================

# ^ always true -> G0 set; % always false -> G1 NOT set
_rc=0; run_qtest -e acs.e ACS_AlwaysTrue -s "$S/acs_always_true.script" || _rc=$?
check_rc "ACS ^ and % exits zero" 0 "$_rc"
assert_contains "ACS ^: G0 set" "$STATE" "GFlags=0"
assert_not_contains "ACS %: G1 not set" "$STATE" "GFlags=0,1"

# G flag: {G0} true with -G0; {!G1} true without G1
_rc=0; run_qtest -G0 -e acs.e ACS_GFlag -s "$S/acs_gflag.script" || _rc=$?
check_rc "ACS G flag exits zero" 0 "$_rc"
assert_contains "ACS G0: P0 set" "$STATE" "PFlags=0"
assert_contains "ACS !G1: P1 set" "$STATE" "PFlags=0,1"

# H flag: {H0} true with -H0
_rc=0; run_qtest -H0 -e acs.e ACS_HFlag -s "$S/acs_hflag.script" || _rc=$?
check_rc "ACS H flag exits zero" 0 "$_rc"
assert_contains "ACS H0: P0 set" "$STATE" "PFlags=0"

# P flag: {P0} true with -P0; result is G flag set (G0)
_rc=0; run_qtest -P0 -e acs.e ACS_PFlag -s "$S/acs_pflag.script" || _rc=$?
check_rc "ACS P flag exits zero" 0 "$_rc"
assert_contains "ACS P0: G0 set" "$STATE" "GFlags=0"

# D flag: {D0} true with -D0
_rc=0; run_qtest -D0 -e acs.e ACS_DFlag -s "$S/acs_dflag.script" || _rc=$?
check_rc "ACS D flag exits zero" 0 "$_rc"
assert_contains "ACS D0: P0 set" "$STATE" "PFlags=0"

# Q flag: {Q1} true with -Q1 (quest 1 marked done via state arg)
_rc=0; run_qtest -Q1 -e acs.e ACS_QuestDone -s "$S/acs_questdone.script" || _rc=$?
check_rc "ACS Q flag exits zero" 0 "$_rc"
assert_contains "ACS Q1: P0 set" "$STATE" "PFlags=0"

# T flag: TFlags cleared at RunEvent start; SetFlag T5 within event sets it;
#         {T5} -> P0 set, {!T6} -> P1 set; TFlags=5 in state
_rc=0; run_qtest -e acs.e ACS_TFlag -s "$S/acs_tflag.script" || _rc=$?
check_rc "ACS T flag exits zero" 0 "$_rc"
assert_contains "ACS T5 in state" "$STATE" "TFlags=5"
assert_contains "ACS {T5}: P0 set" "$STATE" "PFlags=0"
assert_contains "ACS {!T6}: P1 also set" "$STATE" "PFlags=0,1"

# Gold: default 5000; {$4000} true -> P0; {!$9999} true (5000<9999) -> P1
_rc=0; run_qtest -e acs.e ACS_Gold -s "$S/acs_gold.script" || _rc=$?
check_rc "ACS gold exits zero" 0 "$_rc"
assert_contains "ACS gold {$4000}: P0 set" "$STATE" "PFlags=0"
assert_contains "ACS gold {!$9999}: P1 set" "$STATE" "PFlags=0,1"

# Mine level exact (L): default 1; {L1} true -> P0; {!L2} true -> P1
_rc=0; run_qtest -e acs.e ACS_MineL -s "$S/acs_mine_l.script" || _rc=$?
check_rc "ACS mine L exits zero" 0 "$_rc"
assert_contains "ACS {L1}: P0 set" "$STATE" "PFlags=0"
assert_contains "ACS {!L2}: P1 set" "$STATE" "PFlags=0,1"

# Mine level at-least (K): default 1; {K1} true -> P0; {!K3} true -> P1
_rc=0; run_qtest -e acs.e ACS_MineK -s "$S/acs_mine_k.script" || _rc=$?
check_rc "ACS mine K exits zero" 0 "$_rc"
assert_contains "ACS {K1}: P0 set" "$STATE" "PFlags=0"
assert_contains "ACS {!K3}: P1 set" "$STATE" "PFlags=0,1"

# AND: both G0 and G1 must be set
_rc=0; run_qtest -G0,1 -e acs.e ACS_And -s "$S/acs_and_true.script" || _rc=$?
check_rc "ACS AND (both set) exits zero" 0 "$_rc"
assert_contains "ACS AND true: P0 set" "$STATE" "PFlags=0"

_rc=0; run_qtest -G0 -e acs.e ACS_And -s "$S/acs_and_false.script" || _rc=$?
check_rc "ACS AND (G1 missing) exits zero" 0 "$_rc"
assert_not_contains "ACS AND false: P0 not set" "$STATE" "PFlags=0"

# OR: G0 or G2 suffices
_rc=0; run_qtest -G2 -e acs.e ACS_Or -s "$S/acs_or.script" || _rc=$?
check_rc "ACS OR (G2 only) exits zero" 0 "$_rc"
assert_contains "ACS OR: P0 set" "$STATE" "PFlags=0"

# Parentheses: (G0 & G1) | G2; provide only G2
_rc=0; run_qtest -G2 -e acs.e ACS_Paren -s "$S/acs_paren.script" || _rc=$?
check_rc "ACS paren exits zero" 0 "$_rc"
assert_contains "ACS paren {(G0&G1)|G2}: P0 set" "$STATE" "PFlags=0"

# R100 = always true; consumes Random=0
_rc=0; run_qtest -e acs.e ACS_RandomTrue -s "$S/acs_random_true.script" || _rc=$?
check_rc "ACS R100 exits zero" 0 "$_rc"
assert_contains "ACS R100: P0 set" "$STATE" "PFlags=0"

# R0 = always false; consumes Random=0
_rc=0; run_qtest -e acs.e ACS_RandomFalse -s "$S/acs_random_false.script" || _rc=$?
check_rc "ACS R0 exits zero" 0 "$_rc"
assert_not_contains "ACS R0: P0 not set" "$STATE" "PFlags=0"

# Charisma: default leader Charisma=6; {C6} true -> P0; {!C10} true -> P1
_rc=0; run_qtest -e acs.e ACS_Charisma -s "$S/acs_charisma.script" || _rc=$?
check_rc "ACS charisma C exits zero" 0 "$_rc"
assert_contains "ACS {C6}: P0 set" "$STATE" "PFlags=0"
assert_contains "ACS {!C10}: P1 set" "$STATE" "PFlags=0,1"

# Charisma override: -c20 sets leader Charisma=20; {C20} true, {C25} false
_rc=0; run_qtest -c20 -e acs.e ACS_CharismaOverride -s "$S/acs_charisma_override.script" || _rc=$?
check_rc "ACS charisma override exits zero" 0 "$_rc"
assert_contains "ACS -c20 {C20}: P0 set" "$STATE" "PFlags=0"
assert_not_contains "ACS -c20 {C25}: P1 not set" "$STATE" "PFlags=0,1"
assert_contains "ACS -c20: Cha=20" "$STATE" "Member0.Cha=20"

# =========================================================================
# Jump: unconditional branch to another block
# =========================================================================

_rc=0; run_qtest -e jump.e JumpEv -s "$S/jump.script" || _rc=$?
check_rc "Jump exits zero" 0 "$_rc"
assert_contains "Jump: GFlags=7" "$STATE" "GFlags=7"

# =========================================================================
# Display: mock writes to stdout
# =========================================================================

_rc=0; run_qtest -e display.e DisplayEv -s "$S/display.script" || _rc=$?
check_rc "Display exits zero" 0 "$_rc"
assert_contains "Display: mock in stdout" "$OUT" "[MOCK Display]"
assert_contains "Display: filename in stdout" "$OUT" "test.asc"

# =========================================================================
# AddNews: mock writes to stdout
# =========================================================================

_rc=0; run_qtest -e news.e NewsEv -s "$S/news.script" || _rc=$?
check_rc "AddNews exits zero" 0 "$_rc"
assert_contains "AddNews: mock in stdout" "$OUT" "[MOCK News_AddNews]"
assert_contains "AddNews: text in stdout" "$OUT" "Test news item"

# =========================================================================
# Multiple options (3-way)
# =========================================================================

_rc=0; run_qtest -e multi_option.e MultiOpt -s "$S/multi_opt_a.script" || _rc=$?
check_rc "3-way option A exits zero" 0 "$_rc"
assert_contains "option A: GFlags=0" "$STATE" "GFlags=0"

_rc=0; run_qtest -e multi_option.e MultiOpt -s "$S/multi_opt_b.script" || _rc=$?
check_rc "3-way option B exits zero" 0 "$_rc"
assert_contains "option B: GFlags=1" "$STATE" "GFlags=1"

_rc=0; run_qtest -e multi_option.e MultiOpt -s "$S/multi_opt_c.script" || _rc=$?
check_rc "3-way option C exits zero" 0 "$_rc"
assert_contains "option C: GFlags=2" "$STATE" "GFlags=2"

# =========================================================================
# GetKey: same Choice= hook as Option but does not echo the key
# =========================================================================

_rc=0; run_qtest -e getkey.e GetKeyEv -s "$S/getkey_a.script" || _rc=$?
check_rc "GetKey A exits zero" 0 "$_rc"
assert_contains "GetKey A: GFlags=0" "$STATE" "GFlags=0"

_rc=0; run_qtest -e getkey.e GetKeyEv -s "$S/getkey_b.script" || _rc=$?
check_rc "GetKey B exits zero" 0 "$_rc"
assert_contains "GetKey B: GFlags=1" "$STATE" "GFlags=1"

# =========================================================================
# DoneQuest: exits 0, prints completion message, sets QuestsDone=1.
# Script mode has no quest index (events run directly, not via quests.ini)
# so qtest records the result as quest index 1.
# =========================================================================

_rc=0; run_qtest -e donequest.e DoneQuestEv -s "$S/donequest.script" || _rc=$?
check_rc "DoneQuest exits zero" 0 "$_rc"
assert_contains "DoneQuest: completion message" "$OUT" "Quest successfully completed"
assert_contains "DoneQuest: QuestsDone=1" "$STATE" "QuestsDone=1"

# =========================================================================
# Input: text-based menu selection (Topic= hook)
# =========================================================================

_rc=0; run_qtest -e input.e InputEv -s "$S/input_alpha.script" || _rc=$?
check_rc "Input alpha exits zero" 0 "$_rc"
assert_contains "Input alpha: GFlags=0" "$STATE" "GFlags=0"

_rc=0; run_qtest -e input.e InputEv -s "$S/input_beta.script" || _rc=$?
check_rc "Input beta exits zero" 0 "$_rc"
assert_contains "Input beta: GFlags=1" "$STATE" "GFlags=1"

# =========================================================================
# Fight outcomes: Lose and Run (FightCanRunEv allows all three)
# =========================================================================

_rc=0; run_qtest -e fight.e FightCanRunEv -s "$S/fight_lose.script" || _rc=$?
check_rc "Fight=L exits zero" 0 "$_rc"
assert_contains "Fight=L: GFlags=1 (Lose branch)" "$STATE" "GFlags=1"

_rc=0; run_qtest -e fight.e FightCanRunEv -s "$S/fight_run.script" || _rc=$?
check_rc "Fight=R exits zero" 0 "$_rc"
assert_contains "Fight=R: GFlags=2 (Run branch)" "$STATE" "GFlags=2"

# =========================================================================
# State args: -H, -P, -D reflected in state summary
# =========================================================================

_rc=0; run_qtest -H3,7 -e simple.e SimpleEv -s "$S/state_hflags.script" || _rc=$?
check_rc "-H3,7 exits zero" 0 "$_rc"
assert_contains "-H3,7: HFlags=3,7" "$STATE" "HFlags=3,7"

_rc=0; run_qtest -P1 -e simple.e SimpleEv -s "$S/state_pflags.script" || _rc=$?
check_rc "-P1 exits zero" 0 "$_rc"
assert_contains "-P1: PFlags=1" "$STATE" "PFlags=1"

_rc=0; run_qtest -D2 -e simple.e SimpleEv -s "$S/state_dflags.script" || _rc=$?
check_rc "-D2 exits zero" 0 "$_rc"
assert_contains "-D2: DFlags=2" "$STATE" "DFlags=2"

# =========================================================================
# Multi-topic NPC (Wizard): two KnownTopics
# =========================================================================

# Selecting "Information" sets P3 and EndChats
_rc=0; run_qtest -n Wizard -s "$S/npc_wizard_info.script" || _rc=$?
check_rc "NPC Wizard info exits zero" 0 "$_rc"
assert_contains "Wizard info: PFlags=3" "$STATE" "PFlags=3"

# Selecting "Greetings" (no EndChat) then empty Topic= to exit loop
_rc=0; run_qtest -n Wizard -s "$S/npc_wizard_greet_exit.script" || _rc=$?
check_rc "NPC Wizard greet+exit exits zero" 0 "$_rc"
assert_not_contains "Wizard greet: P3 not set" "$STATE" "PFlags=3"

# =========================================================================
# TellTopic: Merchant NPC unlocks hidden topic via TellTopic
# =========================================================================

# Greetings: TellTopic reveals Secret Info (no EndChat, loop continues)
# Secret Info: SetFlag P7, EndChat
_rc=0; run_qtest -n Merchant -s "$S/npc_merchant_telltopic.script" || _rc=$?
check_rc "TellTopic exits zero" 0 "$_rc"
assert_contains "TellTopic: P7 set after Secret Info" "$STATE" "PFlags=7"

# =========================================================================
# TellQuest: makes a hidden quest known (quests.ini has TestQ1, no Known)
# =========================================================================

_rc=0; run_qtest -e tellquest.e TellQuestEv -s "$S/tellquest.script" || _rc=$?
check_rc "TellQuest exits zero" 0 "$_rc"
assert_contains "TellQuest: QuestsKnown=1" "$STATE" "QuestsKnown=1"

# =========================================================================
# GiveItem: mock prints [MOCK Items_GiveItem] to stdout
# =========================================================================

_rc=0; run_qtest -e misc.e GiveItemEv -s "$S/end.script" || _rc=$?
check_rc "GiveItem exits zero" 0 "$_rc"
assert_contains "GiveItem: mock in stdout" "$OUT" "[MOCK Items_GiveItem]"
assert_contains "GiveItem: item name in stdout" "$OUT" "DragonScale"

# =========================================================================
# Pause: door_pause() is a no-op in script mode; event exits cleanly
# =========================================================================

_rc=0; run_qtest -e misc.e PauseEv -s "$S/end.script" || _rc=$?
check_rc "Pause exits zero" 0 "$_rc"

# =========================================================================
# State arg -q: pre-sets QuestsKnown bits (quest known but not done)
# Quest 1 (TestQ1) starts hidden; -q1 marks it known without running it.
# =========================================================================

_rc=0; run_qtest -q1 -e simple.e SimpleEv -s "$S/end.script" || _rc=$?
check_rc "-q1 exits zero" 0 "$_rc"
assert_contains "-q1: QuestsKnown=1" "$STATE" "QuestsKnown=1"

finish
