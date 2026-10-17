# Copyright (c) 2026 Stephen Hurd
# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.22)
if(DEFINED STANDARD_CASE)
	set(APPLE TRUE)
	set(CMAKE_SYSTEM_PROCESSOR x86_64)
	set(CMAKE_C_COMPILE_FEATURES c_std_11)
	set(_expected 11)
	if(STANDARD_CASE STREQUAL c17)
		list(APPEND CMAKE_C_COMPILE_FEATURES c_std_17)
		set(_expected 17)
	elseif(STANDARD_CASE STREQUAL arm)
		set(CMAKE_SYSTEM_PROCESSOR arm64)
	elseif(STANDARD_CASE STREQUAL posix)
		set(APPLE FALSE)
	elseif(STANDARD_CASE STREQUAL unsupported)
		set(CMAKE_C_COMPILE_FEATURES)
	endif()
	include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DeuceSSHCStandard.cmake")
	if(NOT DEUCESSH_C_STANDARD EQUAL _expected
	    OR NOT CMAKE_C_STANDARD EQUAL _expected
	    OR NOT CMAKE_C_STANDARD_REQUIRED OR CMAKE_C_EXTENSIONS)
		message(FATAL_ERROR "Wrong compiler dialect selection")
	endif()
else()
	foreach(_case IN ITEMS c17 c11 arm posix unsupported)
		execute_process(COMMAND "${CMAKE_COMMAND}" "-DSTANDARD_CASE=${_case}"
			-P "${CMAKE_CURRENT_LIST_FILE}" RESULT_VARIABLE _result
			OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
		if(_case MATCHES "^c(11|17)$")
			if(NOT "${_result}" STREQUAL "0")
				message(FATAL_ERROR "${_case}: ${_out}\n${_err}")
			endif()
		elseif("${_result}" STREQUAL "0" OR NOT _err MATCHES "does not support C17")
			message(FATAL_ERROR "${_case}: wrongly accepted missing C17\n${_out}\n${_err}")
		endif()
	endforeach()
	foreach(_case IN ITEMS native c11)
		set(_build "${TEST_ROOT}/${_case}")
		execute_process(COMMAND "${CMAKE_COMMAND}"
			-S "${CMAKE_CURRENT_LIST_DIR}/c_standard_fixture" -B "${_build}"
			"-DSTANDARD_CASE=${_case}"
			RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
		if(NOT "${_result}" STREQUAL "0")
			message(FATAL_ERROR "${_case}: ${_out}\n${_err}")
		endif()
		execute_process(COMMAND "${CMAKE_COMMAND}" --build "${_build}" --config Release
			RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
		if(NOT "${_result}" STREQUAL "0")
			message(FATAL_ERROR "${_case}: ${_out}\n${_err}")
		endif()
	endforeach()
endif()
