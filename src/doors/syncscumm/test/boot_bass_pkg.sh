#!/bin/sh
# Acceptance: boot Beneath a Steel Sky headless from the xtrn/syncbass
# PACKAGE, asserting PPM frames come out. Exit 0 = pass. Requires
# xtrn/syncbass/ populated (run xtrn/syncbass/getdata.js once).
#
# This complements boot_bass.sh, which boots the dev fixture (test/games/bass)
# with sky.cpt supplied via the SYNCSCUMM_DATA search set from a copy that has
# NO in-band sky.cpt. The package boots the opposite, real-world way: the CD
# build carries sky.cpt in the package dir itself, so --path finds it and NO
# SYNCSCUMM_DATA / --extrapath is set -- verified here by not setting it and
# still booting clean (the self-contained single-build path the door uses in
# production).
#
# --path points straight at the package dir (as the door's default --path does
# after sst_select_datadir() finds no ./talkie or ./floppy sub-dir and falls
# back to the base) -- exercising the flat single-build data set the same way
# a real session does.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/syncscumm"
PKG="$DOOR/../../../xtrn/syncbass"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$PKG/sky.cpt" ] || { echo "FAIL: run xtrn/syncbass/getdata.js first"; exit 1; }

DUMP=$(mktemp -d)
INI=$(mktemp)
trap 'rm -rf "$DUMP"; rm -f "$INI"' EXIT
# -c a scratch config: with none given, ScummVM writes a default scummvm.ini
# into the CWD, littering the source tree every run.
SYNCSCUMM_DUMP="$DUMP" timeout 20 "$BIN" --path="$PKG" -c "$INI" sky \
	> "$DUMP/boot.log" 2>&1 || true   # timeout kill (124/143) is the normal end
FRAMES=$(ls "$DUMP" | grep -c '^frame[0-9]*\.ppm$' || true)
[ "$FRAMES" -ge 5 ] || { echo "FAIL: only $FRAMES frames dumped"; cat "$DUMP/boot.log"; exit 1; }
FIRST="$DUMP/$(ls "$DUMP" | grep '^frame[0-9]*\.ppm$' | sort | head -1)"
HDR=$(head -c 32 "$FIRST" | tr '\n' ' ')
case "$HDR" in "P6 320 200 255 "*) ;; *) echo "FAIL: bad PPM header: $HDR"; exit 1;; esac
# at least one frame must have non-black content (intro art)
for f in "$DUMP"/frame*.ppm; do
	if [ "$(tail -c +16 "$f" | tr -d '\0' | wc -c)" -gt 1000 ]; then
		echo "BOOT-BASS-PKG OK ($FRAMES frames)"; exit 0
	fi
done
echo "FAIL: all frames black"; exit 1
