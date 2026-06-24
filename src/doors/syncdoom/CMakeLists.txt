cmake_minimum_required(VERSION 3.13)
project(syncdoom C)

# syncdoom -- Doom for Synchronet / SyncTERM (DOOR32.SYS BBS door).
#
# Cross-platform build, modeled on src/syncterm/CMakeLists.txt (the canonical
# Synchronet CMake build). Builds the door on Linux/GCC and is structured so a
# Windows/MSVC build can be added later (see the WINDOWS notes below).

option(WITHOUT_JPEG_XL "Build without the JPEG XL graphics tier (sixel/text only)" OFF)

# ---------------------------------------------------------------------------
# Vendored Doom engine (doomgeneric / Chocolate Doom) + syncdoom backend + the
# Chocolate Doom net layer.
# ---------------------------------------------------------------------------
set(SOURCES
    dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c
    d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c
    hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c i_scale.c
    i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c m_cheat.c
    m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c p_ceilng.c
    p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c
    p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c
    p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c
    r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c statdump.c st_lib.c
    st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c w_checksum.c w_file.c
    w_main.c w_wad.c z_zone.c w_file_stdc.c i_input.c i_video.c doomgeneric.c
    render_text.c
    deh_str.c deh_io.c deh_main.c deh_mapping.c deh_text.c deh_ammo.c
    deh_bexstr.c deh_cheat.c deh_doom.c deh_frame.c deh_misc.c deh_ptr.c
    deh_sound.c deh_thing.c deh_weapon.c w_merge.c
    net_client.c net_common.c net_dedicated.c net_io.c net_loop.c net_packet.c
    net_query.c net_server.c net_structrw.c aes_prng.c net_gui.c net_udp.c
    mp_server.c
    syncdoom.c)

add_executable(syncdoom ${SOURCES})
target_include_directories(syncdoom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# termgfx: shared terminal-graphics door library (sixel DECSIXEL encoder +
# sbbs_node who's-online/paging). Its PUBLIC include dir gives syncdoom.c the
# sixel.h / sbbs_node.h headers; the sbbs3/xpdev header paths sbbs_node needs
# are now termgfx's concern. More of the renderer/pacing/input moves here over
# time (see the extraction plan).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)
target_link_libraries(syncdoom termgfx)

# Git build info (git_hash.h: GIT_HASH/GIT_DATE/GIT_TIME) for the startup banner
# and the F1 controls screen, via the shared helper in src/build. Regenerated each
# build into the binary dir; syncdoom.c and m_menu.c #include "git_hash.h".
include(${CMAKE_CURRENT_SOURCE_DIR}/../../build/SynchronetMacros.cmake)
synchronet_gitinfo(syncdoom)

# ---------------------------------------------------------------------------
# xpdev: cross-platform sockets (sockwrap), recursive mkpath (dirwrap), ini_file.
# Build the xpdev CMake target (rather than linking a prebuilt static lib) so
# this works the same on every toolchain. syncdoom only needs sockets/dirs/ini --
# pin off xpdev's audio + crypto backends so the sub-build stays lean and doesn't
# require OpenSSL/SDL/etc. (cf. the same pinning in syncterm/CMakeLists.txt).
# ---------------------------------------------------------------------------
if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
    set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
    foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
        set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev xpdev EXCLUDE_FROM_ALL)
endif()
target_include_directories(syncdoom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(syncdoom xpdev)

# ---------------------------------------------------------------------------
# Platform flags.
#
# *nix (GCC/Clang): the doomgeneric platform defines + -Wall -Os.  (NORMALUNIX /
# SNDSERV are vestigial doomgeneric knobs -- nothing in the compiled sources
# references them -- but kept for parity with the upstream build.)
#
# Windows/MSVC: no doomgeneric platform define is needed (the audio/video
# back ends those knobs gate are not compiled here -- syncdoom.c is the back
# end).  Winsock, winmm and friends come transitively from the xpdev target
# (it PUBLIC-links ws2_32/winmm/... and PUBLIC-defines _WIN32), so the only
# Windows-specific bits are silencing the CRT's POSIX-name/secure-API
# deprecation warnings and pinning the C standard for C99/C11 constructs.
# ---------------------------------------------------------------------------
set_property(TARGET syncdoom PROPERTY C_STANDARD 11)

if(NOT WIN32)
    target_compile_definitions(syncdoom PRIVATE NORMALUNIX SNDSERV _DEFAULT_SOURCE __unix__)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(syncdoom PRIVATE LINUX)
    endif()
    target_link_libraries(syncdoom m pthread ${CMAKE_DL_LIBS})
endif()

if(MSVC)
    target_compile_definitions(syncdoom PRIVATE
        _CRT_SECURE_NO_WARNINGS          # fopen/strcpy/snprintf "use the _s variant"
        _CRT_NONSTDC_NO_DEPRECATE        # getpid/getcwd/chdir/strdup POSIX names
        _WINSOCK_DEPRECATED_NO_WARNINGS) # inet_addr/gethostbyname et al.
    target_link_libraries(syncdoom winmm)   # timeGetTime() (also pulled in via xpdev)
else()
    target_compile_options(syncdoom PRIVATE -Wall -Os -ggdb3)
    include(CheckLinkerFlag OPTIONAL RESULT_VARIABLE _have_clf)
    if(_have_clf)
        check_linker_flag(C "-Wl,--gc-sections" _gc_sections)
        if(_gc_sections)
            target_link_options(syncdoom PRIVATE -Wl,--gc-sections)
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# JPEG XL -- the APC/SyncTERM graphics tier. Optional: without it the door still
# serves the sixel and text/block tiers (the code is #ifdef WITH_JXL gated).
#
# *nix: system libjxl via pkg-config (or a bare find_library fallback).
#
# Windows/MSVC: an MSVC-built static libjxl + its deps -- e.g. from vcpkg:
#     vcpkg install libjxl:x86-windows-static-md      (or :x64-windows-static-md)
# then configure with the vcpkg toolchain, or point CMAKE_PREFIX_PATH at the
# install tree:
#     -DCMAKE_PREFIX_PATH=<vcpkg>/installed/x86-windows-static-md
# libjxl ships no umbrella CMake config, so we find the pieces explicitly. A
# static libjxl needs JXL_STATIC_DEFINE (so the headers don't mark the API
# __declspec(dllimport)), and the whole transitive chain must be linked --
# highway (SIMD), brotli enc/dec/common, and lcms2 -- because the *encoder*
# pulls in color management and brotli (the bundled 3rdp/win* libjxl is no use
# here: MinGW .a, libstdc++ ABI, no headers, and decoder-only brotli).
# ---------------------------------------------------------------------------
if(NOT WITHOUT_JPEG_XL)
    if(MSVC)
        find_path(JXL_INCLUDE_DIR jxl/encode.h)
        # The static *encoder* link: jxl + highway + brotli are required; the
        # CMS libs (split out of libjxl) are added when present.
        set(_jxl_required jxl hwy brotlienc brotlidec brotlicommon)
        set(_jxl_optional jxl_cms lcms2)
        set(JPEG_XL_LIBRARIES "")
        set(_jxl_ok TRUE)
        foreach(_jxl_n ${_jxl_required} ${_jxl_optional})
            if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
                # Under the vcpkg toolchain both a release (<triplet>/lib) and a
                # debug (<triplet>/debug/lib) variant are installed. find_library
                # on a multi-config (VS) generator isn't config-aware and would
                # grab the debug lib -- a debug-CRT mismatch against a Release
                # door. Locate both explicitly and let CMake pick per-config via
                # the optimized/debug link keywords.
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
                # Classic mode (CMAKE_PREFIX_PATH at an install tree): the prefix
                # exposes the release libs, so a single find_library is right.
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
            target_compile_definitions(syncdoom PRIVATE JXL_STATIC_DEFINE)
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
        target_compile_definitions(syncdoom PRIVATE WITH_JXL)
        target_include_directories(syncdoom PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        target_link_directories(syncdoom PRIVATE ${JPEG_XL_LIBRARY_DIRS})
        target_link_libraries(syncdoom ${JPEG_XL_LIBRARIES})
        message(STATUS "syncdoom: JPEG XL graphics tier ENABLED")
    else()
        message(WARNING "syncdoom: libjxl not found -- building WITHOUT the JXL tier (sixel/text only)")
    endif()
endif()
