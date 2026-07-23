#!/bin/sh
# Build + run input_term's mapping/tap-hold unit test (M2 / Task 5) against
# Stage 1's prebuilt termgfx/xpdev libraries. Mirrors syncscumm/test/
# unit_termgfx_termio.sh's TERMGFX_TEST pattern for the termgfx_termio.c half;
# input_term.cpp/test_input_term.cpp are plain C++17 needing only EasyRPG's
# headers (no liblcf/SDL2 *library*, just its headers -- keys.h pulls in
# system.h/lcf/enum_tags.h, header-only from this test's point of view).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
[ -f "$DOOR/build/libs/termgfx/libtermgfx.a" ] || { echo "run ./build.sh first"; exit 1; }

# Same JXL/libsndfile link-lib handoff as unit_termgfx_termio.sh: ANY final
# link of libtermgfx.a needs these if Stage 1 found them, regardless of
# whether termgfx_termio.c itself references them.
JXL_LIBS=
if [ -f "$DOOR/build/libs/jxl_libs.txt" ]; then
	for l in $(cat "$DOOR/build/libs/jxl_libs.txt" | tr ';' ' '); do
		case "$l" in
		/*) JXL_LIBS="$JXL_LIBS $l" ;;
		*)  JXL_LIBS="$JXL_LIBS -l$l" ;;
		esac
	done
fi
SNDFILE_LIBS=""
if [ -s "$DOOR/build/libs/sndfile_libs.txt" ]; then
	SNDFILE_LIBS=$(cat "$DOOR/build/libs/sndfile_libs.txt")
fi
XPDEV_INC="-I$DOOR/../../xpdev"

# EasyRPG headers keys.h/system.h/options.h/memory_management.h need: the
# engine's own src/ dir, liblcf's headers (lcf/enum_tags.h), and USE_SDL
# defined (system.h's "#if !(defined(USE_SDL) || defined(PLAYER_UI)) #error"
# guard) -- the same value the real build configures (door/CMakeLists.txt ->
# easyrpg/CMakeLists.txt, PLAYER_TARGET_PLATFORM=SDL2). No SDL2 *header* is
# actually pulled in by keys.h's include chain, so no -isystem SDL2 needed.
EP_INC="-I$DOOR/easyrpg/src -I$DOOR/easyrpg/lib/liblcf/src -DUSE_SDL=2"

cc -c -DTERMGFX_TEST -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   -o /tmp/input_term_termio.o "$DOOR/../termgfx/termgfx_termio.c"

c++ -std=c++17 -c $EP_INC -I"$DOOR/door" -I"$DOOR/../termgfx" \
   -o /tmp/input_term.o "$DOOR/door/input_term.cpp"

c++ -std=c++17 -c $EP_INC -I"$DOOR/door" -I"$DOOR/../termgfx" \
   -o /tmp/test_input_term.o "$HERE/test_input_term.cpp"

c++ -o /tmp/test_input_term \
   /tmp/test_input_term.o /tmp/input_term.o /tmp/input_term_termio.o \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS

/tmp/test_input_term && echo "test_input_term: PASS"
