#!/bin/sh
# ===========================================================================
# build.sh - Configure and build syncdrive (Linux/Unix).
#
#   Usage:  ./build.sh             (release build)
#           ./build.sh debug       (Debug build)
#           ./build.sh clean       (delete the build tree, then exit)
#           ./build.sh clean all   (delete the build tree, then build)
#
# Builds out-of-source in ./build/ and runs the unit tests. Building does NOT
# touch any live install -- run `jsexec deploy.js` for that.
# ===========================================================================
set -e

SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILDDIR="$SRCDIR/build"
CONFIG=Release
DOCLEAN=
DOALL=

for arg in "$@"; do
	case "$arg" in
	clean)             DOCLEAN=1 ;;
	all)               DOALL=1 ;;
	debug | Debug)     CONFIG=Debug ;;
	release | Release) CONFIG=Release ;;
	*) echo "build.sh: ignoring unknown argument '$arg'" >&2 ;;
	esac
done

if [ -n "$DOCLEAN" ]; then
	rm -rf "$BUILDDIR"
	[ -z "$DOALL" ] && exit 0
fi

cmake -S "$SRCDIR" -B "$BUILDDIR" -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILDDIR" -j"$(nproc 2>/dev/null || echo 4)"
(cd "$BUILDDIR" && ctest --output-on-failure)

if [ -x "$BUILDDIR/syncdrive" ]; then
	echo "[build] Built: $BUILDDIR/syncdrive"
	echo "[build] Run 'jsexec deploy.js' to install it into the door's xtrn dir."
fi
