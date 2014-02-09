project (SyncDraw C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (CheckIncludeFiles)

set(SOURCE
	homedir.c
	block.c
	effekt.c
	load.c
	options.c
	fonts.c
	syncdraw.c
	sauce.c
	config.c
	menue.c
	save.c
	crt.c
	help.c
	miscfunctions.c
	tabulator.c
	draw.c
	key.c
)

set(FU_SOURCE
	homedir.c
	fonts.c
	fontutil.c
)

set(BF_SOURCE
	homedir.c
	fonts.c
	buildfont.c
)

add_executable(syncdraw ${SOURCE})
add_executable(fontutil ${FU_SOURCE})
add_executable(buildfont ${BF_SOURCE})

if(NOT ${PARENT_DIRECTORY})
	find_package(XPDev REQUIRED)
	include(${XPDev_CONFIG})
ENDIF()
add_dependencies(syncdraw xpdev)
add_dependencies(fontutil xpdev)
add_dependencies(buildfont xpdev)
include_directories("${XPDev_DIR}/../../../include/xpdev")
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
include_directories($<TARGET_PROPERTY:xpdev,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(syncdraw xpdev)
target_link_libraries(fontutil xpdev)
target_link_libraries(buildfont xpdev)

if(NOT ${PARENT_DIRECTORY})
	find_package(CIOLib REQUIRED)
	include(${CIOLib_CONFIG})
ENDIF()
add_dependencies(syncdraw ciolib)
add_dependencies(fontutil ciolib)
add_dependencies(syncdraw ciolib)
include_directories(syncdraw PRIVATE "${CIOLib_DIR}/../../../include")
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS $<TARGET_PROPERTY:ciolib,INTERFACE_COMPILE_DEFINITIONS>)
include_directories($<TARGET_PROPERTY:ciolib,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(syncdraw ciolib)
target_link_libraries(fontutil ciolib)

install(TARGETS syncdraw fontutil buildfont DESTINATION bin)
