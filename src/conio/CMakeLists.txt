cmake_minimum_required(VERSION 3.11)

project (ConIOLib C)

set(WITHOUT_GDI OFF CACHE BOOL "Disable GDI video support")
set(WITHOUT_SDL OFF CACHE BOOL "Disable SDL video support")
set(WITHOUT_X11 OFF CACHE BOOL "Disable X11 video support")
set(WITHOUT_XRANDR OFF CACHE BOOL "Disable X11 video support")
set(WITHOUT_XRENDER OFF CACHE BOOL "Disable X11 video support")
set(WITHOUT_XINERAMA OFF CACHE BOOL "Disable X11 video support")

INCLUDE (CheckSymbolExists)
find_package(Threads REQUIRED)
if(NOT WITHOUT_X11)
	find_package(X11)
endif()

# So on BSD, this causes the find to fail.  However, it's required for Linux
# This is because on BSD, ncursesw.h does not exist, but libraries use
# ncurses.h
set(CURSES_NEED_WIDE TRUE)
find_package(Curses)

find_package(PkgConfig)
if(NOT WITHOUT_SDL)
	if(WIN32)
		set(SDL2_FOUND ON)
		set(SDL2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/win32.release/sdl2/include)
	else()
		find_package(SDL2 CONFIG PATHS /Library/Frameworks)
	endif()
endif()
if(NOT WITHOUT_X11)
	if(NOT WITHOUT_XRENDER)
		pkg_check_modules(XRENDER xrender)
	endif()
	if(NOT WITHOUT_XINERAMA)
		pkg_check_modules(XINERAMA xinerama)
	endif()
	if(NOT WITHOUT_XRANDR)
		pkg_check_modules(XRANDR xrandr)
	endif()
endif()

# TODO: Don't require allfonts.c when building without bitmap support
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
	if(NOT WITHOUT_GDI)
		list(APPEND SOURCE win32gdi.c)
		if(WITHOUT_SDL)
			list(APPEND SOURCE SDL_win32_main.c)
		endif()
		set(NEED_BITMAP TRUE)
		set(NEED_SCALE TRUE)
	endif()
	list(APPEND SOURCE win32cio.c)
	list(APPEND SOURCE ciolib.rc)
endif()

if(CURSES_FOUND)
	list(APPEND SOURCE curs_cio.c)
	set(USE_CURSES_ANYWAY TRUE)
else()
	if(${CMAKE_SYSTEM_NAME} MATCHES "(Free|Open|Net)BSD")
		list(APPEND SOURCE curs_cio.c)
		set(USE_CURSES_ANYWAY TRUE)
	else()
		if(NOT WIN32)
			message(FATAL_ERROR "No wide-capable curses found")
		endif()
	endif()
endif()

if(NOT WITHOUT_X11)
	if(X11_FOUND)
		list(APPEND SOURCE x_events.c x_cio.c)
		set(NEED_BITMAP TRUE)
		set(NEED_SCALE TRUE)
	endif()
endif()

if(NOT WITHOUT_SDL)
	if(SDL2_FOUND)
		list(APPEND SOURCE sdl_con.c)
		list(APPEND SOURCE sdlfuncs.c)
		if(WIN32)
			list(APPEND SOURCE SDL_win32_main.c)
		endif()
		set(NEED_BITMAP TRUE)
		set(NEED_SCALE TRUE)
	endif()
endif()

if(NEED_BITMAP)
	list(APPEND SOURCE bitmap_con.c)
endif()

if(NEED_SCALE)
	list(APPEND SOURCE scale.c xbr.c)
endif()

check_symbol_exists(vasprintf stdio.h HAVE_VASPRINTF) 

add_library(ciolib OBJECT ${SOURCE})
target_include_directories(ciolib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WITHOUT_X11)
	target_compile_definitions(ciolib PRIVATE NO_X)
	target_compile_definitions(ciolib PUBLIC DISABLE_X11=1)
else()
	if(X11_FOUND)
		target_include_directories(ciolib PRIVATE ${X11_INCLUDE_DIR})
		if(NOT WITHOUT_XRENDER)
			if(XRENDER_FOUND)
				target_compile_definitions(ciolib PUBLIC WITH_XRENDER)
			endif()
		endif()
		if(NOT WITHOUT_XINERAMA)
			if(XINERAMA_FOUND)
				target_compile_definitions(ciolib PUBLIC WITH_XINERAMA)
			endif()
		endif()
		if(NOT WITHOUT_XRANDR)
			if(XRANDR_FOUND)
				target_compile_definitions(ciolib PUBLIC WITH_XRANDR)
			endif()
		endif()
	else()
		target_compile_definitions(ciolib PRIVATE NO_X)
		target_compile_definitions(ciolib PUBLIC DISABLE_X11=1)
		set(WITHOUT_X11 ON CACHE BOOL "Disable X11 video support" FORCE)
	endif()
endif()

if(NOT WITHOUT_SDL)
	if(SDL2_FOUND)
		if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
			target_compile_definitions(ciolib PUBLIC STATIC_SDL)
			target_link_libraries(ciolib SDL2::SDL2)
		endif()
		target_include_directories(ciolib PUBLIC ${SDL2_INCLUDE_DIRS})
		target_compile_definitions(ciolib PUBLIC WITH_SDL)
	endif()
endif()

if(WIN32)
	if(NOT WITHOUT_GDI)
		target_compile_definitions(ciolib PUBLIC WITH_GDI)
	endif()
else()
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
		if(NOT CURSES_NCURSES_LIBRARY)
			target_link_libraries(ciolib ${CURSES_CURSES_LIBRARY})
		else()
			target_link_libraries(ciolib ${CURSES_NCURSES_LIBRARY})
		endif()
		if(CURSES_TINFO_LIBRARY_NAME)
			if(NOT CURSES_NCURSES_HAS_CBREAK OR NOT CURSES_NCURSES_HAS_NODELAY)
				find_library(CURSES_EXTRA_LIBRARY "${CURSES_TINFO_LIBRARY_NAME}")
				if(CURSES_EXTRA_LIBRARY)
					target_link_libraries(ciolib ${CURSES_TINFO_LIBRARY_NAME})
				endif()
			endif()
		endif()
	endif()
endif()

if(HAVE_VASPRINTF)
	target_compile_definitions(ciolib PRIVATE HAVE_VASPRINTF)
endif()
target_link_libraries(ciolib hash encode xpdev)
