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
    retro_bridge.c      # video / audio / input callbacks -> termgfx + door I/O
    syncretro_io.c      # terminal enter/probe/leave + present path
    syncretro_input.c   # socket decode -> cached RetroPad
    syncretro_door.c    # DOOR32.SYS / -s<fd> / -name / -home / -t / -core
    syncretro_config.c  # per-user sandbox + system/save dirs
    syncretro_quant.c)  # truecolor frame -> 256-color indexed, for the sixel tier

# libretro.h lives in this dir (vendored). Our own code is house-style tabs.
target_include_directories(syncretro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(syncretro PROPERTIES LINKER_LANGUAGE CXX C_STANDARD 11)

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
# termgfx). Optional: without it the door serves the sixel/text tiers. This
# mirrors ../syncmoo1/CMakeLists.txt's block (see there for the MSVC/vcpkg
# variant); kept minimal here for the *nix skeleton.
# ---------------------------------------------------------------------------
option(WITHOUT_JPEG_XL "Build without the JPEG XL graphics tier (sixel only)" OFF)
if(NOT WITHOUT_JPEG_XL AND NOT MSVC)
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
