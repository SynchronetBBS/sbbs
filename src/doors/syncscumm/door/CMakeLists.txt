# Umbrella build for the door's C libraries: libtermgfx (static) and xpdev
# (an OBJECT library upstream -- wrapped into a static archive so ScummVM's
# make can link it).  Consumed by build.sh; not part of ScummVM's build.
# termgfx's apc.c needs xpdev's safe_snprintf even on the video-only path.
cmake_minimum_required(VERSION 3.11)
project(syncscumm_libs C CXX)

set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
    set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
endforeach()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx termgfx)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev xpdev EXCLUDE_FROM_ALL)
add_library(xpdev_static STATIC $<TARGET_OBJECTS:xpdev>)

# JPEG XL tier (optional): when libjxl is found, compile termgfx's jxl.c with
# the real encoder and record the link libs for build.sh to append to
# ScummVM's own link (jxl_libs.txt) -- see build.sh's DEFINES/LIBS appends.
option(WITHOUT_JPEG_XL "Build without the JPEG XL graphics tier (sixel only)" OFF)
if(NOT WITHOUT_JPEG_XL)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(JPEG_XL libjxl)
    endif()
    if(NOT JPEG_XL_FOUND)
        find_library(JPEG_XL_LIBRARIES NAMES jxl)
        # find_library sets no include path; on the MSVC/vcpkg build (no
        # pkg-config) jxl.c's <jxl/encode.h> would otherwise not be found, so
        # locate the include dir explicitly via CMAKE_PREFIX_PATH. Harmless on
        # the *nix pkg-config path above, which already sets JPEG_XL_INCLUDE_DIRS.
        find_path(JPEG_XL_INCLUDE_DIRS NAMES jxl/encode.h)
        if(JPEG_XL_LIBRARIES)
            set(JPEG_XL_FOUND TRUE)
        endif()
    endif()
    if(JPEG_XL_FOUND)
        target_compile_definitions(termgfx PRIVATE WITH_JXL)
        target_include_directories(termgfx PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        # Recorded for build.sh to append to ScummVM's LIBS:
        file(WRITE ${CMAKE_BINARY_DIR}/jxl_libs.txt "${JPEG_XL_LIBRARIES}")
    endif()
endif()

# ScummVM's make links libtermgfx.a by hand (build.sh appends to config.mk),
# so every external the archive needs must be transcribed across that seam --
# CMake's PUBLIC link interface does not survive it. termgfx's Ogg/Opus encode
# (audio.c -> libsndfile) is pulled in the moment termgfx_termio.c references
# any termgfx_audio_* symbol, so hand build.sh the flags the same way the JXL
# handoff above does.
get_target_property(_tg_libs termgfx INTERFACE_LINK_LIBRARIES)
set(_snd_libs "")
# IMPORTED targets are directory-scoped: the SndFile::sndfile that
# termgfx/CMakeLists.txt's find_package(SndFile CONFIG) created is not visible
# HERE, in the parent scope, so ask for it again (a no-op second lookup -- the
# result is cached in SndFile_DIR). On the *nix/pkg-config path this finds
# nothing and the elseif below still does the work. Without this, a distro that
# ships SndFileConfig.cmake (Arch, vcpkg -- preferred over pkg-config) leaves an
# empty sndfile_libs.txt, no -lsndfile on ScummVM's link, and undefined
# references to sf_open_virtual / sf_command / sf_writef_short / sf_close from
# termgfx/audio.c.
find_package(SndFile CONFIG QUIET)
if(TARGET SndFile::sndfile)
    set(_snd_out "")
    get_target_property(_snd_loc SndFile::sndfile IMPORTED_LOCATION)
    if(NOT _snd_loc)
        # vcpkg/MSVC: the imported target is per-configuration, so the plain
        # IMPORTED_LOCATION property is unset -- read the Release one.
        get_target_property(_snd_loc SndFile::sndfile IMPORTED_LOCATION_RELEASE)
    endif()
    if(_snd_loc)
        set(_snd_out "${_snd_loc}")
    elseif(NOT MSVC)
        set(_snd_out "-lsndfile")
    endif()
    set(_snd_libs "${_snd_out}")
elseif(_tg_libs MATCHES "(^|;)sndfile(;|$)")
    # pkg-config path (termgfx/CMakeLists.txt's fallback branch, taken on this
    # host): termgfx links the bare library name "sndfile" straight from
    # SNDFILE_LIBRARIES, so no SndFile::sndfile target ever gets created --
    # the check above alone would silently miss it.
    set(_snd_libs "-lsndfile")
endif()
# Safety net -- the invariant is "termgfx compiled with TERMGFX_WITH_SNDFILE
# ==> libtermgfx.a references sf_*() ==> the final link needs libsndfile." If
# the detection above still produced nothing (some unforeseen way for the link
# interface to present libsndfile), key off the compile definition (readable on
# the regular termgfx target from any scope) and fall back to -lsndfile so the
# flag is present whenever the symbols are. Skipped on MSVC, where vcpkg
# auto-links libsndfile and this handoff file is unused.
if(NOT _snd_libs AND NOT MSVC)
    get_target_property(_tg_defs termgfx COMPILE_DEFINITIONS)
    if(_tg_defs MATCHES "TERMGFX_WITH_SNDFILE")
        set(_snd_libs "-lsndfile")
    endif()
endif()
file(WRITE ${CMAKE_BINARY_DIR}/sndfile_libs.txt "${_snd_libs}")
