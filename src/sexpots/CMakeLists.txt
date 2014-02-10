project (SExPOTS C)

cmake_minimum_required(VERSION 2.8.11)

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

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}")
	add_subdirectory(../xpdev xpdev)
ENDIF()
add_dependencies(sexpots xpdev)
target_include_directories(sexpots PRIVATE ../xpdev)
target_compile_definitions(sexpots PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(sexpots PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(sexpots xpdev)

target_include_directories(sexpots PRIVATE ../sbbs3)
target_include_directories(sexpots PRIVATE ../comio)

install(TARGETS sexpots DESTINATION bin)
