#!/bin/sh
# SyncSCUMM build: out-of-tree ScummVM build with the curated engine set.
# Usage: ./build.sh [null|termgfx]   (backend; default: termgfx)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BACKEND="${1:-termgfx}"
mkdir -p "$HERE/build"

# Stage 1: the door's C libraries (termgfx + xpdev), via their own CMake.
cmake -S "$HERE/door" -B "$HERE/build/libs" -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/build/libs" --target termgfx xpdev_static -j"$(nproc)"

# Stage 2: ScummVM with our backend.
cd "$HERE/build"
# --enable-mad: MP3 (libmad) is REQUIRED, not optional. The ScummVM freeware
# "talkie" builds (Flight of the Amazon Queen, and others) store their SPEECH as
# MP3; without libmad the engine loads the game but every spoken line is silent
# ("Using MP3 compressed datafile, but MP3 support not compiled in!"). Forcing it
# on (vs. configure's silent autodetect) makes a missing libmad fail the build
# loudly here rather than ship a talkie door that plays music but no dialogue.
# (libmad0-dev on Debian/Ubuntu.) Vorbis/FLAC stay autodetected -- present here,
# and no curated title has yet needed them the way queen needs MP3.
# --enable-release-mode: build as an official ScummVM release does (defines
# RELEASE_BUILD), NOT a developer build. Without it, an SCI "uninitialized temp
# read" that has no engine workaround is a FATAL error() (engines/sci/engine/
# vm.cpp) -- so a fan game crashes the instant its script hits one. Cascade
# Quest's Sound->Volume does exactly this (TheMenuBar::handleEvent, script 997,
# temp 3), for which ScummVM ships a workaround only for Betrayed Alliance. A
# release build turns that into a warning + fake-0 and continues -- the correct,
# tolerant behavior for a shipped door, and what real ScummVM users get. It also
# drops dev-only asserts/debug (hashmap stats, celobj32 bounds checks, ...), all
# benign. Use --enable-release-mode, NOT --enable-release: the latter also
# forces optimizations and turns on ScummVM's update-checker (a door must not
# phone home).
"$HERE/scummvm/configure" --backend="$BACKEND" --disable-all-engines \
  --enable-engine=scumm,sky,queen,lure,drascula,agi,sci --disable-detection-full \
  --enable-mad --enable-release-mode
# Bridge our libraries into the generated build config: config.mk accumulates
# LIBS with +=, and Makefile.common's link rule places $(LIBS) after the
# objects, so appending here is ordering-correct and touches no vendor file.
# The termgfx include path goes on INCLUDES (not CXXFLAGS): Makefile.common
# folds INCLUDES into CPPFLAGS, which the generic build rules for BOTH %.c
# and %.cpp pick up -- CXXFLAGS alone would leave a door/*.cpp unit (e.g.
# audio_term.cpp/video_term.cpp/syncscumm.cpp, which include
# termgfx_termio.h/termgfx_plat.h) unable to find term.h/caps.h/pace.h/door32.h.
{
	echo "LIBS += $HERE/build/libs/termgfx/libtermgfx.a"
	echo "LIBS += $HERE/build/libs/libxpdev_static.a -lpthread"
	echo "INCLUDES += -I$HERE/../termgfx"
	# door/syncscumm.cpp reads the sysop's syncscumm.ini via xpdev's
	# ini_file.h (subtitles precedence: user > sysop > auto).
	echo "INCLUDES += -I$HERE/../../xpdev"
	# The door's build stamp, shown on the GMM (engines/dialogs.cpp). Generated
	# by the SHARED helper rather than a hand-rolled `git describe` here, so this
	# door reports its build exactly like every sibling -- including the part
	# that matters: a dirty tree yields "~hash" and the BUILD time, so a stamp
	# never claims a commit the binary does not actually contain.
	echo "INCLUDES += -I$HERE/build"
} >> config.mk
cmake -DOUT="$HERE/build/git_hash.h" -DSRCDIR="$HERE" \
      -P "$HERE/../../build/gitinfo.cmake"
# SYNCHRONET DOOR SECURITY: compile the ScummVM launcher and the filesystem
# browser OUT of the door binary -- base/main.cpp's launcherDialog() and
# gui/browser.cpp's BrowserDialog::runModal() are #ifdef'd on this so a remote
# BBS user can never reach the launcher (Add Game, options) or browse/read/
# write arbitrary host files. DEFINES is folded into CPPFLAGS by
# Makefile.common, so it reaches those .cpp units. See PROVENANCE.md.
echo "DEFINES += -DSYNCSCUMM_NO_LAUNCHER" >> config.mk
# JPEG XL tier: door/CMakeLists.txt writes jxl_libs.txt only when libjxl was
# found (target_compile_definitions(termgfx PRIVATE WITH_JXL) then applies to
# termgfx's own jxl.c AND termgfx_termio.c's tier-select, both compiled by
# Stage 1's CMake build above). Nothing left in THIS make (door/module.mk) is
# a plain-C translation unit needing WITH_JXL of its own -- termgfx_termio.c/termgfx_termio.o
# moved into termgfx with the rest of the engine -- but the append below is
# kept as a harmless no-op DEFINES for any future plain-C module.mk object
# that might need it (DEFINES, not CXXFLAGS, because Makefile.common folds
# DEFINES into CPPFLAGS, which both the %.c and %.cpp generic rules pick up).
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
# Rename ScummVM's default "scummvm" output to the door's own name. This IS our
# build (the termgfx backend compiled in), so "syncscumm" both brands it and
# avoids any PATH collision with a system-installed "scummvm" when the xtrn.ini
# cmd runs it. ScummVM's configure has no exe-name option, so rename after make
# (the vendored build system is left untouched).
mv -f "$HERE/build/scummvm" "$HERE/build/syncscumm"
ls -la "$HERE/build/syncscumm"
