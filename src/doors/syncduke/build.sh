#!/bin/sh
# ===========================================================================
# build.sh - Configure and build the Linux/Unix (Release) build of SyncDuke
#            and copy the syncduke binary into the door's xtrn dir.
#
#   Usage:  ./build.sh             (release build)
#           ./build.sh debug       (Debug build)
#           ./build.sh clean       (delete the build tree, then build)
#           ./build.sh debug clean (combine)
#
#   Env:    SYNCDUKE_DEST=<dir>    deploy the binary into this live xtrn/syncduke
#                                  dir too (overrides the $SBBSCTRL auto-detect)
#
# Builds out-of-source in ./build/ and deploys the syncduke binary to the door's
# xtrn dir -- THIS tree's ../../../xtrn/syncduke/ (next to install-xtrn.ini and
# syncduke.example.ini, the install bundle).  On the recommended SYMLINK=1 *nix
# install that in-tree dir IS the live xtrn (xtrn -> repo/xtrn), so the door is
# updated in place.  On a COPY-style install the live xtrn is a separate dir the
# in-tree copy never reaches, so we ALSO deploy to the live install when it can
# be located -- via $SBBSCTRL (install root = $SBBSCTRL/..) or $SYNCDUKE_DEST.
# The binary is a git-ignored build artifact, so unlike the tracked door files it
# is not carried to a live install by `git pull` / `update.js` -- this script is
# what puts it where the running door reads it.
#
# For a first-time install, drop a DUKE3D.GRP beside the binary (or point
# syncduke.ini [grp] dir at one) and register the door with:
#     jsexec install-xtrn ../xtrn/syncduke
#
# SyncDuke links xpdev (sockets + ini_file); CMake builds it as a sub-target, so
# no separate xpdev install is needed.
# ===========================================================================
set -e

# --- Source dir = location of this script ----------------------------------
SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILDDIR="$SRCDIR/build"
DESTDIR="$SRCDIR/../../../xtrn/syncduke"
CONFIG=Release
DOCLEAN=

# --- Parse arguments (order-independent: "debug" and/or "clean") -----------
for arg in "$@"; do
	case "$arg" in
	clean)             DOCLEAN=1 ;;
	debug | Debug)     CONFIG=Debug ;;
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
EXE="$BUILDDIR/syncduke"
if [ ! -x "$EXE" ]; then
	echo "[build] ERROR: expected output not found: $EXE" >&2
	exit 1
fi

# Copy the binary into a door dir, unless that dir's binary already IS the build
# output (e.g. a dev symlink xtrn/syncduke/syncduke -> build/syncduke); cp -f
# onto such a same-file target errors, which under `set -e` would abort the script.
deploy() {   # $1 = destination xtrn/syncduke directory
	mkdir -p "$1"
	if [ "$EXE" -ef "$1/syncduke" ]; then
		echo "[build] $1/syncduke already is the build output -- nothing to copy"
	else
		cp -f "$EXE" "$1/syncduke"
		echo "[build] Deployed: $(CDPATH= cd -- "$1" && pwd)/syncduke"
	fi
}

echo
deploy "$DESTDIR"      # in-tree bundle (== the live xtrn on a SYMLINK=1 install)

# Copy-style installs: the live xtrn is a separate dir the in-tree copy misses.
# Locate it via an explicit override, else $SBBSCTRL (install root = its parent).
LIVE="$SYNCDUKE_DEST"
[ -z "$LIVE" ] && [ -n "$SBBSCTRL" ] && LIVE="$SBBSCTRL/../xtrn/syncduke"
if [ -n "$LIVE" ]; then
	if [ "$LIVE" -ef "$DESTDIR" ]; then
		:   # same directory as the in-tree bundle (symlinked / build-in-place) -- already done
	elif [ -n "$SYNCDUKE_DEST" ] || [ -d "$LIVE" ]; then
		deploy "$LIVE"      # explicit override (create if needed), or an existing live dir
	else
		echo "[build] Live install dir not found at $LIVE -- skipping (run install-xtrn there for a first install)"
	fi
fi
