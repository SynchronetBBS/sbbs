cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the build type")
# CXX is conditionally needed for the Botan backend (xp_tls_botan3.cpp,
# xp_crypt_botan3.cpp, src/ssh/crypto/botan.cpp and a handful of Botan
# key-algo/KEX wrappers).  Declaring it up-front primes CMAKE_CXX_*
# variables so LINKER_LANGUAGE CXX works at generate time.
if(APPLE)
	project (SyncTERM C CXX OBJC)
else()
	project (SyncTERM C CXX)
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
	# Botan's configure.py needs a Python 3 interpreter.  On Windows CI
	# only `python.exe` is on PATH (no `python3.exe`); find_package
	# handles both layouts.  If Python 3 isn't present at all, we
	# can't build vendored Botan — drop all the way to WITHOUT_CRYPTO
	# so the build still completes (no TLS / encrypted-bbslist) rather
	# than failing with an opaque cmake error later.
	find_package(Python3 COMPONENTS Interpreter QUIET)
	if(NOT Python3_Interpreter_FOUND)
		message(WARNING
		    "Python 3 not found; can't build vendored Botan.  "
		    "Disabling crypto (WITHOUT_CRYPTO + WITHOUT_DEUCESSH).")
		set(WITHOUT_CRYPTO ON CACHE BOOL
		    "Disable TLS + encrypted bbslist.ini (xp_tls, xp_crypt)" FORCE)
		set(WITHOUT_DEUCESSH ON CACHE BOOL
		    "Disable SSH support (DeuceSSH)" FORCE)
		set(USE_VENDORED_BOTAN 0)
		# Pin xpdev to the stub backends; otherwise xpdev's default
		# of "OpenSSL" tries find_package(OpenSSL REQUIRED) and fails
		# on hosts that also don't have OpenSSL (Windows MSVC CI).
		set(XP_CRYPTO_BACKEND "none" CACHE STRING
		    "xpdev crypto backend: OpenSSL, Botan, or none" FORCE)
		message(STATUS
		    "Crypto fallback: XP_CRYPTO_BACKEND=none, "
		    "WITHOUT_CRYPTO=ON, WITHOUT_DEUCESSH=ON")
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
	# Minimal module set — keep synchronised with BOTAN_MODULES in
	# 3rdp/build/GNUmakefile.  Botan pulls transitive deps under
	# --minimized-build, so this is the top-level "what we call" list.
	set(_botan_modules
	    "tls12,tls13,aes,cbc,ctr,chacha,chacha20poly1305"
	    ",rc4,des,cast128,hmac,pbkdf2,scrypt"
	    ",sha1,sha2_32,sha2_64,rsa,ed25519,x25519,dh,ecdsa,ecdh,ml_kem"
	    ",pcurves_secp256r1,pcurves_secp384r1,pcurves_secp521r1"
	    ",x509,system_rng,auto_rng")
	string(REPLACE ";" "" _botan_modules "${_botan_modules}")
	set(_botan_configure_args
	    --prefix=${VENDORED_BOTAN_PREFIX}
	    --disable-shared-library
	    --without-documentation
	    --build-targets=static
	    --minimized-build
	    --enable-modules=${_botan_modules})
	if(MSVC)
		# MSVC: Botan's configure.py picks --cc=msvc + nmake Makefile.
		# Output is botan.lib (no libbotan-3.a naming convention).
		list(APPEND _botan_configure_args
		    --cc=msvc --build-tool=nmake)
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			list(APPEND _botan_configure_args --cpu=x86_64)
		else()
			list(APPEND _botan_configure_args --cpu=x86)
		endif()
		set(VENDORED_BOTAN_LIB
		    "${VENDORED_BOTAN_PREFIX}/lib/botan.lib" CACHE INTERNAL "")
		set(_botan_build_cmd    nmake)
		set(_botan_install_cmd  nmake install)
	elseif(CMAKE_CROSSCOMPILING AND WIN32)
		list(APPEND _botan_configure_args --build-tool=make)
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
		# Probe parallel build width; Botan's generated Makefile doesn't
		# use GNU make's jobserver, and inheriting parent MAKEFLAGS with
		# "-j" but no count produces "illegal argument to -j".  Clear
		# MAKEFLAGS and pass an explicit -jN to the sub-make.
		include(ProcessorCount)
		ProcessorCount(_n_cpus)
		if(_n_cpus EQUAL 0)
			set(_n_cpus 2)
		endif()
		set(_botan_build_cmd
		    ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS make -j${_n_cpus})
		set(_botan_install_cmd
		    ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS make install)
	else()
		list(APPEND _botan_configure_args --build-tool=make)
		include(ProcessorCount)
		ProcessorCount(_n_cpus)
		if(_n_cpus EQUAL 0)
			set(_n_cpus 2)
		endif()
		set(_botan_build_cmd
		    ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS make -j${_n_cpus})
		set(_botan_install_cmd
		    ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS make install)
	endif()
	ExternalProject_Add(vendored-botan
	    URL "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/dist/Botan.tar.xz"
	    PREFIX "${CMAKE_BINARY_DIR}/vendored-botan-src"
	    CONFIGURE_COMMAND ${Python3_EXECUTABLE} <SOURCE_DIR>/configure.py ${_botan_configure_args}
	    BUILD_COMMAND ${_botan_build_cmd}
	    INSTALL_COMMAND ${_botan_install_cmd}
	    BUILD_BYPRODUCTS ${VENDORED_BOTAN_LIB}
	    BUILD_IN_SOURCE TRUE)
	# ExternalProject runs at build time, so the .pc file doesn't exist
	# yet when sub-projects run pkg_check_modules at configure time.
	# Set up a manually-defined IMPORTED target + BYPASS hint so xpdev
	# and DeuceSSH use the baked-in paths instead of consulting pkg-
	# config.  The target's OUTPUT file is listed in BUILD_BYPRODUCTS so
	# it works with both make and Ninja.
	file(MAKE_DIRECTORY "${VENDORED_BOTAN_PREFIX}/include/botan-3")
	add_library(vendored-botan-3 STATIC IMPORTED GLOBAL)
	set_target_properties(vendored-botan-3 PROPERTIES
	    IMPORTED_LOCATION "${VENDORED_BOTAN_LIB}"
	    INTERFACE_INCLUDE_DIRECTORIES
	        "${VENDORED_BOTAN_PREFIX}/include/botan-3")
	add_dependencies(vendored-botan-3 vendored-botan)
	# Cache hints picked up by xpdev and DeuceSSH CMakeLists to avoid
	# running pkg-config entirely.
	set(BOTAN3_VENDORED_TARGET "vendored-botan-3" CACHE INTERNAL "")
	# Force DeuceSSH + xpdev to pick Botan (vendored is Botan, not OpenSSL).
	set(DEUCESSH_CRYPTO_BACKEND "Botan" CACHE STRING "" FORCE)
	set(XP_CRYPTO_BACKEND       "Botan" CACHE STRING "" FORCE)
else()
	# Not using the vendored build — force xpdev + DeuceSSH onto whichever
	# system library we already probed.  Botan first (matches the vendored
	# case so switching doesn't change behaviour), OpenSSL otherwise.
	if(_botan_ok)
		set(DEUCESSH_CRYPTO_BACKEND "Botan"   CACHE STRING "" FORCE)
		set(XP_CRYPTO_BACKEND       "Botan"   CACHE STRING "" FORCE)
	elseif(_openssl_ok)
		set(DEUCESSH_CRYPTO_BACKEND "OpenSSL" CACHE STRING "" FORCE)
		set(XP_CRYPTO_BACKEND       "OpenSSL" CACHE STRING "" FORCE)
	endif()
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
# libsndfile: mirrors GNUmakefile.  Default is the dynamic dlopen path
# (WITH_SNDFILE); if the header isn't visible at configure time, drop
# the whole feature to stubs (WITHOUT_SNDFILE) so sndfile.c still
# compiles.  STATIC_SNDFILE isn't exposed via cmake — nothing in CI
# asks for it today.
if(NOT WITHOUT_SNDFILE)
	set(_have_sndfile FALSE)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(SNDFILE sndfile)
		if(SNDFILE_FOUND)
			set(_have_sndfile TRUE)
		endif()
	endif()
	if(NOT _have_sndfile)
		find_path(_sndfile_h sndfile.h)
		if(_sndfile_h)
			set(_have_sndfile TRUE)
		endif()
	endif()
	if(NOT _have_sndfile)
		set(WITHOUT_SNDFILE ON)
	endif()
endif()
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
	audio_apc.c
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
	sndfile.c
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

# Botan backend pulls C++ translation units into libdeucessh.a and into
# xpdev (xp_tls_botan3.cpp, xp_crypt_botan3.cpp); the final link needs
# the C++ driver to resolve libc++/libsupc++ (operator new, __cxa_throw,
# vtables, typeinfo, ...).
if(XP_CRYPTO_BACKEND STREQUAL "Botan" OR DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	set_target_properties(syncterm PROPERTIES LINKER_LANGUAGE CXX)
endif()

if(WITHOUT_CRYPTO)
	target_compile_definitions(syncterm PRIVATE WITHOUT_CRYPTO)
endif()

# libsndfile: dynamic dlopen path when the header is visible,
# stubs-only otherwise.  Matches sndfile.c's #if switch.
if(WITHOUT_SNDFILE)
	target_compile_definitions(syncterm PRIVATE WITHOUT_SNDFILE)
else()
	target_compile_definitions(syncterm PRIVATE WITH_SNDFILE)
	if(SNDFILE_INCLUDE_DIRS)
		target_include_directories(syncterm PRIVATE ${SNDFILE_INCLUDE_DIRS})
	endif()
	if(SNDFILE_CFLAGS_OTHER)
		target_compile_options(syncterm PRIVATE ${SNDFILE_CFLAGS_OTHER})
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
