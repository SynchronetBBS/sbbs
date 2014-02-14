project (UIfC C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../build/SynchronetMacros.cmake)
INCLUDE (FindThreads)

set(SOURCE
	uifc32.c
	uifcx.c
)
add_library(uifc SHARED ${SOURCE})
require_libs(uifc xpdev ciolib)

target_compile_definitions(uifc PRIVATE UIFC_EXPORTS)
target_compile_definitions(uifc INTERFACE UIFC_IMPORTS)

export(PACKAGE UIFC)

install(TARGETS uifc DESTINATION lib EXPORT UIFCConfig)
install(FILES uifc.h DESTINATION include)
install(EXPORT UIFCConfig DESTINATION lib/cmake/UIFC)
