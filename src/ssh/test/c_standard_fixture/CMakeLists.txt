# Copyright (c) 2026 Stephen Hurd
# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.22)
project(CStandardFixture LANGUAGES C)
if(STANDARD_CASE STREQUAL c11)
	set(APPLE TRUE)
	set(CMAKE_SYSTEM_PROCESSOR x86_64)
	list(REMOVE_ITEM CMAKE_C_COMPILE_FEATURES c_std_17)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DeuceSSHCStandard.cmake")
add_executable(c_standard main.c)
target_include_directories(c_standard PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_compile_definitions(c_standard PRIVATE EXPECTED_STANDARD=${DEUCESSH_C_STANDARD})
set_target_properties(c_standard PROPERTIES C_STANDARD ${DEUCESSH_C_STANDARD}
	C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
