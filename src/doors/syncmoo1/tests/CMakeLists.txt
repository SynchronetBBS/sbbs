# Standalone unit tests for the pure syncmoo1 keyboard-mapping module.
#
# Deliberately built from ONLY syncmoo1_map.c + test_map.c against the
# vendored 1oom kbd.h (for mookey_t/MOO_MOD_*) -- no engine/termgfx/xpdev
# sources, so this stays a fast, dependency-free unit test target and never
# drags in the full syncmoo1 executable link.

add_executable(test_map
    ${CMAKE_CURRENT_SOURCE_DIR}/../syncmoo1_map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_map.c)

target_include_directories(test_map PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../1oom/src)

set_target_properties(test_map PROPERTIES C_STANDARD 11)

add_test(NAME test_map COMMAND test_map)
