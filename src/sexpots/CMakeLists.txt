project (SExPOTS C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../build/SynchronetMacros.cmake)
INCLUDE (CheckIncludeFiles)

set(SOURCE
	../comio/comio.c
	../sbbs3/telnet.c
	sexpots.c
)

if(WIN32)
	list(APPEND SOURCE ../comio/comio_win32.c)
else()
	list(APPEND SOURCE ../comio/comio_nix.c)
endif()

add_executable(sexpots ${SOURCE})
require_lib(sexports xpdev)

target_include_directories(sexpots PRIVATE ../sbbs3)
target_include_directories(sexpots PRIVATE ../comio)

install(TARGETS sexpots DESTINATION bin)
