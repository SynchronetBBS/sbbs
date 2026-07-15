#!/bin/sh
# M1 acceptance: boot BASS headless, assert PPM frames come out.
# Exit 0 = pass. Requires test/games/bass (run fetch_bass.sh once).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/scummvm"
GAME="$HERE/games/bass"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$GAME/sky.dsk" ] && [ -f "$GAME/sky.dnr" ] || { echo "FAIL: run test/fetch_bass.sh first"; exit 1; }
# Exercise the SYNCSCUMM_DATA search-set override for real: run from a game
# copy WITHOUT sky.cpt, so the engine can only find it via the search set
# (the fixture's own sky.cpt would otherwise satisfy --path and mask a
# broken override).
DUMP=$(mktemp -d)
trap 'rm -rf "$DUMP" "$INI"' EXIT
GAMECOPY="$DUMP/game"
mkdir -p "$GAMECOPY"
cp "$GAME/sky.dsk" "$GAME/sky.dnr" "$GAMECOPY/"
INI=$(mktemp)
SYNCSCUMM_DATA="$DOOR/scummvm/dists/engine-data" \
SYNCSCUMM_DUMP="$DUMP" timeout 20 "$BIN" --path="$GAMECOPY" \
  -c "$INI" sky \
  > "$DUMP/boot.log" 2>&1 || true   # timeout kill (124/143) is the normal end
FRAMES=$(ls "$DUMP" | grep -c '^frame[0-9]*\.ppm$' || true)
[ "$FRAMES" -ge 5 ] || { echo "FAIL: only $FRAMES frames dumped"; exit 1; }
FIRST="$DUMP/$(ls "$DUMP" | grep '^frame[0-9]*\.ppm$' | sort | head -1)"
HDR=$(head -c 32 "$FIRST" | tr '\n' ' ')
case "$HDR" in "P6 320 200 255 "*) ;; *) echo "FAIL: bad PPM header: $HDR"; exit 1;; esac
# at least one frame must have non-black content (intro art)
for f in "$DUMP"/frame*.ppm; do
	if [ "$(tail -c +16 "$f" | tr -d '\0' | wc -c)" -gt 1000 ]; then
		echo "BOOT-BASS OK ($FRAMES frames)"; exit 0
	fi
done
echo "FAIL: all frames black"; exit 1
