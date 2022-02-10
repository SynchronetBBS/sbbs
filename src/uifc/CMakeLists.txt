project (UIfC C)

cmake_minimum_required(VERSION 3.11)

INCLUDE (FindThreads)

set(SOURCE
	filepick.c
	uifc32.c
	uifcx.c
)

set(HEADER
	filepick.h
	uifc.h
)

add_library(uifc OBJECT ${SOURCE})
target_include_directories(uifc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(uifc ciolib xpdev)
