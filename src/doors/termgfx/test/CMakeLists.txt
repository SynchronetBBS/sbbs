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

# Unit test for font.c -- glyphs drawn into an indexed framebuffer. Compiled
# as a bare SOURCE and linked against nothing, like test_chunk: the module is
# pure pixels, with no I/O and no dependencies at all.
add_executable(test_font
    ${CMAKE_CURRENT_SOURCE_DIR}/../font.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_font.c)
target_include_directories(test_font PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(test_font PROPERTIES C_STANDARD 11)
target_compile_options(test_font PRIVATE -UNDEBUG)
add_test(NAME font COMMAND test_font)

# Unit test for focus.c -- DECSET 1004 focus events. Same shape: the module
# is a two-state machine over one CSI final and depends on nothing.
add_executable(test_focus
    ${CMAKE_CURRENT_SOURCE_DIR}/../focus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_focus.c)
target_include_directories(test_focus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(test_focus PROPERTIES C_STANDARD 11)
target_compile_options(test_focus PRIVATE -UNDEBUG)
add_test(NAME focus COMMAND test_focus)

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

# Unit test for gfxgate.c -- the shared "this terminal can't display the game"
# verdict plus the sysop-overridable notice. Bare SOURCE like the three above:
# the verdict is arithmetic and the notice resolver only reads a file.
#
# Built THREE times from one source. termgfx_gfxgate_notice() resolves once per
# process and caches (it is asked on the way out of a door, so re-reading a
# file on every call would be waste), which means a single binary can exercise
# exactly one of its three tiers. GFXGATE_CASE picks which; the stateless
# verdict half runs in all three.
foreach(case_n RANGE 2)
    add_executable(test_gfxgate${case_n}
        ${CMAKE_CURRENT_SOURCE_DIR}/../gfxgate.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_gfxgate.c)
    target_include_directories(test_gfxgate${case_n} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_definitions(test_gfxgate${case_n} PRIVATE GFXGATE_CASE=${case_n})
    set_target_properties(test_gfxgate${case_n} PROPERTIES C_STANDARD 11)
    target_compile_options(test_gfxgate${case_n} PRIVATE -UNDEBUG)
    add_test(NAME gfxgate${case_n} COMMAND test_gfxgate${case_n})
endforeach()

# The sixel encoder's palette modes. Compiles sixel.c and the control strings it
# reaches for (term.c, whose termgfx_term_sixel_at_cursor() the vscale probe asks
# for the peer's mode-80 sequence) and nothing else, so unlike its neighbours below
# it needs no door build and runs under CTest.
add_executable(test_sixel_palette
    ${CMAKE_CURRENT_SOURCE_DIR}/../sixel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../term.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sixel_palette_used.c)
target_include_directories(test_sixel_palette PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(test_sixel_palette PROPERTIES C_STANDARD 11)
target_compile_options(test_sixel_palette PRIVATE -UNDEBUG)
add_test(NAME sixel_palette COMMAND test_sixel_palette)

# FNV-1a -- the hash the doors content-address their client-cache names with.
# The code and the test both live in the project hash library (src/hash, which
# has its own `gmake fnv1a_test` rule); this target exists so the door builds
# run it too, because a constant that drifts here invalidates every door's cache
# at once, silently. Bare SOURCE like test_chunk/test_idle: the TU is pure
# arithmetic over a byte buffer, no I/O and no dependencies.
add_executable(test_fnv1a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../hash/fnv1a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../hash/fnv1a_test.c)
target_include_directories(test_fnv1a PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../hash)
set_target_properties(test_fnv1a PROPERTIES C_STANDARD 11)
target_compile_options(test_fnv1a PRIVATE -UNDEBUG)
add_test(NAME fnv1a COMMAND test_fnv1a)

# Unit test for term.c's DECSDM (mode 80) polarity table -- which of the two
# sequences asks a given peer to draw its sixels at the text cursor. cterm reversed
# the mode's sense in revision 1.328 and SyncTERM 1.8 ships 1.327, so both
# answers are live in the field at once; getting it backwards anchors every
# image (and every dirty-rect patch) at the screen origin. GitLab #1214. Links
# the full termgfx library, like test_mouse, for term.c and its include dir.
add_executable(test_term_sdm ${CMAKE_CURRENT_SOURCE_DIR}/test_term_sdm.c)
target_link_libraries(test_term_sdm termgfx)
set_target_properties(test_term_sdm PROPERTIES C_STANDARD 11)
target_compile_options(test_term_sdm PRIVATE -UNDEBUG)
add_test(NAME term_sdm COMMAND test_term_sdm)

# The rest of this directory's tests -- termgfx_termio (23 binaries), the
# quantizer, present_rgbx and the repro_fade harness -- are driven by the
# unit_termgfx_*.sh scripts here rather than by CTest: they are POSIX-only
# (socketpair(2) terminals, __wrap_fopen link tricks) and they link against a
# door's already-built stage-1 libraries. Run them after any door's build.sh:
#
#     ./unit_termgfx_termio.sh        # or TERMGFX_LIBS=<door>/build/libs ...
#     ./unit_termgfx_quant.sh
#     ./unit_termgfx_present_rgbx.sh
