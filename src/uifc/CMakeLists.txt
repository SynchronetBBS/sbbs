project (UIFC C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (FindThreads)

set(SOURCE
	uifc32.c
	uifcx.c
)

add_library(uifc SHARED ${SOURCE})

find_package(XPDev REQUIRED)
include(${XPDev_CONFIG})
add_dependencies(uifc xpdev)
target_include_directories(uifc PRIVATE "${XPDev_DIR}/../../../include/xpdev")
target_compile_definitions(uifc PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(uifc xpdev)

find_package(CIOLib REQUIRED)
include(${CIOLib_CONFIG})
add_dependencies(uifc ciolib)
target_include_directories(uifc PRIVATE "${CIOLib_DIR}/../../../include")
target_compile_definitions(uifc PRIVATE $<TARGET_PROPERTY:ciolib,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(uifc ciolib)

set(INSTALL_HEADERS
	uifc.h
)

export(PACKAGE UIFC)
export(TARGETS uifc FILE UIFCConfig.cmake)

install(TARGETS uifc DESTINATION lib EXPORT UIFCConfig)
install(FILES ${INSTALL_HEADERS} DESTINATION include)
install(EXPORT UIFCConfig DESTINATION lib/cmake/UIFC)
