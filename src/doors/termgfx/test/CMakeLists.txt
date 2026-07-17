# Unit tests for chunk.c -- the pure, allocation-only PCM chunk accumulator.
# Compiled as a bare SOURCE and linked against nothing, because the module has
# no dependencies at all: it is plain arithmetic over a sample buffer, no I/O,
# no external libs. The streaming state machine that shares audio_stream.h
# (and calls audio.c's APC builders, and through them libsndfile) lives in
# audio_stream.c, a separate translation unit -- so this target never takes
# the termgfx library. The accumulator under test is still the real
# implementation, not a copy.
add_executable(test_audio_stream
    ${CMAKE_CURRENT_SOURCE_DIR}/../chunk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_audio_stream.c)

target_include_directories(test_audio_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
if(NOT WIN32)
    target_link_libraries(test_audio_stream m pthread)
endif()
set_target_properties(test_audio_stream PROPERTIES C_STANDARD 11)

# This test IS its assert()s/CHECKs. A Release configure puts -DNDEBUG in
# CMAKE_C_FLAGS_<CONFIG>, compiling every assert() to nothing and making the
# target exit 0 no matter what it "checks". Undefine it for this target only;
# target_compile_options lands after the per-config flags, so the -U wins.
target_compile_options(test_audio_stream PRIVATE -UNDEBUG)
add_test(NAME audio_stream COMMAND test_audio_stream)
