project(GTKMonitor C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../../build/SynchronetMacros.cmake)
find_package(GTK2 2.6 COMPONENTS gtk)

if(GTK2_FOUND)
	add_executable(gtkmonitor
		gtkmonitor.c
		util_funcs.c
		events.c
	)
	double_require_libs(gtkmonitor sbbs xpdev smblib)
	target_include_directories(gtkmonitor PRIVATE ..)
	target_link_libraries(gtkmonitor ${GTK2_LIBRARIES})
	target_include_directories(gtkmonitor PRIVATE ${GTK2_INCLUDE_DIRS})
	target_compile_definitions(gtkmonitor PRIVATE ${GTK2_DEFINITIONS})

	# For forkpty...
	if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
			target_link_libraries(gtkmonitor util)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
			target_link_libraries(gtkmonitor util)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
			target_link_libraries(gtkmonitor util)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
			target_link_libraries(gtkmonitor util)
	endif()
	list(APPEND CMAKE_REQUIRED_LIBRARIES util)
	CHECK_FUNCTION_EXISTS(forkpty HAS_FORKPTY)
	if(HAS_FORKPTY)
	else()
		target_compile_definitions(gtkmonitor PRIVATE NEEDS_FORKPTY)
	endif()
endif(GTK2_FOUND)
