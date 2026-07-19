#!/bin/sh
# M5 Task 3 acceptance: boot Flight of the Amazon Queen headless, once per
# data set (Talkie speech build, Floppy text-only build), assert PPM frames
# come out of both. Exit 0 = pass. Requires xtrn/syncqueen/talkie/ and
# xtrn/syncqueen/floppy/ (run xtrn/syncqueen/getdata.js once).
#
# Unlike boot_bass.sh, --path points straight at each variant's own directory
# rather than the package root: sst_select_datadir() (door/sst_io.c) only
# picks talkie/ vs floppy/ under the given --path, so pointing it AT
# talkie/ or floppy/ directly (neither of which has its own nested
# talkie/floppy sub-dir) makes it fall back to using that directory as-is --
# exercising each build independently of the audio-availability probe, which
# a headless run cannot answer the same way a real terminal would.
#
# No SYNCSCUMM_DATA / --extrapath engine-data is needed for Queen (unlike
# BASS): both freeware builds are self-contained (see
# xtrn/syncqueen/install-xtrn.ini's provenance comment) -- verified here by
# NOT setting it and still booting clean.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/syncscumm"
PKG="$DOOR/../../../xtrn/syncqueen"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$PKG/talkie/queen.1c" ] || { echo "FAIL: run xtrn/syncqueen/getdata.js first"; exit 1; }
[ -f "$PKG/floppy/queen.1" ] || { echo "FAIL: run xtrn/syncqueen/getdata.js first"; exit 1; }

for V in talkie floppy; do
	DUMP=$(mktemp -d)
	INI=$(mktemp)
	trap 'rm -rf "$DUMP"; rm -f "$INI"' EXIT
	# -c a scratch config: with none given, ScummVM writes a default
	# scummvm.ini into the CWD, littering the source tree every run.
	SYNCSCUMM_DUMP="$DUMP" timeout 25 "$BIN" --path="$PKG/$V" -c "$INI" queen \
		> "$DUMP/boot.log" 2>&1 || true   # timeout kill (124/143) is the normal end
	FRAMES=$(ls "$DUMP" | grep -c '^frame[0-9]*\.ppm$' || true)
	[ "$FRAMES" -ge 5 ] || { echo "FAIL($V): only $FRAMES frames dumped"; cat "$DUMP/boot.log"; exit 1; }
	FIRST="$DUMP/$(ls "$DUMP" | grep '^frame[0-9]*\.ppm$' | sort | head -1)"
	HDR=$(head -c 32 "$FIRST" | tr '\n' ' ')
	case "$HDR" in "P6 320 200 255 "*) ;; *) echo "FAIL($V): bad PPM header: $HDR"; exit 1;; esac
	# at least one frame must have non-black content (intro art)
	OK=0
	for f in "$DUMP"/frame*.ppm; do
		if [ "$(tail -c +16 "$f" | tr -d '\0' | wc -c)" -gt 1000 ]; then
			OK=1
			break
		fi
	done
	[ "$OK" -eq 1 ] || { echo "FAIL($V): all frames black"; exit 1; }
	echo "BOOT-QUEEN OK ($V, $FRAMES frames)"
	rm -rf "$DUMP"
	rm -f "$INI"
	trap - EXIT
done
