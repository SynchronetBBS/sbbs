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
# Usage: run_wren.sh [--menu|--menu-write|--picker] <script.wren>
#
# The harness uses the GNUmakefile-default debug build at
#     clang.freebsd.amd64.exe.debug/syncterm
# (relative to this script).  Set the SYNCTERM env var to override
# (e.g. when working with a different BUILDPATH).

set -u

MENU_SUITE=false
MENU_WRITABLE=false
PICKER_SUITE=false
if [ "${1:-}" = "--menu" ] || [ "${1:-}" = "--menu-write" ] ||
    [ "${1:-}" = "--picker" ]; then
	MENU_SUITE=true
	if [ "$1" = "--menu-write" ]; then
		MENU_WRITABLE=true
	elif [ "$1" = "--picker" ]; then
		PICKER_SUITE=true
	fi
	shift
fi

SCRIPT="${1:-}"
if [ -z "$SCRIPT" ]; then
	echo "Usage: $0 [--menu|--menu-write|--picker] <script.wren>" >&2
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
trap 'rm -f "$OUT" "$ERR"; rm -rf "$DATA_HOME"' EXIT

if $MENU_SUITE; then
	SCRIPT_ROOT="$DATA_HOME/syncterm/scripts"
	mkdir -p "$SCRIPT_ROOT/auto/menu"
	for MODULE in "$HERE"/scripts/*.wren; do
		ln -s "$MODULE" "$SCRIPT_ROOT/${MODULE##*/}"
	done
	if $PICKER_SUITE; then
		ln -s "$HERE/$SCRIPT" "$SCRIPT_ROOT/picker_test.wren"
		mkdir -p "$SCRIPT_ROOT/auto/picker"
		cat >"$SCRIPT_ROOT/auto/picker/file_picker.wren" <<'EOF'
import "picker_test" for PixelVmTest

class FilePicker {
  static run(request) {
    var result = PixelVmTest.run()
    System.print("=== PICKER TEST: %(result[0] + result[1]) tests, " +
        "%(result[0]) pass, %(result[1]) fail ===")
    request.cancel()
  }
}
EOF
	else
		ln -s "$HERE/$SCRIPT" "$SCRIPT_ROOT/menu_test.wren"
	fi
	if $PICKER_SUITE; then
		cat >"$SCRIPT_ROOT/auto/menu/main_menu.wren" <<'EOF'
import "syncterm" for Host
import "syncterm_menu" for Menu

class MainMenu {
  static prepare() { true }
  static offerSave(source) { false }
  static run(current, connected) {
    Host.pickFile(null, "*", 0)
    Menu.quitApplication()
    return null
  }
}
EOF
	else
		cat >"$SCRIPT_ROOT/auto/menu/main_menu.wren" <<'EOF'
import "syncterm_menu" for Menu
import "menu_test" for MenuTest

class MainMenu {
  static prepare() { true }
  static offerSave(source) { false }
  static run(current, connected) {
    var result = MenuTest.run()
    System.print("=== MENU TEST: %(result[0] + result[1]) tests, " +
        "%(result[0]) pass, %(result[1]) fail ===")
    Menu.quitApplication()
    return null
  }
}
EOF
	fi
else
	SCRIPT_ROOT="$DATA_HOME/syncterm/scripts"
	mkdir -p "$SCRIPT_ROOT"
	ln -s "$HERE/scripts/auto" "$SCRIPT_ROOT/auto"
	for MODULE in "$HERE"/scripts/*.wren; do
		ln -s "$MODULE" "$SCRIPT_ROOT/${MODULE##*/}"
	done

	# A launch script may belong to a separately installable package rather
	# than SyncTERM's embedded source tree.  Stage modules beside the launch
	# script and in a sibling package scripts/ directory so imports exercise
	# the package exactly as they will appear in the user's script root.
	case "$SCRIPT" in
		/*) SCRIPT_PATH="$SCRIPT" ;;
		*)  SCRIPT_PATH="$HERE/$SCRIPT" ;;
	esac
	SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$SCRIPT_PATH")" && pwd)
	for MODULE_DIR in "$SCRIPT_DIR" "$SCRIPT_DIR/../scripts"; do
		if [ -d "$MODULE_DIR" ]; then
			for MODULE in "$MODULE_DIR"/*.wren; do
				[ -f "$MODULE" ] || continue
				ln -sf "$MODULE" "$SCRIPT_ROOT/${MODULE##*/}"
			done
		fi
	done
fi

# Load library, test, and auto-run modules from the source tree.  This keeps
# the harness independent of whatever the developer has installed in their
# personal SyncTERM scripts directory.
export XDG_DATA_HOME="$DATA_HOME"
export XDG_CACHE_HOME="$DATA_HOME/cache"
mkdir -p "$XDG_CACHE_HOME"

export SDL_VIDEODRIVER=offscreen
export SDL_RENDER_DRIVER=software
export SDL_VIDEO_EGL_DRIVER=none

SCRIPT_NAME=${SCRIPT##*/}
CONNECTION='shell:/usr/bin/true'
FULL_SUITE=false
if ! $MENU_SUITE && [ "$SCRIPT_NAME" = "wrentest.wren" ]; then
	CONNECTION='shell:/bin/bash'
	FULL_SUITE=true
fi

# A normal shell disconnect can make SyncTERM return non-zero even after the
# script completed, so the captured diagnostics/report are authoritative.
if $MENU_SUITE; then
	if $MENU_WRITABLE; then
		timeout 30 "$SYNCTERM" -iS -Q >"$OUT" 2>"$ERR"
	else
		timeout 30 "$SYNCTERM" -iS -S -Q >"$OUT" 2>"$ERR"
	fi
else
	timeout 30 "$SYNCTERM" -iS -S -Q -W "$SCRIPT" "$CONNECTION" \
		>"$OUT" 2>"$ERR"
fi
STATUS=$?
if $MENU_SUITE; then
	cat "$ERR"
else
	cat "$OUT"
fi

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

if $MENU_SUITE; then
	if $PICKER_SUITE; then
		if grep -Eq '^=== PICKER TEST: [0-9]+ tests, [0-9]+ pass, 0 fail ===$' \
		    "$ERR" && ! grep -q '^\[wren-picker\]' "$ERR"; then
			exit 0
		fi
		exit 1
	fi
	if grep -Eq '^=== MENU TEST: [0-9]+ tests, [0-9]+ pass, 0 fail ===$' \
	    "$ERR" && ! grep -q '^\[wren-menu\]' "$ERR"; then
		exit 0
	fi
	exit 1
fi

if grep -q '^\[wren\] ' "$ERR"; then
	cat "$ERR" >&2
	exit 1
fi
exit 0
