project (SMBLib C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../build/SynchronetMacros.cmake)

set(SOURCE
	smbadd.c
	smballoc.c
	smbdump.c
	smbfile.c
	smbhash.c
	smblib.c
	smbstr.c
	smbtxt.c
	crc16.c
	crc32.c
	md5.c
	lzh.c
)

add_library(smblib SHARED ${SOURCE})
require_lib(smblib xpdev)

target_compile_definitions(smblib PRIVATE SMB_EXPORTS)
target_compile_definitions(smblib INTERFACE SMB_IMPORTS)
target_compile_definitions(smblib PRIVATE MD5_EXPORTS)
target_compile_definitions(smblib INTERFACE MD5_IMPORTS)
target_compile_definitions(smblib PRIVATE LZH_EXPORTS)
target_compile_definitions(smblib INTERFACE LZH_IMPORTS)
target_compile_definitions(smblib PRIVATE CRC_EXPORTS)
target_compile_definitions(smblib INTERFACE CRC_IMPORTS)
target_link_libraries(smblib ${CMAKE_DL_LIBS})

set(INSTALL_HEADERS
	smbdefs.h
	smblib.h
)

set(INSTALL_HEADERS_SUBDIR
	lzh.h
	md5.h
)

export(PACKAGE SMBLib)

install(TARGETS smblib DESTINATION lib EXPORT SMBLibConfig)
install(FILES ${INSTALL_HEADERS} DESTINATION include)
install(FILES ${INSTALL_HEADERS_SUBDIR} DESTINATION include/smblib)
install(EXPORT SMBLibConfig DESTINATION lib/cmake/SMBLib)
