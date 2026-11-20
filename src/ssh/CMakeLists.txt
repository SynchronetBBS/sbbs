cmake_minimum_required(VERSION 3.22)
project(deucessh VERSION 1.0.0 LANGUAGES C)
# Independent of the package/API release major; retain while published ABIs remain.
set(DEUCESSH_ABI_MAJOR 1)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeuceSSHCStandard.cmake")

# Default to Release for single-config generators when the user hasn't
# picked a build type.  Multi-config generators (Visual Studio, Xcode)
# select per build via CMAKE_CONFIGURATION_TYPES, so leave them alone.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING
		"Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		Debug Release RelWithDebInfo MinSizeRel)
endif()

option(DEUCESSH_BUILD_STATIC "Build static library" ON)
option(DEUCESSH_BUILD_SHARED "Build shared library" ON)
set(DEUCESSH_BOTAN_MIN_VERSION "3.13.0" CACHE STRING
	"Minimum supported Botan version")

if(NOT DEUCESSH_BUILD_STATIC AND NOT DEUCESSH_BUILD_SHARED)
	message(FATAL_ERROR "At least one of DEUCESSH_BUILD_STATIC or DEUCESSH_BUILD_SHARED must be ON")
endif()

set(DEUCESSH_CRYPTO_BACKEND "" CACHE STRING
	"Crypto backend: Botan, OpenSSL, or empty for auto-detect (prefer Botan)")
set_property(CACHE DEUCESSH_CRYPTO_BACKEND
	PROPERTY STRINGS "" "Botan" "OpenSSL")

if(DEUCESSH_CRYPTO_BACKEND STREQUAL "")
	# Auto-detect: try Botan first, then OpenSSL
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(DEUCESSH_BOTAN QUIET IMPORTED_TARGET
			"botan-3>=${DEUCESSH_BOTAN_MIN_VERSION}")
	endif()
	if(DEUCESSH_BOTAN_FOUND)
		set(DEUCESSH_CRYPTO_BACKEND "Botan")
		message(STATUS "Auto-detected crypto backend: Botan")
	else()
		find_package(OpenSSL 3.0 QUIET)
		if(OpenSSL_FOUND)
			set(DEUCESSH_CRYPTO_BACKEND "OpenSSL")
			message(STATUS "Auto-detected crypto backend: OpenSSL")
		else()
			message(FATAL_ERROR
				"No crypto backend found. Install Botan "
				"${DEUCESSH_BOTAN_MIN_VERSION}+ or OpenSSL 3.0+, "
				"or set -DDEUCESSH_CRYPTO_BACKEND=Botan|OpenSSL")
		endif()
	endif()
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	# Parent project may pre-create an IMPORTED target (e.g. when Botan
	# is built via ExternalProject and its pkg-config file doesn't exist
	# at configure time).  BOTAN3_VENDORED_TARGET is the cache hint.
	if(NOT BOTAN3_VENDORED_TARGET)
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(DEUCESSH_BOTAN REQUIRED IMPORTED_TARGET
			"botan-3>=${DEUCESSH_BOTAN_MIN_VERSION}")
	endif()
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	find_package(OpenSSL 3.0 REQUIRED)
else()
	message(FATAL_ERROR
		"Unknown crypto backend: ${DEUCESSH_CRYPTO_BACKEND} "
		"(expected Botan, OpenSSL, or empty)")
endif()

if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	if(BOTAN3_VENDORED_TARGET)
		set(DSSH_CRYPTO_LIB_PRIVATE ${BOTAN3_VENDORED_TARGET})
		set(DSSH_CRYPTO_LIB_INTERFACE
			$<BUILD_INTERFACE:${BOTAN3_VENDORED_TARGET}>
			$<INSTALL_INTERFACE:PkgConfig::DEUCESSH_BOTAN_BINARY>)
		get_target_property(DSSH_CRYPTO_INCLUDE
		    ${BOTAN3_VENDORED_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
	else()
		set(DSSH_CRYPTO_LIB_PRIVATE PkgConfig::DEUCESSH_BOTAN)
		set(DSSH_CRYPTO_LIB_INTERFACE
			$<BUILD_INTERFACE:PkgConfig::DEUCESSH_BOTAN>
			$<INSTALL_INTERFACE:PkgConfig::DEUCESSH_BOTAN_BINARY>)
		set(DSSH_CRYPTO_INCLUDE ${DEUCESSH_BOTAN_INCLUDE_DIRS})
	endif()
	set(DSSH_CRYPTO_IMPL crypto/botan.cpp)
	# Keep the API floor; compatible Botan releases need no exact version pin.
	# Precompiled consumers still require a compatible provider ABI.
	set(DSSH_CONFIG_FIND_DEPENDENCY "find_dependency(PkgConfig)\npkg_check_modules(DEUCESSH_BOTAN_BINARY REQUIRED IMPORTED_TARGET \"botan-3>=${DEUCESSH_BOTAN_MIN_VERSION}\")")
	set(DSSH_PC_REQUIRES_PRIVATE "botan-3 >= ${DEUCESSH_BOTAN_MIN_VERSION}")
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	set(DSSH_CRYPTO_LIB_PRIVATE OpenSSL::Crypto)
	set(DSSH_CRYPTO_LIB_INTERFACE OpenSSL::Crypto)
	set(DSSH_CRYPTO_INCLUDE ${OPENSSL_INCLUDE_DIR})
	set(DSSH_CRYPTO_IMPL crypto/openssl.c)
	set(DSSH_CONFIG_FIND_DEPENDENCY "find_dependency(OpenSSL 3.0)")
	set(DSSH_PC_REQUIRES_PRIVATE "libcrypto >= 3.0")
endif()

# Botan backend uses C++ sources; enable CXX only when needed
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	enable_language(CXX)
endif()

include(CheckCSourceCompiles)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeuceSSHCheckState.cmake")

# The deadline helpers classify long double values. Some C runtimes
# implement isfinite for that type in a separate math library.
function(dssh_find_math_link_library)
	dssh_push_platform_check_state()
	set(probe [[
		#include <math.h>
		int main(void) {
			volatile long double value = 1.0L;
			return isfinite(value) ? 0 : 1;
		}
	]])
	check_c_source_compiles("${probe}" DSSH_LONG_DOUBLE_ISFINITE_IN_LIBC)
	if(DSSH_LONG_DOUBLE_ISFINITE_IN_LIBC)
		set(DSSH_MATH_LINK_LIB "" PARENT_SCOPE)
	else()
		set(CMAKE_REQUIRED_LIBRARIES m)
		check_c_source_compiles("${probe}" DSSH_LONG_DOUBLE_ISFINITE_IN_LIBM)
		if(NOT DSSH_LONG_DOUBLE_ISFINITE_IN_LIBM)
			message(FATAL_ERROR "DeuceSSH requires isfinite for long double in <math.h>.")
		endif()
		set(DSSH_MATH_LINK_LIB m PARENT_SCOPE)
	endif()
	cmake_pop_check_state()
endfunction()
dssh_find_math_link_library()

# Match the POSIX namespace used by library sources and tests. Older Apple
# headers hide timespec_get / TIME_UTC in this namespace even in C11 mode.
# The new cache key intentionally discards the old, unrestricted probe result.
function(dssh_find_timespec_get)
	dssh_push_platform_check_state()
	if(NOT WIN32)
		list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_POSIX_C_SOURCE=200809L)
	endif()
	check_c_source_compiles("
		#include <time.h>
		int main(void) {
			struct timespec ts;
			int (*get_time)(struct timespec *, int) = timespec_get;
			return get_time(&ts, TIME_UTC) == TIME_UTC ? 0 : 1;
		}
	" DSSH_HAVE_TIMESPEC_GET)
	if(NOT DSSH_HAVE_TIMESPEC_GET AND UNIX)
		check_c_source_compiles("
			#include <time.h>
			int main(void) {
				struct timespec ts;
				int (*get_time)(clockid_t, struct timespec *) = clock_gettime;
				return get_time(CLOCK_REALTIME, &ts);
			}
		" DSSH_HAVE_CLOCK_GETTIME)
	endif()
	cmake_pop_check_state()
endfunction()
dssh_find_timespec_get()

set(DSSH_TIME_SHIM_SOURCES "")
set(DSSH_TIME_SHIM_DEFINITIONS "")
if(NOT DSSH_HAVE_TIMESPEC_GET)
	if(UNIX AND DSSH_HAVE_CLOCK_GETTIME)
		set(DSSH_TIME_SHIM_SOURCES posix/timespec.c)
		set(DSSH_TIME_SHIM_DEFINITIONS DSSH_TIMESPEC_GET_SHIM)
		message(STATUS "Using POSIX timespec_get compatibility shim")
	else()
		message(FATAL_ERROR
			"DeuceSSH requires timespec_get / TIME_UTC in <time.h>, "
			"or POSIX clock_gettime(CLOCK_REALTIME) for its compatibility shim.")
	endif()
endif()

# C11 threads: glibc (2.28+), recent macOS SDKs, and FreeBSD 12+ have
# them.  FreeBSD used to ship them in a separate libstdthreads.  Older
# macOS and NetBSD have pthread but no <threads.h>; MinGW-w64 has
# neither.  Probe libc first, then libstdthreads, then fall back to a
# platform shim.
function(dssh_find_threads_in_libc)
	dssh_push_platform_check_state()
	# Header availability alone does not establish a native implementation.
	# A parent may use static-library try-compiles or unrelated probe libraries;
	# neither may hide this executable's actual C11 thread link dependency.
	check_c_source_compiles("
		#include <threads.h>
		int main(void) { mtx_t m; return mtx_init(&m, mtx_plain); }
	" DSSH_THREADS_IN_LIBC_LINKS)
	cmake_pop_check_state()
endfunction()
# Use a fresh result key rather than trust the former unrestricted probe cache.
dssh_find_threads_in_libc()
set(DSSH_THREADS_SHIM_SOURCES "")
set(DSSH_THREADS_SHIM_INCLUDE "")
if(NOT DSSH_THREADS_IN_LIBC_LINKS)
	find_library(STDTHREADS_LIB stdthreads)
	if(STDTHREADS_LIB)
		set(THREADS_LINK_LIB ${STDTHREADS_LIB})
	elseif(WIN32)
		# Prepend win32/ to the include path so <threads.h> resolves
		# to our shim rather than the missing system header.  Applies
		# to MinGW-w64 and MSVC alike — both lack libc <threads.h> but
		# provide the CRITICAL_SECTION / CONDITION_VARIABLE /
		# _beginthreadex primitives the shim wraps.
		set(DSSH_THREADS_SHIM_SOURCES win32/threads.c)
		set(DSSH_THREADS_SHIM_INCLUDE
		    ${CMAKE_CURRENT_SOURCE_DIR}/win32)
		set(THREADS_LINK_LIB "")
	elseif(UNIX)
		# POSIX pthread shim — macOS (pre-14 SDK), older NetBSD, any
		# libc without C11 threads.  Requires pthread but CMake's
		# Threads::Threads already pulls it in for us below.
		set(DSSH_THREADS_SHIM_SOURCES posix/threads.c)
		set(DSSH_THREADS_SHIM_INCLUDE
		    ${CMAKE_CURRENT_SOURCE_DIR}/posix)
		find_package(Threads REQUIRED)
		set(THREADS_LINK_LIB Threads::Threads)
		string(APPEND DSSH_CONFIG_FIND_DEPENDENCY "\nfind_dependency(Threads)")
	else()
		message(FATAL_ERROR
			"C11 <threads.h> not found in libc or libstdthreads, "
			"and no platform-specific shim applies")
	endif()
else()
	set(THREADS_LINK_LIB "")
endif()

# Algorithm modules -- backend-specific
if(WIN32)
	set(DSSH_PRIVATE_KEY_FILE_SOURCE key_algo/private-key-file-win32.c)
	set(DSSH_PRIVATE_KEY_FILE_LIB advapi32)
	set(DSSH_PC_LIBS_PRIVATE "-ladvapi32")
else()
	set(DSSH_PRIVATE_KEY_FILE_SOURCE key_algo/private-key-file-posix.c)
	# Strict C modes do not implicitly expose strdup() or the POSIX APIs used
	# by the file writer and the pthread-backed C11 threads shim.
	set_source_files_properties(ssh-auth.c ssh-conn.c
		key_algo/private-key-file-posix.c posix/threads.c posix/timespec.c
		PROPERTIES COMPILE_DEFINITIONS _POSIX_C_SOURCE=200809L)
	# NetBSD declares fchmod() in its XSI namespace, not its POSIX namespace.
	set_property(SOURCE key_algo/private-key-file-posix.c APPEND PROPERTY
		COMPILE_DEFINITIONS _XOPEN_SOURCE=700)
	set(DSSH_PRIVATE_KEY_FILE_LIB "")
	set(DSSH_PC_LIBS_PRIVATE "")
endif()

if(THREADS_LINK_LIB AND STDTHREADS_LIB)
	string(APPEND DSSH_PC_LIBS_PRIVATE " ${STDTHREADS_LIB}")
elseif(THREADS_LINK_LIB STREQUAL "Threads::Threads" AND CMAKE_THREAD_LIBS_INIT)
	string(REPLACE ";" " " DSSH_PC_THREAD_LIBS
		"${CMAKE_THREAD_LIBS_INIT}")
	string(APPEND DSSH_PC_LIBS_PRIVATE " ${DSSH_PC_THREAD_LIBS}")
endif()

if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	set(DSSH_PC_CXX_RUNTIME_LIBRARIES
		${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
	foreach(lib IN LISTS CMAKE_C_IMPLICIT_LINK_LIBRARIES)
		list(REMOVE_ITEM DSSH_PC_CXX_RUNTIME_LIBRARIES "${lib}")
	endforeach()
	if(DSSH_MATH_LINK_LIB)
		list(REMOVE_ITEM DSSH_PC_CXX_RUNTIME_LIBRARIES "${DSSH_MATH_LINK_LIB}")
	endif()
	foreach(lib IN LISTS DSSH_PC_CXX_RUNTIME_LIBRARIES)
		if(IS_ABSOLUTE "${lib}" OR "${lib}" MATCHES "^-")
			string(APPEND DSSH_PC_LIBS_PRIVATE " ${lib}")
		else()
			string(APPEND DSSH_PC_LIBS_PRIVATE " -l${lib}")
		endif()
	endforeach()
endif()
if(DSSH_MATH_LINK_LIB)
	string(APPEND DSSH_PC_LIBS_PRIVATE " -l${DSSH_MATH_LINK_LIB}")
endif()
string(STRIP "${DSSH_PC_LIBS_PRIVATE}" DSSH_PC_LIBS_PRIVATE)

if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	set(DSSH_ALG_SOURCES
		kex/curve25519-sha256-openssl.c
		kex/dh-gex-sha256-openssl.c
		kex/sntrup761x25519-sha512-openssl.c
		key_algo/ssh-ed25519-openssl.c
		key_algo/rsa-sha2-256-openssl.c
		key_algo/rsa-sha2-512-openssl.c
		enc/aes256-ctr-openssl.c
		enc/aes128-cbc-openssl.c
		mac/hmac-sha2-256-openssl.c
		mac/hmac-sha2-512-openssl.c
	)
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	set(DSSH_ALG_SOURCES
		kex/curve25519-sha256-botan.c
		kex/curve25519-sha256-botan.cpp
		kex/dh-gex-sha256-botan.c
		kex/dh-gex-sha256-botan.cpp
		kex/sntrup761x25519-sha512-botan.c
		kex/sntrup761x25519-sha512-botan.cpp
		key_algo/ssh-ed25519-botan.c
		key_algo/ssh-ed25519-botan.cpp
		key_algo/rsa-sha2-256-botan.c
		key_algo/rsa-sha2-256-botan.cpp
		key_algo/rsa-sha2-512-botan.c
		key_algo/rsa-sha2-512-botan.cpp
		key_algo/native-key-pool.c
		key_algo/native-key-botan.cpp
		enc/aes256-ctr-botan.c
		enc/aes256-ctr-botan.cpp
		enc/aes128-cbc-botan.c
		enc/aes128-cbc-botan.cpp
		mac/hmac-sha2-256-botan.c
		mac/hmac-sha2-256-botan.cpp
		mac/hmac-sha2-512-botan.c
		mac/hmac-sha2-512-botan.cpp
	)
endif()

set(DEUCESSH_SOURCES
	ssh-trans.c ssh.c ssh-auth.c ssh-conn.c
	${DSSH_CRYPTO_IMPL}
	${DSSH_PRIVATE_KEY_FILE_SOURCE}
	key_algo/signature-result.c
	key_algo/client-key.c
	kex/workspace.c
	kex/curve25519-sha256.c
	kex/hybrid-pq-kex.c
	kex/mlkem768x25519-sha256.c
	kex/mlkem768.c
	kex/dh-gex-sha256.c
	kex/dh-fixed.c
	kex/dh-gex-groups.c
	kex/sntrup761.c
	kex/sntrup761_optblocker.c
	${DSSH_ALG_SOURCES}
	${DSSH_THREADS_SHIM_SOURCES}
	${DSSH_TIME_SHIM_SOURCES}
	enc/none.c
	mac/none.c
	comp/none.c
)

set(DEUCESSH_PUBLIC_HEADERS
	deucessh.h deucessh-auth.h deucessh-conn.h
	deucessh-algorithms.h deucessh-crypto.h deucessh-kex.h
	deucessh-key-algo.h deucessh-enc.h deucessh-mac.h
	deucessh-comp.h deucessh-lang.h deucessh-portable.h deucessh-result.h
	deucessh-abi.h
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeuceSSHOptions.cmake")
dssh_configure_options()

include(GNUInstallDirs)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeuceSSHABI.cmake")
dssh_prepare_abi()

set(DEUCESSH_INSTALL_TARGETS "")

# --- Static library ---

if(DEUCESSH_BUILD_STATIC)
	add_library(deucessh_static STATIC ${DEUCESSH_SOURCES})
	add_library(deucessh::static ALIAS deucessh_static)
	set_property(TARGET deucessh_static PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET deucessh_static PROPERTY C_STANDARD_REQUIRED ON)
	if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET deucessh_static PROPERTY CXX_STANDARD 20)
		set_property(TARGET deucessh_static PROPERTY CXX_STANDARD_REQUIRED ON)
	endif()
	set(DSSH_STATIC_OUTPUT_NAME deucessh)
	if(MSVC)
		# The DLL import library also uses deucessh.lib.
		set(DSSH_STATIC_OUTPUT_NAME deucessh-static)
	endif()
	set_property(TARGET deucessh_static PROPERTY OUTPUT_NAME ${DSSH_STATIC_OUTPUT_NAME})
	set_property(TARGET deucessh_static PROPERTY POSITION_INDEPENDENT_CODE ON)
	target_include_directories(deucessh_static
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
			$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/deucessh>)
	if(DSSH_THREADS_SHIM_INCLUDE)
		target_include_directories(deucessh_static BEFORE
			PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
	endif()
	target_include_directories(deucessh_static SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(deucessh_static
		INTERFACE ${DSSH_CRYPTO_LIB_INTERFACE} ${THREADS_LINK_LIB}
			${DSSH_PRIVATE_KEY_FILE_LIB} ${DSSH_PC_CXX_RUNTIME_LIBRARIES} ${DSSH_MATH_LINK_LIB})
	target_compile_definitions(deucessh_static
		PRIVATE DSSH_VERSION_STRING="${PROJECT_VERSION}"
		${DSSH_TIME_SHIM_DEFINITIONS}
		$<$<CONFIG:Release>:NDEBUG>)
	target_compile_options(deucessh_static PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	if(BOTAN3_VENDORED_BUILD_TARGET)
		add_dependencies(deucessh_static ${BOTAN3_VENDORED_BUILD_TARGET})
	endif()
	list(APPEND DEUCESSH_INSTALL_TARGETS deucessh_static)
endif()

# --- Shared library ---

if(DEUCESSH_BUILD_SHARED)
	add_library(deucessh_shared SHARED ${DEUCESSH_SOURCES})
	add_library(deucessh::shared ALIAS deucessh_shared)
	set_property(TARGET deucessh_shared PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET deucessh_shared PROPERTY C_STANDARD_REQUIRED ON)
	if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET deucessh_shared PROPERTY CXX_STANDARD 20)
		set_property(TARGET deucessh_shared PROPERTY CXX_STANDARD_REQUIRED ON)
	endif()
	set_property(TARGET deucessh_shared PROPERTY OUTPUT_NAME deucessh)
	set_property(TARGET deucessh_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
	set_property(TARGET deucessh_shared PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS OFF)
	set_property(TARGET deucessh_shared PROPERTY VERSION ${PROJECT_VERSION})
	set_property(TARGET deucessh_shared PROPERTY SOVERSION ${DEUCESSH_ABI_MAJOR})
	target_include_directories(deucessh_shared
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
			$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/deucessh>)
	if(DSSH_THREADS_SHIM_INCLUDE)
		target_include_directories(deucessh_shared BEFORE
			PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
	endif()
	target_include_directories(deucessh_shared SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(deucessh_shared
		PRIVATE ${DSSH_CRYPTO_LIB_PRIVATE} ${THREADS_LINK_LIB}
			${DSSH_PRIVATE_KEY_FILE_LIB} ${DSSH_MATH_LINK_LIB})
	target_compile_definitions(deucessh_shared
		PRIVATE
			DSSH_SHARED
			${DSSH_TIME_SHIM_DEFINITIONS}
			DSSH_VERSION_STRING="${PROJECT_VERSION}"
			$<$<CONFIG:Release>:NDEBUG>)
	target_compile_options(deucessh_shared PRIVATE
		${DEUCESSH_COMPILE_OPTIONS} ${DEUCESSH_SHARED_COMPILE_OPTIONS})
	if(BOTAN3_VENDORED_BUILD_TARGET)
		add_dependencies(deucessh_shared ${BOTAN3_VENDORED_BUILD_TARGET})
	endif()
	# ELF shared library hardening and optimization (Linux, FreeBSD)
	if(UNIX AND NOT APPLE)
		target_link_options(deucessh_shared PRIVATE
			${DEUCESSH_LINK_OPTIONS})
	endif()
	dssh_apply_abi(deucessh_shared)
	if(WIN32)
		target_compile_definitions(deucessh_shared INTERFACE DSSH_SHARED_IMPORT)
	endif()
	list(APPEND DEUCESSH_INSTALL_TARGETS deucessh_shared)
endif()

# --- Install ---

include(CMakePackageConfigHelpers)

set(DSSH_INSTALL_CONFIGURATIONS "${CMAKE_CONFIGURATION_TYPES}")
if(NOT DSSH_INSTALL_CONFIGURATIONS)
	set(DSSH_INSTALL_CONFIGURATIONS "${CMAKE_BUILD_TYPE}")
endif()
configure_file(cmake/DeuceSSHInstallConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/DeuceSSHInstallConfig.cmake @ONLY)
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/DeuceSSHInstallConfig.cmake)

install(TARGETS ${DEUCESSH_INSTALL_TARGETS}
	EXPORT deucesshTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${DEUCESSH_PUBLIC_HEADERS}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/deucessh)

install(FILES LICENSE.md
	DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/deucessh)

install(EXPORT deucesshTargets
	FILE deucesshTargets.cmake
	NAMESPACE deucessh::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/deucessh)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/deucesshConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion)

configure_file(deucesshConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/deucesshConfig.cmake @ONLY)

# Derive the pkg-config prefix from the installed .pc file so packages remain
# usable after `cmake --install --prefix ...` and when an MSYS/MinGW package is
# moved to a different Windows installation root.
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
	set(DSSH_PC_PREFIX "${CMAKE_INSTALL_PREFIX}")
	set(DSSH_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
	file(RELATIVE_PATH DSSH_PC_PREFIX_FROM_PCDIR
		"${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
		"${CMAKE_CURRENT_BINARY_DIR}")
	string(REGEX REPLACE "/$" "" DSSH_PC_PREFIX_FROM_PCDIR
		"${DSSH_PC_PREFIX_FROM_PCDIR}")
	set(DSSH_PC_PREFIX
		"\${pcfiledir}/${DSSH_PC_PREFIX_FROM_PCDIR}")
	set(DSSH_PC_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
	set(DSSH_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/deucessh")
else()
	set(DSSH_PC_INCLUDEDIR
		"\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}/deucessh")
endif()

if(DEUCESSH_BUILD_STATIC)
	set(DSSH_PC_STATIC_LIBRARY
		"${CMAKE_STATIC_LIBRARY_PREFIX}${DSSH_STATIC_OUTPUT_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

if(DEUCESSH_BUILD_SHARED)
	set(DSSH_PC_REQUIRES_FIELD "Requires.private")
	set(DSSH_PC_MAIN_LIBS "-L\${libdir} -ldeucessh")
	set(DSSH_PC_LIBS_PRIVATE_LINE
		"Libs.private: ${DSSH_PC_LIBS_PRIVATE}")
else()
	set(DSSH_PC_REQUIRES_FIELD "Requires")
	set(DSSH_PC_MAIN_LIBS
		"\${libdir}/${DSSH_PC_STATIC_LIBRARY} ${DSSH_PC_LIBS_PRIVATE}")
	set(DSSH_PC_LIBS_PRIVATE_LINE "")
endif()

configure_file(deucessh.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/deucessh.pc @ONLY)

if(DEUCESSH_BUILD_STATIC)
	configure_file(deucessh-static.pc.in
		${CMAKE_CURRENT_BINARY_DIR}/deucessh-static.pc @ONLY)
endif()

set(DEUCESSH_PC_FILES ${CMAKE_CURRENT_BINARY_DIR}/deucessh.pc)
if(DEUCESSH_BUILD_STATIC)
	list(APPEND DEUCESSH_PC_FILES
		${CMAKE_CURRENT_BINARY_DIR}/deucessh-static.pc)
endif()

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/deucesshConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/deucesshConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/deucessh)

install(FILES ${DEUCESSH_PC_FILES}
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# --- Example executables (not installed) ---
#
# Guarded with EXISTS so a source distribution that ships CMakeLists.txt but
# strips examples/ (e.g. SyncTERM's source tarball) doesn't fail configure on
# the missing add_executable() inputs. The examples are EXCLUDE_FROM_ALL and
# never built as part of a normal build anyway.

if(DEUCESSH_BUILD_STATIC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/client.c")
	add_executable(client EXCLUDE_FROM_ALL examples/client.c)
	target_link_libraries(client PRIVATE deucessh_static)

	add_executable(server EXCLUDE_FROM_ALL examples/server.c)
	target_link_libraries(server PRIVATE deucessh_static)

	add_executable(kbi-game-server EXCLUDE_FROM_ALL examples/kbi-game-server.c)
	target_link_libraries(kbi-game-server PRIVATE deucessh_static)

endif()

# --- Test suite ---

option(DEUCESSH_BUILD_ZLIB_EXAMPLE "Build optional external zlib module example" OFF)
if(DEUCESSH_BUILD_ZLIB_EXAMPLE)
	find_package(ZLIB REQUIRED)
	add_library(dssh_example_zlib STATIC examples/zlib-comp.c)
	set_property(TARGET dssh_example_zlib PROPERTY C_STANDARD 17)
	set_property(TARGET dssh_example_zlib PROPERTY C_STANDARD_REQUIRED ON)
	target_compile_options(dssh_example_zlib PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_include_directories(dssh_example_zlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/examples")
	if(DEUCESSH_BUILD_STATIC)
		target_link_libraries(dssh_example_zlib PRIVATE deucessh_static ZLIB::ZLIB)
	else()
		target_link_libraries(dssh_example_zlib PRIVATE deucessh_shared ZLIB::ZLIB)
	endif()
endif()

option(DEUCESSH_BUILD_TESTS "Build test suite" OFF)
option(DEUCESSH_TEST_OPENSSH "Build OpenSSH interop tests" OFF)
option(DEUCESSH_TEST_SYNCHRONET "Build Synchronet interop tests" OFF)

if(DEUCESSH_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)

	if(TARGET deucessh_test_lib)
		add_executable(kex_test EXCLUDE_FROM_ALL test/kex_test.c)
		target_link_libraries(kex_test PRIVATE deucessh_test_lib)
	endif()
endif()
