project (SyncTERM C)

cmake_minimum_required(VERSION 2.8.11)

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
	../comio/comio.c
	conn_pty.c
)

if(WIN32)
	list(APPEND SOURCE ../comio/comio_win32.c)
else()
	list(APPEND SOURCE ../comio/comio_nix.c)
endif()

CHECK_INCLUDE_FILES(cryptlib.h HAS_CRYPTLIB_H)
if(HAS_CRYPTLIB_H)
	list(APPEND SOURCE ssh.c)
endif()

add_executable(syncterm ${SOURCE})

if(NOT ${PARENT_DIRECTORY})
	find_package(XPDev REQUIRED)
	include(${XPDev_CONFIG})
ENDIF()
add_dependencies(syncterm xpdev)
target_include_directories(syncterm PRIVATE "${XPDev_DIR}/../../../include/xpdev")
target_compile_definitions(syncterm PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(syncterm PRIVATE $<TARGET_PROPERTY:xpdev,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(syncterm xpdev)

if(NOT ${PARENT_DIRECTORY})
	find_package(CIOLib REQUIRED)
	include(${CIOLib_CONFIG})
ENDIF()
add_dependencies(syncterm ciolib)
target_include_directories(syncterm PRIVATE "${CIOLib_DIR}/../../../include")
target_compile_definitions(syncterm PRIVATE $<TARGET_PROPERTY:ciolib,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(syncterm PRIVATE $<TARGET_PROPERTY:ciolib,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(syncterm ciolib)

if(NOT ${PARENT_DIRECTORY})
	find_package(UIFC REQUIRED)
	include(${UIFC_CONFIG})
ENDIF()
add_dependencies(syncterm uifc)
target_include_directories(syncterm PRIVATE "${UIFC_DIR}/../../../include")
target_compile_definitions(syncterm PRIVATE $<TARGET_PROPERTY:uifc,INTERFACE_COMPILE_DEFINITIONS>)
target_include_directories(syncterm PRIVATE $<TARGET_PROPERTY:uifc,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(syncterm uifc)

target_include_directories(syncterm PRIVATE ../uifc)
target_include_directories(syncterm PRIVATE ../sbbs3)
target_include_directories(syncterm PRIVATE ../comio)
target_include_directories(syncterm PRIVATE ../smblib)
if(WIN32)
	include_directories(syncterm PRIVATE ../../3rdp/win32.release/cryptlib/include)
endif()
target_link_libraries(syncterm m)
if(NOT HAS_CRYPTLIB_H)
    target_compile_definitions(syncterm PRIVATE WITHOUT_CRYPTLIB)
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
