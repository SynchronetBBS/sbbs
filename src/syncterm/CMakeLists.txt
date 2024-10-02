cmake_minimum_required(VERSION 3.5)
project (SyncTERM C)

set(WITHOUT_CRYPTLIB OFF CACHE BOOL "Disable cryptlib (ssh and telnet over TLS")
set(WITHOUT_OOII OFF CACHE BOOL "Disable Operation Overkill ][ Terminal")

# CPack stuff...
set(CPACK_PACKAGE_NAME SyncTERM)
set(CPACK_PACKAGE_VENDOR Deuce)
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 2)
set(CPACK_PACKAGE_VERSION_PATCH beta)
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

if(NOT WIN32)
	find_program(3RDP_GNU_MAKE
		NAMES gmake make
	)
	if(${3RDP_GNU_MAKE} STREQUAL "3RDP_GNU_MAKE-NOTFOUND")
		set(WITHOUT_CRYPTLIB ON CACHE BOOL "Disable cryptlib (ssh and telnet over TLS" FORCE)
		message(WARNING "GNU Make not found, cannot build cryptlib")
	else()
		if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
			set(BUILD_TYPE_FLAG DEBUG=1)
		else()
			set(BUILD_TYPE_FLAG RELEASE=1)
		endif()
		execute_process(
			COMMAND ${3RDP_GNU_MAKE} -C ${CMAKE_CURRENT_SOURCE_DIR}/../build --no-print-directory DONT_CLOBBER_CC=please ${BUILD_TYPE_FLAG}
			OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/paths.txt
			ERROR_FILE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/paths.err
		)
		file(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/paths.txt PATHS)
		list(GET PATHS 0 LEGACY_LIBDIR)
		list(GET PATHS 1 LEGACY_OBJDIR)
		list(GET PATHS 2 LEGACY_MTOBJBDIR)
		list(GET PATHS 3 LEGACY_EXEDIR)
		list(GET PATHS 4 LEGACY_3RDPDIR)
		add_custom_target(cryptlib
			${3RDP_GNU_MAKE} cryptlib DONT_CLOBBER_CC=please  ${BUILD_TYPE_FLAG}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/build
		)
	endif()
endif()

if(NOT WITHOUT_OOII)
	list(APPEND SOURCE ooii.c ooii_bmenus.c ooii_cmenus.c ooii_logons.c ooii_sounds.c)
endif()

if(NOT WITHOUT_CRYPTLIB)
	list(APPEND SOURCE ssh.c telnets.c)
	if(WIN32)
		find_library(3RDP_CRYPTLIB_LIB cl32 PATHS ${CMAKE_CURRENT_LIST_DIR}/../../3rdp/win32.release/cryptlib/bin NO_DEFAULT_PATH)
	endif()
endif()

add_executable(syncterm ${SOURCE})
require_libs(syncterm ciolib comio encode hash sftp uifc xpdev)
if(WITHOUT_OOII)
	target_compile_definitions(syncterm PRIVATE WITHOUT_OOII=1)
endif()
if(WITHOUT_CRYPTLIB)
	target_compile_definitions(syncterm PRIVATE WITHOUT_CRYPTLIB)
else()
	if(WIN32)
		target_include_directories(syncterm PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../3rdp/win32.release/cryptlib/include)
		target_link_libraries(syncterm ${3RDP_CRYPTLIB_LIB})
	else()
		add_dependencies(syncterm cryptlib)
		target_include_directories(syncterm PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../3rdp/${LEGACY_3RDPDIR}/cl)
		target_link_directories(syncterm PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../3rdp/${LEGACY_3RDPDIR}/cl)
		target_link_libraries(syncterm cl)
	endif()
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
endif()

install(TARGETS syncterm DESTINATION bin)
if(UNIX)
	install(FILES syncterm.desktop DESTINATION share/applications)
	install(FILES syncterm.png DESTINATION share/icons/hicolor/64x64/apps)
endif()
