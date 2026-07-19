#!/bin/sh
# Acceptance: boot Lure of the Temptress headless from the xtrn/synclure
# PACKAGE, asserting PPM frames come out. Exit 0 = pass. Requires
# xtrn/synclure/ populated (run xtrn/synclure/getdata.js once -- freeware game
# data fetched from ScummVM's own hosting, not bundled; see that README).
#
# The lure engine is exercised here (SCUMM by boot_bass_pkg.sh / boot_queen.sh,
# AGI by boot_spacequest0.sh, SCI by boot_cascadequest.sh, drascula by
# boot_drascula.sh). Lure's game data (Disk1.vga .. Disk4.vga) is a flat set,
# and its engine-data file lure.dat ships in the package, so --path points
# straight at the package dir (as the door's default --path does after
# sst_select_datadir() falls back to the base). ScummVM detects the data as
# game id "lure" (VGA/DOS/English); unlike Drascula, Lure's intro free-runs, so
# a frame-count check is valid here.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/syncscumm"
PKG="$DOOR/../../../xtrn/synclure"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$PKG/Disk1.vga" ] || { echo "FAIL: run xtrn/synclure/getdata.js first"; exit 1; }
[ -f "$PKG/lure.dat" ] || { echo "FAIL: lure.dat missing from the package"; exit 1; }

DUMP=$(mktemp -d)
INI=$(mktemp)
trap 'rm -rf "$DUMP"; rm -f "$INI"' EXIT
# -c a scratch config: with none given, ScummVM writes a default scummvm.ini
# into the CWD, littering the source tree every run.
SYNCSCUMM_DUMP="$DUMP" timeout 25 "$BIN" --path="$PKG" -c "$INI" lure \
	> "$DUMP/boot.log" 2>&1 || true   # timeout kill (124/143) is the normal end
FRAMES=$(ls "$DUMP" | grep -c '^frame[0-9]*\.ppm$' || true)
[ "$FRAMES" -ge 5 ] || { echo "FAIL: only $FRAMES frames dumped"; cat "$DUMP/boot.log"; exit 1; }
FIRST="$DUMP/$(ls "$DUMP" | grep '^frame[0-9]*\.ppm$' | sort | head -1)"
HDR=$(head -c 32 "$FIRST" | tr '\n' ' ')
case "$HDR" in "P6 320 200 255 "*) ;; *) echo "FAIL: bad PPM header: $HDR"; exit 1;; esac
# at least one frame must have non-black content (intro art)
for f in "$DUMP"/frame*.ppm; do
	if [ "$(tail -c +16 "$f" | tr -d '\0' | wc -c)" -gt 500 ]; then
		echo "BOOT-LURE OK ($FRAMES frames)"; exit 0
	fi
done
echo "FAIL: all frames black"; exit 1
