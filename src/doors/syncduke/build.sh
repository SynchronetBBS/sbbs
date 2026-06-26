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
# Builds out-of-source in ./build/ and copies the binary to THIS tree's
# ../../../xtrn/syncduke/ -- the same directory that holds install-xtrn.ini and
# syncduke.example.ini. That is the door's install bundle; deploy it to your live
# install however you normally do, drop a DUKE3D.GRP beside it (or point
# syncduke.ini [grp] dir at one), and register it with:
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

# --- Copy into xtrn/syncduke ----------------------------------------------
EXE="$BUILDDIR/syncduke"
if [ ! -x "$EXE" ]; then
	echo "[build] ERROR: expected output not found: $EXE" >&2
	exit 1
fi

mkdir -p "$DESTDIR"
cp -f "$EXE" "$DESTDIR/syncduke"
echo
echo "[build] Done. Copied: $(CDPATH= cd -- "$DESTDIR" && pwd)/syncduke"
