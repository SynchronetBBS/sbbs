# Standalone unit tests for the pure syncmoo1 keyboard/mouse-mapping code.
#
# Deliberately built from ONLY syncmoo1_map.c + termgfx's sgrmouse.c +
# test_map.c against the vendored 1oom kbd.h (for mookey_t/MOO_MOD_*) -- no
# engine/xpdev sources, so this stays a fast, dependency-free unit test target
# and never drags in the full syncmoo1 executable link.
#
# sgrmouse.c is compiled in as a SOURCE rather than by linking the termgfx
# library on purpose: it is pure, dependency-free C, while the library itself
# pulls in C++/libADLMIDI. Keeping it a source preserves this target's "no
# termgfx link" property while still testing the shared, real implementation
# (not a copy of it).

add_executable(test_map
    ${CMAKE_CURRENT_SOURCE_DIR}/../syncmoo1_map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx/sgrmouse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_map.c)

target_include_directories(test_map PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx
    ${CMAKE_CURRENT_SOURCE_DIR}/../1oom/src)

set_target_properties(test_map PROPERTIES C_STANDARD 11)

# This test IS its assert()s. A Release/RelWithDebInfo configure puts -DNDEBUG
# in CMAKE_C_FLAGS_<CONFIG>, which compiles every assert() to nothing and makes
# the target exit 0 no matter what it "checks" -- a green test that verifies
# literally nothing. Undefine it for this target only; target_compile_options
# lands after the per-config flags, so the -U wins.
target_compile_options(test_map PRIVATE -UNDEBUG)

add_test(NAME test_map COMMAND test_map)

# Pure unit tests for the audio module's content hash + helpers. Same rules as
# test_map: dependency-free SOURCES (never the termgfx library, which drags in
# C++/libADLMIDI), and -UNDEBUG so the assert()s actually run.
add_executable(test_audio
    ${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx/hash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../syncmoo1_audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_audio.c)

target_include_directories(test_audio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx)

set_target_properties(test_audio PROPERTIES C_STANDARD 11)
target_compile_options(test_audio PRIVATE -UNDEBUG)
add_test(NAME test_audio COMMAND test_audio)
