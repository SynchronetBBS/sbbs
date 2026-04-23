cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the build type")
if(APPLE)
	project (SyncTERM C OBJC)
else()
	project (SyncTERM C)
endif()

# On macOS, the native Quartz backend replaces SDL (video and audio).
# Quartz and SDL are mutually exclusive (both need the main thread).
if(APPLE AND NOT WITHOUT_QUARTZ)
	set(WITHOUT_SDL ON CACHE BOOL "Disable SDL video support" FORCE)
	set(WITHOUT_SDL_AUDIO ON CACHE BOOL "Disable SDL Audio" FORCE)
	message(STATUS "macOS: Quartz backend enabled, SDL disabled")
endif()

set(WITHOUT_DEUCESSH OFF CACHE BOOL "Disable SSH support (DeuceSSH)")
set(WITHOUT_CRYPTO   OFF CACHE BOOL "Disable TLS + encrypted bbslist.ini (xp_tls, xp_crypt)")
set(WITHOUT_OOII OFF CACHE BOOL "Disable Operation Overkill ][ Terminal")
set(WITHOUT_JPEG_XL OFF CACHE BOOL "Disable JPEG XL")
set(WITHOUT_RETRO ON CACHE BOOL "Disable libretro")
set(USE_VENDORED_BOTAN "" CACHE STRING
    "Force vendored Botan build (1=force, 0=never, empty=auto-detect)")

# Decide whether we need the vendored Botan (from 3rdp/dist/Botan.tar.xz).
# Auto-detect unless the user pinned USE_VENDORED_BOTAN to 0 or 1:
#   - MinGW / Windows cross-compile always uses vendored.
#   - Otherwise: require system Botan 3.9+ or OpenSSL 3.0+; fall back
#     to vendored when neither is available.
if(USE_VENDORED_BOTAN STREQUAL "")
	find_package(PkgConfig QUIET)
	set(_botan_ok FALSE)
	set(_openssl_ok FALSE)
	if(WIN32 AND NOT CMAKE_HOST_WIN32)
		set(USE_VENDORED_BOTAN 1)
	else()
		if(PkgConfig_FOUND)
			pkg_check_modules(SYS_BOTAN3 QUIET "botan-3>=3.6")
			if(SYS_BOTAN3_FOUND)
				set(_botan_ok TRUE)
			endif()
			pkg_check_modules(SYS_OPENSSL QUIET "libcrypto>=3.0")
			if(SYS_OPENSSL_FOUND)
				set(_openssl_ok TRUE)
			endif()
		endif()
		if(_botan_ok OR _openssl_ok)
			set(USE_VENDORED_BOTAN 0)
		else()
			set(USE_VENDORED_BOTAN 1)
		endif()
	endif()
endif()

if(USE_VENDORED_BOTAN)
	# Build Botan from 3rdp/dist/Botan.tar.xz as an ExternalProject.
	# DeuceSSH's pkg_check_modules(BOTAN3 REQUIRED botan-3) picks it up
	# once PKG_CONFIG_PATH includes the vendored install's pkgconfig/.
	include(ExternalProject)
	set(VENDORED_BOTAN_PREFIX
	    "${CMAKE_BINARY_DIR}/vendored-botan" CACHE INTERNAL "")
	set(VENDORED_BOTAN_LIB
	    "${VENDORED_BOTAN_PREFIX}/lib/libbotan-3.a" CACHE INTERNAL "")
	set(VENDORED_BOTAN_PKGCONFIG
	    "${VENDORED_BOTAN_PREFIX}/lib/pkgconfig" CACHE INTERNAL "")
	set(_botan_configure_args
	    --prefix=${VENDORED_BOTAN_PREFIX}
	    --disable-shared-library
	    --without-documentation
	    --build-targets=static
	    --build-tool=make)
	if(CMAKE_CROSSCOMPILING AND WIN32)
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			list(APPEND _botan_configure_args
			    --os=mingw --cpu=x86_64
			    --cc-bin=${CMAKE_CXX_COMPILER}
			    --ar-command=${CMAKE_AR})
		else()
			list(APPEND _botan_configure_args
			    --os=mingw --cpu=x86
			    --cc-bin=${CMAKE_CXX_COMPILER}
			    --ar-command=${CMAKE_AR})
		endif()
	endif()
	ExternalProject_Add(vendored-botan
	    URL "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/dist/Botan.tar.xz"
	    PREFIX "${CMAKE_BINARY_DIR}/vendored-botan-src"
	    CONFIGURE_COMMAND python3 <SOURCE_DIR>/configure.py ${_botan_configure_args}
	    BUILD_COMMAND make
	    INSTALL_COMMAND make install
	    BUILD_BYPRODUCTS ${VENDORED_BOTAN_LIB}
	    BUILD_IN_SOURCE TRUE)
	# Prepend to PKG_CONFIG_PATH so downstream pkg_check_modules find it.
	if(DEFINED ENV{PKG_CONFIG_PATH})
		set(ENV{PKG_CONFIG_PATH}
		    "${VENDORED_BOTAN_PKGCONFIG}:$ENV{PKG_CONFIG_PATH}")
	else()
		set(ENV{PKG_CONFIG_PATH} "${VENDORED_BOTAN_PKGCONFIG}")
	endif()
	# Force DeuceSSH to pick Botan (vendored is Botan, not OpenSSL).
	set(DEUCESSH_CRYPTO_BACKEND "Botan" CACHE STRING "" FORCE)
endif()

# CPack stuff...
set(CPACK_PACKAGE_NAME SyncTERM)
set(CPACK_PACKAGE_VENDOR Deuce)
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 9)
set(CPACK_PACKAGE_VERSION_PATCH beta)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SyncTERM is a Telnet/RLogin/SSH BBS Client")
set(CPACK_PACKAGE_INSTALL_DIRECTORY SyncTERM)
#set(CPACK_PACKAGE_ICON ${CMAKE_CURRENT_LIST_DIR}/syncterm.png)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/LICENCE)
set(CPACK_MONOLITHIC_INSTALL TRUE)
set(CPACK_PACKAGE_EXECUTABLES "syncterm;SyncTERM")
set(CPACK_DEBIAN_PACKAGE_NAME syncterm)
set(CPACK_DEBIAN_PACKAGE_VERSION 1.9b)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Stephen Hurd")
INCLUDE(CPack)
######

INCLUDE (../build/SynchronetMacros.cmake)
INCLUDE (CheckIncludeFiles)

find_package(PkgConfig)
if(NOT WITHOUT_JPEG_XL)
	pkg_check_modules(JPEG_XL libjxl)
	pkg_check_modules(JPEG_XL_THREADS libjxl_threads)
	if(NOT JPEG_XL_FOUND AND APPLE)
		set(JPEG_XL_3RDP "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/darwin.release/libjxl")
		if(EXISTS "${JPEG_XL_3RDP}/lib/libjxl.a")
			set(JPEG_XL_FOUND TRUE)
			set(JPEG_XL_INCLUDE_DIRS "${JPEG_XL_3RDP}/include")
			set(JPEG_XL_CFLAGS -DJXL_THREADS_STATIC_DEFINE -DJXL_STATIC_DEFINE -DWITH_STATIC_JXL)
			set(JPEG_XL_LIBRARIES
				"${JPEG_XL_3RDP}/lib/libjxl.a"
				"${JPEG_XL_3RDP}/lib/libhwy.a"
				"${JPEG_XL_3RDP}/lib/libbrotlidec.a"
				"${JPEG_XL_3RDP}/lib/libbrotlicommon.a")
			if(EXISTS "${JPEG_XL_3RDP}/lib/libjxl_threads.a")
				set(JPEG_XL_THREADS_FOUND TRUE)
				set(JPEG_XL_THREADS_INCLUDE_DIRS "${JPEG_XL_3RDP}/include")
				set(JPEG_XL_THREADS_CFLAGS "")
				list(INSERT JPEG_XL_LIBRARIES 1 "${JPEG_XL_3RDP}/lib/libjxl_threads.a")
			endif()
			message(STATUS "macOS: using bundled static JPEG XL from 3rdp/darwin.release")
		endif()
	endif()
endif()

set(SOURCE
	bbslist.c
	conn.c
	conn_pty.c
	conn_telnet.c
	fonts.c
	menu.c
	modem.c
	rip_icons.c
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

if(NOT WITHOUT_DEUCESSH)
	list(APPEND SOURCE ssh.c)
endif()
if(NOT WITHOUT_CRYPTO)
	list(APPEND SOURCE
		telnets.c
		legacy_ciphers/idea.c
		legacy_ciphers/rc2.c
		legacy_ciphers/register.c)
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
if(WITHOUT_DEUCESSH)
	target_compile_definitions(syncterm PRIVATE WITHOUT_DEUCESSH)
else()
	# DeuceSSH sub-build — src/ssh/ ships a CMake build system.
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ssh deucessh EXCLUDE_FROM_ALL)
	target_include_directories(syncterm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ssh)
	target_link_libraries(syncterm deucessh_static sftp)
	if(USE_VENDORED_BOTAN)
		add_dependencies(deucessh_static vendored-botan)
	endif()
endif()

if(WITHOUT_CRYPTO)
	target_compile_definitions(syncterm PRIVATE WITHOUT_CRYPTO)
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
		if(JPEG_XL_3RDP)
			target_link_libraries(syncterm ${JPEG_XL_LIBRARIES})
			enable_language(CXX)
			set_target_properties(syncterm PROPERTIES LINKER_LANGUAGE CXX)
		endif()
	endif()
endif()

if(NOT WITHOUT_RETRO)
	target_compile_definitions(syncterm PUBLIC WITH_RETRO)
endif()

enable_testing()
if(UNIX AND NOT WITHOUT_SDL)
	add_executable(termtest termtest.c)
	add_test(NAME termtest
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_termtest.sh
			$<TARGET_FILE:syncterm> $<TARGET_FILE:termtest>)
endif()

install(TARGETS syncterm DESTINATION bin)
if(UNIX)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syncterm.man
		DESTINATION share/man/man1
		RENAME syncterm.1)
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
