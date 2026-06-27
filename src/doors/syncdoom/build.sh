#!/bin/sh
# ===========================================================================
# build.sh - Configure and build the Linux/Unix (Release) build of SyncDOOM.
#
#   Usage:  ./build.sh            (release build; JPEG-XL if libjxl is found)
#           ./build.sh debug      (Debug build)
#           ./build.sh clean      (delete the build tree, then build)
#           ./build.sh debug clean (combine)
#
# The *nix counterpart of build.bat (the Windows/MSVC helper) and the steps in
# COMPILING.md. The JPEG-XL graphics tier (JXL) is enabled automatically when
# CMake finds libjxl; without it the door still builds with the sixel/text
# tiers. Builds out-of-source in ./build/, leaving the binary at ./build/syncdoom.
# Building does NOT touch any live install -- run ./deploy.sh afterwards when you
# actually want the running door updated.  (Keeping deploy separate means a sysop
# can rebuild and test before pushing a new binary to a live BBS.)
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

echo
echo "[build] Built: $EXE"
echo "[build] Run ./deploy.sh to install it into the door's xtrn dir."
