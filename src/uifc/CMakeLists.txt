project (UIfC C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (FindThreads)

set(SOURCE
	uifc32.c
	uifcx.c
)

add_library(uifc SHARED ${SOURCE})

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}")
	add_subdirectory(../xpdev xpdev)
ENDIF()
add_dependencies(uifc xpdev)
target_include_directories(uifc PRIVATE ../xpdev)
target_compile_definitions(uifc PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(uifc PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(uifc xpdev)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}")
	add_subdirectory(../conio ciolib)
ENDIF()
add_dependencies(uifc ciolib)
target_include_directories(uifc PRIVATE ../conio)
target_compile_definitions(uifc PRIVATE $<TARGET_PROPERTY:ciolib,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(uifc PRIVATE $<TARGET_PROPERTY:ciolib,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(uifc ciolib)

target_compile_definitions(ciolib PRIVATE UIFC_EXPORTS)
target_compile_definitions(ciolib INTERFACE UIFC_IMPORTS)

set(INSTALL_HEADERS
	uifc.h
)

export(PACKAGE UIFC)

install(TARGETS uifc DESTINATION lib EXPORT UIFCConfig)
install(FILES ${INSTALL_HEADERS} DESTINATION include)
install(EXPORT UIFCConfig DESTINATION lib/cmake/UIFC)
