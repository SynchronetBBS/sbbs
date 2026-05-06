#!/bin/sh
# run_wren.sh — Compile / execute a Wren script under SyncTERM headless
#
# Drives SyncTERM with the SDL backend in offscreen mode, connects via
# a PTY to /usr/bin/true so the session ends immediately, and loads the
# named Wren script via -W after the embedded + user auto-load chain
# has finished.  Any compile or runtime errors caught by the host's
# Wren errorFn appear on standard error with the "[wren] " prefix; the
# script's own Host.print output reaches standard output.
#
# Exit code: 0 if no "[wren] " lines hit stderr, 1 otherwise — suitable
# for CI / pre-commit gating.
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

ERR=$(mktemp /tmp/run_wren.XXXXXX)
trap 'rm -f "$ERR"' EXIT

export SDL_VIDEODRIVER=offscreen
export SDL_RENDER_DRIVER=software
export SDL_VIDEO_EGL_DRIVER=none

# SyncTERM may exit non-zero on the failed shell:/usr/bin/true session
# even when the Wren load succeeded; ignore the exit code and decide
# based on stderr scraping instead.
timeout 30 "$SYNCTERM" -iS -S -Q -W "$SCRIPT" 'shell:/usr/bin/true' 2>"$ERR" || true

if grep -q '^\[wren\] ' "$ERR"; then
	grep '^\[wren\] ' "$ERR" >&2
	exit 1
fi
exit 0
