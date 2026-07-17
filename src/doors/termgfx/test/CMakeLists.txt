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
