#!/bin/sh
#
# Integration tests for gtest (game/combat test tool).
#

cd "$(dirname "$0")"
TOOL="${BINDIR}gtest${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/gtest"
T="${CLANS_TMPDIR}/gtest"
mkdir -p "$T"

# Locate compiled data files (two dirs above tests/).
CLANSRC="$(cd "$FIXTURE_DIR/../.." && pwd)"
LANGFILE="${CLANSRC}/data/strings.xl"
PAKFILE="${CLANSRC}/data/clans.pak"
if [ ! -f "$LANGFILE" ]; then
	printf "SKIP  language file not built: %s\n" "$LANGFILE"
	exit 0
fi
if [ ! -f "$PAKFILE" ]; then
	printf "SKIP  clans.pak not built: %s\n" "$PAKFILE"
	exit 0
fi

# Set up working directory with symlinks to data files.
ln -sf "$LANGFILE" "$T/strings.xl"
ln -sf "$PAKFILE"  "$T/clans.pak"
cat > "$T/clans.ini" << 'INI'
Language   strings.xl
Spells     /dat/Spells
Classes    /dat/Classes
Races      /dat/Races
INI

OUT="$T/out.txt"
STATE="$T/state.txt"

# run_gtest COMMAND [args...] — run gtest in the temp directory,
# capturing stdout to $OUT and stderr (state summary) to $STATE.
run_gtest() {
	_cmd="$1"
	shift
	(cd "$T" && "$TOOL" -c "$_cmd" "$@") >"$OUT" 2>"$STATE"
}

# =========================================================================
# Argument validation
# =========================================================================

assert_fails "no args exits nonzero" \
	"$TOOL"

assert_fails "unknown command exits nonzero" \
	"$TOOL" -c bogus

assert_fails "missing -c value exits nonzero" \
	"$TOOL" -c

# =========================================================================
# Autofight — fixed random (fully deterministic, no PRNG)
# =========================================================================

assert_succeeds "autofight -r 1 exits zero" \
	run_gtest autofight -l 1 -r 1
assert_contains "has FightResult"  "$STATE" "FightResult="
assert_contains "has FightsLeft=9" "$STATE" "FightsLeft=9"
assert_contains "has Member0.Name" "$STATE" "Member0.Name=Fighter"
assert_contains "has Points"       "$STATE" "Points="

# Same -r value always produces same result
cp "$STATE" "$T/fixed1.txt"
assert_succeeds "fixed rand reproducible" \
	run_gtest autofight -l 1 -r 1
if diff -q "$T/fixed1.txt" "$STATE" >/dev/null 2>&1; then
	pass_test "deterministic: same -r = same state"
else
	fail_test "deterministic: same -r = same state"
fi

# Different -r value produces different result
assert_succeeds "different -r runs" \
	run_gtest autofight -l 1 -r 50
if diff -q "$T/fixed1.txt" "$STATE" >/dev/null 2>&1; then
	fail_test "different -r = different state"
else
	pass_test "different -r = different state"
fi

# =========================================================================
# Autofight — seeded PRNG
# =========================================================================

assert_succeeds "autofight -R 42 exits zero" \
	run_gtest autofight -l 1 -R 42

# =========================================================================
# Autofight — higher level
# =========================================================================

assert_succeeds "autofight level 5 exits zero" \
	run_gtest autofight -l 5 -r 1
assert_contains "level 5 has result" "$STATE" "FightResult="

# =========================================================================
# Autofight — gold argument
# =========================================================================

assert_succeeds "autofight with -g exits zero" \
	run_gtest autofight -l 1 -r 1 -g9999
assert_not_contains "gold not default" "$STATE" "Gold=5000"

# =========================================================================
# Autofight — loss/run at high level
# =========================================================================

assert_succeeds "autofight level 20 -r 0" \
	run_gtest autofight -l 20 -r 0
assert_not_contains "high level not won" "$STATE" "FightResult=WON"
assert_contains "not-won loses points"   "$STATE" "Points=-3"

# =========================================================================
# Autofight — XP gained on win
# =========================================================================

assert_succeeds "autofight XP check" \
	run_gtest autofight -l 1 -r 1
assert_not_contains "fighter gained XP" "$STATE" "Member0.XP=0"
assert_not_contains "mage gained XP"    "$STATE" "Member1.XP=0"

# =========================================================================
# Autofight — gold increases on win
# =========================================================================

assert_succeeds "autofight gold check" \
	run_gtest autofight -l 1 -r 1
assert_not_contains "gold increased"    "$STATE" "Gold=5000"
assert_contains "win earns points"      "$STATE" "Points=5"

# =========================================================================
# Autofight — fight result always present
# =========================================================================

assert_succeeds "autofight level 10 runs" \
	run_gtest autofight -l 10 -r 1
assert_contains "level 10 has result"   "$STATE" "FightResult="
assert_contains "level 10 has HP"       "$STATE" "Member0.HP="

# =========================================================================
# Manual fight — attack
# =========================================================================

assert_succeeds "fight -a A wins" \
	run_gtest fight -l 1 -r 1 -a A
assert_contains "fight attack wins"     "$STATE" "FightResult=WON"
assert_contains "fight attack points"   "$STATE" "Points=5"
assert_not_contains "fight attack XP"   "$STATE" "Member0.XP=0"

# =========================================================================
# Manual fight — run
# =========================================================================

assert_succeeds "fight -a R runs" \
	run_gtest fight -l 1 -r 1 -a R
assert_contains "fight run result"      "$STATE" "FightResult=RAN"
assert_contains "fight run loses pts"   "$STATE" "Points=-3"

# =========================================================================
# Manual fight — deterministic
# =========================================================================

assert_succeeds "fight -a A -r 1 first" \
	run_gtest fight -l 1 -r 1 -a A
cp "$STATE" "$T/fight1.txt"

assert_succeeds "fight -a A -r 1 second" \
	run_gtest fight -l 1 -r 1 -a A
if diff -q "$T/fight1.txt" "$STATE" >/dev/null 2>&1; then
	pass_test "manual fight deterministic"
else
	fail_test "manual fight deterministic"
fi

# =========================================================================
# Level-up
# =========================================================================

assert_succeeds "levelup exits zero" \
	run_gtest levelup
assert_contains "members leveled up"    "$STATE" "Member0.Level=2"
assert_contains "all members leveled"   "$STATE" "Member3.Level=2"
assert_contains "XP preserved"          "$STATE" "Member0.XP=1000"

# =========================================================================

finish
