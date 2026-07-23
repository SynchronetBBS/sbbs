cmake_minimum_required(VERSION 3.13)
project(syncmoo1 C CXX)   # CXX: termgfx links C++ libADLMIDI

# syncmoo1 -- Master of Orion 1 (via the vendored 1oom engine) for Synchronet /
# SyncTERM (DOOR32.SYS BBS door). Parallels src/doors/syncduke and
# src/doors/syncdoom: the vendored engine's platform layer is a new 1oom `hw`
# backend (hw_term.c, a peer of hw/nop, hw/sdl, hw/alleg) rather than a source
# patch, so the vendored 1oom tree in 1oom/ stays frozen and unedited.
#
# Task 2 (this file): prove the source enumeration + config.h + termgfx link
# by building headlessly against a stub hw backend. No real terminal I/O yet
# (that's syncmoo1_io.c etc., added in later tasks).

set(OOM ${CMAKE_CURRENT_SOURCE_DIR}/1oom/src)

# Top-level source lists -- copied verbatim from 1oom/src/Makefile.am.
set(UTIL_SRC cfg.c gfxaux.c lbx.c lbxfont.c lbxgfx.c lbxpal.c lib.c log.c
    main.c menu.c options.c pbx.c rnd.c util.c util_cstr.c util_math.c)
set(AUDIO_SRC fmt_mus.c fmt_sfx.c)
set(UI_SRC font8x8.c font8x8_draw.c kbd.c mouse.c palette.c)
list(TRANSFORM UTIL_SRC  PREPEND ${OOM}/)
list(TRANSFORM AUDIO_SRC PREPEND ${OOM}/)
list(TRANSFORM UI_SRC    PREPEND ${OOM}/)

file(GLOB GAME_SRC    ${OOM}/game/*.c)          # libgame + libgamemain
file(GLOB CLASSIC_SRC ${OOM}/ui/classic/*.c)
# Drop tool-only / saveconv TUs that define competing main()/entry:
list(FILTER GAME_SRC EXCLUDE REGEX "saveconv")

# 1oom ships a pluggable per-OS backend (os/{unix,win32,msdos}/os.c + osdefs.h).
# OS_DIR goes on the include path for its osdefs.h (the FSDEV_* path-separator
# macros: '/' vs '\\').
#
# On *nix we compile 1oom's own os/unix/os.c unchanged. On Windows we supply
# OUR OWN backend, syncmoo1_os_win32.c -- a peer of hw_term.c, and for the same
# reason: the vendored os/win32/os.c has an os_make_path() that calls mkdir()
# with no "already exists" guard (the unix backend guards with access()), so it
# fails on any existing directory and thereby breaks cfg_save() AND game saving
# (game/game_save.c:882). That is a vendored bug; per CLAUDE.md we route around
# it in our own glue rather than patching 1oom/. See syncmoo1_os_win32.c.
if(WIN32)
    set(OS_DIR ${OOM}/os/win32)
    set(OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/syncmoo1_os_win32.c)
else()
    set(OS_DIR ${OOM}/os/unix)
    set(OS_SRC ${OS_DIR}/os.c)
endif()

add_executable(syncmoo1
    ${UTIL_SRC} ${AUDIO_SRC} ${UI_SRC} ${GAME_SRC} ${CLASSIC_SRC} ${OS_SRC}
    hw_term.c
    syncmoo1_io.c
    syncmoo1_input.c
    syncmoo1_door.c
    syncmoo1_map.c
    syncmoo1_geom.c
    syncmoo1_cfgprune.c
    syncmoo1_config.c
    syncmoo1_audio.c
    syncmoo1_music.c
    syncmoo1_plat.c)   # the door's entire Windows-vs-POSIX seam, over xpdev

target_include_directories(syncmoo1 PRIVATE ${OOM} ${OOM}/game ${OS_DIR})
target_compile_definitions(syncmoo1 PRIVATE HAVE_CONFIG_H)
set_target_properties(syncmoo1 PROPERTIES LINKER_LANGUAGE CXX C_STANDARD 11)

if(MSVC)
    # Absorb legacy-engine warnings; never patch upstream. /w is MSVC's -w.
    # /FI force-includes compat/msvc_compat.h into every TU of this target, to
    # neutralize the one GCC __attribute__ in the vendored engine (log.h) --
    # see that header for why this can't be a plain /D.
    target_compile_options(syncmoo1 PRIVATE
        /w
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/compat/msvc_compat.h")
    target_compile_definitions(syncmoo1 PRIVATE
        _CRT_SECURE_NO_WARNINGS          # fopen/strcpy/snprintf "use the _s variant"
        _CRT_NONSTDC_NO_DEPRECATE        # getcwd/chdir/unlink/mkdir POSIX names
        _WINSOCK_DEPRECATED_NO_WARNINGS) # inet_addr/gethostbyname et al. (via sockwrap.h)

    # MSVC has no <strings.h> (the vendored 1oom/src/util.h includes it for
    # strcasecmp) and no <unistd.h> (1oom/src/game/game_save.c includes it for
    # unlink; our syncmoo1_config.c for getcwd/chdir). compat/ supplies both as
    # thin shims onto <string.h>/<io.h>/<direct.h>, so the frozen 1oom/ tree
    # stays unedited -- CLAUDE.md's ours-vs-vendored contract, and the same
    # trick as src/doors/syncduke/compat/. MSVC-only: every other toolchain has
    # the real headers, and must not see these.
    target_include_directories(syncmoo1 BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat)

    # Winsock: the door's DOOR32.SYS handle is a SOCKET (see syncmoo1_plat.c).
    target_link_libraries(syncmoo1 ws2_32)

    # /Zi emits a .pdb for the optimized Release build and /MAP a symbol map, so
    # procdump+cdb can symbolize a crash dump; /OPT:REF,/OPT:ICF restore the
    # optimizations /DEBUG would otherwise disable (cf. syncduke).
    target_compile_options(syncmoo1 PRIVATE /Zi)
    target_link_options(syncmoo1 PRIVATE /MAP /DEBUG /OPT:REF /OPT:ICF)
else()
    # Absorb legacy-engine warnings; never patch upstream. -fcommon: 1oom
    # declares tentative globals in headers, and GCC 10+ defaults to -fno-common.
    target_compile_options(syncmoo1 PRIVATE -w -fcommon)
endif()

# libtermgfx -- the shared sixel/JXL encoders + APC transport + cap-probe +
# sbbs_node. Its PUBLIC include dir gives us "sixel.h"/"jxl.h"/"apc.h"/
# "caps.h"/"term.h"/"pace.h"/"sbbs_node.h" for the later I/O tasks.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)
target_link_libraries(syncmoo1 termgfx)

# Git build info (git_hash.h: GIT_HASH/GIT_DATE/GIT_TIME), via the shared
# helper in src/build. Regenerated each build into the binary dir.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../build/SynchronetMacros.cmake)
synchronet_gitinfo(syncmoo1)

# ---------------------------------------------------------------------------
# JPEG XL -- the APC/SyncTERM graphics tier (shared with SyncDOOM/SyncDuke via
# termgfx). Optional: without it the door still serves the sixel tier (the
# JXL present path is #ifdef WITH_JXL gated, and termgfx/jxl.c compiles a
# return-0 stub). See src/doors/syncduke/CMakeLists.txt for the full
# rationale; this mirrors that block for the syncmoo1 target.
# ---------------------------------------------------------------------------
option(WITHOUT_JPEG_XL "Build without the JPEG XL graphics tier (sixel only)" OFF)
if(NOT WITHOUT_JPEG_XL)
    if(MSVC)
        find_path(JXL_INCLUDE_DIR jxl/encode.h)
        set(_jxl_required jxl hwy brotlienc brotlidec brotlicommon)
        set(_jxl_optional jxl_cms lcms2)
        set(JPEG_XL_LIBRARIES "")
        set(_jxl_ok TRUE)
        foreach(_jxl_n ${_jxl_required} ${_jxl_optional})
            if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
                set(_jxl_root ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
                find_library(JXL_${_jxl_n}_REL NAMES ${_jxl_n} PATHS ${_jxl_root}/lib NO_DEFAULT_PATH)
                find_library(JXL_${_jxl_n}_DBG NAMES ${_jxl_n} PATHS ${_jxl_root}/debug/lib NO_DEFAULT_PATH)
                if(JXL_${_jxl_n}_REL)
                    list(APPEND JPEG_XL_LIBRARIES optimized ${JXL_${_jxl_n}_REL})
                    if(JXL_${_jxl_n}_DBG)
                        list(APPEND JPEG_XL_LIBRARIES debug ${JXL_${_jxl_n}_DBG})
                    else()
                        list(APPEND JPEG_XL_LIBRARIES debug ${JXL_${_jxl_n}_REL})
                    endif()
                endif()
                set(_jxl_hit ${JXL_${_jxl_n}_REL})
            else()
                find_library(JXL_${_jxl_n}_LIB NAMES ${_jxl_n})
                if(JXL_${_jxl_n}_LIB)
                    list(APPEND JPEG_XL_LIBRARIES ${JXL_${_jxl_n}_LIB})
                endif()
                set(_jxl_hit ${JXL_${_jxl_n}_LIB})
            endif()
            if(NOT _jxl_hit)
                list(FIND _jxl_required ${_jxl_n} _jxl_req)
                if(NOT _jxl_req EQUAL -1)
                    set(_jxl_ok FALSE)
                endif()
            endif()
        endforeach()
        if(JXL_INCLUDE_DIR AND _jxl_ok)
            set(JPEG_XL_FOUND TRUE)
            set(JPEG_XL_INCLUDE_DIRS ${JXL_INCLUDE_DIR})
            target_compile_definitions(syncmoo1 PRIVATE JXL_STATIC_DEFINE)
            target_compile_definitions(termgfx PRIVATE JXL_STATIC_DEFINE)
        endif()
    endif()
    if(NOT JPEG_XL_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(JPEG_XL libjxl)
        endif()
    endif()
    if(NOT JPEG_XL_FOUND AND NOT MSVC)
        find_library(JPEG_XL_LIBRARIES NAMES jxl)
        if(JPEG_XL_LIBRARIES)
            set(JPEG_XL_FOUND TRUE)
        endif()
    endif()
    if(JPEG_XL_FOUND)
        target_compile_definitions(syncmoo1 PRIVATE WITH_JXL)
        target_include_directories(syncmoo1 PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        # termgfx/jxl.c needs the JXL headers + WITH_JXL to compile the
        # encoder; the libjxl symbols resolve at syncmoo1's final link.
        target_compile_definitions(termgfx PRIVATE WITH_JXL)
        target_include_directories(termgfx PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        target_link_directories(syncmoo1 PRIVATE ${JPEG_XL_LIBRARY_DIRS})
        target_link_libraries(syncmoo1 ${JPEG_XL_LIBRARIES})
        message(STATUS "syncmoo1: JPEG XL graphics tier ENABLED")
    else()
        message(WARNING "syncmoo1: libjxl not found -- building WITHOUT the JXL tier (sixel only)")
    endif()
endif()

# xpdev: ini_file (syncmoo1.ini parsing) and sockets (sockwrap) for
# the door I/O port. Build the xpdev CMake target so this works on every
# toolchain; pin off its audio + crypto backends so the sub-build stays lean
# (cf. syncduke/syncdoom CMakeLists.txt). The guard avoids re-adding xpdev when
# syncmoo1 is built inside a larger tree that already has it.
if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
    set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
    foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
        set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev xpdev EXCLUDE_FROM_ALL)
endif()
target_include_directories(syncmoo1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(syncmoo1 xpdev)

if(NOT WIN32)
    target_link_libraries(syncmoo1 m pthread ${CMAKE_DL_LIBS})
endif()

# Pure unit tests (syncmoo1_map.c only -- see tests/CMakeLists.txt). Only
# add it when this door is the top-level project, so building syncmoo1 as
# part of a larger tree never drags in the test target unasked.
if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests tests)
endif()
