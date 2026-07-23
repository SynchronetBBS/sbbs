# Unit tests for chunk.c -- the pure, allocation-only PCM chunk accumulator.
# Compiled as a bare SOURCE and linked against nothing, because the module has
# no dependencies at all: it is plain arithmetic over a sample buffer, no I/O,
# no external libs. The streaming state machine that shares audio_stream.h
# (and calls audio.c's APC builders, and through them libsndfile) lives in
# audio_stream.c, a separate translation unit -- so this target never takes
# the termgfx library. The accumulator under test is still the real
# implementation, not a copy.
#
# Named for what it covers: the target was `test_audio_stream`, which read as
# a test of the state machine and implied coverage this does not have. The
# state machine's guard is syncretro's test_audio_bytes golden, which needs
# the real encoder and libsndfile -- exactly the dependency this target exists
# to avoid -- so the two stay split, and the names now say which is which.
add_executable(test_chunk
    ${CMAKE_CURRENT_SOURCE_DIR}/../chunk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_audio_stream.c)

target_include_directories(test_chunk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
if(NOT WIN32)
    target_link_libraries(test_chunk m pthread)
endif()
set_target_properties(test_chunk PROPERTIES C_STANDARD 11)

# This test IS its assert()s/CHECKs. A Release configure puts -DNDEBUG in
# CMAKE_C_FLAGS_<CONFIG>, compiling every assert() to nothing and making the
# target exit 0 no matter what it "checks". Undefine it for this target only;
# target_compile_options lands after the per-config flags, so the -U wins.
target_compile_options(test_chunk PRIVATE -UNDEBUG)
add_test(NAME chunk COMMAND test_chunk)

# Unit test for mouse.c -- the SGR mouse handshake module. Links the full
# termgfx library (rather than compiling mouse.c standalone like test_chunk
# does for chunk.c) because mouse.c calls termgfx_sgr_classify() from
# sgrmouse.c, and linking termgfx also brings in its PUBLIC include dir so
# test_mouse.c's #include "mouse.h" resolves without an extra -I.
add_executable(test_mouse
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mouse.c)
target_link_libraries(test_mouse termgfx)
set_target_properties(test_mouse PROPERTIES C_STANDARD 11)
target_compile_options(test_mouse PRIVATE -UNDEBUG)
add_test(NAME mouse COMMAND test_mouse)

# Unit test for idle.c -- the pure idle-user clock. Compiled as a bare SOURCE
# and linked against nothing, like test_chunk: the module is arithmetic over
# timestamps, with no I/O and no dependencies at all.
add_executable(test_idle
    ${CMAKE_CURRENT_SOURCE_DIR}/../idle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_idle.c)
target_include_directories(test_idle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(test_idle PROPERTIES C_STANDARD 11)
target_compile_options(test_idle PRIVATE -UNDEBUG)
add_test(NAME idle COMMAND test_idle)

# Unit test for stats.c -- the shared Ctrl-S strip's window arithmetic and
# field formatting. Bare SOURCE like test_chunk/test_idle: the module is
# arithmetic plus snprintf, with no I/O and no dependencies. Worth its own
# target because the three doors' strips now agree only as far as this file
# does -- the KB/s-to-MB/s threshold in particular is what drifted before.
add_executable(test_stats
    ${CMAKE_CURRENT_SOURCE_DIR}/../stats.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_stats.c)
target_include_directories(test_stats PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(test_stats PROPERTIES C_STANDARD 11)
target_compile_options(test_stats PRIVATE -UNDEBUG)
add_test(NAME stats COMMAND test_stats)

# The rest of this directory's tests -- termgfx_termio (23 binaries), the
# quantizer, present_rgbx, the sixel palette test and the repro_fade harness --
# are driven by the unit_termgfx_*.sh scripts here rather than by CTest: they
# are POSIX-only (socketpair(2) terminals, __wrap_fopen link tricks) and they
# link against a door's already-built stage-1 libraries. Run them after any
# door's build.sh:
#
#     ./unit_termgfx_termio.sh        # or TERMGFX_LIBS=<door>/build/libs ...
#     ./unit_termgfx_quant.sh
#     ./unit_termgfx_present_rgbx.sh
