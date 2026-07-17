#!/bin/sh
# SyncSCUMM build: out-of-tree ScummVM build with the curated engine set.
# Usage: ./build.sh [null|synchronet]   (backend; default: synchronet)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BACKEND="${1:-synchronet}"
mkdir -p "$HERE/build"

# Stage 1: the door's C libraries (termgfx + xpdev), via their own CMake.
cmake -S "$HERE/door" -B "$HERE/build/libs" -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/build/libs" --target termgfx xpdev_static -j"$(nproc)"

# Stage 2: ScummVM with our backend.
cd "$HERE/build"
"$HERE/scummvm/configure" --backend="$BACKEND" --disable-all-engines \
  --enable-engine=scumm,sky,queen,lure,drascula --disable-detection-full
# Bridge our libraries into the generated build config: config.mk accumulates
# LIBS with +=, and Makefile.common's link rule places $(LIBS) after the
# objects, so appending here is ordering-correct and touches no vendor file.
# The termgfx include path goes on INCLUDES (not CXXFLAGS): Makefile.common
# folds INCLUDES into CPPFLAGS, which the generic build rules for BOTH %.c
# and %.cpp pick up -- CXXFLAGS alone would leave a plain-C module.mk object
# (e.g. sst_io.o) unable to find term.h/caps.h/pace.h/door32.h.
{
	echo "LIBS += $HERE/build/libs/termgfx/libtermgfx.a"
	echo "LIBS += $HERE/build/libs/libxpdev_static.a -lpthread"
	echo "INCLUDES += -I$HERE/../termgfx"
	# door/syncscumm.cpp reads the sysop's syncscumm.ini via xpdev's
	# ini_file.h (subtitles precedence: user > sysop > auto).
	echo "INCLUDES += -I$HERE/../../xpdev"
} >> config.mk
# JPEG XL tier: door/CMakeLists.txt writes jxl_libs.txt only when libjxl was
# found (target_compile_definitions(termgfx PRIVATE WITH_JXL) then applies to
# termgfx's own jxl.c). sst_io.c's tier-select (door/sst_io.c) is compiled by
# THIS make, a separate translation unit from termgfx's CMake build, so it
# needs its own WITH_JXL -- DEFINES (not CXXFLAGS) because Makefile.common
# folds DEFINES into CPPFLAGS, which both the %.c and %.cpp generic rules
# pick up, so a plain-C module.mk object like sst_io.o sees it too.
if [ -f "$HERE/build/libs/jxl_libs.txt" ]; then
	echo "DEFINES += -DWITH_JXL" >> config.mk
	for l in $(cat "$HERE/build/libs/jxl_libs.txt" | tr ';' ' '); do
		case "$l" in
		/*) echo "LIBS += $l" ;;
		*)  echo "LIBS += -l$l" ;;
		esac
	done >> config.mk
fi
# libsndfile: termgfx's Ogg/Opus encode (M4 audio). Same seam as JXL above --
# LIBS, because ScummVM's make does the final link and never saw CMake's
# PUBLIC link interface. May be an absolute path or an -l token.
if [ -s "$HERE/build/libs/sndfile_libs.txt" ]; then
	SNDFILE_LIBS=$(cat "$HERE/build/libs/sndfile_libs.txt")
	echo "LIBS += $SNDFILE_LIBS" >> config.mk
fi
# VER_REV= : the vendored tree lives inside the sbbs repo; without this,
# ScummVM's version probe appends the HOST repo's git state ("dirty").
make -j"$(nproc)" VER_REV=
ls -la "$HERE/build/scummvm"
