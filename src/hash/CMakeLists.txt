project (Hash C)
find_package(XPDev QUIET CONFIG PATHS lib/cmake/XPDev)

cmake_minimum_required(VERSION 3.11)

set(SOURCE
	crc16.c
	crc32.c
	md5.c
	sha1.c
)

set(HEADER
	crc16.h
	crc32.h
	md5.h
	sha1.h
)

add_library(hash OBJECT ${SOURCE})
target_include_directories(hash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hash xpdev)
