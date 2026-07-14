# gitinfo.cmake -- shared Synchronet CMake helper: generate a git_hash.h
# (GIT_HASH / GIT_DATE / GIT_TIME) for any project, mirroring the src/sbbs3
# targets.mk recipe. Run at build time from a custom target, e.g.:
#   cmake -DOUT=<path/git_hash.h> -DSRCDIR=<source dir> -P <...>/build/gitinfo.cmake
# Writes the header only when its content changes (so it doesn't force a needless
# recompile), and falls back to "unknown" + the build time when git or the
# repository isn't available (e.g. building from a source tarball).
#
# A build from a DIRTY tree is marked, because otherwise the stamp describes a
# commit the binary isn't:
#
#   clean:  GIT_HASH "a1009085"    GIT_DATE = that COMMIT's date
#   dirty:  GIT_HASH "~a1009085"   GIT_DATE = the BUILD time
#
# The leading '~' is Vanilla Conquer's convention (see syncconquer/vanilla:
# "%s%s", GitUncommittedChanges ? "~" : "", GitShortSHA1) -- it reads as "based on
# this commit, plus uncommitted changes", and it cannot be mistaken for part of the
# hash. The DATE moves with it on purpose: the commit's timestamp says nothing
# about a binary carrying newer, uncommitted code, and reporting it invites exactly
# the mistake of thinking a freshly-built door is an old one. When there is no
# commit that describes the binary, the only honest date is when it was compiled.
#
# Dirtiness is judged from TRACKED changes under src/ only (`--porcelain -uno --
# :/src`). Untracked files would make every tree dirty (a working install has
# hundreds), and tracked churn outside src/ -- game data a door rewrites at
# runtime, e.g. xtrn/lord2/*.dat -- has nothing to do with what got compiled.

set(GIT_HASH "unknown")
set(GIT_DATE "")
set(GIT_TIME "0")
set(_dirty "")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SRCDIR}" log -1 HEAD --format=%h
        OUTPUT_VARIABLE _h OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _r ERROR_QUIET)
    if(_r EQUAL 0 AND _h)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${SRCDIR}" status --porcelain -uno -- :/src
            OUTPUT_VARIABLE _st OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _sr ERROR_QUIET)
        if(_sr EQUAL 0 AND NOT _st STREQUAL "")
            set(_dirty "~")
        endif()
        set(GIT_HASH "${_dirty}${_h}")
        if(_dirty STREQUAL "")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${SRCDIR}" log -1 HEAD --format=%cd "--date=format-local:%b %d %Y %H:%M"
                OUTPUT_VARIABLE GIT_DATE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${SRCDIR}" log -1 HEAD --format=%cd --date=unix
                OUTPUT_VARIABLE GIT_TIME OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        endif()
    endif()
endif()

# No commit describes this binary (dirty tree, or no git at all) -> stamp the build.
if(GIT_DATE STREQUAL "")
    string(TIMESTAMP GIT_DATE "%b %d %Y %H:%M")
endif()
if(GIT_TIME STREQUAL "" OR GIT_TIME STREQUAL "0")
    string(TIMESTAMP GIT_TIME "%s")
endif()

set(_content "#define GIT_HASH \"${GIT_HASH}\"\n#define GIT_DATE \"${GIT_DATE}\"\n#define GIT_TIME ${GIT_TIME}\n")
set(_old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" _old)
endif()
if(NOT _old STREQUAL _content)
    file(WRITE "${OUT}" "${_content}")
endif()
