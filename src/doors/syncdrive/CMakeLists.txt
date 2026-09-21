cmake_minimum_required(VERSION 3.13)
project(syncdrive C CXX)   # CXX: termgfx links C++ libADLMIDI

# syncdrive -- Test Drive (1987) for Synchronet / SyncTERM, via the vendored
# faithful C port in tdport/ (kylofon/test-drive-sdl3) and termgfx's shared
# session engine. See DESIGN.md.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TDPORT ${CMAKE_CURRENT_SOURCE_DIR}/tdport/src)
file(GLOB_RECURSE TD_SOURCES CONFIGURE_DEPENDS ${TDPORT}/*.c)
list(FILTER TD_SOURCES EXCLUDE REGEX "/platform/gfx_cga\\.c$")

add_executable(syncdrive ${TD_SOURCES}
	door/syncdrive.c
	door/host_term.c
	door/frame.c
	door/keymap.c
	door/help_card.c
	door/keyscript.c
	door/speaker.c
	door/alias.c
	door/scores_lock.c)

# compat/ first: tdport/src/mem.c includes <SDL3/SDL.h>, which must be the shim.
target_include_directories(syncdrive BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat)
target_include_directories(syncdrive PRIVATE ${TDPORT} ${CMAKE_CURRENT_SOURCE_DIR}/door)
set_target_properties(syncdrive PROPERTIES LINKER_LANGUAGE CXX)

# The vendored engine compiles warning-free-by-fiat (-w / /w): it is frozen, so
# a warning there is not actionable.  GCC/Clang additionally need
# -fno-strict-aliasing, because tdport reads multi-byte values through byte
# pointers (see its mem.h); MSVC has no equivalent switch and needs none.
# (Our own sources keep MSVC's default /W3, which is why only the vendored ones
# are given a flag here; cl then reports a cosmetic D9025 for the override.)
if(MSVC)
	set_source_files_properties(${TD_SOURCES} PROPERTIES COMPILE_OPTIONS "/w")
else()
	set_source_files_properties(${TD_SOURCES} PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-w")
	target_compile_options(syncdrive PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# MSVC / Windows.  Win32 (x86) is the one supported Windows target -- see
# build.bat's header for why.  The door's platform difference is all inside
# termgfx + xpdev, so this is only the CRT quiets, the Winsock link and the
# symbol-map flags, mirroring ../syncmoo1 and ../syncretro.
# ---------------------------------------------------------------------------
if(MSVC)
	target_compile_definitions(syncdrive PRIVATE
		_CRT_SECURE_NO_WARNINGS          # fopen/strcpy/snprintf "use the _s variant"
		_CRT_NONSTDC_NO_DEPRECATE        # open/close/strdup POSIX names
		_WINSOCK_DEPRECATED_NO_WARNINGS) # inet_addr et al. reached via sockwrap.h

	# Winsock: the door's DOOR32.SYS handle is a SOCKET (see termgfx_termio.c).
	target_link_libraries(syncdrive ws2_32)

	# /MAP leaves a symbol map beside the binary, so procdump+cdb can symbolize
	# a crash dump; /OPT:REF,/OPT:ICF keep the optimizations /DEBUG would
	# otherwise disable (cf. syncmoo1/syncretro).  /Zi and /DEBUG bite only in a
	# configuration other than Release|Win32: for that one,
	# src/Directory.Build.targets -- auto-imported into every vcxproj under src/,
	# this build's generated ones included -- suppresses PDBs, to keep mspdbsrv
	# out of the toolchain.
	target_compile_options(syncdrive PRIVATE /Zi)
	target_link_options(syncdrive PRIVATE /MAP /DEBUG /OPT:REF /OPT:ICF)
endif()

# termgfx without WITH_JXL: sixel only (DESIGN.md sec. 1).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)
target_link_libraries(syncdrive termgfx)

# libsndfile is not optional for SOUND here, whatever termgfx's own status line
# says: the speaker PCM goes out through termgfx's audio stream, whose Ogg/Opus
# encoder is a stub without it, and a chunk that will not encode is dropped
# (termgfx/audio_stream.c).  Silent door, no other symptom -- so say so at
# configure time.  MSVC only: there find_package(CONFIG) is the one detection
# path termgfx has, so its result here is the same answer termgfx got.
if(MSVC)
	find_package(SndFile CONFIG QUIET)
	if(NOT SndFile_FOUND)
		message(WARNING "syncdrive: libsndfile not found -- the door will build and run, SILENT. "
		                "Install it (vcpkg install libsndfile:x86-windows-static-md) and point "
		                "CMAKE_PREFIX_PATH at the prefix, as build.bat does.")
	endif()
endif()

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
	set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
	set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
	foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
		set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
	endforeach()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev xpdev EXCLUDE_FROM_ALL)
endif()
target_include_directories(syncdrive PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(syncdrive xpdev)
if(NOT WIN32)
	target_link_libraries(syncdrive m pthread ${CMAKE_DL_LIBS})
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../../build/SynchronetMacros.cmake)
synchronet_gitinfo(syncdrive)

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
	enable_testing()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests tests)
endif()
