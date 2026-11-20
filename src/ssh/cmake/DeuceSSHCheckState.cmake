# Copyright (c) 2026 Stephen Hurd
# SPDX-License-Identifier: BSD-2-Clause
include_guard(GLOBAL)
include(CMakePushCheckState)

# Use only inside a function, so the try-compile type is local to that probe.
# RESET unsets variables, which can expose cached parent inputs again. Empty
# bindings exclude those inputs without changing the parent's check state.
macro(dssh_push_platform_check_state)
	cmake_push_check_state(RESET)
	foreach(dssh_check_input IN ITEMS CMAKE_EXTRA_INCLUDE_FILES
		CMAKE_REQUIRED_FLAGS CMAKE_REQUIRED_DEFINITIONS CMAKE_REQUIRED_INCLUDES
		CMAKE_REQUIRED_LINK_OPTIONS CMAKE_REQUIRED_LINK_DIRECTORIES
		CMAKE_REQUIRED_LIBRARIES CMAKE_REQUIRED_QUIET)
		set(${dssh_check_input} "")
	endforeach()
	set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
endmacro()
