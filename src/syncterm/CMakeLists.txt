cmake_minimum_required(VERSION 3.22)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the build type")
# CXX is conditionally needed for the Botan backend (xptls provider sources,
# src/ssh/crypto/botan.cpp and a handful of Botan
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
set(WITHOUT_CRYPTO   OFF CACHE BOOL "Disable TLS + encrypted bbslist.ini (xptls)")
set(WITHOUT_OOII OFF CACHE BOOL "Disable Operation Overkill ][ Terminal")
set(WITHOUT_JPEG_XL OFF CACHE BOOL "Disable JPEG XL")
set(WITHOUT_RETRO ON CACHE BOOL "Disable libretro")

# Resolve/build the provider once, then pin both independent libraries to it.
# The shared module owns vendored Botan; DeuceSSH itself remains unchanged.
include(../build/SynchronetCrypto.cmake)
synchronet_configure_crypto()

# CPack stuff...
set(CPACK_PACKAGE_NAME SyncTERM)
set(CPACK_PACKAGE_VENDOR Deuce)
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 10)
set(CPACK_PACKAGE_VERSION_PATCH alpha)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SyncTERM is a Telnet/RLogin/SSH BBS Client")
set(CPACK_PACKAGE_INSTALL_DIRECTORY SyncTERM)
#set(CPACK_PACKAGE_ICON ${CMAKE_CURRENT_LIST_DIR}/syncterm.png)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/LICENCE)
set(CPACK_MONOLITHIC_INSTALL TRUE)
set(CPACK_PACKAGE_EXECUTABLES "syncterm;SyncTERM")
set(CPACK_DEBIAN_PACKAGE_NAME syncterm)
set(CPACK_DEBIAN_PACKAGE_VERSION 1.10a)
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
	bbslist_model.c
	menu_settings.c
	host_ui.c
	conn.c
	conn_buf.c
	ansi_filter.c
	conn_pty.c
	conn_telnet.c
	fonts.c
	modem.c
	pixel_image.c
	rip_icons.c
	ripper.c
	rlogin.c
	xp_sndfile.c
	syncterm.c
	theme.c
	theme_cloud.c
	telnet_io.c
	term.c
	xfer_queue.c
	xfer_recv.c
	webget.c
	window.c
	wren_host.c
	wren_menu_host.c
	wren_picker_host.c
	wren_bind.c
	wren_bind_menu.c
	wren_bind_menu_fonts.c
	wren_bind_menu_settings.c
	wren_bind_picker.c
	wren_bind_sftp.c
	wren_bind_fs.c
	wren_bind_pixels.c
	wren_bind_screen.c
	wren_bind_conn.c
	wren_bind_hook.c
	wren_bind_won.c
	wren_bind_xfer.c
	wren_token.c
	../sbbs3/telnet.c
	../sbbs3/xmodem.c
	../sbbs3/zmodem.c
)

# Wren scripting VM (vendored under wren/).  Compiled as a static
# library so its private vm/ + optional/ include paths don't leak into
# syncterm's translation units.  Public API is wren/include/wren.h,
# which wren_host.c / wren_bind.c consume.
file(GLOB WREN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/wren/vm/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/wren/optional/*.c")
add_library(wrenvm STATIC ${WREN_SOURCES})
target_include_directories(wrenvm
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/wren/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wren/vm
            ${CMAKE_CURRENT_SOURCE_DIR}/wren/optional)

# RE1 (Russ Cox's NFA regex engine, BSD-3-Clause).  Static library
# because the pedagogical sources don't pass our usual warning set;
# isolating them here means -w applies only to RE1's TUs.  Public API
# is re1/regexp.h, consumed by wren_bind.c for Hook.onMatch.
add_library(re1 STATIC
    re1/compile.c
    re1/pike.c
    re1/sub.c
    re1/y.tab.c)
target_include_directories(re1
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/re1)
target_compile_options(re1 PRIVATE -w)

# Embedded-script codegen.  Mirrors src/conio/genmap.c's pattern: a
# small C tool reads scripts/*.wren and writes embedded_scripts.c
# which compiles into syncterm alongside wren_host.c.  In native
# builds the tool builds with the project CC; in cross-builds we
# shell out at configure time to a host CC ($BUILD_CC, default `cc`)
# so the tool itself can run on the build host.
#
# Two source directories feed the embed:
#   * scripts/*.wren                  pure library modules, loaded only
#                                     when something imports them.
#   * scripts/auto/<event>/*.wren     auto-loaded entry scripts.  Each
#                                     subdirectory under auto/ names a
#                                     framework event.  `menu` loads once
#                                     in the trusted persistent VM;
#                                     `connected` loads in each session VM;
#                                     `picker` loads once in its trusted VM.
file(GLOB WREN_LIB_SCRIPTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.wren")
file(GLOB WREN_AUTO_SCRIPTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/auto/connected/*.wren"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/auto/menu/*.wren"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/auto/picker/*.wren")
set(WREN_SCRIPTS ${WREN_LIB_SCRIPTS} ${WREN_AUTO_SCRIPTS})
# Developer-only files are not embedded:
#   * scripts/auto/connected/runtests.wren — Alt+T hotkey trigger
#   * scripts/wrentest.wren                — Wren self-test suite
#   * scripts/*_test.wren                  — per-library test modules
#   * scripts/*_demo.wren                  — interactive playgrounds
# Devs run against the source scripts/ dir to pick them up; user
# builds get a binary without them.
list(FILTER WREN_SCRIPTS EXCLUDE REGEX
    "/(runtests|wrentest|.*_test|.*_demo)\\.wren$")

if(NOT CMAKE_CROSSCOMPILING)
    add_executable(wren_embed_gen wren_embed_gen.c)
    set(WREN_EMBED_GEN_BIN $<TARGET_FILE:wren_embed_gen>)
    set(WREN_EMBED_GEN_DEP wren_embed_gen)
else()
    set(_host_cc "$ENV{BUILD_CC}")
    if(NOT _host_cc)
        set(_host_cc "cc")
    endif()
    set(WREN_EMBED_GEN_BIN
        "${CMAKE_CURRENT_BINARY_DIR}/wren_embed_gen")
    execute_process(
        COMMAND ${_host_cc} -O2
                "${CMAKE_CURRENT_SOURCE_DIR}/wren_embed_gen.c"
                -o "${WREN_EMBED_GEN_BIN}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "Failed to build wren_embed_gen with host CC ${_host_cc}")
    endif()
    set(WREN_EMBED_GEN_DEP "")
endif()

add_custom_command(
    OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/embedded_scripts.c"
    COMMAND ${WREN_EMBED_GEN_BIN} ${WREN_SCRIPTS}
            > "${CMAKE_CURRENT_BINARY_DIR}/embedded_scripts.c"
    DEPENDS ${WREN_EMBED_GEN_DEP} ${WREN_SCRIPTS}
            "${CMAKE_CURRENT_SOURCE_DIR}/wren_embed_gen.c"
    VERBATIM)

list(APPEND SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/embedded_scripts.c")

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

# ini_crypt.c and the xptls disabled backend are always built, even
# with WITHOUT_CRYPTO — bbslist.c and syncterm.c reference the
# encrypted-INI API unconditionally (iniWriteEncryptedFile is the
# general write path; with INI_CRYPT_ALGO_NONE it falls through to
# plain iniWriteFile). The "none" backend supplies linkable stubs, so
# nothing drags in OpenSSL or Botan.
list(APPEND SOURCE ini_crypt.c)

if(NOT WITHOUT_CRYPTO)
	# telnets.c + legacy decrypt-only ciphers only make sense when a
	# real backend is linked — the "none" path can't decrypt anything.
	# conn_mqtt.c rides the same TLS-PSK path so it's gated identically.
	list(APPEND SOURCE
		telnets.c
		conn_mqtt.c
		legacy_ciphers/idea.c
		legacy_ciphers/rc2.c)
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
set_property(TARGET syncterm PROPERTY C_STANDARD 17)
set_property(TARGET syncterm PROPERTY C_STANDARD_REQUIRED ON)
# Botan 3 requires C++20 in the provider libraries.
# Setting CXX_STANDARD on the target is harmless when no .cpp sources
# are present (OpenSSL / none backends).
set_property(TARGET syncterm PROPERTY CXX_STANDARD 20)
set_property(TARGET syncterm PROPERTY CXX_STANDARD_REQUIRED ON)
if(MSVC)
	target_compile_options(syncterm PRIVATE /experimental:c11atomics)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_sources(syncterm PRIVATE SyncTERM.icns)
	set_source_files_properties(SyncTERM.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()
require_libs(syncterm ciolib comio encode hash sftp xpdev)
# Add xptls after xpdev.  The xptls archive contains only xptls objects;
# xpdev remains a separate final-link dependency.
add_subdirectory(../xptls xptls EXCLUDE_FROM_ALL)
target_link_libraries(syncterm wrenvm re1)
target_compile_definitions(syncterm PRIVATE TELNET_NO_DLL)
if(WITHOUT_OOII)
	target_compile_definitions(syncterm PRIVATE WITHOUT_OOII=1)
endif()
if(NOT WITHOUT_DEUCESSH)
	# Probe DeuceSSH's cmake configure at outer configure-time.  If it
	# fails for any reason — C17 not supported, missing crypto headers,
	# whatever — flip WITHOUT_DEUCESSH so the rest of syncterm still
	# builds instead of fataling here.  Matches the gmake probe in
	# GNUmakefile so cmake users aren't left to diagnose this manually.
	#
	# Only probe once the crypto backend DeuceSSH needs is actually on
	# disk — DeuceSSH's configure runs pkg_check_modules on it, and a
	# not-yet-built vendored Botan would look identical to a real
	# failure.  In CI the [botan] job / botan.tgz extraction ensures
	# the .pc file is present by the time we get here; fresh local
	# checkouts with no system crypto skip the probe and let
	# add_subdirectory proceed normally (using the BOTAN3_VENDORED_TARGET
	# imported-target machinery that was set up earlier in this file).
	set(_dssh_crypto_ready FALSE)
	set(_dssh_extra_pkgconfig "")
	if(USE_VENDORED_BOTAN)
		if(EXISTS "${VENDORED_BOTAN_PREFIX}/lib/pkgconfig/botan-3.pc")
			set(_dssh_crypto_ready TRUE)
			set(_dssh_extra_pkgconfig
			    "${VENDORED_BOTAN_PREFIX}/lib/pkgconfig")
		endif()
	else()
		set(_dssh_crypto_ready TRUE)
	endif()

	if(_dssh_crypto_ready)
		set(_dssh_probe_dir "${CMAKE_BINARY_DIR}/deucessh-probe")
		if(_dssh_extra_pkgconfig)
			set(ENV{PKG_CONFIG_PATH}
			    "${_dssh_extra_pkgconfig}:$ENV{PKG_CONFIG_PATH}")
		endif()
		execute_process(
		    COMMAND ${CMAKE_COMMAND}
		        -S ${CMAKE_CURRENT_SOURCE_DIR}/../ssh
		        -B ${_dssh_probe_dir}
		        -DCMAKE_BUILD_TYPE=Release
		        -DDEUCESSH_CRYPTO_BACKEND=${DEUCESSH_CRYPTO_BACKEND}
		    RESULT_VARIABLE _dssh_cfg_rc
		    OUTPUT_FILE ${CMAKE_BINARY_DIR}/deucessh-probe.log
		    ERROR_FILE ${CMAKE_BINARY_DIR}/deucessh-probe.log)
		if(_dssh_cfg_rc OR NOT EXISTS "${_dssh_probe_dir}/deucessh.pc")
			message(STATUS
			    "DeuceSSH configure failed (see "
			    "${CMAKE_BINARY_DIR}/deucessh-probe.log) — "
			    "disabling SSH support")
			set(WITHOUT_DEUCESSH ON CACHE BOOL "" FORCE)
		endif()
	endif()
endif()

if(WITHOUT_DEUCESSH)
	target_compile_definitions(syncterm PRIVATE WITHOUT_DEUCESSH)
else()
	# DeuceSSH sub-build — src/ssh/ ships a CMake build system.
	target_sources(syncterm PRIVATE ssh.c)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ssh deucessh EXCLUDE_FROM_ALL)
	target_include_directories(syncterm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ssh)
	target_link_libraries(syncterm deucessh_static sftp)
	if(USE_VENDORED_BOTAN)
		add_dependencies(deucessh_static vendored-botan)
	endif()
endif()

# TLS, ciphers, KDFs, and random generation are in the independent xptls
# library.  It and DeuceSSH share only the selected crypto provider.
target_link_libraries(syncterm xptls::static)

# Botan backend pulls C++ translation units into libdeucessh.a and into
# syncterm (the Botan xptls backend); the final link needs
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
target_include_directories(syncterm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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
add_executable(theme_test theme_test.c theme.c theme_cloud.c)
require_libs(theme_test hash xpdev)
target_include_directories(theme_test PRIVATE ../conio ../sbbs3)
target_compile_definitions(theme_test PRIVATE
	SDL_MAIN_HANDLED
	SYNCTERM_THEME_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/themes")
add_test(NAME theme_test COMMAND theme_test)
add_executable(xfer_queue_test xfer_queue_test.c xfer_queue.c)
add_test(NAME xfer_queue_test COMMAND xfer_queue_test)
add_executable(xmodem_cancel_test
	xmodem_cancel_test.c
	xfer_recv.c
	../sbbs3/xmodem.c)
require_libs(xmodem_cancel_test hash xpdev)
target_include_directories(xmodem_cancel_test PRIVATE ../sbbs3)
target_compile_definitions(xmodem_cancel_test PRIVATE SDL_MAIN_HANDLED)
add_test(NAME xmodem_cancel_test COMMAND xmodem_cancel_test)
add_executable(conn_buffer_test conn_buffer_test.c conn_buf.c)
require_libs(conn_buffer_test xpdev)
target_include_directories(conn_buffer_test PRIVATE ../conio)
target_compile_definitions(conn_buffer_test PRIVATE SDL_MAIN_HANDLED)
add_test(NAME conn_buffer_test COMMAND conn_buffer_test)
if(NOT WITHOUT_CRYPTO)
	add_executable(ini_crypt_test
		ini_crypt_test.c ini_crypt.c
		legacy_ciphers/idea.c legacy_ciphers/rc2.c)
	target_link_libraries(ini_crypt_test PRIVATE xptls::static xpdev)
	target_include_directories(ini_crypt_test PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../xpdev)
	if(XP_CRYPTO_BACKEND STREQUAL "Botan")
		set_target_properties(ini_crypt_test PROPERTIES LINKER_LANGUAGE CXX)
	endif()
	add_test(NAME ini_crypt_test COMMAND ini_crypt_test)
endif()
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
