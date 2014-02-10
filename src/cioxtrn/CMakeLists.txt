project (CIOXtrn C)

cmake_minimum_required(VERSION 2.8.11)

set(SOURCE
	cioxtrn.c
)

add_executable(cioxtrn ${SOURCE})
set_property(TARGET cioxtrn PROPERTY WIN32_EXECUTABLE TRUE)

if(NOT ${PARENT_DIRECTORY})
	find_package(XPDev REQUIRED)
	include(${XPDev_CONFIG})
ENDIF()
add_dependencies(cioxtrn xpdev)
include_directories("${XPDev_DIR}/../../../include/xpdev")
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
include_directories($<TARGET_PROPERTY:xpdev,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(cioxtrn xpdev)

if(NOT ${PARENT_DIRECTORY})
	find_package(CIOLib REQUIRED)
	include(${CIOLib_CONFIG})
ENDIF()
add_dependencies(cioxtrn ciolib)
target_include_directories(cioxtrn PRIVATE "${CIOLib_DIR}/../../../include")
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS $<TARGET_PROPERTY:ciolib,INTERFACE_COMPILE_DEFINITIONS>)
include_directories($<TARGET_PROPERTY:ciolib,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(cioxtrn ciolib)

install(TARGETS cioxtrn DESTINATION bin)
