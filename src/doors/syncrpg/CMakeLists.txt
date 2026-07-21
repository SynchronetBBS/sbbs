# SyncRPG Stage 2: configure + build the vendored EasyRPG Player (unmodified
# CMakeLists.txt, add_subdirectory'd below) as a CMake OBJECT library
# (EasyRPG_Player -- the target its own top-level CMakeLists.txt already
# defines for every platform), then link our own door main (door/syncrpg.cpp)
# against it plus Stage 1's prebuilt libtermgfx.a/libxpdev_static.a (absolute
# paths, supplied by build.sh via -DTERMGFX_LIB/-DXPDEV_LIB).
#
# Why an OBJECT-library link instead of touching EasyRPG's CMake: linking
# target_link_libraries(syncrpg PRIVATE EasyRPG_Player) pulls in every
# PUBLIC-linked dependency the engine's own CMakeLists.txt already resolved
# (liblcf, SDL2, PNG, fmt, pixman, freetype/harfbuzz, ...) transitively --
# CMake propagates an OBJECT library's usage requirements to whatever links
# it, so no vendored file needs a patch and no dependency needs restating
# here. PLAYER_TARGET_PLATFORM=SDL2 still builds EasyRPG's own SDL2-backed
# easyrpg-player executable as a side effect (that target is simply never
# requested by build.sh, which asks for --target syncrpg only); Task 3
# replaces the SDL2 UI with a termgfx-native BaseUi backend.
cmake_minimum_required(VERSION 3.18...3.31)
project(syncrpg CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Stage 1 output (build.sh passes these as absolute paths):
set(TERMGFX_LIB "" CACHE FILEPATH "Path to Stage 1's libtermgfx.a")
set(XPDEV_LIB "" CACHE FILEPATH "Path to Stage 1's libxpdev_static.a")
# libtermgfx.a's own externals (JPEG XL tier, libsndfile encode) recorded by
# Stage 1's CMake to jxl_libs.txt/sndfile_libs.txt; build.sh transcribes them
# here since libtermgfx.a is linked by absolute path, not via its CMake
# target, so its PUBLIC link interface does not otherwise survive the seam.
set(JXL_LIBS "" CACHE STRING "Extra JPEG XL link libs for libtermgfx (from Stage 1)")
set(SNDFILE_LIBS "" CACHE STRING "Extra libsndfile link libs for libtermgfx (from Stage 1)")
if(NOT TERMGFX_LIB OR NOT XPDEV_LIB)
	message(FATAL_ERROR "TERMGFX_LIB/XPDEV_LIB not set -- run via build.sh (Stage 1 must build first)")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/easyrpg easyrpg)

# UI substitution: run EasyRPG through our headless BaseUi_termgfx instead
# of its SDL2 Sdl2Ui. EasyRPG selects the UI at compile time in src/baseui.cpp:
# USE_SDL==2 -> Sdl2Ui, else the PLAYER_UI macro names the class CreateUi
# builds (the same hook its libretro/3ds/switch ports use). We force-include
# door/ui_term.h into just that one engine source; the header does
# `#undef USE_SDL` + `#define PLAYER_UI BaseUi_termgfx`, so CreateUi returns
# ours. A source-level #undef beats the build's -DUSE_SDL=2 no matter how CMake
# orders -D/-U on the command line -- so this is a build-wiring change scoped to
# baseui.cpp, with NO edit to the vendored tree. (The SDL sources still compile
# into the object library but go unreferenced; nothing constructs an Sdl2Ui, so
# no display is ever opened.) TARGET_DIRECTORY (CMake 3.18+) lets the parent set
# a property on a source owned by the add_subdirectory'd EasyRPG_Player target.
set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/easyrpg/src/baseui.cpp
	TARGET_DIRECTORY EasyRPG_Player
	PROPERTIES COMPILE_OPTIONS
		"-include;${CMAKE_CURRENT_SOURCE_DIR}/door/ui_term.h")

add_executable(syncrpg door/syncrpg.cpp door/ui_term.cpp door/video_term.cpp door/input_term.cpp door/audio_term.cpp)
# List EasyRPG's src/ ahead of ../termgfx: both trees have an "audio.h" (and
# other short names), and the door TUs need EasyRPG's. Ordering the engine
# headers first makes a bare `#include "audio.h"` (and audio_term.h's bare
# `#include "audio_generic.h"`, which itself bare-includes "audio.h" from its
# OWN directory -- a quote-include searches the including file's directory
# first, so that one resolves to EasyRPG's regardless of this order) resolve
# to EasyRPG's, not termgfx's. termgfx's own internal includes are
# unaffected -- a quote-include searches the including file's own directory
# first, so termgfx_termio.h still finds its termgfx siblings regardless of
# -I order, and audio_term.cpp's own `#include "termgfx_termio.h"` resolves
# correctly too: that filename has no counterpart in easyrpg/src to collide
# with.
# ../../xpdev: door/syncrpg.cpp's per-user --save-path (Task 8) needs
# dirwrap.h's mkpath() to create data/user/<####>/yumenikki/ before EasyRPG's
# FileFinder ever looks at it (see that file's comment); XPDEV_LIB above
# supplies the linked symbol, this supplies the header.
target_include_directories(syncrpg PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/easyrpg/src
	${CMAKE_CURRENT_SOURCE_DIR}/../termgfx
	${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(syncrpg PRIVATE EasyRPG_Player ${TERMGFX_LIB} ${XPDEV_LIB} ${JXL_LIBS} ${SNDFILE_LIBS} pthread)
