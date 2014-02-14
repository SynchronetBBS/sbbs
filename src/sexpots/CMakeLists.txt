project (SExPOTS C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../build/SynchronetMacros.cmake)
INCLUDE (CheckIncludeFiles)

set(SOURCE
	../sbbs3/telnet.c
	sexpots.c
)

add_executable(sexpots ${SOURCE})
require_libs(sexpots xpdev comio)

target_include_directories(sexpots PRIVATE ../sbbs3)

install(TARGETS sexpots DESTINATION bin)
