#!/bin/sh
# M2 acceptance (offline half): boot BASS with the terminal path forced into
# file-capture mode; assert real sixel frames were emitted.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/scummvm"
GAME="$HERE/games/bass"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$GAME/sky.dsk" ] || { echo "FAIL: run test/fetch_bass.sh first"; exit 1; }
CAP=$(mktemp -d)
trap 'rm -rf "$CAP"' EXIT
INI="$CAP/scummvm.ini"
SYNCSCUMM_DATA="$DOOR/scummvm/dists/engine-data" \
SYNCSCUMM_SIXELOUT="$CAP/frames.six" timeout 20 "$BIN" --path="$GAME" \
  -c "$INI" sky > "$CAP/boot.log" 2>&1 || true
[ -s "$CAP/frames.six" ] || { echo "FAIL: no sixel output"; exit 1; }
# DCS sixel introducer + raster attributes + palette definition + ST
# (grep -P is GNU-only; build the ESC P bytes with printf instead so this
# works under any POSIX grep, not just GNU grep)
pat=$(printf '\033P')
grep -qa "$pat" "$CAP/frames.six" || { echo "FAIL: no DCS introducer"; exit 1; }
grep -qa  '"1;1;'  "$CAP/frames.six" || { echo "FAIL: no raster attributes"; exit 1; }
grep -qa  '#0;2;'  "$CAP/frames.six" || { echo "FAIL: no palette registers"; exit 1; }
SIZE=$(wc -c < "$CAP/frames.six")
[ "$SIZE" -gt 10000 ] || { echo "FAIL: capture too small ($SIZE)"; exit 1; }
echo "BOOT-SIXEL OK ($SIZE bytes)"
