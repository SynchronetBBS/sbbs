#!/usr/bin/env bash
#
# Headless save/load regression test for SyncDuke.
#
# Drives the real door binary into gameplay, then the in-engine self-test hook
# (compiled in, gated by SYNCDUKE_SAVETEST=1) calls saveplayer(0) and loadplayer(0)
# directly from the sim loop and keeps the game running afterward. This exercises the
# 64-bit save/load path -- the CON-script-pointer relocation in saveplayer/loadplayer
# (menues.c) -- without any fragile menu navigation. Before the LP64 fix, saving
# truncated the live CON pointers and the next execute() SIGSEGV'd; this test fails
# (the door dies with signal SEGV) if that regresses.
#
# Pass criteria: the door does NOT crash, both "save returned"/"load returned" markers
# are logged, and a non-empty game<slot>.sav is written.
#
# Usage: tests/save_load_test.sh [path-to-syncduke-binary] [path-to-DUKE3D.GRP]
set -u

here="$(cd "$(dirname "$0")" && pwd)"
door="${1:-$here/../build/syncduke}"
grp="${2:-$here/../build/DUKE3D.GRP}"

[ -x "$door" ] || { echo "FAIL: door binary not found/executable: $door"; exit 2; }
[ -f "$grp" ]  || { echo "SKIP: DUKE3D.GRP not found: $grp (shareware GRP not bundled)"; exit 77; }

run="$(mktemp -d)"; trap 'rm -rf "$run"' EXIT
cp "$grp" "$run/" || { echo "FAIL: cannot stage GRP"; exit 2; }
fifo="$run/keys.fifo"; mkfifo "$fifo"
err="$run/door.err"

# DOOR32.SYS: commtype=0 (local) so no socket; the door falls back to stdin/stdout,
# which we drive via the FIFO and discard.
printf '0\n0\n0\nTEST\n1\nTester\nTester\n90\n60\n1\n' > "$run/door32.sys"

# Feed keystrokes: skip the intro, start a new game (4x Enter on defaults), then walk
# forward so the sim keeps ticking. The save/load hook fires automatically once in
# gameplay (~100 tics in, then load ~200 tics in).
(
  exec > "$fifo"
  sleep 4.0                                   # cold start: GRP load + CON compile + art load
  for _ in 1 2 3 4; do printf '\r'; sleep 1.2; done   # skip intro, New Game -> episode -> skill -> start
  for _ in $(seq 1 90); do printf 'w'; sleep 0.1; done  # walk so the sim keeps ticking
  sleep 1.0
) &
feeder=$!

( cd "$run" && SYNCDUKE_SAVETEST=1 SYNCDUKE_SIXELOUT=/dev/null \
    timeout 25 "$door" door32.sys > /dev/null 2> "$err" < "$fifo" )
st=$?
kill "$feeder" 2>/dev/null; wait "$feeder" 2>/dev/null

ok=1
# 139 = 128+SIGSEGV, 134 = 128+SIGABRT -> a crash; 124 = timeout (we killed it) = survived.
if [ "$st" -eq 139 ] || [ "$st" -eq 134 ] || [ "$st" -eq 136 ]; then
  echo "FAIL: door crashed (exit $st)"; ok=0
fi
grep -q "SYNCDUKE_SAVETEST: save returned" "$err" || { echo "FAIL: saveplayer did not return"; ok=0; }
grep -q "SYNCDUKE_SAVETEST: load returned" "$err" || { echo "FAIL: loadplayer did not return"; ok=0; }
sav="$run/game0.sav"
if [ ! -s "$sav" ]; then echo "FAIL: game0.sav not written (or empty)"; ok=0; fi

echo "--- self-test log ---"; grep -a "SYNCDUKE_SAVETEST" "$err" || true
if [ "$ok" -eq 1 ]; then
  echo "PASS: save+load survived, $(stat -c %s "$sav") byte save written"
  exit 0
fi
exit 1
