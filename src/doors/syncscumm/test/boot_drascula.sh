#!/bin/sh
# Acceptance: confirm the Drascula game data in the xtrn/syncdrascula PACKAGE
# is detected by our binary as game id "drascula". Exit 0 = pass. Requires
# xtrn/syncdrascula/ populated (run xtrn/syncdrascula/getdata.js once -- a
# freeware game fetched from ScummVM's own hosting, not bundled; see that
# package's README).
#
# Unlike the other boot_*.sh checks, this asserts DETECTION rather than a frame
# count. Drascula's opening advances on the CD-music/sound clock and player
# input; in the silent, input-less headless SYNCSCUMM_DUMP mode it sits on the
# intro's first (black) frame instead of free-running like BASS's or Cascade
# Quest's timer-driven intros. So the deterministic, headless-valid check here
# is that the data resolves to the right game id (drascula:drascula) -- actual
# rendering is a live play-test item.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/syncscumm"
PKG="$DOOR/../../../xtrn/syncdrascula"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$PKG/Packet.001" ] || { echo "FAIL: run xtrn/syncdrascula/getdata.js first"; exit 1; }

OUT=$("$BIN" --path="$PKG" --detect 2>&1 || true)
if echo "$OUT" | grep -qiE '^drascula:drascula'; then
	echo "BOOT-DRASCULA OK (detected drascula:drascula)"; exit 0
fi
echo "FAIL: game data did not detect as drascula:drascula"; echo "$OUT"; exit 1
