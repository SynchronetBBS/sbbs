# Umbrella build for the door's C libraries: libtermgfx (static) and xpdev
# (an OBJECT library upstream -- wrapped into a static archive so the
# top-level syncrpg/CMakeLists.txt can link it by absolute path).  Consumed
# by build.sh's Stage 1; not part of EasyRPG's own CMake build (see
# syncrpg/CMakeLists.txt for that, Stage 2). Mirrors syncscumm/door/
# CMakeLists.txt verbatim -- same two libraries, same reasons.
cmake_minimum_required(VERSION 3.18)
project(syncrpg_libs C CXX)

set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
    set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
endforeach()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx termgfx)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev xpdev EXCLUDE_FROM_ALL)
add_library(xpdev_static STATIC $<TARGET_OBJECTS:xpdev>)

# inih (2 vendored files, easyrpg/lib/inih/): liblcf's src/inireader.cpp needs
# it, resolved at its configure time through find_package(inih) / the explicit
# INIH_INCLUDE_DIR + INIH_LIBRARY cache vars Stage 2 is handed. Neither Debian
# nor vcpkg is depended on for it -- the sources are vendored, so build it
# here, where a toolchain is already configured. build.sh (*nix) hand-compiles
# the same two files with `cc` before Stage 2 and never requests this target;
# build.bat (MSVC, where there is no `cc`) asks for it by name.
add_library(inih STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../easyrpg/lib/inih/ini.c)
target_include_directories(inih PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../easyrpg/lib/inih)

# JPEG XL tier (optional): when libjxl is found, compile termgfx's jxl.c with
# the real encoder and record the link libs for build.sh to append to the
# Stage 2 link (jxl_libs.txt) -- see build.sh.
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
        # vcpkg's libjxl is split across companion static archives, and the
        # find_library() branch above (the MSVC path -- no pkg-config to read
        # libjxl.pc's Requires.private) resolves only jxl.lib itself. Nothing
        # else in the door's link brings them in -- termgfx is libjxl's only
        # consumer -- so the encoder's own dependencies would come up as
        # thousands of unresolved externals at Stage 2. Name them here; each is
        # skipped when absent, so a shared-library or *nix prefix (where the
        # closure is already resolved) records just jxl.
        set(JPEG_XL_LINK "${JPEG_XL_LIBRARIES}")
        if(MSVC)
            foreach(_jxldep jxl_cms hwy brotlienc brotlidec brotlicommon lcms2)
                find_library(JXL_DEP_${_jxldep} NAMES ${_jxldep})
                if(JXL_DEP_${_jxldep})
                    list(APPEND JPEG_XL_LINK "${JXL_DEP_${_jxldep}}")
                endif()
            endforeach()
        endif()
        # Recorded for build.sh / build.bat to append to Stage 2's link:
        file(WRITE ${CMAKE_BINARY_DIR}/jxl_libs.txt "${JPEG_XL_LINK}")
    endif()
endif()

# Stage 2 links libtermgfx.a by absolute path (build.sh passes -DTERMGFX_LIB),
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
# nothing and the elseif below still does the work.
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
file(WRITE ${CMAKE_BINARY_DIR}/sndfile_libs.txt "${_snd_libs}")
