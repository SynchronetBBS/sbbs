project (CIOXtrn C)

cmake_minimum_required(VERSION 2.8.11)

set(SOURCE
	cioxtrn.c
)

add_executable(cioxtrn ${SOURCE})
set_property(TARGET cioxtrn PROPERTY WIN32_EXECUTABLE TRUE)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}")
	add_subdirectory(../xpdev xpdev)
ENDIF()
add_dependencies(cioxtrn xpdev)
target_include_directories(cioxtrn PRIVATE ../xpdev)
target_compile_definitions(cioxtrn PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(cioxtrn PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(cioxtrn xpdev)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}")
	add_subdirectory(../conio ciolib)
ENDIF()
add_dependencies(cioxtrn ciolib)
target_include_directories(cioxtrn PRIVATE ../conio)
target_compile_definitions(cioxtrn PRIVATE $<TARGET_PROPERTY:ciolib,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(cioxtrn PRIVATE $<TARGET_PROPERTY:ciolib,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(cioxtrn ciolib)

install(TARGETS cioxtrn DESTINATION bin)
