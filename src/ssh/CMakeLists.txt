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
	deucessh-algorithms.h deucessh-portable.h
)

set(DEUCESSH_COMPILE_OPTIONS
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
		-Wall -Wpedantic -pedantic -Werror>
	$<$<C_COMPILER_ID:MSVC>:
		/W4>)

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
		PRIVATE $<$<CONFIG:Release>:NDEBUG>)
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
			$<$<CONFIG:Release>:NDEBUG>)
	target_compile_options(deucessh_shared PRIVATE
		${DEUCESSH_COMPILE_OPTIONS}
		$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
			-fvisibility=hidden>)
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

	# Test-instrumented static library (exposes DSSH_TESTABLE functions)
	add_library(deucessh_test_lib STATIC ${DEUCESSH_SOURCES})
	set_property(TARGET deucessh_test_lib PROPERTY C_STANDARD 17)
	set_property(TARGET deucessh_test_lib PROPERTY C_STANDARD_REQUIRED ON)
	target_compile_definitions(deucessh_test_lib PRIVATE DSSH_TESTING)
	target_include_directories(deucessh_test_lib
		PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
		PRIVATE ${OPENSSL_INCLUDE_DIR})
	target_link_libraries(deucessh_test_lib
		INTERFACE ${OPENSSL_CRYPTO_LIBRARIES} ${THREADS_LINK_LIB})
	target_compile_options(deucessh_test_lib PRIVATE
		${DEUCESSH_COMPILE_OPTIONS}
		$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
			-Wno-unused-function>)

	# Mock I/O library
	add_library(dssh_mock_io STATIC test/mock_io.c)
	set_property(TARGET dssh_mock_io PROPERTY C_STANDARD 17)
	set_property(TARGET dssh_mock_io PROPERTY C_STANDARD_REQUIRED ON)
	target_include_directories(dssh_mock_io PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/test)
	target_link_libraries(dssh_mock_io PRIVATE deucessh_test_lib)

	# Mock allocator library (--wrap based)
	add_library(dssh_mock_alloc STATIC test/mock_alloc.c)
	set_property(TARGET dssh_mock_alloc PROPERTY C_STANDARD 17)
	set_property(TARGET dssh_mock_alloc PROPERTY C_STANDARD_REQUIRED ON)
	target_include_directories(dssh_mock_alloc PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/test)

	set(DSSH_WRAP_FLAGS
		-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free)

	# Helper function for adding test executables
	function(dssh_add_test test_name source_file)
		add_executable(dssh_test_${test_name} ${source_file})
		set_property(TARGET dssh_test_${test_name} PROPERTY C_STANDARD 17)
		set_property(TARGET dssh_test_${test_name} PROPERTY C_STANDARD_REQUIRED ON)
		target_include_directories(dssh_test_${test_name} PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}/test
			${OPENSSL_INCLUDE_DIR})
		target_link_libraries(dssh_test_${test_name} PRIVATE
			deucessh_test_lib ${OPENSSL_CRYPTO_LIBRARIES} ${THREADS_LINK_LIB})
		target_compile_options(dssh_test_${test_name} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	endfunction()

	# Tier 1: Unit tests
	dssh_add_test(arch test/test_arch.c)
	add_test(NAME dssh_unit_arch COMMAND dssh_test_arch)

	dssh_add_test(chan test/test_chan.c)
	add_test(NAME dssh_unit_chan COMMAND dssh_test_chan)

	dssh_add_test(algo_enc test/test_algo_enc.c)
	add_test(NAME dssh_unit_algo_enc COMMAND dssh_test_algo_enc)

	dssh_add_test(algo_mac test/test_algo_mac.c)
	add_test(NAME dssh_unit_algo_mac COMMAND dssh_test_algo_mac)

	dssh_add_test(algo_key test/test_algo_key.c)
	add_test(NAME dssh_unit_algo_key COMMAND dssh_test_algo_key)

	# Tier 2: Layer tests (need mock I/O)
	dssh_add_test(transport test/test_transport.c)
	target_link_libraries(dssh_test_transport PRIVATE dssh_mock_io)
	add_test(NAME dssh_layer_transport COMMAND dssh_test_transport)
	add_test(NAME dssh_layer_transport_dhgex COMMAND dssh_test_transport)
	set_tests_properties(dssh_layer_transport_dhgex PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex")
	add_test(NAME dssh_layer_transport_rsa COMMAND dssh_test_transport)
	set_tests_properties(dssh_layer_transport_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=rsa")
	add_test(NAME dssh_layer_transport_dhgex_rsa COMMAND dssh_test_transport)
	set_tests_properties(dssh_layer_transport_dhgex_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa")

	dssh_add_test(auth test/test_auth.c)
	target_link_libraries(dssh_test_auth PRIVATE dssh_mock_io)
	add_test(NAME dssh_layer_auth COMMAND dssh_test_auth)
	add_test(NAME dssh_layer_auth_dhgex COMMAND dssh_test_auth)
	set_tests_properties(dssh_layer_auth_dhgex PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex")
	add_test(NAME dssh_layer_auth_rsa COMMAND dssh_test_auth)
	set_tests_properties(dssh_layer_auth_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=rsa")
	add_test(NAME dssh_layer_auth_dhgex_rsa COMMAND dssh_test_auth)
	set_tests_properties(dssh_layer_auth_dhgex_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa")

	dssh_add_test(conn test/test_conn.c)
	target_link_libraries(dssh_test_conn PRIVATE dssh_mock_io)
	add_test(NAME dssh_layer_conn COMMAND dssh_test_conn)
	add_test(NAME dssh_layer_conn_dhgex COMMAND dssh_test_conn)
	set_tests_properties(dssh_layer_conn_dhgex PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex")
	add_test(NAME dssh_layer_conn_rsa COMMAND dssh_test_conn)
	set_tests_properties(dssh_layer_conn_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=rsa")
	add_test(NAME dssh_layer_conn_dhgex_rsa COMMAND dssh_test_conn)
	set_tests_properties(dssh_layer_conn_dhgex_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa")

	# Tier 1b: Alloc failure tests (need mock allocator + mock I/O)
	dssh_add_test(alloc test/test_alloc.c)
	target_link_libraries(dssh_test_alloc PRIVATE dssh_mock_io dssh_mock_alloc)
	target_link_options(dssh_test_alloc PRIVATE ${DSSH_WRAP_FLAGS})
	add_test(NAME dssh_unit_alloc COMMAND dssh_test_alloc)

	# Test algorithm modules (enc/mac with failure injection)
	add_library(dssh_test_algos STATIC test/test_enc.c test/test_mac.c)
	set_property(TARGET dssh_test_algos PROPERTY C_STANDARD 17)
	set_property(TARGET dssh_test_algos PROPERTY C_STANDARD_REQUIRED ON)
	target_include_directories(dssh_test_algos PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/test)
	target_link_libraries(dssh_test_algos PRIVATE deucessh_test_lib)

	# Tier 2b: Transport error tests (need test enc/mac + mock I/O)
	dssh_add_test(transport_errors test/test_transport_errors.c)
	target_link_libraries(dssh_test_transport_errors PRIVATE
		dssh_mock_io dssh_test_algos)
	add_test(NAME dssh_layer_transport_errors COMMAND dssh_test_transport_errors)

	# Tier 3a: Self-test (default integration)
	dssh_add_test(selftest test/test_selftest.c)
	add_test(NAME dssh_integration_selftest COMMAND dssh_test_selftest)
	add_test(NAME dssh_integration_selftest_dhgex COMMAND dssh_test_selftest)
	set_tests_properties(dssh_integration_selftest_dhgex PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex")
	add_test(NAME dssh_integration_selftest_rsa COMMAND dssh_test_selftest)
	set_tests_properties(dssh_integration_selftest_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=rsa")
	add_test(NAME dssh_integration_selftest_dhgex_rsa COMMAND dssh_test_selftest)
	set_tests_properties(dssh_integration_selftest_dhgex_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa")

	# Tier 3b: OpenSSH interop (optional)
	if(DEUCESSH_TEST_OPENSSH)
		dssh_add_test(openssh_server test/test_openssh_server.c)
		dssh_add_test(openssh_client test/test_openssh_client.c)
		add_test(NAME dssh_integration_openssh
			COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/test_openssh_interop.sh
				${CMAKE_CURRENT_BINARY_DIR}/dssh_test_openssh_server
				${CMAKE_CURRENT_BINARY_DIR}/dssh_test_openssh_client)
	endif()

	# Tier 3c: Synchronet interop (optional)
	if(DEUCESSH_TEST_SYNCHRONET)
		dssh_add_test(synchronet test/test_synchronet.c)
		add_test(NAME dssh_integration_synchronet COMMAND dssh_test_synchronet)
	endif()
endif()
