cmake_minimum_required(VERSION 3.25)
project(syncconquer C CXX)

# SyncConquer -- Red Alert (Vanilla Conquer engine) for Synchronet /
# SyncTERM (DOOR32.SYS BBS door). Parallels src/doors/syncduke/ and
# src/doors/syncdoom/: a vendored engine (vanilla/, Vanilla Conquer) with
# its platform layer (video/keyboard) replaced by termgfx-backed shims
# (door/video_termgfx.cpp, door/wwkeyboard_termgfx.cpp) that hand frames to
# / take input from door/door_core.c. See DESIGN.md, PROVENANCE.md, and
# plans/m2-proto/FACTS.md for the engine-seam rationale this evolved from.
#
# The vendored engine's own CMakeLists.txt (vanilla/CMakeLists.txt) already
# builds a headless "null" video/keyboard/audio configuration when
# SDL2/OPENAL are OFF (how the Remaster DLL target builds); this project
# pins that configuration below, then -- post-hoc, since the targets don't
# exist until add_subdirectory(vanilla) returns -- swaps the null keyboard
# backend for ours and turns on NEW_VIDEO_BUILD for it. See the commonv /
# VanillaRA block for exactly why and how.
# BUILD_VANILLATD: default OFF (v1 = Red Alert / syncalert only), but overridable
# -- pass -DBUILD_VANILLATD=ON to also build the Tiberian Dawn engine + door
# (syncdawn). Plain CACHE (no FORCE) so the RA-only default is untouched while a
# separate build dir can opt TD in. The VanillaTD target config is a guarded
# block at the bottom of this file (if(TARGET VanillaTD)).
set(BUILD_VANILLATD OFF CACHE BOOL "Also build the Tiberian Dawn engine + syncdawn door")
set(SDL2 OFF CACHE BOOL "" FORCE)
set(OPENAL OFF CACHE BOOL "" FORCE)

# vanilla/CMakeLists.txt (and redalert/CMakeLists.txt) resolve several
# paths from CMAKE_SOURCE_DIR assuming vanilla/ itself is the top-level
# project -- true for the M1 standalone build (build.sh passed -S vanilla
# directly), no longer true now that vanilla/ is a subdirectory of this
# door-level project. CMAKE_SOURCE_DIR can't be overridden for a
# subdirectory's scope (CMake recomputes it to the real top of the source
# tree on every directory scope, ignoring any set() in a parent), so each
# use gets its own narrow fix below rather than a patch to the vendored
# CMakeLists.txt files (PROVENANCE.md's local-patches list stays limited
# to video_null.cpp/conquer.cpp/globals.cpp/init.cpp):
#   - cmake/ (CMAKE_MODULE_PATH, for include(Compilers)/GitWatcher/
#     ProductVersion/BuildIcons): add the real path ourselves.
#   - resources/ (the vanillara icon build): resources/ below is a symlink
#     to vanilla/resources so "${CMAKE_SOURCE_DIR}/resources/..." (really
#     this project's root) still resolves.
#   - VanillaRA's own #include search path (target_include_directories(
#     VanillaRA PUBLIC ${CMAKE_SOURCE_DIR} .), how e.g. conquer.cpp's
#     #include "common/framelimit.h" resolves): add the real vanilla/ dir
#     as an extra PUBLIC include dir after add_subdirectory() returns.
#   - cmake/ (ours, listed FIRST): shadows vanilla/cmake/BuildIcons.cmake so
#     redalert/CMakeLists.txt's include(BuildIcons) picks up our no-op
#     make_icon(). The vendored one FATAL_ERRORs on a Windows checkout, where
#     the resources/ symlink below is a plain text file. See
#     cmake/BuildIcons.cmake for the full rationale.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vanilla/cmake")
add_subdirectory(vanilla)
target_include_directories(VanillaRA PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vanilla)

# The vendored redalert/CMakeLists.txt links VanillaRA as a GUI app
# (/subsystem:windows, with /ENTRY:mainCRTStartup to keep main()). That's right
# for a desktop vanillara.exe and wrong for a BBS door: with no console attached,
# the CRT hands main() invalid std handles, so every stdio write -- including
# door_core.c's local-mode/-h output and the fatal-error messages a sysop needs
# when the door won't start -- goes nowhere. Re-link as a console app; the last
# /subsystem on the link line wins, and these options land after the vendored
# ones because add_subdirectory(vanilla) has already returned.
if(MSVC)
    target_link_options(VanillaRA PRIVATE /subsystem:console /ENTRY:mainCRTStartup)
endif()

# libtermgfx -- the shared sixel/JXL encoders + APC transport + cap-probe +
# audio that door_core.c calls into (this task uses only sixel_encode()).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)

# ---------------------------------------------------------------------------
# Fold the door's engine-seam TUs into VanillaRA, the null-backend build
# selected above (commonv's else() branch in vanilla/common/CMakeLists.txt
# already picked video_null.cpp + wwkeyboard_null.cpp since SDL2/OPENAL/
# DDRAW/DSOUND are all off). Two things the vendored CMake can't express on
# its own, both required by the door build and neither wanted for a
# vendored-vanilla-only build (that configuration must keep working
# unmodified -- see PROVENANCE.md's M1 notes):
#
# 1. Drop wwkeyboard_null.cpp from commonv's sources. It defines
#    CreateWWKeyboardClass(), and so does door/wwkeyboard_termgfx.cpp
#    (added to VanillaRA below); linking both is a duplicate symbol.
#    video_null.cpp (locally patched -- PROVENANCE.md -- to hold real
#    surface memory) stays: the door's video_termgfx.cpp supplies only the
#    three free functions video_null.cpp doesn't define (Get_Video_Mouse,
#    Set_Video_Cursor, Video_Render_Frame), it is not a full backend.
# 1b. Drop soundio_null.cpp from commonv's sources too (Task 4, audio): it's
#    a full no-op reimplementation of the whole audio.h game-facing API
#    (Play_Sample/Load_Sample/...), which would conflict with
#    soundio_common.cpp below (the SAME API, backed by the real AUD-decode/
#    streaming logic). vqaaudio_null.cpp is ALSO dropped (see below): FMV
#    cutscenes play (video + A/V-synced audio), so door/vqaaudio_termgfx.cpp
#    replaces the stub rather than leaving the VQA player's audio hooks no-ops.
#    soundio_common.cpp is normally paired with soundio_openal.cpp
#    (OPENAL=ON, vanilla/common/CMakeLists.txt); we add it here unmodified
#    and pair it with door/soundio_termgfx.cpp (added to VanillaRA below)
#    instead -- same seam (soundio_imp.h's SoundImp_* functions), backed by
#    termgfx's audio_mgr instead of a real OpenAL device.
# 2. Define NEW_VIDEO_BUILD for that build. Upstream only sets it for the
#    SDL1/SDL2 branches (vanilla/common/CMakeLists.txt); it's what
#    GameInFocus (redalert/globals.cpp), Get_Video_Mouse's callers
#    (wwmouse.cpp), and Frame_Limiter's render call (framelimit.cpp) all
#    gate on -- see plans/m2-proto/FACTS.md #2/#5/#7. GameInFocus being
#    forced true is also what lets soundio_common.cpp's
#    Start_Primary_Sound_Buffer() ever return true (it refuses while
#    !GameInFocus).
# ---------------------------------------------------------------------------
get_target_property(_syncalert_commonv_srcs commonv SOURCES)
list(FILTER _syncalert_commonv_srcs EXCLUDE REGEX "wwkeyboard_null\\.cpp$")
list(FILTER _syncalert_commonv_srcs EXCLUDE REGEX "soundio_null\\.cpp$")
# FMV cutscene audio: drop the vqaaudio_null.cpp stub and use door/
# vqaaudio_termgfx.cpp (below), which ships decoded VQA audio to the client via
# termgfx's streaming path so cutscenes have dialog.
list(FILTER _syncalert_commonv_srcs EXCLUDE REGEX "vqaaudio_null\\.cpp$")
list(APPEND _syncalert_commonv_srcs "${CMAKE_CURRENT_SOURCE_DIR}/vanilla/common/soundio_common.cpp")
set_target_properties(commonv PROPERTIES SOURCES "${_syncalert_commonv_srcs}")
target_compile_definitions(commonv PUBLIC NEW_VIDEO_BUILD)

target_sources(VanillaRA PRIVATE
    door/video_termgfx.cpp
    door/wwkeyboard_termgfx.cpp
    door/soundio_termgfx.cpp
    door/vqaaudio_termgfx.cpp
    door/door_core.c
    door/door_io.c
    door/door_input.c
    door/door_node.c
)
target_include_directories(VanillaRA PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/door)
target_link_libraries(VanillaRA termgfx)

# Windows: keep the argv our pre-main constructor rewrote.
#
# door_io.c's constructor neuters the door's own tokens out of argv (-s<fd>,
# -home <dir>, the DOOR32.SYS drop-file path -- Parse_Command_Line() strstr()s
# every token, so an unsanitized path can mis-set an engine option), and can
# inject -NOMOVIES into a freed slot when SYNCALERT_NOMOVIES is set (movies play
# by default now). On *nix the engine's main() hands that argv straight to
# Parse_Command_Line() and it all works. On Windows it does NOT:
# redalert/startup.cpp's `UtfArgs args(argc, argv)` REPLACES ArgV with a fresh
# parse of GetCommandLineW(), silently throwing our edits away -- so the engine
# saw the raw command line, the door's own tokens (and any -NOMOVIES
# suppression) never took, and the door's flags reached the engine's parser.
#
# The vendored utfargs.h is patched to honor this define (PROVENANCE.md), which
# is scoped to VanillaRA -- startup.cpp is its only includer. UNICODE/_UNICODE
# stay defined, so common/'s UTF-8 path handling (utf.h's TCHARToUTF8,
# paths_win.cpp, rawfile.cpp) is unaffected; only the argv re-derivation is
# skipped. Engine argv is then the CRT's, exactly as on *nix.
if(WIN32)
    target_compile_definitions(VanillaRA PRIVATE SYNCALERT_KEEP_CRT_ARGV)
endif()

# ---------------------------------------------------------------------------
# JPEG XL -- the APC/SyncTERM graphics tier (shared with SyncDuke/SyncDOOM via
# termgfx). Optional: without it door_io.c's JXL path is #ifdef WITH_JXL
# gated and termgfx/jxl.c compiles a return-0 stub, so the door still serves
# sixel/PPM/text. *nix: system libjxl via pkg-config (or a bare find_library
# fallback). Windows/MSVC: an MSVC-built static libjxl + deps (vcpkg) --
# mirrors src/doors/syncduke/CMakeLists.txt's block for the syncduke target.
# ---------------------------------------------------------------------------
option(WITHOUT_JPEG_XL "Build without the JPEG XL graphics tier (sixel/PPM only)" OFF)
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
            target_compile_definitions(VanillaRA PRIVATE JXL_STATIC_DEFINE)
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
        target_compile_definitions(VanillaRA PRIVATE WITH_JXL)
        target_include_directories(VanillaRA PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        # termgfx/jxl.c needs the JXL headers + WITH_JXL to compile the encoder;
        # the libjxl symbols resolve at VanillaRA's final link (JPEG_XL_LIBRARIES).
        target_compile_definitions(termgfx PRIVATE WITH_JXL)
        target_include_directories(termgfx PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        target_link_directories(VanillaRA PRIVATE ${JPEG_XL_LIBRARY_DIRS})
        target_link_libraries(VanillaRA ${JPEG_XL_LIBRARIES})
        message(STATUS "syncalert: JPEG XL graphics tier ENABLED")
    else()
        message(WARNING "syncalert: libjxl not found -- building WITHOUT the JXL tier (sixel/PPM/text only)")
    endif()
endif()

# ---------------------------------------------------------------------------
# xpdev (Task 4, audio): termgfx/audio_mgr.c needs genwrap.h's safe_snprintf
# and threadwrap.h's _beginthread/mutex wrappers (the async-music worker
# thread) unconditionally -- every audio_mgr.c consumer links xpdev for this,
# same as src/doors/syncduke/CMakeLists.txt. Pin off xpdev's own local-audio-
# device backends (unrelated to termgfx's SyncTERM-side audio) and crypto so
# this sub-build stays lean; the guard avoids re-adding xpdev when syncconquer
# is built inside a larger tree that already has it.
# ---------------------------------------------------------------------------
if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
    set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
    foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
        set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev xpdev EXCLUDE_FROM_ALL)
endif()
target_include_directories(VanillaRA PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(VanillaRA xpdev)

# Match the door's binary name (build.sh/deploy.sh expect build/syncalert
# regardless of how the vendored redalert/CMakeLists.txt names its own
# vanillara executable).
set_target_properties(VanillaRA PROPERTIES OUTPUT_NAME syncalert)

# ---------------------------------------------------------------------------
# Tiberian Dawn (syncdawn) -- the SECOND title, built from the SAME door seam +
# vendored engine. add_subdirectory(vanilla) above created VanillaTD only when
# BUILD_VANILLATD is ON, so this whole block is a no-op in the default RA-only
# build (protecting it). It mirrors every VanillaRA target tweak above: console
# subsystem, the door engine-seam TUs, termgfx, JXL, xpdev, the Windows argv
# define, and the door binary name (syncdawn). The commonv modifications
# (null-backend swaps + NEW_VIDEO_BUILD) are on the shared commonv lib, so both
# targets inherit them -- not repeated here. The engine-specific door coupling
# (SyncConquer_Music_Volume_Step) is provided by tiberiandawn/options.cpp
# (PROVENANCE #21), so the shared door TUs link against TD too.
# ---------------------------------------------------------------------------
if(TARGET VanillaTD)
    if(MSVC)
        target_link_options(VanillaTD PRIVATE /subsystem:console /ENTRY:mainCRTStartup)
    endif()
    target_include_directories(VanillaTD PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vanilla)
    target_sources(VanillaTD PRIVATE
        door/video_termgfx.cpp
        door/wwkeyboard_termgfx.cpp
        door/soundio_termgfx.cpp
        door/vqaaudio_termgfx.cpp
        door/door_core.c
        door/door_io.c
        door/door_input.c
        door/door_node.c
    )
    target_include_directories(VanillaTD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/door)
    target_link_libraries(VanillaTD termgfx)
    # Select the Tiberian Dawn branding + MIX manifest + data/log basename in the
    # shared door code (door_io.c). RA is the default (no define needed).
    target_compile_definitions(VanillaTD PRIVATE SYNCCONQUER_TD)
    if(WIN32)
        target_compile_definitions(VanillaTD PRIVATE SYNCALERT_KEEP_CRT_ARGV)
    endif()
    if(NOT WITHOUT_JPEG_XL AND JPEG_XL_LIBRARIES)
        target_compile_definitions(VanillaTD PRIVATE WITH_JXL)
        target_include_directories(VanillaTD PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        target_link_libraries(VanillaTD ${JPEG_XL_LIBRARIES})
        if(MSVC)
            target_compile_definitions(VanillaTD PRIVATE JXL_STATIC_DEFINE)
        endif()
    endif()
    target_include_directories(VanillaTD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
    target_link_libraries(VanillaTD xpdev)
    set_target_properties(VanillaTD PROPERTIES OUTPUT_NAME syncdawn)
endif()
