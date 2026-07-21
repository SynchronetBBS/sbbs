#!/bin/sh
# run_wren.sh — Compile / execute a Wren script under SyncTERM headless
#
# Drives SyncTERM with the SDL backend in offscreen mode, exposes this
# checkout's scripts directory through an isolated XDG data root, and loads
# the named Wren script via -W after the embedded + source auto-load chain
# has finished.  Ordinary scripts get a short-lived /usr/bin/true PTY.
# wrentest.wren gets the persistent Bash PTY its connection tests require;
# the suite sends "exit" after its final report.
#
# Exit code: ordinary scripts succeed if no "[wren] " lines hit stderr.
# wrentest.wren succeeds only after reporting a TOTAL with zero failures.
#
# Usage: run_wren.sh <script.wren>
#
# The harness uses the GNUmakefile-default debug build at
#     clang.freebsd.amd64.exe.debug/syncterm
# (relative to this script).  Set the SYNCTERM env var to override
# (e.g. when working with a different BUILDPATH).

set -u

SCRIPT="${1:-}"
if [ -z "$SCRIPT" ]; then
	echo "Usage: $0 <script.wren>" >&2
	exit 1
fi

HERE=$(cd "$(dirname "$0")" && pwd)
SYNCTERM="${SYNCTERM:-$HERE/clang.freebsd.amd64.exe.debug/syncterm}"

if [ ! -x "$SYNCTERM" ]; then
	echo "syncterm binary not found at: $SYNCTERM" >&2
	echo "Run \`gmake\` from $HERE, or set SYNCTERM env var." >&2
	exit 1
fi

OUT=$(mktemp /tmp/run_wren.out.XXXXXX)
ERR=$(mktemp /tmp/run_wren.err.XXXXXX)
DATA_HOME=$(mktemp -d /tmp/run_wren.data.XXXXXX)
mkdir -p "$DATA_HOME/syncterm"
ln -s "$HERE/scripts" "$DATA_HOME/syncterm/scripts"
trap 'rm -f "$OUT" "$ERR"; rm -rf "$DATA_HOME"' EXIT

# Load library, test, and auto-run modules from the source tree.  This keeps
# the harness independent of whatever the developer has installed in their
# personal SyncTERM scripts directory.
export XDG_DATA_HOME="$DATA_HOME"

export SDL_VIDEODRIVER=offscreen
export SDL_RENDER_DRIVER=software
export SDL_VIDEO_EGL_DRIVER=none

SCRIPT_NAME=${SCRIPT##*/}
CONNECTION='shell:/usr/bin/true'
FULL_SUITE=false
if [ "$SCRIPT_NAME" = "wrentest.wren" ]; then
	CONNECTION='shell:/bin/bash'
	FULL_SUITE=true
fi

# A normal shell disconnect can make SyncTERM return non-zero even after the
# script completed, so the captured diagnostics/report are authoritative.
timeout 30 "$SYNCTERM" -iS -S -Q -W "$SCRIPT" "$CONNECTION" \
	>"$OUT" 2>"$ERR"
STATUS=$?
cat "$OUT"

if [ "$STATUS" -eq 124 ]; then
	echo "Wren test timed out" >&2
	cat "$ERR" >&2
	exit 1
fi

if $FULL_SUITE; then
	if grep -Eq '^=== TOTAL: [0-9]+ tests, [0-9]+ pass, 0 fail ===$' "$OUT"; then
		exit 0
	fi
	cat "$ERR" >&2
	exit 1
fi

if grep -q '^\[wren\] ' "$ERR"; then
	cat "$ERR" >&2
	exit 1
fi
exit 0
