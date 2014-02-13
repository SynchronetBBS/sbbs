project(GTKUserEdit C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../../build/SynchronetMacros.cmake)
find_package(GTK2 2.6 COMPONENTS gtk glade)

if(GTK2_FOUND)
	add_executable(gtkuseredit
		gtkuseredit.c
		events.c
	)
	double_require_libs(gtkuseredit sbbs xpdev smblib)
	target_include_directories(gtkuseredit PRIVATE ..)
	target_link_libraries(gtkuseredit ${GTK2_LIBRARIES})
	target_include_directories(gtkuseredit PRIVATE ${GTK2_INCLUDE_DIRS})
	target_compile_definitions(gtkuseredit PRIVATE ${GTK2_DEFINITIONS})

	# For forkpty...
	if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
			target_link_libraries(gtkuseredit util)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
			target_link_libraries(gtkuseredit util)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
			target_link_libraries(gtkuseredit util)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
			target_link_libraries(gtkuseredit util)
	endif()
	list(APPEND CMAKE_REQUIRED_LIBRARIES util)
	CHECK_FUNCTION_EXISTS(forkpty HAS_FORKPTY)
	if(HAS_FORKPTY)
	else()
		target_compile_definitions(gtkuseredit PRIVATE NEEDS_FORKPTY)
	endif()
endif(GTK2_FOUND)
