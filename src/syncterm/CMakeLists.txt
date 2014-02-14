project (SyncTERM C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../build/SynchronetMacros.cmake)
INCLUDE (CheckIncludeFiles)

set(SOURCE
	uifc32.c
	uifcx.c
)

set(SOURCE
	bbslist.c
	uifcinit.c
	../uifc/filepick.c
	fonts.c
	rlogin.c
	telnet_io.c
	conn_telnet.c
	conn.c
	../sbbs3/telnet.c
	term.c
	window.c
	menu.c
	../smblib/crc16.c
	../smblib/crc32.c
	../sbbs3/zmodem.c
	../sbbs3/xmodem.c
	st_crypt.c
	syncterm.c
	ooii.c
	ooii_logons.c
	ooii_cmenus.c
	ooii_bmenus.c
	ooii_sounds.c
	modem.c
	conn_pty.c
)

if(WIN32)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/../../3rdp/win32.release/cryptlib/include)
endif()

CHECK_INCLUDE_FILES(cryptlib.h HAS_CRYPTLIB_H)
if(HAS_CRYPTLIB_H)
	list(APPEND SOURCE ssh.c)
endif()

add_executable(syncterm ${SOURCE})
require_libs(syncterm xpdev ciolib uifc comio)

target_include_directories(syncterm PRIVATE ../sbbs3)
target_include_directories(syncterm PRIVATE ../smblib)

if(NOT HAS_CRYPTLIB_H)
    target_compile_definitions(syncterm PRIVATE WITHOUT_CRYPTLIB)
else()
	if(WIN32)
		target_include_directories(syncterm PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../3rdp/win32.release/cryptlib/include)
	endif()
endif()

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
