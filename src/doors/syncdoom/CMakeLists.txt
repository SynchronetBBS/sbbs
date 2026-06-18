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
    cli_data.c render_text.c render_sixel.c
    deh_str.c deh_io.c deh_main.c deh_mapping.c deh_text.c deh_ammo.c
    deh_bexstr.c deh_cheat.c deh_doom.c deh_frame.c deh_misc.c deh_ptr.c
    deh_sound.c deh_thing.c deh_weapon.c w_merge.c
    net_client.c net_common.c net_dedicated.c net_io.c net_loop.c net_packet.c
    net_query.c net_server.c net_structrw.c aes_prng.c net_gui.c net_udp.c
    mp_server.c
    syncdoom.c)

add_executable(syncdoom ${SOURCES})
target_include_directories(syncdoom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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
# Platform flags (the *nix set: the doomgeneric platform defines + -Wall -Os).
# WINDOWS/MSVC: the doomgeneric platform layer needs its own config (NOT
# NORMALUNIX/LINUX/__unix__) and Winsock; left as a follow-up -- this pass
# targets the Linux/GCC build.
# ---------------------------------------------------------------------------
if(NOT WIN32)
    target_compile_definitions(syncdoom PRIVATE NORMALUNIX SNDSERV _DEFAULT_SOURCE __unix__)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(syncdoom PRIVATE LINUX)
    endif()
    target_link_libraries(syncdoom m pthread ${CMAKE_DL_LIBS})
endif()

if(NOT MSVC)
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
# Linux: system libjxl via pkg-config (or find_library). For a future Windows
# build, point at an MSVC-built libjxl (vcpkg, or an MSVC 3rdp/<plat>/libjxl --
# the existing 3rdp/win*.release/libjxl are mingw-built and won't link with MSVC).
# ---------------------------------------------------------------------------
if(NOT WITHOUT_JPEG_XL)
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
