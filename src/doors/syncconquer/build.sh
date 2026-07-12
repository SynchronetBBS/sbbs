#!/bin/sh
# ===========================================================================
# build.sh - Configure and build the SyncConquer engine (headless vanillara).
#
#   Usage:  ./build.sh             (release build)
#           ./build.sh debug       (Debug build)
#           ./build.sh clean       (delete the build tree, then exit)
#
# Builds out-of-source in ./build/, leaving the binary at ./build/syncalert.
# Building does NOT touch any live install -- run `jsexec deploy.js` afterwards when you
# actually want the running door updated.  (Keeping deploy separate means a sysop
# can rebuild and test before pushing a new binary to a live BBS.)
#
# SyncConquer uses CMake with the vanilla engine and libtermgfx as
# sub-projects (see ./CMakeLists.txt for how the door's engine-seam TUs
# fold into the vendored vanillara build).
# ===========================================================================
set -e

# --- Source dir = location of this script ----------------------------------
SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILDDIR="$SRCDIR/build"
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

# --- Clean if requested (and exit) -----------------------------------------
if [ -n "$DOCLEAN" ]; then
	if [ -d "$BUILDDIR" ]; then
		echo "[build] Removing build tree $BUILDDIR"
		rm -rf "$BUILDDIR"
	fi
	exit 0
fi

# --- Configure + build -----------------------------------------------------
echo "[build] Configuring ($CONFIG) ..."
cmake -B "$BUILDDIR" -S "$SRCDIR" \
	-DCMAKE_BUILD_TYPE="$CONFIG"

echo "[build] Building ..."
cmake --build "$BUILDDIR" -j"$(nproc 2>/dev/null || echo 4)"

# --- Confirm the build produced the binary --------------------------------
EXE="$BUILDDIR/syncalert"
if [ ! -x "$EXE" ]; then
	echo "[build] ERROR: expected output not found: $EXE" >&2
	exit 1
fi

echo
ls -la "$EXE"
