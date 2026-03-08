#!/bin/sh
#
# Integration tests for ecomp.
# Input:  event script (.evt or .txt)
# Output: compiled event binary (.e or .q)

cd "$(dirname "$0")"
TOOL="${BINDIR}ecomp${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/ecomp"
T="${CLANS_TMPDIR}/ecomp"
mkdir -p "$T"

# --- happy path: .evt -> .e ---
assert_succeeds  "valid .evt exits zero" \
	"$TOOL" "$F/valid.evt" "$T/out.e"
assert_nonempty  "valid .evt produces .e file" \
	"$T/out.e"

# --- happy path: same source compiled to .q ---
assert_succeeds  "valid .evt to .q exits zero" \
	"$TOOL" "$F/valid.evt" "$T/out.q"
assert_nonempty  "valid .evt produces .q file" \
	"$T/out.q"

# --- comprehensive: Result, Option, Prompt, TellQuest, SetFlag, etc. ---
assert_succeeds  "comprehensive.evt exits zero" \
	"$TOOL" "$F/comprehensive.evt" "$T/comprehensive.e"
assert_nonempty  "comprehensive.evt produces .e file" \
	"$T/comprehensive.e"

# --- Fight/Heal/AddEnemy/EndQ ---
assert_succeeds  "fight_addenemy.evt exits zero" \
	"$TOOL" "$F/fight_addenemy.evt" "$T/fight.e"
assert_nonempty  "fight_addenemy.evt produces .e file" \
	"$T/fight.e"

# --- {legal} prefix, >> block comments, $ terminator ---
assert_succeeds  "comments_legal.evt exits zero" \
	"$TOOL" "$F/comments_legal.evt" "$T/comments.e"
assert_nonempty  "comments_legal.evt produces .e file" \
	"$T/comments.e"

# --- Chat, Topic, TellTopic, Display, JoinClan, EndChat ---
assert_succeeds  "chat_topics.evt exits zero" \
	"$TOOL" "$F/chat_topics.evt" "$T/chat.e"
assert_nonempty  "chat_topics.evt produces .e file" \
	"$T/chat.e"

# --- GetKey (~, letter), Input ---
assert_succeeds  "getkey_input.evt exits zero" \
	"$TOOL" "$F/getkey_input.evt" "$T/getkey.e"
assert_nonempty  "getkey_input.evt produces .e file" \
	"$T/getkey.e"

# --- FalseFlag and ResetEnemies (no-op keywords, silently ignored) ---
assert_succeeds  "falseflag.evt exits zero" \
	"$TOOL" "$F/falseflag.evt" "$T/falseflag.e"
assert_nonempty  "falseflag.evt produces .e file" \
	"$T/falseflag.e"

# --- error: wrong arg count ---
assert_fails     "wrong argc exits nonzero" \
	"$TOOL" "$F/valid.evt"

# --- error: missing input file ---
assert_fails     "missing input exits nonzero" \
	"$TOOL" "$T/nonexistent.evt" "$T/err.e"

finish
