project (COMIO C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../build/SynchronetMacros.cmake)

set(SOURCE
	comio.c
)
if(WIN32)
	list(APPEND SOURCE comio_win32.c)
else()
	list(APPEND SOURCE comio_nix.c)
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_compile_definitions(comio PRIVATE SPEED_MACROS_ONLY)
	endif()
endif()
add_library(comio SHARED ${SOURCE})
set_target_properties(comio PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

require_lib(comio xpdev)
target_compile_definitions(comio PRIVATE COMIO_EXPORTS)
target_compile_definitions(comio INTERFACE COMIO_IMPORTS)

set(INSTALL_HEADERS
	comio.h
)

export(PACKAGE COMIO)

install(TARGETS comio DESTINATION lib EXPORT COMIOConfig)
install(FILES ${INSTALL_HEADERS} DESTINATION include)
install(EXPORT COMIOConfig DESTINATION lib/cmake/COMIO)
