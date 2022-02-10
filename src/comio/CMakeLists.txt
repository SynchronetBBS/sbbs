project (COMIO C)

cmake_minimum_required(VERSION 3.11)

set(SOURCE
	comio.c
)

set(HEADER
	comio.h
)

if(WIN32)
	list(APPEND SOURCE comio_win32.c)
else()
	list(APPEND SOURCE comio_nix.c)
endif()

add_library(comio OBJECT ${SOURCE})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_definitions(comio PRIVATE SPEED_MACROS_ONLY)
endif()
target_include_directories(comio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(comio xpdev)
