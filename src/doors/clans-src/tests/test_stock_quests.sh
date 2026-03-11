#!/bin/sh
#
# Regression tests for the stock quest campaign.
#
# Compiles the real data files (quests.evt, eventmon.txt, npcquote.u8.txt,
# clans.u8.txt) and runs every quest through qtest in script mode.  Each
# quest path is tested for correct exit code, gold, flags, and rewards.
#

cd "$(dirname "$0")"
TOOL="${BINDIR}qtest${EXEFILE}"
ECOMP="${BINDIR}ecomp${EXEFILE}"
MCOMP="${BINDIR}mcomp${EXEFILE}"
MAKENPC="${BINDIR}makenpc${EXEFILE}"
MAKEPAK="${BINDIR}makepak${EXEFILE}"
for _b in "$TOOL" "$ECOMP" "$MCOMP" "$MAKENPC" "$MAKEPAK"; do
	if [ ! -x "$_b" ]; then
		printf "SKIP  tool not built: %s\n" "$_b"
		exit 0
	fi
done
. ./test_harness.sh

CLANSRC="$(cd "$FIXTURE_DIR/../.." && pwd)"
D="$CLANSRC/data"
R="$CLANSRC/release"
F="${FIXTURE_DIR}/stock_quests"
S="${F}/scripts"
T="${CLANS_TMPDIR}/stock_quests"
mkdir -p "$T"

# =========================================================================
# Compile stock data files
# =========================================================================
LANGFILE="$D/strings.xl"
if [ ! -f "$LANGFILE" ]; then
	printf "SKIP  language file not built: %s\n" "$LANGFILE"
	exit 0
fi

"$ECOMP"   "$D/quests.evt"   "$T/quests.e"    >/dev/null 2>&1
"$ECOMP"   "$D/npcquote.u8.txt" "$T/npcquote.q"  >/dev/null 2>&1
"$MCOMP"   "$D/eventmon.txt"   "$T/eventmon.mon" >/dev/null 2>&1
"$MAKENPC" "$D/clans.u8.txt"   "$T/clans.npc"    >/dev/null 2>&1

for _f in "$T/quests.e" "$T/npcquote.q" "$T/eventmon.mon" "$T/clans.npc"; do
	if [ ! -s "$_f" ]; then
		printf "FAIL  fixture compile failed: %s\n" "$_f"
		exit 1
	fi
done

# Build minimal PAK (needed for NPC Chat commands that use /q/NpcQuote).
cp "$F/pak.lst" "$T/"
(cd "$T" && "$MAKEPAK" clans.pak pak.lst >/dev/null 2>&1)
if [ ! -s "$T/clans.pak" ]; then
	printf "FAIL  makepak failed\n"
	exit 1
fi

# Set up qtest working directory.
cp "$R/quests.ini" "$T/"
ln -sf "$LANGFILE" "$T/strings.xl"
cat > "$T/clans.ini" << 'INI'
Language   strings.xl
NpcFile    clans.npc
INI

# Output capture files.
OUT="$T/out.txt"
STATE="$T/state.txt"

run_qtest() {
	sh -c "cd '$T' && '$TOOL' $* >'$OUT' 2>'$STATE'"
}

check_rc() {
	_desc="$1"; _want="$2"; _got="$3"
	if [ "$_got" -eq "$_want" ]; then
		pass_test "$_desc"
	else
		fail_test "$_desc  (expected rc=$_want, got rc=$_got)"
	fi
}

# =========================================================================
# Quest 1: The Orcs -- Act I (linear, Chat _Jester)
# =========================================================================

_rc=0; run_qtest -e quests.e Quest1 -s "$S/q1_win.script" || _rc=$?
check_rc "Q1 win exits zero" 0 "$_rc"
assert_contains "Q1: Gold=5000"      "$STATE" "Gold=5000"
assert_contains "Q1: QuestsDone=1"   "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 2: The Quest for Justice -- Act I
# =========================================================================

_rc=0; run_qtest -e quests.e Quest2 -s "$S/q2_attack.script" || _rc=$?
check_rc "Q2 attack exits zero" 0 "$_rc"
assert_contains "Q2 attack: PFlags=1"      "$STATE" "PFlags=1"
assert_contains "Q2 attack: QuestsDone=1"  "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest2 -s "$S/q2_talk.script" || _rc=$?
check_rc "Q2 talk exits zero" 0 "$_rc"
assert_contains "Q2 talk: QuestsDone=1"    "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 3: The Quest for Justice -- Act II
# =========================================================================

_rc=0; run_qtest -P1 -e quests.e Quest3 -s "$S/q3_enter_drink_p1.script" || _rc=$?
check_rc "Q3 enter+drink+P1 exits zero" 0 "$_rc"
assert_contains "Q3 E/Y/P1: Gold=5000"    "$STATE" "Gold=5000"
assert_contains "Q3 E/Y/P1: QuestsDone=1" "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest3 -s "$S/q3_sneak_skip.script" || _rc=$?
check_rc "Q3 sneak+skip exits zero" 0 "$_rc"
assert_contains "Q3 S/N: Gold=5500"       "$STATE" "Gold=5500"
assert_contains "Q3 S/N: QuestsDone=1"    "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 4: Caravan
# =========================================================================

_rc=0; run_qtest -e quests.e Quest4 -s "$S/q4_investigate.script" || _rc=$?
check_rc "Q4 investigate exits zero" 0 "$_rc"
assert_contains "Q4 inv: Gold=6500"       "$STATE" "Gold=6500"
assert_contains "Q4 inv: TFlags=1"        "$STATE" "TFlags=1"
assert_contains "Q4 inv: QuestsDone=1"    "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest4 -s "$S/q4_ignore.script" || _rc=$?
check_rc "Q4 ignore exits zero" 0 "$_rc"
assert_contains "Q4 ign: Gold=7000"       "$STATE" "Gold=7000"
assert_contains "Q4 ign: QuestsDone=1"    "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 5: The Minstrel
# =========================================================================

_rc=0; run_qtest -e quests.e Quest5 -s "$S/q5_rr_success.script" || _rc=$?
check_rc "Q5 R/R success exits zero" 0 "$_rc"
assert_contains "Q5 R/R: Gold=6000"       "$STATE" "Gold=6000"
assert_contains "Q5 R/R: QuestsDone=1"    "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest5 -s "$S/q5_ll_deadend.script" || _rc=$?
check_rc "Q5 L/L dead end exits zero" 0 "$_rc"
assert_not_contains "Q5 L/L: no QuestsDone" "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 6: Drakuul's Legacy
# =========================================================================

_rc=0; run_qtest -e quests.e Quest6 -s "$S/q6_altar.script" || _rc=$?
check_rc "Q6 altar exits zero" 0 "$_rc"
assert_contains "Q6 altar: Gold=7500"     "$STATE" "Gold=7500"
assert_contains "Q6 altar: QuestsDone=1"  "$STATE" "QuestsDone=1"
assert_contains "Q6 altar: AddNews"       "$OUT"   "[MOCK News_AddNews]"

_rc=0; run_qtest -e quests.e Quest6 -s "$S/q6_face.script" || _rc=$?
check_rc "Q6 face exits zero" 0 "$_rc"
assert_contains "Q6 face: Gold=8000"      "$STATE" "Gold=8000"
assert_contains "Q6 face: QuestsDone=1"   "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 7: The Quest for the Lost King (Chat _King)
# =========================================================================

_rc=0; run_qtest -e quests.e Quest7 -s "$S/q7_search.script" || _rc=$?
check_rc "Q7 search exits zero" 0 "$_rc"
assert_contains "Q7 search: Gold=7200"    "$STATE" "Gold=7200"
assert_contains "Q7 search: PFlags=2"     "$STATE" "PFlags=2"
assert_contains "Q7 search: QuestsDone=1" "$STATE" "QuestsDone=1"
assert_contains "Q7 search: AddNews"      "$OUT"   "[MOCK News_AddNews]"

_rc=0; run_qtest -e quests.e Quest7 -s "$S/q7_press.script" || _rc=$?
check_rc "Q7 press exits zero" 0 "$_rc"
assert_contains "Q7 press: Gold=7000"     "$STATE" "Gold=7000"
assert_contains "Q7 press: PFlags=2"      "$STATE" "PFlags=2"

# =========================================================================
# Quest 8: The Quest for the Lost Knight
# =========================================================================

_rc=0; run_qtest -e quests.e Quest8 -s "$S/q8_down.script" || _rc=$?
check_rc "Q8 down exits zero" 0 "$_rc"
assert_contains "Q8 down: Gold=5800"      "$STATE" "Gold=5800"
assert_contains "Q8 down: PFlags=5"       "$STATE" "PFlags=5"
assert_contains "Q8 down: QuestsDone=1"   "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest8 -s "$S/q8_explore.script" || _rc=$?
check_rc "Q8 explore exits zero" 0 "$_rc"
assert_contains "Q8 explore: Gold=6000"   "$STATE" "Gold=6000"
assert_contains "Q8 explore: PFlags=5"    "$STATE" "PFlags=5"

# =========================================================================
# Quest 9: The Quest for Vengeance
# =========================================================================

_rc=0; run_qtest -e quests.e Quest9 -s "$S/q9_ambush.script" || _rc=$?
check_rc "Q9 ambush exits zero" 0 "$_rc"
assert_contains "Q9 ambush: Gold=5400"    "$STATE" "Gold=5400"
assert_contains "Q9 ambush: QuestsDone=1" "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest9 -s "$S/q9_challenge.script" || _rc=$?
check_rc "Q9 challenge exits zero" 0 "$_rc"
assert_contains "Q9 challenge: Gold=5400" "$STATE" "Gold=5400"

# =========================================================================
# Quest 10: The Spirits of Lovers
# =========================================================================

_rc=0; run_qtest -e quests.e Quest10 -s "$S/q10_help.script" || _rc=$?
check_rc "Q10 help exits zero" 0 "$_rc"
assert_contains "Q10 help: Gold=6000"     "$STATE" "Gold=6000"
assert_contains "Q10 help: QuestsDone=1"  "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest10 -s "$S/q10_take.script" || _rc=$?
check_rc "Q10 take exits zero" 0 "$_rc"
assert_contains "Q10 take: Gold=6000"     "$STATE" "Gold=6000"
assert_contains "Q10 take: QuestsDone=1"  "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 11: The Orcs -- Act II (linear)
# =========================================================================

_rc=0; run_qtest -e quests.e Quest11 -s "$S/q11_win.script" || _rc=$?
check_rc "Q11 win exits zero" 0 "$_rc"
assert_contains "Q11: Gold=5800"          "$STATE" "Gold=5800"
assert_contains "Q11: QuestsDone=1"       "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 12: The Lost Treasure
# =========================================================================

_rc=0; run_qtest -e quests.e Quest12 -s "$S/q12_left.script" || _rc=$?
check_rc "Q12 left exits zero" 0 "$_rc"
assert_contains "Q12 left: Gold=8300"     "$STATE" "Gold=8300"
assert_contains "Q12 left: QuestsDone=1"  "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest12 -s "$S/q12_right.script" || _rc=$?
check_rc "Q12 right exits zero" 0 "$_rc"
assert_contains "Q12 right: Gold=8000"    "$STATE" "Gold=8000"

# =========================================================================
# Quest 13: The Spirit World (linear, 4-stage)
# =========================================================================

_rc=0; run_qtest -e quests.e Quest13 -s "$S/q13_win.script" || _rc=$?
check_rc "Q13 win exits zero" 0 "$_rc"
assert_contains "Q13: Gold=8000"          "$STATE" "Gold=8000"
assert_contains "Q13: QuestsDone=1"       "$STATE" "QuestsDone=1"
assert_contains "Q13: AddNews"            "$OUT"   "[MOCK News_AddNews]"

# =========================================================================
# Quest 14: The Wyvern Lord (linear, 3-stage)
# =========================================================================

_rc=0; run_qtest -e quests.e Quest14 -s "$S/q14_win.script" || _rc=$?
check_rc "Q14 win exits zero" 0 "$_rc"
assert_contains "Q14: Gold=7000"          "$STATE" "Gold=7000"
assert_contains "Q14: QuestsDone=1"       "$STATE" "QuestsDone=1"
assert_contains "Q14: AddNews"            "$OUT"   "[MOCK News_AddNews]"

# =========================================================================
# Quest 15: The Orc Lord
# =========================================================================

_rc=0; run_qtest -e quests.e Quest15 -s "$S/q15_charge.script" || _rc=$?
check_rc "Q15 charge exits zero" 0 "$_rc"
assert_contains "Q15 charge: Gold=7000"   "$STATE" "Gold=7000"
assert_contains "Q15 charge: PFlags=4"    "$STATE" "PFlags=4"
assert_contains "Q15 charge: QuestsDone=1" "$STATE" "QuestsDone=1"
assert_contains "Q15 charge: AddNews"     "$OUT"   "[MOCK News_AddNews]"

_rc=0; run_qtest -e quests.e Quest15 -s "$S/q15_flank.script" || _rc=$?
check_rc "Q15 flank exits zero" 0 "$_rc"
assert_contains "Q15 flank: Gold=6800"    "$STATE" "Gold=6800"
assert_contains "Q15 flank: PFlags=4"     "$STATE" "PFlags=4"

# =========================================================================
# Quest 16: The Dragon Lord (linear)
# =========================================================================

_rc=0; run_qtest -e quests.e Quest16 -s "$S/q16_win.script" || _rc=$?
check_rc "Q16 win exits zero" 0 "$_rc"
assert_contains "Q16: Gold=7500"          "$STATE" "Gold=7500"
assert_contains "Q16: QuestsDone=1"       "$STATE" "QuestsDone=1"
assert_contains "Q16: AddNews"            "$OUT"   "[MOCK News_AddNews]"

# =========================================================================
# Quest 17: The Demon Lord (linear, 4-stage)
# =========================================================================

_rc=0; run_qtest -e quests.e Quest17 -s "$S/q17_win.script" || _rc=$?
check_rc "Q17 win exits zero" 0 "$_rc"
assert_contains "Q17: Gold=8000"          "$STATE" "Gold=8000"
assert_contains "Q17: QuestsDone=1"       "$STATE" "QuestsDone=1"
assert_contains "Q17: AddNews"            "$OUT"   "[MOCK News_AddNews]"

# =========================================================================
# Quest 18: Sword of The Heavens (linear, 4-stage, GiveItem)
# =========================================================================

_rc=0; run_qtest -e quests.e Quest18 -s "$S/q18_win.script" || _rc=$?
check_rc "Q18 win exits zero" 0 "$_rc"
assert_contains "Q18: Gold=8500"          "$STATE" "Gold=8500"
assert_contains "Q18: PFlags=3"           "$STATE" "PFlags=3"
assert_contains "Q18: QuestsDone=1"       "$STATE" "QuestsDone=1"
assert_contains "Q18: GiveItem"           "$OUT"   "[MOCK Items_GiveItem]"
assert_contains "Q18: Heavenly Sword"     "$OUT"   "Heavenly Sword"
assert_contains "Q18: AddNews"            "$OUT"   "[MOCK News_AddNews]"

# =========================================================================
# Quest 19: The Poet's Quill
# =========================================================================

_rc=0; run_qtest -e quests.e Quest19 -s "$S/q19_fight.script" || _rc=$?
check_rc "Q19 fight exits zero" 0 "$_rc"
assert_contains "Q19 fight: Gold=5600"    "$STATE" "Gold=5600"
assert_contains "Q19 fight: QuestsDone=1" "$STATE" "QuestsDone=1"

_rc=0; run_qtest -e quests.e Quest19 -s "$S/q19_negotiate_pay.script" || _rc=$?
check_rc "Q19 negotiate+pay exits zero" 0 "$_rc"
assert_contains "Q19 pay: Gold=4900"      "$STATE" "Gold=4900"
assert_contains "Q19 pay: QuestsDone=1"   "$STATE" "QuestsDone=1"

_rc=0; run_qtest -g100 -e quests.e Quest19 -s "$S/q19_negotiate_broke.script" || _rc=$?
check_rc "Q19 negotiate broke exits zero" 0 "$_rc"
assert_contains "Q19 broke: Gold=700"     "$STATE" "Gold=700"
assert_contains "Q19 broke: QuestsDone=1" "$STATE" "QuestsDone=1"

# =========================================================================
# Quest 20: The Dark One
# =========================================================================

_rc=0; run_qtest -P3 -e quests.e Quest20 -s "$S/q20_win_sword.script" || _rc=$?
check_rc "Q20 win+sword exits zero" 0 "$_rc"
assert_contains "Q20 sword: Gold=10000"   "$STATE" "Gold=10000"
assert_contains "Q20 sword: GFlags=0"     "$STATE" "GFlags=0"
assert_contains "Q20 sword: QuestsDone=1" "$STATE" "QuestsDone=1"
assert_contains "Q20 sword: AddNews"      "$OUT"   "[MOCK News_AddNews]"

_rc=0; run_qtest -e quests.e Quest20 -s "$S/q20_win_nosword.script" || _rc=$?
check_rc "Q20 win no sword exits zero" 0 "$_rc"
assert_contains "Q20 no sword: Gold=10000" "$STATE" "Gold=10000"
assert_contains "Q20 no sword: GFlags=0"  "$STATE" "GFlags=0"

finish
