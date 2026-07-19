#!/bin/sh
# M2 acceptance: subtitles precedence (user > sysop > auto), boot_bass.sh-style.
# Three headless boots, ~8s timeouts each, asserting the one stderr line
# door/syncscumm.cpp's resolveSubtitles() logs for each precedence case.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/syncscumm"
GAME="$HERE/games/bass"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$GAME/sky.dsk" ] && [ -f "$GAME/sky.dnr" ] || { echo "FAIL: run test/fetch_bass.sh first"; exit 1; }
DATA="$DOOR/scummvm/dists/engine-data"

# (a) No sysop ini, no user "subtitles" key at all -> auto. These boots are
# headless (no terminal fd), so there is no session to probe for audio and
# sst_io_audio_available() reports none immediately -> subtitles on.
CWD_A=$(mktemp -d)
DUMP_A=$(mktemp -d)
INI_A=$(mktemp)
trap 'rm -rf "$CWD_A" "$DUMP_A" "$INI_A" "$CWD_B" "$DUMP_B" "$INI_B" "$CWD_C" "$DUMP_C" "$INI_C"' EXIT
(
	cd "$CWD_A"
	SYNCSCUMM_DATA="$DATA" SYNCSCUMM_DUMP="$DUMP_A" timeout 8 "$BIN" \
		--path="$GAME" -c "$INI_A" sky > "$DUMP_A/boot.log" 2>&1
) || true   # timeout kill (124/143) is the normal end, as in boot_bass.sh
grep -q "syncscumm: subtitles auto -> on (no audio this session)" "$DUMP_A/boot.log" \
	|| { echo "FAIL(a): no auto->on line"; cat "$DUMP_A/boot.log" >&2; exit 1; }

# (b) Sysop syncscumm.ini in the door's real CWD (startup_dir) says off ->
# sysop wins, and the auto/transient "on" decision must NOT also appear.
CWD_B=$(mktemp -d)
DUMP_B=$(mktemp -d)
INI_B=$(mktemp)
printf 'subtitles = off\n' > "$CWD_B/syncscumm.ini"
(
	cd "$CWD_B"
	SYNCSCUMM_DATA="$DATA" SYNCSCUMM_DUMP="$DUMP_B" timeout 8 "$BIN" \
		--path="$GAME" -c "$INI_B" sky > "$DUMP_B/boot.log" 2>&1
) || true
grep -q "syncscumm: subtitles: sysop off" "$DUMP_B/boot.log" \
	|| { echo "FAIL(b): no sysop-off line"; cat "$DUMP_B/boot.log" >&2; exit 1; }
grep -q "syncscumm: subtitles auto -> on" "$DUMP_B/boot.log" \
	&& { echo "FAIL(b): auto->on also fired -- sysop should have short-circuited it"; exit 1; }

# (c) User's own "-c" file already sets subtitles (persistent [scummvm]
# domain) AND a sysop ini says off -> user wins over sysop.
CWD_C=$(mktemp -d)
DUMP_C=$(mktemp -d)
INI_C=$(mktemp)
printf 'subtitles = off\n' > "$CWD_C/syncscumm.ini"
printf '[scummvm]\nsubtitles=true\n' > "$INI_C"
(
	cd "$CWD_C"
	SYNCSCUMM_DATA="$DATA" SYNCSCUMM_DUMP="$DUMP_C" timeout 8 "$BIN" \
		--path="$GAME" -c "$INI_C" sky > "$DUMP_C/boot.log" 2>&1
) || true
grep -q "syncscumm: subtitles: user preference respected" "$DUMP_C/boot.log" \
	|| { echo "FAIL(c): no user-preference line"; cat "$DUMP_C/boot.log" >&2; exit 1; }

echo "SUBTITLES OK (user > sysop > auto)"
