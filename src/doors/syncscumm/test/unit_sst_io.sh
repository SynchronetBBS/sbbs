#!/bin/sh
# Build + run the sst_io unit test against the stage-1 libraries.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
[ -f "$DOOR/build/libs/termgfx/libtermgfx.a" ] || { echo "run ./build.sh first"; exit 1; }
# When door/CMakeLists.txt's libjxl probe found the library, libtermgfx.a's
# jxl.o has real libjxl calls (not the stub) -- ANY final link of it needs
# -l<lib> regardless of whether sst_io.c itself uses WITH_JXL. Compiling
# sst_io.c here WITH -DWITH_JXL too (mirrors build.sh's config.mk DEFINES
# append) is what actually exercises the JXL present path in this test.
JXL_DEFINE=
JXL_LIBS=
if [ -f "$DOOR/build/libs/jxl_libs.txt" ]; then
	JXL_DEFINE="-DWITH_JXL"
	for l in $(cat "$DOOR/build/libs/jxl_libs.txt" | tr ';' ' '); do
		case "$l" in
		/*) JXL_LIBS="$JXL_LIBS $l" ;;
		*)  JXL_LIBS="$JXL_LIBS -l$l" ;;
		esac
	done
fi
# sst_io.c reads syncscumm.ini via xpdev's ini_file.h (sixel_max override --
# door/syncscumm.cpp's subtitles read does the same), so every cc invocation
# below needs xpdev's own include dir, not just its static lib.
XPDEV_INC="-I$DOOR/../../xpdev"

cc -o /tmp/test_sst_io $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS
/tmp/test_sst_io

# The non-graphics-terminal gate needs a fresh sst_io session (file-static
# state, no reset), so it's its own binary rather than another case above.
cc -o /tmp/test_sst_io_nogfx $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_nogfx.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS
/tmp/test_sst_io_nogfx

# The canvas-size startup hold (Foot geometry bug) likewise needs a fresh
# session; its own binary for the same reason.
cc -o /tmp/test_sst_io_canvas $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_canvas.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS
/tmp/test_sst_io_canvas

# The XTSMGRAPHICS sixel-ceiling clamp (xterm oversized-sixel discard fix)
# likewise needs a fresh session; its own binary for the same reason.
cc -o /tmp/test_sst_io_gfxmax $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_gfxmax.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS
/tmp/test_sst_io_gfxmax

# XTVERSION-identified xterm: an exact canvas report with no XTSMGRAPHICS
# reply must still clamp to TERMGFX_SIXEL_SAFE_MAX once the terminal has
# positively identified itself as xterm -- fresh session, own binary.
cc -o /tmp/test_sst_io_xterm_ceiling $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_xterm_ceiling.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS
/tmp/test_sst_io_xterm_ceiling

# Sysop "sixel_max" ini override beats even a reported XTSMGRAPHICS ceiling.
# Needs a syncscumm.ini in CWD when sst_io_init() runs, so this one is
# invoked from a dedicated temp directory rather than $HERE/$DOOR.
cc -o /tmp/test_sst_io_sixelmax_override $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_sixelmax_override.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS
SIXELMAX_TMPDIR=$(mktemp -d)
trap 'rm -rf "$SIXELMAX_TMPDIR"' EXIT
printf 'sixel_max = 800\n' > "$SIXELMAX_TMPDIR/syncscumm.ini"
(cd "$SIXELMAX_TMPDIR" && /tmp/test_sst_io_sixelmax_override)
