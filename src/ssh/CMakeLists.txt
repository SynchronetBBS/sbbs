cmake_minimum_required(VERSION 3.21)
project(deucessh VERSION 0.1 LANGUAGES C)

option(DEUCESSH_BUILD_STATIC "Build static library" ON)
option(DEUCESSH_BUILD_SHARED "Build shared library" ON)

if(NOT DEUCESSH_BUILD_STATIC AND NOT DEUCESSH_BUILD_SHARED)
	message(FATAL_ERROR "At least one of DEUCESSH_BUILD_STATIC or DEUCESSH_BUILD_SHARED must be ON")
endif()

find_package(OpenSSL REQUIRED)

# C11 threads: FreeBSD needs -lstdthreads, Linux/macOS have it in libc
include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c17")
check_c_source_compiles("
	#include <threads.h>
	int main(void) { mtx_t m; mtx_init(&m, mtx_plain); return 0; }
" HAVE_THREADS_IN_LIBC)
if(NOT HAVE_THREADS_IN_LIBC)
	find_library(STDTHREADS_LIB stdthreads)
	if(NOT STDTHREADS_LIB)
		message(FATAL_ERROR "C11 <threads.h> not found in libc or libstdthreads")
	endif()
	set(THREADS_LINK_LIB ${STDTHREADS_LIB})
else()
	set(THREADS_LINK_LIB "")
endif()

set(DEUCESSH_SOURCES
	ssh-arch.c ssh-trans.c ssh.c ssh-auth.c ssh-conn.c ssh-chan.c
	kex/curve25519-sha256.c
	kex/dh-gex-sha256.c
	kex/sntrup761.c
	kex/sntrup761x25519-sha512.c
	key_algo/ssh-ed25519.c
	key_algo/rsa-sha2-256.c
	enc/aes256-ctr.c
	enc/none.c
	mac/hmac-sha2-256.c
	mac/none.c
	comp/none.c
)

set(DEUCESSH_PUBLIC_HEADERS
	deucessh.h deucessh-arch.h deucessh-auth.h deucessh-conn.h
	deucessh-algorithms.h deucessh-kex.h deucessh-key-algo.h
	deucessh-enc.h deucessh-mac.h deucessh-comp.h
	deucessh-portable.h
)

include(CheckCCompilerFlag)
include(CheckLinkerFlag)

# Probe for supported hardening flags (per OpenSSF Compiler Hardening Guide)
set(DEUCESSH_COMPILE_OPTIONS "")
set(DEUCESSH_LINK_OPTIONS "")

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
	# Baseline warnings (GCC 2.95+, Clang 4+)
	list(APPEND DEUCESSH_COMPILE_OPTIONS
		-Wall -Wpedantic -pedantic -Werror
		-Wformat -Wformat=2
		-Wimplicit-fallthrough
		-Werror=format-security
		-Werror=implicit
		-Werror=incompatible-pointer-types
		-Werror=int-conversion)

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
	set_property(TARGET deucessh_static PROPERTY OUTPUT_NAME deucessh)
	target_include_directories(deucessh_static
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
			$<INSTALL_INTERFACE:include/deucessh>
		PRIVATE
			${OPENSSL_INCLUDE_DIR})
	target_link_libraries(deucessh_static
		INTERFACE ${OPENSSL_CRYPTO_LIBRARIES} ${THREADS_LINK_LIB})
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
	set_property(TARGET deucessh_shared PROPERTY OUTPUT_NAME deucessh)
	set_property(TARGET deucessh_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
	set_property(TARGET deucessh_shared PROPERTY VERSION ${PROJECT_VERSION})
	set_property(TARGET deucessh_shared PROPERTY SOVERSION ${PROJECT_VERSION_MAJOR})
	target_include_directories(deucessh_shared
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
			$<INSTALL_INTERFACE:include/deucessh>
		PRIVATE
			${OPENSSL_INCLUDE_DIR})
	target_link_libraries(deucessh_shared
		PRIVATE ${OPENSSL_CRYPTO_LIBRARIES} ${THREADS_LINK_LIB})
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

configure_file(deucessh.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/deucessh.pc @ONLY)

install(FILES
	${CMAKE_CURRENT_SOURCE_DIR}/deucesshConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/deucesshConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/deucessh)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/deucessh.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# --- Example executables (not installed) ---

if(DEUCESSH_BUILD_STATIC)
	add_executable(client EXCLUDE_FROM_ALL client.c)
	target_link_libraries(client PRIVATE deucessh_static)

	add_executable(server EXCLUDE_FROM_ALL server.c)
	target_link_libraries(server PRIVATE deucessh_static)
endif()

# --- Test suite ---

option(DEUCESSH_BUILD_TESTS "Build test suite" OFF)
option(DEUCESSH_TEST_OPENSSH "Build OpenSSH interop tests" OFF)
option(DEUCESSH_TEST_SYNCHRONET "Build Synchronet interop tests" OFF)

if(DEUCESSH_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()
