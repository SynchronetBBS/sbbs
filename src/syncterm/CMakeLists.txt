cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the build type")
project (SyncTERM C)

set(WITHOUT_CRYPTLIB OFF CACHE BOOL "Disable cryptlib (ssh and telnet over TLS")
set(WITHOUT_OOII OFF CACHE BOOL "Disable Operation Overkill ][ Terminal")

# CPack stuff...
set(CPACK_PACKAGE_NAME SyncTERM)
set(CPACK_PACKAGE_VENDOR Deuce)
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 2)
set(CPACK_PACKAGE_VERSION_PATCH rc1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SyncTERM is a Telnet/RLogin/SSH BBS Client")
set(CPACK_PACKAGE_INSTALL_DIRECTORY SyncTERM)
#set(CPACK_PACKAGE_ICON ${CMAKE_CURRENT_LIST_DIR}/syncterm.png)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/LICENCE)
set(CPACK_MONOLITHIC_INSTALL TRUE)
set(CPACK_PACKAGE_EXECUTABLES "syncterm;SyncTERM")
set(CPACK_DEBIAN_PACKAGE_NAME syncterm)
set(CPACK_DEBIAN_PACKAGE_VERSION 1.2b)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Stephen Hurd")
INCLUDE(CPack)
######

INCLUDE (../build/SynchronetMacros.cmake)
INCLUDE (CheckIncludeFiles)

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
	window.c
	../sbbs3/telnet.c
	../sbbs3/zmodem.c
	../sbbs3/xmodem.c
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/build ${CMAKE_CURRENT_SOURCE_DIR}/cryptlib EXCLUDE_FROM_ALL)

if(NOT WITHOUT_OOII)
	list(APPEND SOURCE ooii.c ooii_bmenus.c ooii_cmenus.c ooii_logons.c ooii_sounds.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	list(APPEND SOURCE DarwinWrappers.m)
	find_library(FOUNDATION_LIBRARY Foundation)
endif()

if(NOT WITHOUT_CRYPTLIB)
	list(APPEND SOURCE ssh.c telnets.c)
endif()

if(WIN32)
	list(APPEND SOURCE syncterm.rc)
endif()

if(WITHOUT_GDI AND WITHOUT_SDL)
	set(WIN_MAIN_TYPE "")
else()
	set(WIN_MAIN_TYPE WIN32)
endif()

add_executable(syncterm MACOSX_BUNDLE ${WIN_MAIN_TYPE} ${SOURCE})
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
endif()

install(TARGETS syncterm DESTINATION bin)
if(UNIX)
	install(FILES syncterm.desktop DESTINATION share/applications)
	install(FILES syncterm.png DESTINATION share/icons/hicolor/64x64/apps)
endif()
