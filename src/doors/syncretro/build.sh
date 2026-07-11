#!/bin/sh
# ===========================================================================
# build.sh - Configure and build the Linux/Unix build of syncretro.
#
#   Usage:  ./build.sh             (release build)
#           ./build.sh debug       (Debug build)
#           ./build.sh clean       (delete the build tree, then build)
#           ./build.sh debug clean (combine)
#
# Builds out-of-source in ./build/, leaving the binary at ./build/syncretro.
# Building does NOT touch any live install -- run `jsexec deploy.js` afterwards
# to install the binary into the door bundle's per-target sub-dir.
#
# syncretro links xpdev (sockets) and ../termgfx (sixel/JXL encode, APC
# transport, cap-probing, pacing); CMake builds both as sub-targets. It loads
# libretro cores at runtime via dlopen -- NO core is linked or vendored. libjxl
# is optional (its absence degrades to the sixel/text tiers).
#
# NOTE: requires the vendored libretro.h in this directory (see PROVENANCE.md).
# ===========================================================================
set -e

SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILDDIR="$SRCDIR/build"
CONFIG=Release
DOCLEAN=

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

if [ ! -f "$SRCDIR/libretro.h" ]; then
	echo "[build] ERROR: libretro.h not vendored in $SRCDIR -- see PROVENANCE.md" >&2
	exit 1
fi

if [ -n "$DOCLEAN" ] && [ -d "$BUILDDIR" ]; then
	echo "[build] Removing build tree $BUILDDIR"
	rm -rf "$BUILDDIR"
fi

echo "[build] Configuring ($CONFIG) ..."
cmake -S "$SRCDIR" -B "$BUILDDIR" -DCMAKE_BUILD_TYPE="$CONFIG"

echo "[build] Building ..."
cmake --build "$BUILDDIR" -j"$(nproc 2>/dev/null || echo 4)"

EXE="$BUILDDIR/syncretro"
if [ ! -x "$EXE" ]; then
	echo "[build] ERROR: expected output not found: $EXE" >&2
	exit 1
fi

echo
echo "[build] Built: $EXE"
echo "[build] Run 'jsexec deploy.js' to install it into the door's xtrn dir."
