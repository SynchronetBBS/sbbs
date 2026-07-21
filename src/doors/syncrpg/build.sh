#!/bin/sh
# SyncRPG build: vendored EasyRPG Player + the shared termgfx/xpdev door
# libraries, two stages mirroring syncscumm/build.sh.
# Usage: ./build.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$HERE/build"

# Stage 1: the door's C libraries (termgfx + xpdev), via their own CMake --
# identical umbrella to syncscumm/door/CMakeLists.txt.
cmake -S "$HERE/door" -B "$HERE/build/libs" -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/build/libs" --target termgfx xpdev_static -j"$(nproc)"

# Stage 2a: hand-build inih (2 vendored files: easyrpg/lib/inih/ini.{c,h}).
# Debian ships no inih dev package (a runtime-only libinih1 with no headers/
# pkg-config file may be installed, but that's not enough); EasyRPG/liblcf's
# CMake only accepts find_package(inih) or the explicit INIH_INCLUDE_DIR/
# INIH_LIBRARY cache vars used in Stage 2b below (see
# easyrpg/lib/liblcf/builds/cmake/Modules/Findinih.cmake). Recipe confirmed
# working in the scouting pass (syncrpg-scout-report.md).
mkdir -p "$HERE/build/inih"
cc -c "$HERE/easyrpg/lib/inih/ini.c" -o "$HERE/build/inih/ini.o" \
	-I"$HERE/easyrpg/lib/inih"
ar rcs "$HERE/build/inih/libinih.a" "$HERE/build/inih/ini.o"

# Stage 2b: configure + build the vendored EasyRPG Player (its own
# CMakeLists.txt, unmodified -- add_subdirectory'd from syncrpg/CMakeLists.txt)
# as an OBJECT library, plus our own minimal door main (door/syncrpg.cpp),
# linked against Stage 1's libtermgfx.a/libxpdev_static.a. Flags are the
# scout's proven recipe: SDL2 platform (Task 3 replaces this UI backend with
# a termgfx-native one; for Task 2 it only needs to compile+link, never run),
# liblcf built in-tree from the vendored lib/liblcf/ (PLAYER_BUILD_LIBLCF=ON
# skips its git-clone step when lib/liblcf/ already exists, which it does
# here), inih from Stage 2a.
# libtermgfx.a's own external libs (JPEG XL tier, libsndfile encode) don't
# survive the Stage-1-CMake -> Stage-2-CMake seam either -- door/CMakeLists.txt
# (Stage 1) recorded them to jxl_libs.txt/sndfile_libs.txt at its configure
# time (same handoff syncscumm/build.sh uses); transcribe them here as
# CMake list cache vars for Stage 2 to link.
JXL_LIBS=""
if [ -s "$HERE/build/libs/jxl_libs.txt" ]; then
	for l in $(cat "$HERE/build/libs/jxl_libs.txt" | tr ';' ' '); do
		case "$l" in
		/*) JXL_LIBS="$JXL_LIBS;$l" ;;
		*)  JXL_LIBS="$JXL_LIBS;-l$l" ;;
		esac
	done
fi
SNDFILE_LIBS=""
if [ -s "$HERE/build/libs/sndfile_libs.txt" ]; then
	SNDFILE_LIBS=$(cat "$HERE/build/libs/sndfile_libs.txt")
fi

#
# -DPLAYER_WITH_NATIVE_MIDI=OFF: EasyRPG's CMakeLists.txt unconditionally
# wires in src/platform/linux/midiout_device_alsa.cpp whenever it finds ALSA
# on a UNIX build (regardless of PLAYER_TARGET_PLATFORM); that file lives
# under src/platform/linux/, which the vendoring pass intentionally pruned
# (only platform/sdl/ + clock.h were kept -- see PROVENANCE.md). Rather than
# un-prune/patch the vendored tree, turn off native MIDI via this documented
# CMake option: EasyRPG falls back to its built-in FmMidi sequencer
# (PLAYER_ENABLE_FMMIDI, on by default), and Task 3's real audio backend
# routes MIDI through termgfx's shared audio_stream module regardless, not
# native ALSA, so nothing here is lost.
cmake -S "$HERE" -B "$HERE/build/rpg" -DCMAKE_BUILD_TYPE=Release \
	-DPLAYER_TARGET_PLATFORM=SDL2 \
	-DPLAYER_BUILD_LIBLCF=ON \
	-DPLAYER_WITH_NATIVE_MIDI=OFF \
	-DINIH_INCLUDE_DIR="$HERE/easyrpg/lib/inih" \
	-DINIH_LIBRARY="$HERE/build/inih/libinih.a" \
	-DTERMGFX_LIB="$HERE/build/libs/termgfx/libtermgfx.a" \
	-DXPDEV_LIB="$HERE/build/libs/libxpdev_static.a" \
	-DJXL_LIBS="$JXL_LIBS" \
	-DSNDFILE_LIBS="$SNDFILE_LIBS"
cmake --build "$HERE/build/rpg" --target syncrpg -j"$(nproc)"

cp -f "$HERE/build/rpg/syncrpg" "$HERE/build/syncrpg"
ls -la "$HERE/build/syncrpg"
