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

# Pure unit tests for the sixel fit/encode geometry. termgfx's geometry.c is
# compiled in as a SOURCE (like sgrmouse.c above): pure, dependency-free C,
# so this target keeps its "no termgfx link" property.
add_executable(test_geom
    ${CMAKE_CURRENT_SOURCE_DIR}/../syncmoo1_geom.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx/geometry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_geom.c)

target_include_directories(test_geom PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx)

set_target_properties(test_geom PROPERTIES C_STANDARD 11)
target_compile_options(test_geom PRIVATE -UNDEBUG)
add_test(NAME test_geom COMMAND test_geom)

# Pure unit tests for the 1oom config pruner. No engine, no termgfx: the module
# is deliberately I/O-free so this target stays dependency-free (see test_map).
add_executable(test_cfgprune
    ${CMAKE_CURRENT_SOURCE_DIR}/../syncmoo1_cfgprune.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_cfgprune.c)

target_include_directories(test_cfgprune PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(test_cfgprune PROPERTIES C_STANDARD 11)
target_compile_options(test_cfgprune PRIVATE -UNDEBUG)
add_test(NAME test_cfgprune COMMAND test_cfgprune)
