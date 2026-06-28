# termgfx -- Synchronet terminal-graphics door library.
#
# Reusable, engine-agnostic pieces shared by SyncDOOM (and future framebuffer
# doors like SyncDuke): render a game's framebuffer to the user's terminal, pace
# delivery over the link, and wire in the BBS (who's-online / paging). This first
# slice holds the two already-standalone modules; more of syncdoom.c's renderer,
# pacing and input move here over time (see the extraction plan).
#
#   sixel.{c,h} -- indexed DECSIXEL encoder (was render_sixel)
#   term.{c,h}  -- canonical terminal control strings (enter/probe/leave;
#                  notably the DECSDM ?80 sixel-scrolling reset)
#   sbbs_node  -- door-native node list / paging / who's-online (node.dab/.exb)

add_library(termgfx STATIC
    sixel.c
    apc.c
    jxl.c
    caps.c
    text.c
    term.c
    geometry.c
    sbbs_node.c)

# Public: consumers that #include "sixel.h" / "sbbs_node.h" get this dir.
target_include_directories(termgfx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Private: sbbs_node.c needs nodedefs.h (BBS source) + xpdev headers
# (dirwrap/filewrap). Headers only -- the xpdev *symbols* resolve when the door
# links xpdev; termgfx itself pulls in no engine/sbbs/xpdev linkage.
target_include_directories(termgfx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../sbbs3
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)

set_property(TARGET termgfx PROPERTY C_STANDARD 11)

if(MSVC)
    target_compile_definitions(termgfx PRIVATE
        _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    target_compile_definitions(termgfx PRIVATE _DEFAULT_SOURCE __unix__)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(termgfx PRIVATE LINUX)
    endif()
    target_compile_options(termgfx PRIVATE -Wall -Os -ggdb3)
endif()
