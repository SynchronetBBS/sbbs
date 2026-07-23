#!/bin/sh
# Acceptance: boot Betrayed Alliance Book 1 headless from the
# xtrn/betrayedalliance PACKAGE, asserting PPM frames come out. Exit 0 = pass.
# Requires xtrn/betrayedalliance/ populated (run its getdata.js once -- an
# MIT-licensed freeware game fetched from the author's GitHub, not bundled).
#
# A second SCI-engine boot check (alongside boot_cascadequest.sh). The SCI
# resource files are a flat set with no talkie/floppy split, so --path points
# straight at the package dir (as the door's default --path does after
# termgfx_select_datadir() falls back to the base). ScummVM detects the data as
# game id "sci-fanmade" (Betrayed Alliance 1.3.3.1); it renders EGA doubled to
# a 320x200 surface, parser/keyboard-driven, so no input is needed for the
# intro.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/syncscumm"
PKG="$DOOR/../../../xtrn/betrayedalliance"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$PKG/resource.map" ] || { echo "FAIL: run xtrn/betrayedalliance/getdata.js first"; exit 1; }

DUMP=$(mktemp -d)
INI=$(mktemp)
trap 'rm -rf "$DUMP"; rm -f "$INI"' EXIT
# -c a scratch config: with none given, ScummVM writes a default scummvm.ini
# into the CWD, littering the source tree every run.
SYNCSCUMM_DUMP="$DUMP" timeout 25 "$BIN" --path="$PKG" -c "$INI" sci-fanmade \
	> "$DUMP/boot.log" 2>&1 || true   # timeout kill (124/143) is the normal end
FRAMES=$(ls "$DUMP" | grep -c '^frame[0-9]*\.ppm$' || true)
[ "$FRAMES" -ge 5 ] || { echo "FAIL: only $FRAMES frames dumped"; cat "$DUMP/boot.log"; exit 1; }
FIRST="$DUMP/$(ls "$DUMP" | grep '^frame[0-9]*\.ppm$' | sort | head -1)"
HDR=$(head -c 32 "$FIRST" | tr '\n' ' ')
case "$HDR" in "P6 320 200 255 "*) ;; *) echo "FAIL: bad PPM header: $HDR"; exit 1;; esac
# at least one frame must have non-black content (intro art)
for f in "$DUMP"/frame*.ppm; do
	if [ "$(tail -c +16 "$f" | tr -d '\0' | wc -c)" -gt 500 ]; then
		echo "BOOT-BETRAYEDALLIANCE OK ($FRAMES frames)"; exit 0
	fi
done
echo "FAIL: all frames black"; exit 1
