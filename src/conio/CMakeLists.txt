project (ConIOLib C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (CheckFunctionExists)
INCLUDE (FindSDL)
INCLUDE (FindX11)
INCLUDE (FindCurses)
INCLUDE (FindThreads)

option(X11				"Set to OFF to disable X11" ON)
option(SDL				"Set to OFF to disable SDL" ON)

set(SOURCE
	ansi_cio.c
	ciolib.c
	cterm.c
	vidmodes.c
	allfonts.c
	mouse.c
)

if(WIN32)
	list(APPEND SOURCE SDL_win32_main.c win32cio.c ciolib_res.c)
endif()

if(CURSES_FOUND)
	list(APPEND SOURCE curs_cio.c)
endif()

if(X11_FOUND)
	if(X11)
		list(APPEND SOURCE x_events.c x_cio.c)
		set(NEED_BITMAP TRUE)
		set(NEED_DLOPEN TRUE)
	endif()
endif()

if(SDL_FOUND)
	# TODO: This should be included if XPDev is set up with SDL_Audio...
	list(APPEND SOURCE sdl_con.c)
	if(SDL)
		# TODO: This should NOT be included unless SDL is on
		list(APPEND SOURCE sdlfuncs.c)
		if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
			list(APPEND SOURCE SDLMain.m)
		endif()
		set(NEED_BITMAP TRUE)
		set(NEED_DLOPEN TRUE)
	endif()
endif()

if(NEED_BITMAP)
	list(APPEND SOURCE bitmap_con.c)
endif()

add_library(ciolib SHARED ${SOURCE})
find_package(XPDev REQUIRED)
include(${XPDev_CONFIG})
add_dependencies(ciolib xpdev)
target_include_directories(ciolib PRIVATE "${XPDev_DIR}/../../../include/xpdev")
target_compile_definitions(ciolib PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(ciolib xpdev)

if(SDL_FOUND)
	if(SDL)
		target_include_directories(ciolib PRIVATE ${SDL_INCLUDE_DIR})
	endif()
endif()

if(NEED_BITMAP)
	target_compile_definitions(ciolib PUBLIC HAS_VSTAT)
endif()

if(NOT WIN32)
	target_link_libraries(ciolib pthread)
endif()

if(CURSES_FOUND)
	if(CURSES_HAVE_NCURSES_H)
		target_compile_definitions(ciolib PRIVATE N_CURSES_LIB)
	endif()
	target_link_libraries(ciolib ${CURSES_LIBRARIES})
endif()

CHECK_FUNCTION_EXISTS(vasprintf HAVE_VASPRINTF) 
if(HAVE_VASPRINTF)
	target_compile_definitions(ciolib PRIVATE HAVE_VASPRINTF)
endif()

target_link_libraries(ciolib ${CMAKE_DL_LIBS})

set(INSTALL_HEADERS
	ciolib.h
	cterm.h
)

export(PACKAGE CIOLib)
export(TARGETS ciolib FILE CIOLibConfig.cmake)

install(TARGETS ciolib DESTINATION lib EXPORT CIOLibConfig)
install(FILES ${INSTALL_HEADERS} DESTINATION include)
install(EXPORT CIOLibConfig DESTINATION lib/cmake/CIOLib)
