cmake_minimum_required(VERSION 3.13)
project(syncretro C CXX)   # CXX: termgfx links C++ libADLMIDI

# syncretro -- legacy game consoles as a Synchronet / SyncTERM door, by hosting
# an external libretro core (FreeIntv, fceumm, ...) and rendering it through the
# shared ../termgfx stack. UNLIKE the sibling doors, there is NO vendored engine
# source tree: the "engine" is a core .so loaded at runtime (dlopen), so we own
# 100% of the C here. The only vendored artifact is libretro.h (PROVENANCE.md).
# See DESIGN.md for the architecture.

add_executable(syncretro
    main.c
    retro_core.c        # dlopen the core, resolve the retro_* ABI
    retro_env.c         # the retro_environment_t callback (frontend policy)
    retro_options.c     # core options: what the core offers, what a console pins
    retro_bridge.c      # video / audio / input callbacks -> termgfx + door I/O
    syncretro_io.c      # terminal enter/probe/leave + present path
    syncretro_input.c   # socket decode -> cached RetroPad
    syncretro_binds.c   # the binding tables: keys -> pad / keypad / door
    syncretro_profile.c # which console's controller the keyboard imitates
    syncretro_keypad.c  # Intellivision keypad digits -> RetroPad analog vector
    syncretro_audio.c   # the core's PCM stream -> SyncTERM's audio APC (M4)
    syncretro_door.c    # DOOR32.SYS / -s<fd> / -name / -home / -t / -core
    syncretro_config.c  # per-user sandbox + system/save dirs
    syncretro_quant.c   # truecolor frame -> 256-color indexed, for the sixel tier
    syncretro_dirty.c   # which cells changed: patch the frame instead of repainting
    syncretro_plat.c)   # the door's Windows-vs-POSIX seam (clock/sleep/socket/stderr)

# libretro.h lives in this dir (vendored). Our own code is house-style tabs.
target_include_directories(syncretro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(syncretro PROPERTIES LINKER_LANGUAGE CXX C_STANDARD 11)

# ---------------------------------------------------------------------------
# MSVC / Windows. Win32 (x86) is the one supported Windows target -- see
# build.bat's header for why (a Win32 door runs on both a Win32 and a future
# Win64 Synchronet host; DOOR32.SYS's socket handle is 32-bit-significant and
# crosses the process-bitness boundary). All of the door's platform difference
# is in syncretro_plat.c + retro_core.c's dlopen seam; the block below just
# supplies the CRT quiets, the Winsock link, and crash-symbolication flags,
# mirroring ../syncmoo1/CMakeLists.txt.
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_definitions(syncretro PRIVATE
        _CRT_SECURE_NO_WARNINGS          # fopen/strcpy/snprintf "use the _s variant"
        _CRT_NONSTDC_NO_DEPRECATE        # getcwd/chdir POSIX names (syncretro_config.c)
        _WINSOCK_DEPRECATED_NO_WARNINGS) # inet_addr et al. reached via sockwrap.h

    # Winsock: the door's DOOR32.SYS handle is a SOCKET (see syncretro_plat.c).
    target_link_libraries(syncretro ws2_32)

    # /Zi emits a .pdb for the optimized Release build and /MAP a symbol map, so
    # procdump+cdb can symbolize a crash dump; /OPT:REF,/OPT:ICF restore the
    # optimizations /DEBUG would otherwise disable (cf. syncmoo1/syncduke).
    target_compile_options(syncretro PRIVATE /Zi)
    target_link_options(syncretro PRIVATE /MAP /DEBUG /OPT:REF /OPT:ICF)
endif()

# libtermgfx -- shared sixel/JXL encode + APC transport + cap-probe + geometry +
# pacing + sbbs_node. PUBLIC include dir gives us sixel.h/jxl.h/term.h/caps.h/
# geometry.h/pace.h/audio_mgr.h/sbbs_node.h.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)
target_link_libraries(syncretro termgfx)

# Git build info (git_hash.h), via the shared helper in src/build.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../build/SynchronetMacros.cmake)
synchronet_gitinfo(syncretro)

# ---------------------------------------------------------------------------
# JPEG XL -- the APC/SyncTERM graphics tier (shared with the other doors via
# termgfx). Optional: without it the door serves the sixel/text tiers (the JXL
# present path is #ifdef WITH_JXL gated, and termgfx/jxl.c compiles a return-0
# stub). Mirrors ../syncmoo1/CMakeLists.txt: on MSVC the static libjxl + its
# dependencies come from a classic-mode vcpkg prefix (x86-windows-static-md);
# everywhere else, pkg-config / find_library.
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
            target_compile_definitions(syncretro PRIVATE JXL_STATIC_DEFINE)
            target_compile_definitions(termgfx   PRIVATE JXL_STATIC_DEFINE)
        endif()
    endif()
    if(NOT JPEG_XL_FOUND AND NOT MSVC)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(JPEG_XL libjxl)
        endif()
        if(NOT JPEG_XL_FOUND)
            find_library(JPEG_XL_LIBRARIES NAMES jxl)
            if(JPEG_XL_LIBRARIES)
                set(JPEG_XL_FOUND TRUE)
            endif()
        endif()
    endif()
    if(JPEG_XL_FOUND)
        target_compile_definitions(syncretro PRIVATE WITH_JXL)
        target_compile_definitions(termgfx   PRIVATE WITH_JXL)
        target_include_directories(syncretro PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        target_include_directories(termgfx   PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        target_link_directories(syncretro PRIVATE ${JPEG_XL_LIBRARY_DIRS})
        target_link_libraries(syncretro ${JPEG_XL_LIBRARIES})
        message(STATUS "syncretro: JPEG XL graphics tier ENABLED")
    else()
        message(WARNING "syncretro: libjxl not found -- building WITHOUT the JXL tier (sixel only)")
    endif()
endif()

# xpdev: sockets (sockwrap) + ini_file for the door I/O port + config. Pin off
# its audio + crypto backends so the sub-build stays lean (cf. the sibling
# doors). Guard against re-adding when built inside a larger tree.
if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
    set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
    foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
        set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev xpdev EXCLUDE_FROM_ALL)
endif()
target_include_directories(syncretro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(syncretro xpdev)

# dlopen for the core; m/pthread for termgfx.
if(NOT WIN32)
    target_link_libraries(syncretro m pthread ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------------------
# probe_core -- the core diagnostic (opt-in: cmake -DSYNCRETRO_PROBE=ON).
#
# NOT a test and NOT part of the door: it loads a core, reports what the core
# actually does (rotation, geometry changes, options, colour count, a thumbnail)
# and exits. It is how a console's cost gets measured BEFORE an install dir is
# written -- M3_MULTICORE.md sec 2's fceumm table came from its throwaway
# ancestor. See probe_core.c's header.
#
# It links the door's REAL retro_core.c so what it measures is what the door
# would do, and supplies its own sr_bridge_install()/sr_environment() -- hence
# no termgfx, no xpdev, no libjxl, and no terminal.
# ---------------------------------------------------------------------------
option(SYNCRETRO_PROBE "Build probe_core, the libretro core diagnostic" OFF)
if(SYNCRETRO_PROBE)
    add_executable(probe_core probe_core.c retro_core.c retro_options.c)
    target_include_directories(probe_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(probe_core PROPERTIES C_STANDARD 11)
    if(MSVC)
        target_compile_definitions(probe_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_link_libraries(probe_core ${CMAKE_DL_LIBS})
    endif()
endif()

# ---------------------------------------------------------------------------
# Unit tests. Opt-in: cmake -DSYNCRETRO_TESTS=ON. No core is ever linked, and
# the first three link no termgfx either -- they are pure (test_audio_bytes is
# the exception; see its own note below). test_keypad.c in particular is the
# only gate we have on the keypad digit map, because exercising a real
# cartridge needs the BIOS (see M2_INPUT.md sec 7).
# ---------------------------------------------------------------------------
option(SYNCRETRO_TESTS "Build the SyncRetro unit tests" OFF)
if(SYNCRETRO_TESTS)
    enable_testing()

    add_executable(test_keypad test_keypad.c syncretro_keypad.c)
    target_include_directories(test_keypad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT WIN32)
        target_link_libraries(test_keypad m)   # libm: no such separate lib on MSVC
    endif()
    add_test(NAME keypad COMMAND test_keypad)

    add_executable(test_binds test_binds.c syncretro_binds.c syncretro_profile.c)
    target_include_directories(test_binds PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME binds COMMAND test_binds)

    # test_axis -- like test_audio_bytes, this one links the REAL module
    # (syncretro_input.c) rather than a stub, and fakes only the platform read
    # seam: the claim under test is that a keystroke survives the whole door
    # path and arrives at the core as an analog deflection. A twin-stick
    # cabinet's second stick lives on that axis and nowhere else, so a stubbed
    # input layer would prove nothing. termgfx for the key-mode parsers.
    add_executable(test_axis test_axis.c syncretro_input.c syncretro_binds.c
                   syncretro_profile.c syncretro_keypad.c)
    target_include_directories(test_axis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                               ${CMAKE_CURRENT_SOURCE_DIR}/../termgfx
                               ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
    target_link_libraries(test_axis termgfx xpdev)
    if(NOT WIN32)
        target_link_libraries(test_axis m pthread)
    endif()
    add_test(NAME axis COMMAND test_axis)

    # test_games -- the per-cabinet table. Links xpdev for the ini reader; no
    # termgfx and no core, so it runs anywhere.
    add_executable(test_games test_games.c syncretro_games.c)
    target_include_directories(test_games PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                               ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
    target_link_libraries(test_games xpdev)
    add_test(NAME games COMMAND test_games)

    add_executable(test_quant test_quant.c syncretro_quant.c)
    target_include_directories(test_quant PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME quant COMMAND test_quant)

    # test_dirty pins the rectangle geometry: that the rects COVER every changed
    # pixel (a miss leaves stale pixels on screen until the next full frame) and
    # stay cell-aligned (a sixel can only be drawn at a cell corner).
    add_executable(test_dirty test_dirty.c syncretro_dirty.c)
    target_include_directories(test_dirty PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME dirty COMMAND test_dirty)

    # test_audio_bytes -- UNLIKE the three tests above, this one links the
    # real termgfx audio encoder (needs libsndfile): the whole point is
    # driving the actual call path syncretro_audio.c hands it, not a stub.
    # It stubs only the module's door seam (three output calls, five config
    # getters -- see the file header) and diffs a normalized transcript
    # against test_audio_bytes.golden, the byte-capture guard for the M4
    # audio extraction into libtermgfx.
    add_executable(test_audio_bytes test_audio_bytes.c syncretro_audio.c)
    target_include_directories(test_audio_bytes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_audio_bytes termgfx)
    if(NOT WIN32)
        target_link_libraries(test_audio_bytes m pthread)
    endif()
    add_test(NAME audio_bytes COMMAND test_audio_bytes --check
             ${CMAKE_CURRENT_SOURCE_DIR}/test_audio_bytes.golden)
endif()
