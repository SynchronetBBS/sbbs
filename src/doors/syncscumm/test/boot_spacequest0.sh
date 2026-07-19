#!/bin/sh
# Acceptance: boot Space Quest 0: Replicated headless from the xtrn/spacequest0
# PACKAGE, asserting PPM frames come out. Exit 0 = pass. Requires
# xtrn/spacequest0/ populated (run xtrn/spacequest0/getdata.js once -- a fan
# game fetched from the author's host, not bundled; see that package's README).
#
# This is the AGI-engine boot check (the SCUMM engine is covered by
# boot_bass_pkg.sh / boot_queen.sh). AGI game data (LOGDIR + VOL.*) is a flat
# set with no talkie/floppy split, so --path points straight at the package
# dir (as the door's default --path does after sst_select_datadir() finds no
# ./talkie or ./floppy sub-dir and falls back to the base). ScummVM detects the
# data as game id "sq0"; AGI renders 16-color 160x168 doubled to a 320x200
# surface, keyboard/parser-driven (no mouse), so no input is needed to reach
# the intro art.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/syncscumm"
PKG="$DOOR/../../../xtrn/spacequest0"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$PKG/LOGDIR" ] || { echo "FAIL: run xtrn/spacequest0/getdata.js first"; exit 1; }

DUMP=$(mktemp -d)
INI=$(mktemp)
trap 'rm -rf "$DUMP"; rm -f "$INI"' EXIT
# -c a scratch config: with none given, ScummVM writes a default scummvm.ini
# into the CWD, littering the source tree every run.
SYNCSCUMM_DUMP="$DUMP" timeout 20 "$BIN" --path="$PKG" -c "$INI" sq0 \
	> "$DUMP/boot.log" 2>&1 || true   # timeout kill (124/143) is the normal end
FRAMES=$(ls "$DUMP" | grep -c '^frame[0-9]*\.ppm$' || true)
[ "$FRAMES" -ge 5 ] || { echo "FAIL: only $FRAMES frames dumped"; cat "$DUMP/boot.log"; exit 1; }
FIRST="$DUMP/$(ls "$DUMP" | grep '^frame[0-9]*\.ppm$' | sort | head -1)"
HDR=$(head -c 32 "$FIRST" | tr '\n' ' ')
case "$HDR" in "P6 320 200 255 "*) ;; *) echo "FAIL: bad PPM header: $HDR"; exit 1;; esac
# at least one frame must have non-black content (intro art)
for f in "$DUMP"/frame*.ppm; do
	if [ "$(tail -c +16 "$f" | tr -d '\0' | wc -c)" -gt 500 ]; then
		echo "BOOT-SPACEQUEST0 OK ($FRAMES frames)"; exit 0
	fi
done
echo "FAIL: all frames black"; exit 1
