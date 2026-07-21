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

if(MSVC)
	# The vcpkg triplet this door builds against (x86-windows-static-md) is
	# static libraries + the DYNAMIC (/MD) CRT, and every translation unit in
	# the link has to agree. EasyRPG's builds/cmake/Modules/PlayerConfigureWindows
	# .cmake derives that from VCPKG_CRT_LINKAGE -- a variable the vcpkg
	# toolchain leaves undefined here -- so its generator expression collapses to
	# plain "MultiThreaded" (/MT) and every vcpkg archive then comes up as
	# thousands of unresolved __imp__* externals at link time. Set it before
	# add_subdirectory(easyrpg), with FORCE: EasyRPG's own set(... CACHE ...)
	# would otherwise keep whatever a previous configure of this build dir left
	# behind.
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
		CACHE STRING "MSVC runtime library (must match the vcpkg triplet)" FORCE)
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
# (MSVC spells the force-include /FI<path>, one token, no separator.)
if(MSVC)
	set(SYNCRPG_FORCE_INCLUDE "/FI${CMAKE_CURRENT_SOURCE_DIR}/door/ui_term.h")
else()
	set(SYNCRPG_FORCE_INCLUDE "-include;${CMAKE_CURRENT_SOURCE_DIR}/door/ui_term.h")
endif()
set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/easyrpg/src/baseui.cpp
	TARGET_DIRECTORY EasyRPG_Player
	PROPERTIES COMPILE_OPTIONS "${SYNCRPG_FORCE_INCLUDE}")

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
target_link_libraries(syncrpg PRIVATE EasyRPG_Player ${TERMGFX_LIB} ${XPDEV_LIB} ${JXL_LIBS} ${SNDFILE_LIBS})
if(WIN32)
	# libtermgfx.lib/xpdev_static.lib are linked by absolute path, so the Win32
	# system libraries their CMake targets declare (xpdev/CMakeLists.txt:
	# iphlpapi/ws2_32/winmm/netapi32/ole32/uuid) do not survive that seam either
	# -- restate them here. winmm is additionally needed by EasyRPG itself: its
	# CMakeLists.txt compiles src/platform/windows/midiout_device_win32.cpp into
	# the object library unconditionally on WIN32, so the midiOut* references
	# must resolve even though PLAYER_WITH_NATIVE_MIDI is OFF (a door must never
	# play the game's MIDI out of the *BBS host's* sound card -- the music goes
	# to the caller's terminal through termgfx; nothing constructs a
	# Win32MidiOutDevice with HAVE_NATIVE_MIDI undefined).
	target_link_libraries(syncrpg PRIVATE ws2_32 iphlpapi winmm netapi32 ole32 uuid)
else()
	target_link_libraries(syncrpg PRIVATE pthread)
endif()
if(MSVC)
	target_compile_definitions(syncrpg PRIVATE
		_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
	# /Zc:__cplusplus -- without it MSVC reports __cplusplus as 199711L even in
	# C++17 mode, and xpdev's gen_defs.h (reached from dirwrap.h) gates its
	# HAS_STDINT_H/HAS_INTTYPES_H on that macro: it concludes the compiler has
	# no <stdint.h> and rolls its own int8_t/int32_t/uint32_t typedefs, which
	# then collide with the real ones the C++ standard headers already pulled
	# in (error C2371, "redefinition; different basic types"). The same flag is
	# used by the sibling SyncConquer MSVC build.
	target_compile_options(syncrpg PRIVATE /Zc:__cplusplus)
endif()
