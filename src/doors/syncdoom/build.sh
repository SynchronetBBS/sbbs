#!/bin/sh
# ===========================================================================
# build.sh - Configure and build the Linux/Unix (Release) build of SyncDOOM
#            and install the syncdoom binary into the door's xtrn dir.
#
#   Usage:  ./build.sh            (release build; JPEG-XL if libjxl is found)
#           ./build.sh debug      (Debug build)
#           ./build.sh clean      (delete the build tree, then build)
#           ./build.sh debug clean (combine)
#
#   Env:    SYNCDOOM_DEST=<dir>   deploy the binary into this live xtrn/syncdoom
#                                 dir too (overrides the $SBBSCTRL auto-detect)
#
# The *nix counterpart of build.bat (the Windows/MSVC helper) and the steps in
# COMPILING.md. The JPEG-XL graphics tier (JXL) is enabled automatically when
# CMake finds libjxl; without it the door still builds with the sixel/text
# tiers. Builds out-of-source in ./build/ and deploys the binary next to the
# lobby in the door's xtrn dir -- THIS tree's ../../../xtrn/syncdoom/ (the same
# place build.bat installs the .exe). On the recommended SYMLINK=1 *nix install
# that in-tree dir IS the live xtrn (xtrn -> repo/xtrn), so the running door is
# updated in place. On a COPY-style install the live xtrn is a separate dir the
# in-tree copy never reaches, so we ALSO deploy to the live install when it can
# be located -- via $SBBSCTRL (install root = $SBBSCTRL/..) or $SYNCDOOM_DEST.
# The binary is a git-ignored build artifact, so unlike the tracked door files
# (lobby, getwads.js, ...) it is not carried to a live install by `git pull` /
# `update.js` -- this script is what puts it where the running door reads it.
# ===========================================================================
set -e

# --- Source dir = location of this script ----------------------------------
SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILDDIR="$SRCDIR/build"
DESTDIR="$SRCDIR/../../../xtrn/syncdoom"
CONFIG=Release
DOCLEAN=

# --- Parse arguments (order-independent: "debug" and/or "clean") -----------
for arg in "$@"; do
	case "$arg" in
	clean)            DOCLEAN=1 ;;
	debug | Debug)    CONFIG=Debug ;;
	release | Release) CONFIG=Release ;;
	*) echo "build.sh: ignoring unknown argument '$arg'" >&2 ;;
	esac
done

if ! command -v cmake >/dev/null 2>&1; then
	echo "[build] ERROR: cmake not found in PATH" >&2
	exit 1
fi

# --- Clean if requested ----------------------------------------------------
if [ -n "$DOCLEAN" ] && [ -d "$BUILDDIR" ]; then
	echo "[build] Removing build tree $BUILDDIR"
	rm -rf "$BUILDDIR"
fi

# --- Configure + build -----------------------------------------------------
echo "[build] Configuring ($CONFIG) ..."
cmake -S "$SRCDIR" -B "$BUILDDIR" -DCMAKE_BUILD_TYPE="$CONFIG"

echo "[build] Building ..."
cmake --build "$BUILDDIR" -j"$(nproc 2>/dev/null || echo 4)"

# --- Deploy the freshly-built binary --------------------------------------
EXE="$BUILDDIR/syncdoom"
if [ ! -x "$EXE" ]; then
	echo "[build] ERROR: expected output not found: $EXE" >&2
	exit 1
fi

# Copy the binary into a door dir, unless that dir's binary already IS the build
# output (e.g. a dev symlink xtrn/syncdoom/syncdoom -> build/syncdoom); cp -f
# onto such a same-file target errors, which under `set -e` would abort the script.
deploy() {   # $1 = destination xtrn/syncdoom directory
	mkdir -p "$1"
	if [ "$EXE" -ef "$1/syncdoom" ]; then
		echo "[build] $1/syncdoom already is the build output -- nothing to copy"
	else
		cp -f "$EXE" "$1/syncdoom"
		echo "[build] Deployed: $(CDPATH= cd -- "$1" && pwd)/syncdoom"
	fi
}

echo
deploy "$DESTDIR"      # in-tree bundle (== the live xtrn on a SYMLINK=1 install)

# Copy-style installs: the live xtrn is a separate dir the in-tree copy misses.
# Locate it via an explicit override, else $SBBSCTRL (install root = its parent).
LIVE="$SYNCDOOM_DEST"
[ -z "$LIVE" ] && [ -n "$SBBSCTRL" ] && LIVE="$SBBSCTRL/../xtrn/syncdoom"
if [ -n "$LIVE" ]; then
	if [ "$LIVE" -ef "$DESTDIR" ]; then
		:   # same directory as the in-tree bundle (symlinked / build-in-place) -- already done
	elif [ -n "$SYNCDOOM_DEST" ] || [ -d "$LIVE" ]; then
		deploy "$LIVE"      # explicit override (create if needed), or an existing live dir
	else
		echo "[build] Live install dir not found at $LIVE -- skipping (run install-xtrn there for a first install)"
	fi
fi
