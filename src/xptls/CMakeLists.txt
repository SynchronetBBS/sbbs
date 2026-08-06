cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(xptls LANGUAGES C)
	include(CTest)
	if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
		set(CMAKE_BUILD_TYPE Release CACHE STRING
			"Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
		set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
			Debug Release RelWithDebInfo MinSizeRel)
	endif()
endif()

find_package(PkgConfig QUIET)

# xptls uses xpdev's socket/path portability API and its monotonic timer.
# Standalone/test builds add xpdev for that dependency; raw archive builds
# such as SyncTERM's GNUmake sub-build can use headers/compile definitions
# only because the parent links its own libxpdev_mt.a.
option(XPTLS_BUILD_XPDEV "Build/propagate xpdev objects to CMake consumers" ON)
set(XPTLS_HAS_XPDEV_OBJECTS FALSE)
if(TARGET xpdev)
	set(XPTLS_XPDEV_TARGET xpdev)
	set(XPTLS_HAS_XPDEV_OBJECTS TRUE)
elseif(XPTLS_BUILD_XPDEV)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../xpdev xpdev EXCLUDE_FROM_ALL)
	set(XPTLS_XPDEV_TARGET xpdev)
	set(XPTLS_HAS_XPDEV_OBJECTS TRUE)
else()
	add_library(xptls_xpdev_usage INTERFACE)
	target_include_directories(xptls_xpdev_usage INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/../xpdev)
	if(WIN32)
		target_compile_definitions(xptls_xpdev_usage INTERFACE
			_WIN32 _WIN32_WINNT=0x0600 WINVER=0x0600
			MSVCRT_VERSION=0x0800 _WIN32_IE=0x0600)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
		target_compile_definitions(xptls_xpdev_usage INTERFACE
			__unix__ USE_SNPRINTF)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		target_compile_definitions(xptls_xpdev_usage INTERFACE
			__unix__ __DARWIN__ USE_SNPRINTF)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_compile_definitions(xptls_xpdev_usage INTERFACE
			_DEFAULT_SOURCE _BSD_SOURCE SPEED_MACROS_ONLY _GNU_SOURCE
			_FILE_OFFSET_BITS=64)
	endif()
	if(MSVC)
		target_compile_options(xptls_xpdev_usage INTERFACE
			$<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>)
	endif()
	set(XPTLS_XPDEV_TARGET xptls_xpdev_usage)
endif()

# CRYPTO_BACKEND is the neutral parent-build selector.  Keep accepting the
# original component-specific variable for direct/older xptls builds.
set(CRYPTO_BACKEND "" CACHE STRING
	"Shared crypto backend: Botan, OpenSSL, or empty for auto-detect")
set_property(CACHE CRYPTO_BACKEND PROPERTY STRINGS "" "Botan" "OpenSSL")

if(CRYPTO_BACKEND AND DEFINED XP_CRYPTO_BACKEND
		AND NOT XP_CRYPTO_BACKEND STREQUAL "none"
		AND NOT XP_CRYPTO_BACKEND STREQUAL CRYPTO_BACKEND)
	message(FATAL_ERROR
		"Conflicting crypto backends: CRYPTO_BACKEND=${CRYPTO_BACKEND}, "
		"XP_CRYPTO_BACKEND=${XP_CRYPTO_BACKEND}")
endif()

if(NOT DEFINED XP_CRYPTO_BACKEND OR XP_CRYPTO_BACKEND STREQUAL "")
	if(CRYPTO_BACKEND)
		set(XP_CRYPTO_BACKEND "${CRYPTO_BACKEND}" CACHE STRING
			"xptls crypto backend" FORCE)
	else()
		if(PkgConfig_FOUND)
			pkg_check_modules(SYS_BOTAN3 QUIET "botan-3>=3.6")
		endif()
		if(SYS_BOTAN3_FOUND)
			set(XP_CRYPTO_BACKEND "Botan" CACHE STRING
				"xptls crypto backend" FORCE)
		else()
			find_package(OpenSSL 3.0 QUIET)
			if(OpenSSL_FOUND)
				set(XP_CRYPTO_BACKEND "OpenSSL" CACHE STRING
					"xptls crypto backend" FORCE)
			else()
				set(XP_CRYPTO_BACKEND "none" CACHE STRING
					"xptls crypto backend" FORCE)
			endif()
		endif()
	endif()
endif()
set_property(CACHE XP_CRYPTO_BACKEND PROPERTY STRINGS "Botan" "OpenSSL" "none")

message(STATUS "xptls: crypto backend = ${XP_CRYPTO_BACKEND}")

if(XP_CRYPTO_BACKEND STREQUAL "Botan")
	set(XPTLS_PROVIDER_SOURCES
		xp_tls_botan3.cpp xp_ca_botan3.cpp xp_digest_botan3.cpp
		xp_crypto_botan3.cpp xp_cipher_botan3.cpp xp_kdf_botan3.cpp)
elseif(XP_CRYPTO_BACKEND STREQUAL "OpenSSL")
	set(XPTLS_PROVIDER_SOURCES
		xp_tls_openssl.c xp_ca_openssl.c xp_digest_openssl.c
		xp_crypto_openssl.c xp_cipher_openssl.c xp_kdf_openssl.c
		xp_key_pkcs11_openssl.c)
elseif(XP_CRYPTO_BACKEND STREQUAL "none")
	set(XPTLS_PROVIDER_SOURCES
		xp_tls_none.c xp_ca_none.c xp_digest_none.c
		xp_crypto_none.c xp_cipher_none.c xp_kdf_none.c)
else()
	message(FATAL_ERROR
		"Unknown XP_CRYPTO_BACKEND: ${XP_CRYPTO_BACKEND} "
		"(expected Botan, OpenSSL, or none)")
endif()

if(XP_CRYPTO_BACKEND STREQUAL "Botan" OR BUILD_TESTING)
	enable_language(CXX)
endif()

set(XPTLS_SOURCES
	${XPTLS_PROVIDER_SOURCES}
	xp_crypto_common.c xp_key_store.c xp_keyset.c xp_tls_credentials.c)

add_library(xptls_static STATIC ${XPTLS_SOURCES})
add_library(xptls::static ALIAS xptls_static)
# Build-tree compatibility for existing consumers.  New consumers should use
# the namespaced target, matching DeuceSSH's target convention.
add_library(xptls ALIAS xptls_static)
set_target_properties(xptls_static PROPERTIES
	OUTPUT_NAME xptls
	C_STANDARD 11
	C_STANDARD_REQUIRED ON)

target_include_directories(xptls_static
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../xpdev
		$<TARGET_PROPERTY:${XPTLS_XPDEV_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)

# xpdev is an OBJECT library.  Linking it PRIVATE/PUBLIC here would copy every
# xpdev object into libxptls.a.  Copy only its compile usage requirements into
# xptls, then expose xpdev solely at the final-consumer interface.
get_target_property(XPTLS_XPDEV_COMPILE_DEFINITIONS
	${XPTLS_XPDEV_TARGET} INTERFACE_COMPILE_DEFINITIONS)
if(NOT XPTLS_XPDEV_COMPILE_DEFINITIONS)
	set(XPTLS_XPDEV_COMPILE_DEFINITIONS "")
endif()
if(WIN32)
	# xpdev still supports an XP-era Windows API floor, but MinGW's C++
	# std::mutex implementation exposes its condition-variable primitives only
	# for Vista or newer.  The Botan TLS backend uses those primitives.  Keep
	# xpdev's other feature definitions while setting xptls's own floor.
	list(FILTER XPTLS_XPDEV_COMPILE_DEFINITIONS EXCLUDE REGEX
		"^(_WIN32|_WIN32_WINNT=.*|WINVER=.*|MSVCRT_VERSION=.*|_WIN32_IE=.*)$")
	list(APPEND XPTLS_XPDEV_COMPILE_DEFINITIONS
		_WIN32 _WIN32_WINNT=0x0600 WINVER=0x0600
		MSVCRT_VERSION=0x0800 _WIN32_IE=0x0600)
endif()
target_compile_definitions(xptls_static PRIVATE
	${XPTLS_XPDEV_COMPILE_DEFINITIONS})
get_target_property(XPTLS_XPDEV_COMPILE_OPTIONS
	${XPTLS_XPDEV_TARGET} INTERFACE_COMPILE_OPTIONS)
if(NOT XPTLS_XPDEV_COMPILE_OPTIONS)
	set(XPTLS_XPDEV_COMPILE_OPTIONS "")
endif()
target_compile_options(xptls_static PRIVATE
	${XPTLS_XPDEV_COMPILE_OPTIONS})
if(MSVC)
	# Visual Studio generators evaluate target-wide COMPILE_LANGUAGE
	# expressions as CXX for mixed C/C++ targets.  Apply these directly to
	# the C sources that use <stdatomic.h>, so Botan builds do not lose them.
	set_property(SOURCE xp_tls_credentials.c APPEND PROPERTY COMPILE_OPTIONS
		/experimental:c11atomics /U__STDC_NO_ATOMICS__)
	if(XP_CRYPTO_BACKEND STREQUAL "OpenSSL")
		set_property(SOURCE xp_ca_openssl.c APPEND PROPERTY COMPILE_OPTIONS
			/experimental:c11atomics /U__STDC_NO_ATOMICS__)
	endif()
endif()
if(XPTLS_HAS_XPDEV_OBJECTS)
	target_link_libraries(xptls_static INTERFACE xpdev)
	target_sources(xptls_static INTERFACE $<TARGET_OBJECTS:xpdev>)
endif()

if(XP_CRYPTO_BACKEND STREQUAL "Botan")
	if(BOTAN3_VENDORED_TARGET)
		target_link_libraries(xptls_static PRIVATE ${BOTAN3_VENDORED_TARGET})
	else()
		if(NOT TARGET PkgConfig::SYS_BOTAN3)
			find_package(PkgConfig REQUIRED)
			pkg_check_modules(SYS_BOTAN3 REQUIRED IMPORTED_TARGET "botan-3>=3.6")
		endif()
		target_link_libraries(xptls_static PRIVATE PkgConfig::SYS_BOTAN3)
	endif()
	# Botan's optional public p11 headers include <pkcs11.h>; packaged Botan
	# installs that header one level below the ordinary include root.
	find_path(BOTAN3_PKCS11_INCLUDE_DIR pkcs11.h
		HINTS ${SYS_BOTAN3_INCLUDE_DIRS}
		PATH_SUFFIXES botan)
	if(BOTAN3_PKCS11_INCLUDE_DIR)
		target_include_directories(xptls_static PRIVATE
			${BOTAN3_PKCS11_INCLUDE_DIR})
	endif()
	set_target_properties(xptls_static PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		LINKER_LANGUAGE CXX)
elseif(XP_CRYPTO_BACKEND STREQUAL "OpenSSL")
	find_package(OpenSSL 3.0 REQUIRED)
	target_link_libraries(xptls_static PRIVATE OpenSSL::SSL OpenSSL::Crypto)
	if(PkgConfig_FOUND)
		pkg_check_modules(P11KIT QUIET IMPORTED_TARGET p11-kit-1)
	endif()
	if(TARGET PkgConfig::P11KIT)
		target_compile_definitions(xptls_static PRIVATE XPTLS_HAS_P11KIT=1)
		target_link_libraries(xptls_static PRIVATE PkgConfig::P11KIT)
	endif()
endif()

if(BUILD_TESTING AND NOT XP_CRYPTO_BACKEND STREQUAL "none")
	find_package(Threads REQUIRED)
	add_executable(xptls_ca_test test_xp_ca.c)
	target_link_libraries(xptls_ca_test PRIVATE xptls::static)
	set_property(TARGET xptls_ca_test PROPERTY C_STANDARD 11)
	set_property(TARGET xptls_ca_test PROPERTY C_STANDARD_REQUIRED ON)
	if(XP_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET xptls_ca_test PROPERTY LINKER_LANGUAGE CXX)
	endif()
	add_test(NAME xptls_ca_test COMMAND xptls_ca_test)

	add_executable(xptls_crypto_test test_xp_crypto.c)
	target_link_libraries(xptls_crypto_test PRIVATE xptls::static)
	set_property(TARGET xptls_crypto_test PROPERTY C_STANDARD 11)
	set_property(TARGET xptls_crypto_test PROPERTY C_STANDARD_REQUIRED ON)
	target_compile_definitions(xptls_crypto_test PRIVATE
		TEST_PKCS1_PEM="${CMAKE_CURRENT_SOURCE_DIR}/test_rsa_pkcs1.pem")
	if(XP_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET xptls_crypto_test PROPERTY LINKER_LANGUAGE CXX)
	endif()
	add_test(NAME xptls_crypto_test COMMAND xptls_crypto_test)

	add_executable(xptls_keyset_test test_xp_keyset.c)
	target_link_libraries(xptls_keyset_test PRIVATE xptls::static)
	set_property(TARGET xptls_keyset_test PROPERTY C_STANDARD 11)
	set_property(TARGET xptls_keyset_test PROPERTY C_STANDARD_REQUIRED ON)
	if(XP_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET xptls_keyset_test PROPERTY LINKER_LANGUAGE CXX)
	endif()
	add_test(NAME xptls_keyset_test COMMAND xptls_keyset_test)

	add_executable(xptls_tls_test test_xp_tls.cpp)
	target_link_libraries(xptls_tls_test PRIVATE xptls::static Threads::Threads)
	set_property(TARGET xptls_tls_test PROPERTY CXX_STANDARD 17)
	set_property(TARGET xptls_tls_test PROPERTY CXX_STANDARD_REQUIRED ON)
	add_test(NAME xptls_tls_test COMMAND xptls_tls_test)
elseif(BUILD_TESTING)
	add_executable(xptls_crypto_none_test test_xp_crypto_none.c)
	target_link_libraries(xptls_crypto_none_test PRIVATE xptls::static)
	set_property(TARGET xptls_crypto_none_test PROPERTY C_STANDARD 11)
	add_test(NAME xptls_crypto_none_test COMMAND xptls_crypto_none_test)
endif()
