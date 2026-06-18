# gitinfo.cmake -- shared Synchronet CMake helper: generate a git_hash.h
# (GIT_HASH / GIT_DATE / GIT_TIME) for any project, mirroring the src/sbbs3
# targets.mk recipe. Run at build time from a custom target, e.g.:
#   cmake -DOUT=<path/git_hash.h> -DSRCDIR=<source dir> -P <...>/build/gitinfo.cmake
# Writes the header only when its content changes (so it doesn't force a needless
# recompile), and falls back to "unknown" + the build time when git or the
# repository isn't available (e.g. building from a source tarball).

set(GIT_HASH "unknown")
set(GIT_DATE "")
set(GIT_TIME "0")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SRCDIR}" log -1 HEAD --format=%h
        OUTPUT_VARIABLE _h OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _r ERROR_QUIET)
    if(_r EQUAL 0 AND _h)
        set(GIT_HASH "${_h}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${SRCDIR}" log -1 HEAD --format=%cd "--date=format-local:%b %d %Y %H:%M"
            OUTPUT_VARIABLE GIT_DATE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${SRCDIR}" log -1 HEAD --format=%cd --date=unix
            OUTPUT_VARIABLE GIT_TIME OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    endif()
endif()

if(GIT_DATE STREQUAL "")
    string(TIMESTAMP GIT_DATE "%b %d %Y %H:%M")
endif()
if(GIT_TIME STREQUAL "")
    set(GIT_TIME "0")
endif()

set(_content "#define GIT_HASH \"${GIT_HASH}\"\n#define GIT_DATE \"${GIT_DATE}\"\n#define GIT_TIME ${GIT_TIME}\n")
set(_old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" _old)
endif()
if(NOT _old STREQUAL _content)
    file(WRITE "${OUT}" "${_content}")
endif()
