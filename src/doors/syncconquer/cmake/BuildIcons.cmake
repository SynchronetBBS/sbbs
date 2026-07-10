# SyncConquer's replacement for vanilla/cmake/BuildIcons.cmake.
#
# This file shadows the vendored module: our project root is prepended to
# CMAKE_MODULE_PATH (see ../CMakeLists.txt), so vanilla/redalert/CMakeLists.txt's
# `include(BuildIcons)` resolves to *this* file and its `make_icon()` call below
# becomes a no-op. Nothing in vanilla/ is patched -- keeping PROVENANCE.md's
# local-patch list short is worth more than reusing the upstream module.
#
# Two reasons the door doesn't want the vendored one:
#
#  1. It hard-fails on Windows. make_icon() does
#         if(NOT EXISTS "${ARG_INPUT}") message(FATAL_ERROR ...)
#     against "${CMAKE_SOURCE_DIR}/resources/vanillara_icon.svg", and our
#     resources/ is a *git symlink* to vanilla/resources. Git on Windows checks
#     that out as a plain 17-byte text file (the link target as text) unless
#     core.symlinks is on and the user has Developer Mode / SeCreateSymbolicLink,
#     so the .svg is unreachable and configure dies before a single TU compiles.
#
#  2. Even where the symlink resolves, generating the icon needs ImageMagick,
#     and the product is a console BBS door launched by Synchronet over
#     DOOR32.SYS -- it has no window, so a .ico/.icns is dead weight. Upstream
#     already treats the icon as optional: ProductVersion.cmake documents ICON
#     as "no icon will be included if not provided", and vendored BuildIcons
#     itself only warns (not errors) when ImageMagick is missing. Passing an
#     empty VANILLARA_ICON is therefore a configuration upstream supports.
#
# Signature matches the vendored make_icon(INPUT <svg> OUTPUT <var>): we accept
# and ignore INPUT, and clear OUTPUT in the caller's scope so `${VANILLARA_ICON}`
# expands to nothing in redalert/CMakeLists.txt's add_executable() and
# generate_product_version(ICON ...).
function(make_icon)
    cmake_parse_arguments(ARG "" "INPUT;OUTPUT" "" ${ARGN})
    if(ARG_OUTPUT)
        set(${ARG_OUTPUT} "" PARENT_SCOPE)
    endif()
endfunction()
