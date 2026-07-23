# termgfx -- Synchronet terminal-graphics door library.
#
# Reusable, engine-agnostic pieces shared by SyncDOOM (and future framebuffer
# doors like SyncDuke): render a game's framebuffer to the user's terminal, pace
# delivery over the link, and wire in the BBS (who's-online / paging). This first
# slice holds the two already-standalone modules; more of syncdoom.c's renderer,
# pacing and input move here over time (see the extraction plan).
#
#   sixel.{c,h} -- indexed DECSIXEL encoder (was render_sixel)
#   term.{c,h}  -- canonical terminal control strings (enter/probe/leave;
#                  notably the DECSDM ?80 sixel-scrolling reset)
#   sbbs_node  -- door-native node list / paging / who's-online (node.dab/.exb)

add_library(termgfx STATIC
    sixel.c
    apc.c
    jxl.c
    caps.c
    audio.c
    audio_mgr.c
    audio_midi.c
    audio_stream.c
    chunk.c
    text.c
    term.c
    geometry.c
    hash.c
    door32.c
    charset.c
    sgrmouse.c
    mouse.c
    keymode.c
    pace.c
    idle.c
    stats.c
    sst_plat.c
    termgfx_termio.c
    termgfx_quant.c
    sbbs_node.c)

# Public: consumers that #include "sixel.h" / "sbbs_node.h" get this dir.
target_include_directories(termgfx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Private: sbbs_node.c needs nodedefs.h (BBS source) + xpdev headers
# (dirwrap/filewrap). Headers only -- the xpdev *symbols* resolve when the door
# links xpdev; termgfx itself pulls in no engine/sbbs/xpdev linkage.
target_include_directories(termgfx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../sbbs3
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)

# libADLMIDI -- vendored OPL3 (Nuked) MIDI synth, for authentic FM game music
# (audio_midi.c renders Duke .MID / Doom MUS to PCM via its C API). Force a lean
# static build: Nuked emulator only, embedded banks (no external SoundFont),
# MUS+XMI in (Doom uses MUS), and all the demo/util/test sub-projects off so the
# trimmed vendor tree (no utils/test/examples) configures cleanly.
set(libADLMIDI_STATIC ON  CACHE BOOL "" FORCE)
set(libADLMIDI_SHARED OFF CACHE BOOL "" FORCE)
set(USE_NUKED_EMULATOR  ON  CACHE BOOL "" FORCE)
set(USE_DOSBOX_EMULATOR OFF CACHE BOOL "" FORCE)
set(USE_OPAL_EMULATOR   OFF CACHE BOOL "" FORCE)
set(USE_JAVA_EMULATOR   OFF CACHE BOOL "" FORCE)
set(WITH_MIDI_SEQUENCER ON  CACHE BOOL "" FORCE)
set(WITH_EMBEDDED_BANKS ON  CACHE BOOL "" FORCE)
set(WITH_MUS_SUPPORT    ON  CACHE BOOL "" FORCE)
set(WITH_XMI_SUPPORT    ON  CACHE BOOL "" FORCE)
set(WITH_MIDIPLAY OFF CACHE BOOL "" FORCE)
set(WITH_OLD_UTILS OFF CACHE BOOL "" FORCE)
set(WITH_GENADLDATA OFF CACHE BOOL "" FORCE)
set(EXAMPLE_SDL2_AUDIO OFF CACHE BOOL "" FORCE)
set(WITH_VLC_PLUGIN OFF CACHE BOOL "" FORCE)
add_subdirectory(libADLMIDI EXCLUDE_FROM_ALL)
# PUBLIC so the door's final link pulls in ADLMIDI_static (+ its include dir);
# a C executable linking this C++ lib also needs the C++ runtime -- the door
# CMake adds stdc++ on non-MSVC.
target_link_libraries(termgfx PUBLIC ADLMIDI_static)

# libsndfile (OPTIONAL) -- encode rendered music PCM to OGG/Vorbis before the
# SyncTERM:C;S upload. Raw 44.1k/stereo/16-bit PCM is huge (a 60-240s track =
# 11-40MB) -- uploaded over the wire each session AND parked on the player's
# SyncTERM disk cache; OGG/Vorbis shrinks that ~10-15x. SyncTERM decodes OGG via
# the very same libsndfile (Deuce bundles it on Windows too as of 2026-06-29).
# Compile-time gated: found -> TERMGFX_WITH_SNDFILE + OGG encode in audio.c;
# absent -> raw-PCM WAV fallback (termgfx_audio_have_ogg() == 0).
#
# Detection order:
#   1. find_package(SndFile CONFIG) -- the vcpkg / modern-CMake path. This is the
#      ONLY way it resolves under MSVC (no pkg-config there), and it chains in
#      libsndfile's transitive static deps (Vorbis/Ogg/FLAC/Opus) via its own
#      config package, so we don't enumerate them by hand. Works in both classic
#      vcpkg mode (CMAKE_PREFIX_PATH -> the installed prefix) and manifest mode.
#   2. pkg-config -- the usual *nix system-libsndfile path.
find_package(SndFile CONFIG QUIET)
if(SndFile_FOUND)
    target_compile_definitions(termgfx PRIVATE TERMGFX_WITH_SNDFILE)
    target_link_libraries(termgfx PUBLIC SndFile::sndfile)
    message(STATUS "termgfx: libsndfile ${SndFile_VERSION} (CONFIG) found -> OGG music compression")
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SNDFILE QUIET sndfile)
    endif()
    if(SNDFILE_FOUND)
        target_compile_definitions(termgfx PRIVATE TERMGFX_WITH_SNDFILE)
        target_include_directories(termgfx PRIVATE ${SNDFILE_INCLUDE_DIRS})
        target_link_libraries(termgfx PUBLIC ${SNDFILE_LIBRARIES})
        target_link_directories(termgfx PUBLIC ${SNDFILE_LIBRARY_DIRS})
        message(STATUS "termgfx: libsndfile ${SNDFILE_VERSION} (pkg-config) found -> OGG music compression")
    else()
        message(STATUS "termgfx: libsndfile NOT found -> music ships as raw PCM WAV")
    endif()
endif()

set_property(TARGET termgfx PROPERTY C_STANDARD 11)

if(MSVC)
    target_compile_definitions(termgfx PRIVATE
        _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    target_compile_definitions(termgfx PRIVATE _DEFAULT_SOURCE __unix__)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(termgfx PRIVATE LINUX)
    endif()
    target_compile_options(termgfx PRIVATE -Wall -Os -ggdb3)
endif()

# Opt-in unit tests (see test/CMakeLists.txt). OFF by default so a consuming
# door's add_subdirectory() of this library never builds them.
option(TERMGFX_TESTS "Build the libtermgfx unit tests" OFF)
if(TERMGFX_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
