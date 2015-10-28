project (ConIOLib C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (CheckFunctionExists)
INCLUDE (FindSDL)
INCLUDE (FindX11)
INCLUDE (FindCurses)
INCLUDE (FindThreads)
find_package(XPDev QUIET CONFIG PATHS lib/cmake/XPDev)

set(SOURCE
	ansi_cio.c
	ciolib.c
	cterm.c
	vidmodes.c
	allfonts.c
	mouse.c
)

if(WIN32)
	list(APPEND SOURCE win32cio.c ciolib.rc)
endif()

if(CURSES_FOUND)
	list(APPEND SOURCE curs_cio.c)
endif()

if(X11_FOUND)
	list(APPEND SOURCE x_events.c x_cio.c)
	set(NEED_BITMAP TRUE)
	set(NEED_DLOPEN TRUE)
endif()

if(SDL_FOUND)
	list(APPEND SOURCE sdl_con.c)
	list(APPEND SOURCE sdlfuncs.c)
	if(WIN32)
		list(APPEND SOURCE SDL_win32_main.c)
	endif()
	if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		list(APPEND SOURCE SDLMain.m)
	endif()
	set(NEED_BITMAP TRUE)
	set(NEED_DLOPEN TRUE)
endif()

if(NEED_BITMAP)
	list(APPEND SOURCE bitmap_con.c)
endif()

add_library(ciolib SHARED ${SOURCE})
if(XPDev_FOUND)
	target_link_libraries(ciolib xpdev)
else()
	INCLUDE(../build/SynchronetMacros.cmake)
	require_lib(ciolib xpdev)
endif()
target_compile_definitions(ciolib PRIVATE CIOLIB_EXPORTS)
target_compile_definitions(ciolib INTERFACE CIOLIB_IMPORTS)

if(SDL_FOUND)
	target_include_directories(ciolib PRIVATE ${SDL_INCLUDE_DIR})
	target_compile_definitions(ciolib PUBLIC WITH_SDL)
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

install(TARGETS ciolib DESTINATION lib EXPORT CIOLibConfig)
install(FILES ${INSTALL_HEADERS} DESTINATION include)
install(EXPORT CIOLibConfig DESTINATION lib/cmake/CIOLib)
