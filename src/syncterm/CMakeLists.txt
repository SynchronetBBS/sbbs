cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the build type")
project (SyncTERM C)

set(WITHOUT_CRYPTLIB OFF CACHE BOOL "Disable cryptlib (ssh and telnet over TLS")
set(WITHOUT_OOII OFF CACHE BOOL "Disable Operation Overkill ][ Terminal")
set(WITHOUT_JPEG_XL OFF CACHE BOOL "Disable JPEG XL")
set(WITHOUT_RETRO ON CACHE BOOL "Disable libretro")

# CPack stuff...
set(CPACK_PACKAGE_NAME SyncTERM)
set(CPACK_PACKAGE_VENDOR Deuce)
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 8)
set(CPACK_PACKAGE_VERSION_PATCH rc1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SyncTERM is a Telnet/RLogin/SSH BBS Client")
set(CPACK_PACKAGE_INSTALL_DIRECTORY SyncTERM)
#set(CPACK_PACKAGE_ICON ${CMAKE_CURRENT_LIST_DIR}/syncterm.png)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/LICENCE)
set(CPACK_MONOLITHIC_INSTALL TRUE)
set(CPACK_PACKAGE_EXECUTABLES "syncterm;SyncTERM")
set(CPACK_DEBIAN_PACKAGE_NAME syncterm)
set(CPACK_DEBIAN_PACKAGE_VERSION 1.8rc1)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Stephen Hurd")
INCLUDE(CPack)
######

INCLUDE (../build/SynchronetMacros.cmake)
INCLUDE (CheckIncludeFiles)

find_package(PkgConfig)
if(NOT WITHOUT_JPEG_XL)
	pkg_check_modules(JPEG_XL libjxl)
	pkg_check_modules(JPEG_XL_THREADS libjxl_threads)
endif()

set(SOURCE
	bbslist.c
	conn.c
	conn_pty.c
	conn_telnet.c
	fonts.c
	menu.c
	modem.c
	ripper.c
	rlogin.c
	syncterm.c
	telnet_io.c
	term.c
	uifcinit.c
	webget.c
	window.c
	../sbbs3/telnet.c
	../sbbs3/xmodem.c
	../sbbs3/zmodem.c
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/build ${CMAKE_CURRENT_SOURCE_DIR}/cryptlib EXCLUDE_FROM_ALL)

if(NOT WITHOUT_OOII)
	list(APPEND SOURCE ooii.c ooii_bmenus.c ooii_cmenus.c ooii_logons.c ooii_sounds.c)
endif()

if(NOT WITHOUT_JPEG_XL)
	if (JPEG_XL_FOUND)
		list(APPEND SOURCE libjxl.c)
	endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	list(APPEND SOURCE DarwinWrappers.m)
	find_library(FOUNDATION_LIBRARY Foundation)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
	list(APPEND SOURCE haiku.rdef)
	execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCHITECTURE_TYPE OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

if(NOT WITHOUT_CRYPTLIB)
	list(APPEND SOURCE ssh.c telnets.c)
endif()

if(WIN32)
	list(APPEND SOURCE syncterm.rc)
endif()

if(UNIX)
	FIND_PROGRAM(SED_BIN sed REQUIRED)
	# TODO: I'm not sure how to make this work properly...
	add_custom_command(OUTPUT syncterm.man
		COMMAND ${SED_BIN} -E -e 's|%%DESTDIR%%|${DESTDIR}|g' -e 's|%%PREFIX%%|${CMAKE_INSTALL_PREFIX}|g' < ${CMAKE_CURRENT_SOURCE_DIR}/syncterm.man.in > ${CMAKE_CURRENT_BINARY_DIR}/syncterm.man
		DEPENDS syncterm.man.in)
	list(APPEND SOURCE syncterm.man.in)
	list(APPEND SOURCE syncterm.man)
endif()

if(WITHOUT_GDI AND WITHOUT_SDL)
	set(WIN_MAIN_TYPE "")
else()
	set(WIN_MAIN_TYPE WIN32)
endif()

if(NOT WITHOUT_RETRO)
	add_library(syncterm SHARED ${SOURCE})
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else()
	add_executable(syncterm MACOSX_BUNDLE ${WIN_MAIN_TYPE} ${SOURCE})
endif()
set_target_properties(syncterm PROPERTIES
	MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_sources(syncterm PRIVATE SyncTERM.icns)
	set_source_files_properties(SyncTERM.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()
require_libs(syncterm ciolib comio encode hash sftp uifc xpdev)
target_compile_definitions(syncterm PRIVATE TELNET_NO_DLL)
if(WITHOUT_OOII)
	target_compile_definitions(syncterm PRIVATE WITHOUT_OOII=1)
endif()
if(WITHOUT_CRYPTLIB)
	target_compile_definitions(syncterm PRIVATE WITHOUT_CRYPTLIB)
else()
	target_link_libraries(syncterm cl)
endif()

target_include_directories(syncterm PRIVATE ../sbbs3)

if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
	target_link_libraries(syncterm util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	target_link_libraries(syncterm util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	target_link_libraries(syncterm util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_link_libraries(syncterm util)
	target_link_libraries(syncterm ${FOUNDATION_LIBRARY})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(syncterm util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
	target_link_libraries(syncterm network bsd)
	configure_file(PackageInfo.in ${CMAKE_CURRENT_BINARY_DIR}/.PackageInfo @ONLY)
	add_custom_command(TARGET syncterm PRE_LINK
		COMMAND rc -o ${CMAKE_CURRENT_BINARY_DIR}/syncterm.rsrc ${CMAKE_CURRENT_SOURCE_DIR}/haiku.rdef)
	add_custom_command(TARGET syncterm POST_BUILD
		COMMAND xres -o $<TARGET_FILE:syncterm> ${CMAKE_CURRENT_BINARY_DIR}/syncterm.rsrc)
endif()
if(NOT WITHOUT_JPEG_XL)
	if(JPEG_XL_FOUND)
		target_compile_definitions(syncterm PUBLIC WITH_JPEG_XL)
		target_include_directories(syncterm PRIVATE ${JPEG_XL_INCLUDE_DIRS})
		target_compile_options(syncterm PRIVATE ${JPEG_XL_CFLAGS})
		if(JPEG_XL_THREADS_FOUND)
			target_compile_definitions(syncterm PUBLIC WITH_JPEG_XL_THREADS)
			target_include_directories(syncterm PRIVATE ${JPEG_XL_THREADS_INCLUDE_DIRS})
			target_compile_options(syncterm PRIVATE ${JPEG_XL_THREADS_CFLAGS})
		endif()
	endif()
endif()

if(NOT WITHOUT_RETRO)
	target_compile_definitions(syncterm PUBLIC WITH_RETRO)
endif()

install(TARGETS syncterm DESTINATION bin)
if(UNIX)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syncterm.man TYPE MAN)
	install(FILES syncterm.desktop DESTINATION share/applications)
	install(FILES syncterm.desktop DESTINATION share/applications)
	install(FILES syncterm16.png DESTINATION share/icons/hicolor/16x16/apps RENAME syncterm.png)
	install(FILES syncterm22.png DESTINATION share/icons/hicolor/22x22/apps RENAME syncterm.png)
	install(FILES syncterm24.png DESTINATION share/icons/hicolor/24x24/apps RENAME syncterm.png)
	install(FILES syncterm32.png DESTINATION share/icons/hicolor/32x32/apps RENAME syncterm.png)
	install(FILES syncterm36.png DESTINATION share/icons/hicolor/36x36/apps RENAME syncterm.png)
	install(FILES syncterm48.png DESTINATION share/icons/hicolor/48x48/apps RENAME syncterm.png)
	install(FILES syncterm64.png DESTINATION share/icons/hicolor/64x64/apps RENAME syncterm.png)
	install(FILES syncterm256.png DESTINATION share/icons/hicolor/256x256/apps RENAME syncterm.png)
	install(FILES icons/syncterm.svg DESTINATION share/icons/hicolor/scalable/apps)
	install(FILES icons/syncterm-mini.svg DESTINATION share/icons/hicolor/scalable/apps)
endif()
