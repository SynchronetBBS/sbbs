project (Tone C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (CheckIncludeFiles)

set(SOURCE
	tone.c
)

add_executable(tone ${SOURCE})

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}")
	add_subdirectory(../xpdev xpdev)
ENDIF()
add_dependencies(tone xpdev)
target_include_directories(tone PRIVATE ../xpdev)
target_compile_definitions(tone PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(tone PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(tone xpdev)

install(TARGETS tone DESTINATION bin)
