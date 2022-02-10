project (ConIOLib C)

cmake_minimum_required(VERSION 3.11)

INCLUDE (CheckFunctionExists)
find_package(X11)
# So on BSD, this causes the find to fail.  However, it's required for Linux
# This is because on BSD, ncursesw.h does not exist, but libraries use
# ncurses.h
set(CURSES_NEED_WIDE TRUE)
find_package(Threads REQUIRED)
if(NOT WIN32)
	find_package(Curses)
	find_package(PkgConfig)
	pkg_check_modules(SDL2 sdl2)
endif()

set(SOURCE
	allfonts.c
	ansi_cio.c
	ciolib.c
	ciolib.rc
	cterm.c
	mouse.c
	syncicon64.c
	utf8_codepages.c
	vidmodes.c
	xbr.c
)

set(HEADER
	ciolib.h
	cterm.h
	keys.h
	mouse.h
	utf8_codepages.h
	vidmodes.h
)

if(WIN32)
	list(APPEND SOURCE win32cio.c ciolib.rc)
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	if(CURSES_FOUND)
		list(APPEND SOURCE curs_cio.c)
		set(USE_CURSES_ANYWAY TRUE)
	else()
		if(${CMAKE_SYSTEM_NAME} MATCHES "(Free|Open|Net)BSD")
			list(APPEND SOURCE curs_cio.c)
			set(USE_CURSES_ANYWAY TRUE)
		else()
			message(FATAL_ERROR "ncursesw not found")
		endif()
	endif()
endif()

if(X11_FOUND)
	list(APPEND SOURCE x_events.c x_cio.c)
	set(NEED_BITMAP TRUE)
	set(NEED_DLOPEN TRUE)
	set(NEED_SCALE TRUE)
endif()

if(SDL2_INCLUDE_DIRS)
	list(APPEND SOURCE sdl_con.c)
	list(APPEND SOURCE sdlfuncs.c)
	set(NEED_SCALE TRUE)
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

if(NEED_SCALE)
	list(APPEND SOURCE scale.c)
endif()

add_library(ciolib OBJECT ${SOURCE})
target_include_directories(ciolib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(CURSES_FOUND)
	if(${CMAKE_SYSTEM_NAME} MATCHES "(Free|Open|Net)BSD")
		list(APPEND SOURCE curs_cio.c)
		set(USE_CURSES_ANYWAY TRUE)
	endif()
endif()

if(X11_FOUND)
	target_include_directories(ciolib PRIVATE ${X11_INCLUDE_DIR})
else()
	target_compile_definitions(ciolib PRIVATE NO_X)
endif()

if(SDL2_INCLUDE_DIRS)
	target_include_directories(ciolib PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

if(NOT WIN32)
	target_link_libraries(ciolib pthread)
endif()

if(USE_CURSES_ANYWAY)
	if(CURSES_FOUND)
		if (CURSES_HAVE_NCURSES_NCURSES_H)
			target_compile_definitions(ciolib PRIVATE DEBIAN_HATES_YOU)
		elseif(CURSES_HAVE_NCURSES_H)
			target_compile_definitions(ciolib PRIVATE N_CURSES_LIB)
		endif()
		target_link_libraries(ciolib ${NCURSES_LIBRARIES})
	else()
		target_link_libraries(ciolib ${CURSES_NCURSES_LIBRARY})
		target_link_libraries(ciolib ${CURSES_TINFO_LIBRARY_NAME})
	endif()
endif()

CHECK_FUNCTION_EXISTS(vasprintf HAVE_VASPRINTF) 
if(HAVE_VASPRINTF)
	target_compile_definitions(ciolib PRIVATE HAVE_VASPRINTF)
endif()
target_link_libraries(ciolib hash encode xpdev)
