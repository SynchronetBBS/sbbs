#!/bin/sh
# Build + run the present_rgbx unit test against the stage-1 libraries.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
TERMGFX=$(dirname "$HERE")
DOORS=$(dirname "$TERMGFX")
# Stage-1 libraries: any door's build tree will do -- they all build the same
# libtermgfx.a + libxpdev_static.a from these sources. Point TERMGFX_LIBS at a
# specific <door>/build/libs to override the search.
LIBS="${TERMGFX_LIBS:-}"
if [ -z "$LIBS" ]; then
	for d in "$DOORS"/*/build/libs; do
		if [ -f "$d/termgfx/libtermgfx.a" ]; then LIBS="$d"; break; fi
	done
fi
[ -n "$LIBS" ] && [ -f "$LIBS/termgfx/libtermgfx.a" ] || { echo "no libtermgfx.a -- run a door's build.sh first (e.g. $DOORS/syncscumm/build.sh)"; exit 1; }
# When door/CMakeLists.txt's libjxl probe found the library, libtermgfx.a's
# jxl.o has real libjxl calls (not the stub) -- ANY final link of it needs
# -l<lib> regardless of whether termgfx_termio.c itself uses WITH_JXL. Compiling
# termgfx_termio.c here WITH -DWITH_JXL too (mirrors build.sh's config.mk DEFINES
# append) is what actually exercises the JXL present path in this test.
JXL_DEFINE=
JXL_LIBS=
if [ -f "$LIBS/jxl_libs.txt" ]; then
	JXL_DEFINE="-DWITH_JXL"
	for l in $(cat "$LIBS/jxl_libs.txt" | tr ';' ' '); do
		case "$l" in
		/*) JXL_LIBS="$JXL_LIBS $l" ;;
		*)  JXL_LIBS="$JXL_LIBS -l$l" ;;
		esac
	done
fi
# libsndfile: same handoff as JXL above (door/CMakeLists.txt writes
# sndfile_libs.txt) -- ANY final link of libtermgfx.a needs it once termgfx
# was built with libsndfile, regardless of whether termgfx_termio.c itself uses it.
SNDFILE_LIBS=""
if [ -s "$LIBS/sndfile_libs.txt" ]; then
	SNDFILE_LIBS=$(cat "$LIBS/sndfile_libs.txt")
fi
# termgfx_termio.c reads <door>.ini via xpdev's ini_file.h (sixel_max override --
# door/syncscumm.cpp's subtitles read does the same), so every cc invocation
# below needs xpdev's own include dir, not just its static lib.
XPDEV_INC="-I$DOORS/../xpdev"

# present_rgbx's own truecolor session: a fresh process (termgfx_termio keeps
# file-static state, no reset), its own binary for the same reason every
# other case in unit_termgfx_termio.sh gets one.
cc -o /tmp/test_termgfx_present_rgbx $JXL_DEFINE -I"$TERMGFX" $XPDEV_INC \
   "$HERE/test_termgfx_present_rgbx.c" "$TERMGFX/termgfx_termio.c" \
   "$LIBS/termgfx/libtermgfx.a" "$LIBS/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_termgfx_present_rgbx
