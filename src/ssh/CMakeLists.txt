cmake_minimum_required(VERSION 3.21)
project(deucessh VERSION 0.1 LANGUAGES C)

option(DEUCESSH_BUILD_STATIC "Build static library" ON)
option(DEUCESSH_BUILD_SHARED "Build shared library" ON)

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
		pkg_check_modules(BOTAN3 QUIET IMPORTED_TARGET botan-3)
	endif()
	if(BOTAN3_FOUND)
		set(DEUCESSH_CRYPTO_BACKEND "Botan")
		message(STATUS "Auto-detected crypto backend: Botan")
	else()
		find_package(OpenSSL QUIET)
		if(OpenSSL_FOUND)
			set(DEUCESSH_CRYPTO_BACKEND "OpenSSL")
			message(STATUS "Auto-detected crypto backend: OpenSSL")
		else()
			message(FATAL_ERROR
				"No crypto backend found. Install Botan 3 or OpenSSL 3.0+, "
				"or set -DDEUCESSH_CRYPTO_BACKEND=Botan|OpenSSL")
		endif()
	endif()
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	# Parent project may pre-create an IMPORTED target (e.g. when Botan
	# is built via ExternalProject and its pkg-config file doesn't exist
	# at configure time).  BOTAN3_VENDORED_TARGET is the cache hint.
	if(NOT BOTAN3_VENDORED_TARGET)
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(BOTAN3 REQUIRED IMPORTED_TARGET botan-3)
	endif()
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	find_package(OpenSSL REQUIRED)
else()
	message(FATAL_ERROR
		"Unknown crypto backend: ${DEUCESSH_CRYPTO_BACKEND} "
		"(expected Botan, OpenSSL, or empty)")
endif()

if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	if(BOTAN3_VENDORED_TARGET)
		set(DSSH_CRYPTO_LIB_PRIVATE ${BOTAN3_VENDORED_TARGET})
		set(DSSH_CRYPTO_LIB_INTERFACE "")
		get_target_property(DSSH_CRYPTO_INCLUDE
		    ${BOTAN3_VENDORED_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
	else()
		set(DSSH_CRYPTO_LIB_PRIVATE PkgConfig::BOTAN3)
		set(DSSH_CRYPTO_LIB_INTERFACE ${BOTAN3_LDFLAGS})
		set(DSSH_CRYPTO_INCLUDE ${BOTAN3_INCLUDE_DIRS})
	endif()
	set(DSSH_CRYPTO_IMPL crypto/botan.cpp)
	set(DSSH_CONFIG_FIND_DEPENDENCY "find_dependency(PkgConfig)\npkg_check_modules(BOTAN3 REQUIRED IMPORTED_TARGET botan-3)")
	set(DSSH_PC_REQUIRES_PRIVATE "botan-3")
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	set(DSSH_CRYPTO_LIB_PRIVATE OpenSSL::Crypto)
	set(DSSH_CRYPTO_LIB_INTERFACE ${OPENSSL_CRYPTO_LIBRARIES})
	set(DSSH_CRYPTO_INCLUDE ${OPENSSL_INCLUDE_DIR})
	set(DSSH_CRYPTO_IMPL crypto/openssl.c)
	set(DSSH_CONFIG_FIND_DEPENDENCY "find_dependency(OpenSSL)")
	set(DSSH_PC_REQUIRES_PRIVATE "libcrypto")
endif()

# Botan backend uses C++ sources; enable CXX only when needed
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	enable_language(CXX)
endif()

# C11 threads: glibc (2.28+), recent macOS SDKs, and FreeBSD 12+ have
# them.  FreeBSD used to ship them in a separate libstdthreads.  Older
# macOS and NetBSD have pthread but no <threads.h>; MinGW-w64 has
# neither.  Probe libc first, then libstdthreads, then fall back to a
# platform shim.
include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c17")
check_c_source_compiles("
	#include <threads.h>
	int main(void) { mtx_t m; mtx_init(&m, mtx_plain); return 0; }
" HAVE_THREADS_IN_LIBC)
set(DSSH_THREADS_SHIM_SOURCES "")
set(DSSH_THREADS_SHIM_INCLUDE "")
if(NOT HAVE_THREADS_IN_LIBC)
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
	else()
		message(FATAL_ERROR
			"C11 <threads.h> not found in libc or libstdthreads, "
			"and no platform-specific shim applies")
	endif()
else()
	set(THREADS_LINK_LIB "")
endif()
# Backwards-compat alias: older code paths referenced the Win32-only
# name.  Keep both pointing at whichever shim is active.
set(DSSH_WIN32_THREADS_SOURCES ${DSSH_THREADS_SHIM_SOURCES})

# Algorithm modules -- backend-specific
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	set(DSSH_ALG_SOURCES
		kex/curve25519-sha256-openssl.c
		kex/dh-gex-sha256-openssl.c
		kex/sntrup761x25519-sha512-openssl.c
		kex/mlkem768x25519-sha256-openssl.c
		key_algo/ssh-ed25519-openssl.c
		key_algo/rsa-sha2-256-openssl.c
		key_algo/rsa-sha2-512-openssl.c
		enc/aes256-ctr-openssl.c
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
		kex/mlkem768x25519-sha256-botan.c
		kex/mlkem768x25519-sha256-botan.cpp
		key_algo/ssh-ed25519-botan.c
		key_algo/ssh-ed25519-botan.cpp
		key_algo/rsa-sha2-256-botan.c
		key_algo/rsa-sha2-256-botan.cpp
		key_algo/rsa-sha2-512-botan.c
		key_algo/rsa-sha2-512-botan.cpp
		enc/aes256-ctr-botan.c
		enc/aes256-ctr-botan.cpp
		mac/hmac-sha2-256-botan.c
		mac/hmac-sha2-256-botan.cpp
		mac/hmac-sha2-512-botan.c
		mac/hmac-sha2-512-botan.cpp
	)
endif()

set(DEUCESSH_SOURCES
	ssh-trans.c ssh.c ssh-auth.c ssh-conn.c
	${DSSH_CRYPTO_IMPL}
	kex/curve25519-sha256.c
	kex/hybrid-pq-kex.c
	kex/dh-gex-sha256.c
	kex/dh-gex-groups.c
	kex/sntrup761.c
	kex/sntrup761_optblocker.c
	kex/mlkem768.c
	${DSSH_ALG_SOURCES}
	${DSSH_WIN32_THREADS_SOURCES}
	enc/none.c
	mac/none.c
	comp/none.c
)

set(DEUCESSH_PUBLIC_HEADERS
	deucessh.h deucessh-auth.h deucessh-conn.h
	deucessh-algorithms.h deucessh-crypto.h deucessh-kex.h
	deucessh-key-algo.h deucessh-enc.h deucessh-mac.h
	deucessh-comp.h deucessh-portable.h
)

include(CheckCCompilerFlag)
include(CheckLinkerFlag)

# Probe for supported hardening flags (per OpenSSF Compiler Hardening Guide)
set(DEUCESSH_COMPILE_OPTIONS "")
set(DEUCESSH_LINK_OPTIONS "")

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
	# Baseline warnings (GCC 2.95+, Clang 4+).  -Werror=implicit,
	# -Werror=incompatible-pointer-types, -Werror=int-conversion are
	# C-only diagnostics; g++ rejects them with "not valid for C++".
	list(APPEND DEUCESSH_COMPILE_OPTIONS
		-Wall -Wpedantic -pedantic -Werror
		-Wformat -Wformat=2
		-Wimplicit-fallthrough
		-Werror=format-security
		$<$<COMPILE_LANGUAGE:C>:-Werror=implicit>
		$<$<COMPILE_LANGUAGE:C>:-Werror=incompatible-pointer-types>
		$<$<COMPILE_LANGUAGE:C>:-Werror=int-conversion>)

	# Strict probes: we compile with -Werror, so "flag accepted but
	# unused" (Apple Clang emits -Wunused-command-line-argument for
	# -fstack-clash-protection etc.) would still break the real build.
	# Push -Werror into CMAKE_REQUIRED_FLAGS so the probe matches the
	# actual compile discipline.
	set(CMAKE_REQUIRED_FLAGS "-std=c17 -Werror")

	check_c_compiler_flag(-Wconversion HAVE_WCONVERSION)
	if(HAVE_WCONVERSION)
		list(APPEND DEUCESSH_COMPILE_OPTIONS -Wconversion)
	endif()

	# GCC-only warnings
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		check_c_compiler_flag(-Wtrampolines HAVE_WTRAMPOLINES)
		if(HAVE_WTRAMPOLINES)
			list(APPEND DEUCESSH_COMPILE_OPTIONS -Wtrampolines)
		endif()
		check_c_compiler_flag(-Wbidi-chars=any HAVE_WBIDI)
		if(HAVE_WBIDI)
			list(APPEND DEUCESSH_COMPILE_OPTIONS -Wbidi-chars=any)
		endif()
		check_c_compiler_flag(-fzero-init-padding-bits=all HAVE_ZERO_PADDING)
		if(HAVE_ZERO_PADDING)
			list(APPEND DEUCESSH_COMPILE_OPTIONS
				-fzero-init-padding-bits=all)
		endif()
	endif()

	# Runtime protection — probe each flag
	check_c_compiler_flag(-fstack-protector-strong HAVE_STACK_PROTECTOR)
	if(HAVE_STACK_PROTECTOR)
		list(APPEND DEUCESSH_COMPILE_OPTIONS -fstack-protector-strong)
	endif()

	check_c_compiler_flag(-fstack-clash-protection HAVE_STACK_CLASH)
	if(HAVE_STACK_CLASH)
		list(APPEND DEUCESSH_COMPILE_OPTIONS -fstack-clash-protection)
	endif()

	check_c_compiler_flag(-fstrict-flex-arrays=3 HAVE_STRICT_FLEX)
	if(HAVE_STRICT_FLEX)
		list(APPEND DEUCESSH_COMPILE_OPTIONS -fstrict-flex-arrays=3)
	endif()

	check_c_compiler_flag(-ftrivial-auto-var-init=zero HAVE_TRIVIAL_INIT)
	if(HAVE_TRIVIAL_INIT)
		list(APPEND DEUCESSH_COMPILE_OPTIONS
			-ftrivial-auto-var-init=zero)
	endif()

	# UB mitigations
	list(APPEND DEUCESSH_COMPILE_OPTIONS
		-fno-delete-null-pointer-checks
		-fno-strict-overflow
		-fno-strict-aliasing)

	# FORTIFY_SOURCE (requires -O1+, incompatible with sanitizers)
	if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
		list(APPEND DEUCESSH_COMPILE_OPTIONS
			-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
	endif()

	# Architecture-specific control-flow protection
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
		check_c_compiler_flag(-fcf-protection=full HAVE_CF_PROTECTION)
		if(HAVE_CF_PROTECTION)
			list(APPEND DEUCESSH_COMPILE_OPTIONS
				-fcf-protection=full)
		endif()
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
		check_c_compiler_flag(-mbranch-protection=standard
			HAVE_BRANCH_PROTECTION)
		if(HAVE_BRANCH_PROTECTION)
			list(APPEND DEUCESSH_COMPILE_OPTIONS
				-mbranch-protection=standard)
		endif()
	endif()
elseif(MSVC)
	list(APPEND DEUCESSH_COMPILE_OPTIONS /W4)
endif()

# Linker hardening flags (ELF only)
if(UNIX AND NOT APPLE)
	foreach(flag
		"-Wl,-z,nodlopen"
		"-Wl,-z,noexecstack"
		"-Wl,--as-needed"
		"-Wl,--no-copy-dt-needed-entries")
		string(MAKE_C_IDENTIFIER "${flag}" flag_var)
		check_linker_flag(C "${flag}" ${flag_var})
		if(${flag_var})
			list(APPEND DEUCESSH_LINK_OPTIONS "${flag}")
		endif()
	endforeach()
endif()

set(DEUCESSH_INSTALL_TARGETS "")

# --- Static library ---

if(DEUCESSH_BUILD_STATIC)
	add_library(deucessh_static STATIC ${DEUCESSH_SOURCES})
	add_library(deucessh::static ALIAS deucessh_static)
	set_property(TARGET deucessh_static PROPERTY C_STANDARD 17)
	set_property(TARGET deucessh_static PROPERTY C_STANDARD_REQUIRED ON)
	if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET deucessh_static PROPERTY CXX_STANDARD 20)
		set_property(TARGET deucessh_static PROPERTY CXX_STANDARD_REQUIRED ON)
	endif()
	set_property(TARGET deucessh_static PROPERTY OUTPUT_NAME deucessh)
	target_include_directories(deucessh_static
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
			$<INSTALL_INTERFACE:include/deucessh>)
	if(DSSH_THREADS_SHIM_INCLUDE)
		target_include_directories(deucessh_static BEFORE
			PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
	endif()
	target_include_directories(deucessh_static SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(deucessh_static
		INTERFACE ${DSSH_CRYPTO_LIB_INTERFACE} ${THREADS_LINK_LIB})
	target_compile_definitions(deucessh_static
		PRIVATE DSSH_VERSION_STRING="${PROJECT_VERSION}"
		$<$<CONFIG:Release>:NDEBUG>)
	target_compile_options(deucessh_static PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	list(APPEND DEUCESSH_INSTALL_TARGETS deucessh_static)
endif()

# --- Shared library ---

if(DEUCESSH_BUILD_SHARED)
	add_library(deucessh_shared SHARED ${DEUCESSH_SOURCES})
	add_library(deucessh::shared ALIAS deucessh_shared)
	set_property(TARGET deucessh_shared PROPERTY C_STANDARD 17)
	set_property(TARGET deucessh_shared PROPERTY C_STANDARD_REQUIRED ON)
	if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET deucessh_shared PROPERTY CXX_STANDARD 20)
		set_property(TARGET deucessh_shared PROPERTY CXX_STANDARD_REQUIRED ON)
	endif()
	set_property(TARGET deucessh_shared PROPERTY OUTPUT_NAME deucessh)
	set_property(TARGET deucessh_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
	set_property(TARGET deucessh_shared PROPERTY VERSION ${PROJECT_VERSION})
	set_property(TARGET deucessh_shared PROPERTY SOVERSION ${PROJECT_VERSION_MAJOR})
	target_include_directories(deucessh_shared
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
			$<INSTALL_INTERFACE:include/deucessh>)
	if(DSSH_THREADS_SHIM_INCLUDE)
		target_include_directories(deucessh_shared BEFORE
			PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
	endif()
	target_include_directories(deucessh_shared SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(deucessh_shared
		PRIVATE ${DSSH_CRYPTO_LIB_PRIVATE} ${THREADS_LINK_LIB})
	target_compile_definitions(deucessh_shared
		PRIVATE
			DSSH_SHARED
			DSSH_VERSION_STRING="${PROJECT_VERSION}"
			$<$<CONFIG:Release>:NDEBUG>)
	target_compile_options(deucessh_shared PRIVATE
		${DEUCESSH_COMPILE_OPTIONS}
		$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
			-fvisibility=hidden -fno-semantic-interposition>)
	# ELF shared library hardening and optimization (Linux, FreeBSD)
	if(UNIX AND NOT APPLE)
		target_link_options(deucessh_shared PRIVATE
			-Wl,-z,relro,-z,now
			-Wl,--hash-style=gnu
			-Wl,-Bsymbolic-functions
			-Wl,-O2
			${DEUCESSH_LINK_OPTIONS})
	endif()
	list(APPEND DEUCESSH_INSTALL_TARGETS deucessh_shared)
endif()

# --- Install ---

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ${DEUCESSH_INSTALL_TARGETS}
	EXPORT deucesshTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${DEUCESSH_PUBLIC_HEADERS}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/deucessh)

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

configure_file(deucessh.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/deucessh.pc @ONLY)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/deucesshConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/deucesshConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/deucessh)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/deucessh.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# --- Example executables (not installed) ---

if(DEUCESSH_BUILD_STATIC)
	add_executable(client EXCLUDE_FROM_ALL examples/client.c)
	target_link_libraries(client PRIVATE deucessh_static)

	add_executable(server EXCLUDE_FROM_ALL examples/server.c)
	target_link_libraries(server PRIVATE deucessh_static)

endif()

# --- Test suite ---

option(DEUCESSH_BUILD_TESTS "Build test suite" OFF)
option(DEUCESSH_TEST_OPENSSH "Build OpenSSH interop tests" OFF)
option(DEUCESSH_TEST_SYNCHRONET "Build Synchronet interop tests" OFF)

if(DEUCESSH_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)

	add_executable(kex_test EXCLUDE_FROM_ALL test/kex_test.c)
	target_link_libraries(kex_test PRIVATE deucessh_test_lib)
endif()
