# Copyright (c) 2026 Stephen Hurd
# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.22)
set(previous "")
set(compiler_options "-DCMAKE_C_COMPILER=${TEST_C_COMPILER}")
if(TEST_BACKEND STREQUAL "Botan")
	list(APPEND compiler_options "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}")
endif()
# Reassert requested settings after CMake has processed a possible compiler
# change in this reused fixture directory. That change can reset cache inputs
# at the end of its first configure; it must not become a comparison baseline.
execute_process(COMMAND "${CMAKE_COMMAND}" --no-warn-unused-cli
	-S "${CMAKE_CURRENT_LIST_DIR}/c_standard_fixture" -B "${TEST_ROOT}"
	${compiler_options} -DOPTION_CASE=
	RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${result}" STREQUAL "0")
	message(FATAL_ERROR "Compiler setup: ${out}\n${err}")
endif()
foreach(case IN ITEMS clean cached normal clean lowercase)
	set(parent_case "${case}")
	set(build_type Debug)
	if(case STREQUAL lowercase)
		set(parent_case clean)
		set(build_type debug)
	endif()
	execute_process(COMMAND "${CMAKE_COMMAND}"
		-S "${CMAKE_CURRENT_LIST_DIR}/c_standard_fixture" -B "${TEST_ROOT}"
		${compiler_options}
		"-DDEUCESSH_CRYPTO_BACKEND=${TEST_BACKEND}"
		"-DCMAKE_BUILD_TYPE=${build_type}" "-DOPTION_CASE=${parent_case}"
		RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
	if(NOT "${result}" STREQUAL "0")
		message(FATAL_ERROR "${case}: ${out}\n${err}")
	endif()
	file(SHA256 "${TEST_ROOT}/options.txt" current)
	if(previous AND NOT current STREQUAL previous)
		message(FATAL_ERROR "Parent check inputs changed selected build options")
	endif()
	set(previous "${current}")
	execute_process(COMMAND "${CMAKE_COMMAND}" --build "${TEST_ROOT}" --config Debug
		RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
	if(NOT "${result}" STREQUAL "0")
		message(FATAL_ERROR "${case}: ${out}\n${err}")
	endif()
endforeach()
message(STATUS "build_options states=5 cold=1 context=1 preserved=1 invalid_compile=0 invalid_link=0")
