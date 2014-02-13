project (CIOXtrn C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../build/SynchronetMacros.cmake)

if(WIN32)
	add_executable(cioxtrn WIN32 cioxtrn.c)
	require_lib(cioxtrn xpdev)
	require_lib_dir(cioxtrn ciolob conio)

	install(TARGETS cioxtrn DESTINATION bin)
endif()
