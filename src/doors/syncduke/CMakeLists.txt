cmake_minimum_required(VERSION 3.13)
project(syncduke C)

# syncduke -- Duke Nukem 3D for Synchronet / SyncTERM (DOOR32.SYS BBS door).
#
# Vendored Chocolate Duke3D (Build engine + Duke game) with its SDL platform
# layer (Engine/src/display.c) replaced by our headless shim (syncduke_plat.c):
# the 8-bit framebuffer is captured at the page-flip and rendered to the user's
# terminal via libtermgfx's sixel encoder. Parallels src/doors/syncdoom/.
#
# USE_SDL is intentionally NOT defined (drops the SDL.h/SDL_mixer.h includes that
# duke3d.h/dukewin.h gate). Audio + multiplayer use the engine's shipped stubs.

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Engine/src)
set(GAME_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/Game/src)

set(SOURCES
    # Build engine: the libEngine.a set MINUS display.c (-> syncduke_plat.c) and
    # MINUS mmulti.c/network.c (-> dummy_multi.c stub; ENet dropped).
    ${ENGINE_DIR}/cache.c ${ENGINE_DIR}/draw.c ${ENGINE_DIR}/dummy_multi.c
    ${ENGINE_DIR}/engine.c ${ENGINE_DIR}/filesystem.c ${ENGINE_DIR}/fixedPoint_math.c
    ${ENGINE_DIR}/tiles.c
    # Duke game: the libGame.a set (already uses dummy_audiolib.c) + premap.c.
    ${GAME_DIR}/actors.c ${GAME_DIR}/animlib.c ${GAME_DIR}/config.c ${GAME_DIR}/console.c
    ${GAME_DIR}/control.c ${GAME_DIR}/cvar_defs.c ${GAME_DIR}/cvars.c
    ${GAME_DIR}/dummy_audiolib.c ${GAME_DIR}/game.c ${GAME_DIR}/gamedef.c
    ${GAME_DIR}/global.c ${GAME_DIR}/keyboard.c ${GAME_DIR}/menues.c ${GAME_DIR}/player.c
    ${GAME_DIR}/premap.c ${GAME_DIR}/rts.c ${GAME_DIR}/scriplib.c ${GAME_DIR}/sector.c
    ${GAME_DIR}/sounds.c
    # Our headless platform shim (replaces display.c) + terminal I/O / sixel
    # present path + terminal-input keymap + no-op audio/MP stubs.
    syncduke_plat.c
    syncduke_io.c
    syncduke_input.c
    syncduke_door.c
    syncduke_config.c
    syncduke_game.c
    syncduke_stubs.c
)

add_executable(syncduke ${SOURCES})
target_include_directories(syncduke PRIVATE ${ENGINE_DIR} ${GAME_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat)   # no-op SDL.h shim for the 2 ungated SDL refs
target_compile_definitions(syncduke PRIVATE PLATFORM_UNIX)
set_property(TARGET syncduke PROPERTY C_STANDARD 11)

# The 2013/2019 Build-engine sources predate modern GCC: int<->pointer conversions
# are now hard errors -- downgrade them to (silenced) warnings; we don't edit the
# vendored engine. -w quiets the rest of the vendored noise.
target_compile_options(syncduke PRIVATE
    -w
    -fcommon   # the code defines tentative globals in headers (_argc, cvars, ...);
               # GCC 10+ defaults to -fno-common -> multiple-definition. Restore merge.
    -Wno-error=int-conversion -Wno-error=implicit-int
    -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration)

# premap.c MUST be -O0 (upstream note: a new game crashes if it's optimized).
set_source_files_properties(${GAME_DIR}/premap.c PROPERTIES COMPILE_OPTIONS -O0)

# libtermgfx -- the shared sixel/JXL encoders + APC transport + cap-probe. Its
# PUBLIC include dir gives us "sixel.h"/"jxl.h"/"apc.h"/"caps.h".
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)
target_link_libraries(syncduke termgfx)

# ---------------------------------------------------------------------------
# JPEG XL -- the APC/SyncTERM graphics tier (shared with SyncDOOM via termgfx).
# Optional: without it the door still serves the sixel tier (the JXL present path
# is #ifdef WITH_JXL gated, and termgfx/jxl.c compiles a return-0 stub). *nix:
# system libjxl via pkg-config (or a bare find_library fallback). Windows/MSVC: an
# MSVC-built static libjxl + deps (vcpkg) -- see src/doors/syncdoom/CMakeLists.txt
# for the full rationale; this mirrors that block for the syncduke target.
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
            target_compile_definitions(syncduke PRIVATE JXL_STATIC_DEFINE)
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
        target_compile_definitions(syncduke PRIVATE WITH_JXL)
        target_include_directories(syncduke PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        # termgfx/jxl.c needs the JXL headers + WITH_JXL to compile the encoder; the
        # libjxl symbols resolve at syncduke's final link (JPEG_XL_LIBRARIES below).
        target_compile_definitions(termgfx PRIVATE WITH_JXL)
        target_include_directories(termgfx PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        target_link_directories(syncduke PRIVATE ${JPEG_XL_LIBRARY_DIRS})
        target_link_libraries(syncduke ${JPEG_XL_LIBRARIES})
        message(STATUS "syncduke: JPEG XL graphics tier ENABLED")
    else()
        message(WARNING "syncduke: libjxl not found -- building WITHOUT the JXL tier (sixel only)")
    endif()
endif()

# xpdev: ini_file (syncduke.ini parsing, like syncdoom) -- and the sockets
# (sockwrap) the Windows door I/O port will use. Build the xpdev CMake target so
# this works on every toolchain; pin off its audio + crypto backends so the
# sub-build stays lean (cf. syncdoom/CMakeLists.txt). The guard avoids re-adding
# xpdev when syncduke is built inside a larger tree that already has it.
if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
    set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
    foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
        set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev xpdev EXCLUDE_FROM_ALL)
endif()
target_include_directories(syncduke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(syncduke xpdev)

# The vendored engine ships its own *nix DOS/Watcom-compat helpers (strupr/strlwr/
# itoa/ltoa/ultoa/filelength) which xpdev (genwrap/strwrap/filewrap) also defines
# -- ABI-identical dupes. Engine objects link first so its versions win; allow the
# overlap rather than editing the vendored engine. (Unlike doomgeneric, which has
# none, so syncdoom links xpdev with no overlap.)
if(NOT MSVC)
    target_link_options(syncduke PRIVATE -Wl,--allow-multiple-definition)
endif()

if(NOT WIN32)
    target_link_libraries(syncduke m pthread ${CMAKE_DL_LIBS})
endif()
