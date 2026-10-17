# Copyright (c) 2026 Stephen Hurd
# SPDX-License-Identifier: BSD-2-Clause

set(DEUCESSH_C_STANDARD 17)
if(NOT "c_std_17" IN_LIST CMAKE_C_COMPILE_FEATURES)
	if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|i[3-6]86)$"
	    AND "c_std_11" IN_LIST CMAKE_C_COMPILE_FEATURES)
		set(DEUCESSH_C_STANDARD 11)
		message(STATUS "C17 mode unavailable on macOS x86; using C11")
	else()
		message(FATAL_ERROR "The selected C compiler does not support C17")
	endif()
endif()

# These defaults also apply to compile/link probes and test/example targets.
set(CMAKE_C_STANDARD ${DEUCESSH_C_STANDARD})
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
