# Keep exit status authoritative while also requiring the fixture's result text.
function(dssh_add_output_test name target required_output)
	set(command "$<TARGET_FILE:${target}>" ${ARGN})
	get_target_property(emulator ${target} CROSSCOMPILING_EMULATOR)
	if(emulator)
		list(PREPEND command ${emulator})
	endif()
	get_target_property(launcher ${target} TEST_LAUNCHER)
	if(launcher)
		list(PREPEND command ${launcher})
	endif()
	add_test(NAME "${name}" COMMAND "${CMAKE_COMMAND}"
		"-DTEST_COMMAND=${command}" "-DTEST_REQUIRED_OUTPUT=${required_output}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/check_output.cmake")
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/abi.cmake")

add_test(NAME dssh_test_output_policy COMMAND "${CMAKE_COMMAND}"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/check_output_test.cmake")

add_test(NAME dssh_c_standard COMMAND "${CMAKE_COMMAND}"
	"-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/c-standard-tests"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/c_standard.cmake")

if(UNIX AND NOT APPLE)
	add_test(NAME dssh_build_options COMMAND "${CMAKE_COMMAND}"
		"-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/c-standard-tests/options"
		"-DTEST_C_COMPILER=${CMAKE_C_COMPILER}"
		"-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
		"-DTEST_BACKEND=${DEUCESSH_CRYPTO_BACKEND}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/build_options.cmake")
endif()

if(UNIX)
	add_executable(dssh_test_timespec_shim test_timespec.c)
	set_property(TARGET dssh_test_timespec_shim PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET dssh_test_timespec_shim PROPERTY C_STANDARD_REQUIRED ON)
	target_compile_definitions(dssh_test_timespec_shim PRIVATE
		_POSIX_C_SOURCE=200809L DSSH_TIMESPEC_GET_SHIM)
	target_compile_options(dssh_test_timespec_shim PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	dssh_add_output_test(dssh_timespec_shim dssh_test_timespec_shim
		"timespec_shim cases=6 calls=4 valid=1")
endif()

# The shim translation tests are independent of the library's selected C11
# facility. Application coexistence targets deliberately use native headers.
if(UNIX)
	find_package(Threads REQUIRED)
	add_executable(dssh_test_threads_shim_posix test_threads_shim_posix.c)
	set_target_properties(dssh_test_threads_shim_posix PROPERTIES
		C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
	target_compile_definitions(dssh_test_threads_shim_posix PRIVATE _POSIX_C_SOURCE=200809L)
	target_compile_options(dssh_test_threads_shim_posix PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_link_libraries(dssh_test_threads_shim_posix PRIVATE Threads::Threads)
	dssh_add_output_test(dssh_threads_shim_posix dssh_test_threads_shim_posix
		"posix_thread_shim checks=[0-9]+ results=21 allocations=23 releases=23 valid=1")

	function(dssh_find_application_threads)
		dssh_push_platform_check_state()
		set(probe "#include <threads.h>
int main(void) { mtx_t m; return mtx_init(&m, mtx_plain); }")
		check_c_source_compiles("${probe}" DSSH_APP_THREADS_IN_LIBC)
		if(DSSH_APP_THREADS_IN_LIBC)
			set(DSSH_APP_THREADS_AVAILABLE TRUE PARENT_SCOPE)
			set(DSSH_APP_THREADS_LIBRARY "" PARENT_SCOPE)
		else()
			find_library(DSSH_APP_STDTHREADS_LIBRARY stdthreads)
			if(DSSH_APP_STDTHREADS_LIBRARY)
				set(CMAKE_REQUIRED_LIBRARIES "${DSSH_APP_STDTHREADS_LIBRARY}")
				check_c_source_compiles("${probe}" DSSH_APP_THREADS_IN_STDTHREADS)
				if(DSSH_APP_THREADS_IN_STDTHREADS)
					set(DSSH_APP_THREADS_AVAILABLE TRUE PARENT_SCOPE)
					set(DSSH_APP_THREADS_LIBRARY "${DSSH_APP_STDTHREADS_LIBRARY}" PARENT_SCOPE)
				endif()
			endif()
		endif()
		cmake_pop_check_state()
	endfunction()
	dssh_find_application_threads()
	if(DSSH_APP_THREADS_AVAILABLE)
		foreach(kind IN ITEMS static shared)
			if(TARGET deucessh_${kind})
				set(target dssh_test_application_threads_${kind})
				add_executable(${target} test_application_threads.c)
				set_target_properties(${target} PROPERTIES
					C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
				target_compile_options(${target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
				target_link_libraries(${target} PRIVATE deucessh_${kind} ${DSSH_APP_THREADS_LIBRARY})
				dssh_add_output_test(dssh_application_threads_${kind} ${target}
					"application_threads workers=4 sessions=64 failures=0 native=[0-9]+ valid=1")
			endif()
		endforeach()
	endif()
endif()

if(WIN32)
	add_executable(dssh_test_threads_shim_win32 test_threads_shim_win32.c)
	set_target_properties(dssh_test_threads_shim_win32 PROPERTIES
		C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
	target_compile_options(dssh_test_threads_shim_win32 PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	dssh_add_output_test(dssh_threads_shim_win32 dssh_test_threads_shim_win32
		"Win32 thread join, deadline and sleep checks passed")
endif()

if(UNIX OR WIN32)
	add_library(dssh_visibility_probe SHARED visibility_probe.c)
	set_target_properties(dssh_visibility_probe PROPERTIES C_STANDARD ${DEUCESSH_C_STANDARD}
		C_STANDARD_REQUIRED ON WINDOWS_EXPORT_ALL_SYMBOLS OFF)
	target_compile_definitions(dssh_visibility_probe PRIVATE DSSH_SHARED)
	target_compile_options(dssh_visibility_probe PRIVATE
		${DEUCESSH_COMPILE_OPTIONS} ${DEUCESSH_SHARED_COMPILE_OPTIONS})
	add_executable(dssh_test_visibility test_visibility.c)
	set_target_properties(dssh_test_visibility PROPERTIES C_STANDARD ${DEUCESSH_C_STANDARD}
		C_STANDARD_REQUIRED ON)
	target_compile_definitions(dssh_test_visibility PRIVATE DSSH_SHARED_IMPORT)
	target_compile_options(dssh_test_visibility PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_link_libraries(dssh_test_visibility PRIVATE dssh_visibility_probe ${CMAKE_DL_LIBS})
	if(UNIX AND NOT APPLE AND "-Wl,--exclude-libs,ALL" IN_LIST DEUCESSH_LINK_OPTIONS)
		add_library(dssh_visibility_archive STATIC visibility_archive.c)
		set_target_properties(dssh_visibility_archive PROPERTIES
			C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
		target_compile_definitions(dssh_visibility_archive PRIVATE DSSH_SHARED)
		target_compile_options(dssh_visibility_archive PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
		target_link_libraries(dssh_visibility_probe PRIVATE dssh_visibility_archive)
		target_compile_definitions(dssh_visibility_probe PRIVATE DSSH_TEST_ELF_ARCHIVE)
		target_compile_definitions(dssh_test_visibility PRIVATE DSSH_TEST_ELF_ARCHIVE)
		target_link_options(dssh_visibility_probe PRIVATE ${DEUCESSH_LINK_OPTIONS})
	endif()
	dssh_add_output_test(dssh_visibility dssh_test_visibility
		"visibility_probe public=42 private=0 valid=1" $<TARGET_FILE_NAME:dssh_visibility_probe>)
endif()

# Cached key files for test runs — avoids repeated RSA keygen.
# These are stored in the build directory and reused across runs.
set(DSSH_TEST_KEY_ENV
	"DSSH_TEST_ED25519_KEY=${CMAKE_CURRENT_BINARY_DIR}/test_ed25519.pem"
	"DSSH_TEST_RSA_KEY=${CMAKE_CURRENT_BINARY_DIR}/test_rsa.pem")

# Fresh-process production-library consumers must not use the test registry
# reset, which would otherwise hide first-use publication behavior.
foreach(kind IN ITEMS static shared)
	if(TARGET deucessh_${kind})
		set(target dssh_test_session_init_${kind})
		add_executable(${target} test_session_init.c)
		# A shared-library consumer supplies its own C11 thread facility;
		# the library's private dependency is not part of its public API.
		foreach(shim IN LISTS DSSH_THREADS_SHIM_SOURCES)
			target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/${shim}")
		endforeach()
		set_property(TARGET ${target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
		set_property(TARGET ${target} PROPERTY C_STANDARD_REQUIRED ON)
		target_compile_options(${target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
		target_link_libraries(${target} PRIVATE deucessh_${kind} ${THREADS_LINK_LIB})
		if(DSSH_THREADS_SHIM_INCLUDE)
			target_include_directories(${target} BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
		endif()
		dssh_add_output_test(dssh_session_init_${kind}_default ${target} "workers=8 sessions=256 failures=0 frozen=1")
		dssh_add_output_test(dssh_session_init_${kind}_configured ${target} "workers=8 sessions=256 failures=0 frozen=1"
			configured)
		set_tests_properties(dssh_session_init_${kind}_default dssh_session_init_${kind}_configured
			PROPERTIES TIMEOUT 60)
		if(UNIX)
			if(EXISTS "${PROJECT_SOURCE_DIR}/examples/server.c"
				AND EXISTS "${PROJECT_SOURCE_DIR}/examples/kbi-game-server.c"
				AND EXISTS "${PROJECT_SOURCE_DIR}/examples/server-io.h"
				AND EXISTS "${PROJECT_SOURCE_DIR}/examples/server-output.h"
				AND EXISTS "${PROJECT_SOURCE_DIR}/examples/server-shell.h"
				AND EXISTS "${PROJECT_SOURCE_DIR}/examples/server-port.h")
				foreach(example IN ITEMS server game)
					set(io_target dssh_server_io_${example}_${kind})
					add_executable(${io_target} test_server_io.c)
					set_property(TARGET ${io_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
					set_property(TARGET ${io_target} PROPERTY C_STANDARD_REQUIRED ON)
					target_compile_definitions(${io_target} PRIVATE _DEFAULT_SOURCE)
					if(example STREQUAL "game")
						target_compile_definitions(${io_target} PRIVATE DSSH_GAME_SERVER)
					endif()
					target_compile_options(${io_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
					target_link_libraries(${io_target} PRIVATE deucessh_${kind})
					dssh_add_output_test(${io_target} ${io_target} "cases=26 failures=0 integration=1")
					set_tests_properties(${io_target} PROPERTIES TIMEOUT 60)
					set(output_target dssh_server_output_${example}_${kind})
					add_executable(${output_target} test_server_output.c)
					set_property(TARGET ${output_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
					set_property(TARGET ${output_target} PROPERTY C_STANDARD_REQUIRED ON)
					target_compile_definitions(${output_target} PRIVATE _DEFAULT_SOURCE)
					set(output_cases 12)
					if(example STREQUAL "game")
						target_compile_definitions(${output_target} PRIVATE DSSH_GAME_SERVER)
						set(output_cases 8)
					endif()
					target_compile_options(${output_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
					target_link_libraries(${output_target} PRIVATE deucessh_${kind})
					dssh_add_output_test(${output_target} ${output_target} "cases=${output_cases} failures=0 formatter=1")
					set_tests_properties(${output_target} PROPERTIES TIMEOUT 60)
					set(lifecycle_target dssh_server_lifecycle_${example}_${kind})
					add_executable(${lifecycle_target} test_server_lifecycle.c)
					set_property(TARGET ${lifecycle_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
					set_property(TARGET ${lifecycle_target} PROPERTY C_STANDARD_REQUIRED ON)
					target_compile_definitions(${lifecycle_target} PRIVATE _DEFAULT_SOURCE)
					if(example STREQUAL "game")
						target_compile_definitions(${lifecycle_target} PRIVATE DSSH_GAME_SERVER)
					endif()
					target_compile_options(${lifecycle_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
					target_link_libraries(${lifecycle_target} PRIVATE deucessh_${kind})
					dssh_add_output_test(${lifecycle_target} ${lifecycle_target} "cases=26 failures=0")
					set_tests_properties(${lifecycle_target} PROPERTIES TIMEOUT 60)
					set(startup_target dssh_server_startup_${example}_${kind})
					add_executable(${startup_target} test_server_startup.c)
					set_property(TARGET ${startup_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
					set_property(TARGET ${startup_target} PROPERTY C_STANDARD_REQUIRED ON)
					target_compile_definitions(${startup_target} PRIVATE _DEFAULT_SOURCE)
					if(example STREQUAL "game")
						target_compile_definitions(${startup_target} PRIVATE DSSH_GAME_SERVER)
					endif()
					target_compile_options(${startup_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
					target_link_libraries(${startup_target} PRIVATE deucessh_${kind})
					dssh_add_output_test(${startup_target} ${startup_target} "cases=58 failures=0")
					set_tests_properties(${startup_target} PROPERTIES TIMEOUT 60)
				endforeach()
			endif()
			set(auth_target dssh_auth_actions_${kind})
			add_executable(${auth_target} test_auth_actions.c mock_io.c)
			foreach(shim IN LISTS DSSH_THREADS_SHIM_SOURCES)
				target_sources(${auth_target} PRIVATE "${PROJECT_SOURCE_DIR}/${shim}")
			endforeach()
			set_property(TARGET ${auth_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
			set_property(TARGET ${auth_target} PROPERTY C_STANDARD_REQUIRED ON)
			target_compile_definitions(${auth_target} PRIVATE _DEFAULT_SOURCE)
			target_compile_options(${auth_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			target_link_libraries(${auth_target} PRIVATE deucessh_${kind} ${THREADS_LINK_LIB})
			if(DSSH_THREADS_SHIM_INCLUDE)
				target_include_directories(${auth_target} BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
			endif()
			foreach(mode IN ITEMS begin direct)
				dssh_add_output_test(dssh_auth_actions_${kind}_${mode} ${auth_target}
					"valid=1" ${mode})
				set_tests_properties(dssh_auth_actions_${kind}_${mode} PROPERTIES TIMEOUT 60)
			endforeach()
		endif()
		set(release_target dssh_test_release_entry_${kind})
		add_executable(${release_target} test_release_entry.c)
		set_property(TARGET ${release_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
		set_property(TARGET ${release_target} PROPERTY C_STANDARD_REQUIRED ON)
		target_compile_options(${release_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
		target_link_libraries(${release_target} PRIVATE deucessh_${kind})
		dssh_add_output_test(dssh_release_entry_${kind} ${release_target} "release_entries=48 releases=4 before=1 after=1 restored=1")
		set_tests_properties(dssh_release_entry_${kind} PROPERTIES TIMEOUT 60)
		if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
			set(rng_target dssh_test_rng_provider_${kind})
			add_executable(${rng_target} test_rng_provider.c)
			set_property(TARGET ${rng_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
			set_property(TARGET ${rng_target} PROPERTY C_STANDARD_REQUIRED ON)
			target_compile_options(${rng_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			target_link_libraries(${rng_target} PRIVATE deucessh_${kind} OpenSSL::Crypto)
			# A privately embedded archive has its own native global context.
			# Configuring the application's copy must leave that provider intact.
			if(kind STREQUAL shared AND UNIX AND NOT APPLE
			    AND "-Wl,--exclude-libs,ALL" IN_LIST DEUCESSH_LINK_OPTIONS)
				get_target_property(rng_crypto OpenSSL::Crypto IMPORTED_LOCATION)
				if(rng_crypto MATCHES "[.]a$")
					target_compile_definitions(${rng_target} PRIVATE DSSH_TEST_RNG_PRIVATE_ARCHIVE)
				endif()
			endif()
			foreach(mode IN ITEMS aes128 aes192 aes256 invalid)
				dssh_add_output_test(dssh_rng_provider_${kind}_${mode} ${rng_target}
					"rng_policy=1 mode=${mode} production=1" ${mode})
			endforeach()
		endif()
		if(UNIX)
			if(EXISTS "${PROJECT_SOURCE_DIR}/examples/client.c")
				set(client_target dssh_test_example_client_${kind})
				add_executable(${client_target} test_example_client.c)
				foreach(shim IN LISTS DSSH_THREADS_SHIM_SOURCES)
					target_sources(${client_target} PRIVATE "${PROJECT_SOURCE_DIR}/${shim}")
				endforeach()
				set_property(TARGET ${client_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
				set_property(TARGET ${client_target} PROPERTY C_STANDARD_REQUIRED ON)
				target_compile_definitions(${client_target} PRIVATE _POSIX_C_SOURCE=200809L)
				target_compile_options(${client_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
				target_include_directories(${client_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
				target_link_libraries(${client_target} PRIVATE deucessh_${kind} ${THREADS_LINK_LIB}
					${DSSH_MATH_LINK_LIB})
				if(DSSH_THREADS_SHIM_INCLUDE)
					target_include_directories(${client_target} BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
				endif()
				dssh_add_output_test(dssh_example_client_${kind} ${client_target}
					"20 tests: 20 passed, 0 failed, 0 skipped")
				set_tests_properties(dssh_example_client_${kind} PROPERTIES TIMEOUT 60)
				add_executable(dssh_client_driver_${kind} client_example_driver.c)
				foreach(shim IN LISTS DSSH_THREADS_SHIM_SOURCES)
					target_sources(dssh_client_driver_${kind} PRIVATE "${PROJECT_SOURCE_DIR}/${shim}")
				endforeach()
				set_property(TARGET dssh_client_driver_${kind} PROPERTY C_STANDARD 17)
				set_property(TARGET dssh_client_driver_${kind} PROPERTY C_STANDARD_REQUIRED ON)
				target_compile_definitions(dssh_client_driver_${kind} PRIVATE _POSIX_C_SOURCE=200809L)
				target_compile_options(dssh_client_driver_${kind} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
				target_link_libraries(dssh_client_driver_${kind} PRIVATE deucessh_${kind} ${THREADS_LINK_LIB})
				if(DSSH_THREADS_SHIM_INCLUDE)
					target_include_directories(dssh_client_driver_${kind} BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
				endif()
			endif()

			set(event_target dssh_test_event_peer_${kind})
			add_executable(${event_target} test_event_peer.c mock_io.c)
			foreach(shim IN LISTS DSSH_THREADS_SHIM_SOURCES)
				target_sources(${event_target} PRIVATE "${PROJECT_SOURCE_DIR}/${shim}")
			endforeach()
			set_property(TARGET ${event_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
			set_property(TARGET ${event_target} PROPERTY C_STANDARD_REQUIRED ON)
			target_compile_options(${event_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			# This application fixture uses the host socket test driver,
			# including its nonblocking drain helper; it is not library code.
			target_compile_definitions(${event_target} PRIVATE _DEFAULT_SOURCE)
			target_link_libraries(${event_target} PRIVATE deucessh_${kind} ${THREADS_LINK_LIB})
			if(DSSH_THREADS_SHIM_INCLUDE)
				target_include_directories(${event_target} BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
			endif()
			dssh_add_output_test(dssh_event_peer_${kind} ${event_target} "events=130 queue=0 positions=3/2 production=1")
			set_tests_properties(dssh_event_peer_${kind} PROPERTIES TIMEOUT 60)
			set(callback_target dssh_test_accept_callback_peer_${kind})
			add_executable(${callback_target} test_accept_callback_peer.c mock_io.c)
			foreach(shim IN LISTS DSSH_THREADS_SHIM_SOURCES)
				target_sources(${callback_target} PRIVATE "${PROJECT_SOURCE_DIR}/${shim}")
			endforeach()
			set_property(TARGET ${callback_target} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
			set_property(TARGET ${callback_target} PROPERTY C_STANDARD_REQUIRED ON)
			target_compile_options(${callback_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			target_compile_definitions(${callback_target} PRIVATE _DEFAULT_SOURCE)
			target_link_libraries(${callback_target} PRIVATE deucessh_${kind} ${THREADS_LINK_LIB})
			if(DSSH_THREADS_SHIM_INCLUDE)
				target_include_directories(${callback_target} BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
			endif()
			foreach(mode RANGE 0 5)
				dssh_add_output_test(dssh_accept_callback_peer_${kind}_${mode} ${callback_target} "setup_contract=1 kind=${mode}.*production=1"
					${mode})
				set_tests_properties(dssh_accept_callback_peer_${kind}_${mode} PROPERTIES TIMEOUT 60)
			endforeach()

		endif()
	endif()
endforeach()

# Actual production C17 entry across native provider key/exception boundaries.
# An independent application crypto dependency belongs outside our shared object.
if(UNIX AND NOT APPLE)
	find_library(DSSH_OBSERVER_CRYPTO_LIBRARY NAMES crypto PATHS /usr/lib /usr/local/lib NO_DEFAULT_PATH)
	find_path(DSSH_OBSERVER_CRYPTO_INCLUDE openssl/evp.h PATHS /usr/include /usr/local/include NO_DEFAULT_PATH)
endif()
foreach(kind IN ITEMS static shared)
	if(TARGET deucessh_${kind})
		set(target dssh_test_provider_boundary_${kind})
		add_executable(${target} provider_boundary.c)
		set_target_properties(${target} PROPERTIES
			C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
		target_compile_options(${target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
		target_link_libraries(${target} PRIVATE deucessh_${kind})
		if(kind STREQUAL shared)
			target_compile_definitions(${target} PRIVATE DSSH_SHARED_IMPORT)
			if(UNIX AND NOT APPLE AND DSSH_OBSERVER_CRYPTO_LIBRARY AND DSSH_OBSERVER_CRYPTO_INCLUDE)
				target_compile_definitions(${target} PRIVATE DSSH_PROVIDER_SYSTEM_OPENSSL)
				target_include_directories(${target} SYSTEM PRIVATE ${DSSH_OBSERVER_CRYPTO_INCLUDE})
				target_link_libraries(${target} PRIVATE ${DSSH_OBSERVER_CRYPTO_LIBRARY} ${CMAKE_DL_LIBS})
			endif()
		endif()
		if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan" AND UNIX AND NOT APPLE)
			target_sources(${target} PRIVATE provider_boundary_cpp.cpp key_file_cpp_alloc.cpp)
			set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
			target_compile_definitions(${target} PRIVATE DSSH_PROVIDER_CPP_CONTROLS)
		endif()
		dssh_add_output_test(dssh_provider_boundary_${kind} ${target}
			"provider_boundary algorithms=3 rejected=3 wrong=3 cpp=[01] faults=[04] valid=1"
			"${CMAKE_CURRENT_BINARY_DIR}/provider-boundary-${kind}.pem"
			"${CMAKE_CURRENT_BINARY_DIR}/test_ed25519.pem" "${CMAKE_CURRENT_BINARY_DIR}/test_rsa.pem")
		set_tests_properties(dssh_provider_boundary_${kind} PROPERTIES FIXTURES_REQUIRED dssh_cached_keys)
	endif()
endforeach()

if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	add_executable(dssh_test_key_file_bio test_key_file_bio.c)
	set_target_properties(dssh_test_key_file_bio PROPERTIES
		C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
	target_compile_options(dssh_test_key_file_bio PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	if(TARGET deucessh_static)
		set(key_bio_library deucessh_static)
	else()
		set(key_bio_library deucessh_shared)
	endif()
	target_link_libraries(dssh_test_key_file_bio PRIVATE
		${key_bio_library} OpenSSL::Crypto ${DSSH_MATH_LINK_LIB})
	if(DSSH_THREADS_SHIM_INCLUDE)
		target_include_directories(dssh_test_key_file_bio BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
	endif()
	dssh_add_output_test(dssh_key_file_bio dssh_test_key_file_bio
		"4 tests: 4 passed, 0 failed, 0 skipped")
endif()

if(WIN32)
	include("${CMAKE_CURRENT_SOURCE_DIR}/windows.cmake")
	return()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/client-keys.cmake")

# Prepend project root to source paths (they're relative to ssh/, not test/)
set(DSSH_TEST_LIB_SOURCES "")
foreach(src ${DEUCESSH_SOURCES})
	list(APPEND DSSH_TEST_LIB_SOURCES "${PROJECT_SOURCE_DIR}/${src}")
endforeach()

# Test-instrumented static library (exposes DSSH_TESTABLE functions)
add_library(deucessh_test_lib STATIC ${DSSH_TEST_LIB_SOURCES})
set_property(TARGET deucessh_test_lib PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET deucessh_test_lib PROPERTY C_STANDARD_REQUIRED ON)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	set_property(TARGET deucessh_test_lib PROPERTY CXX_STANDARD 20)
	set_property(TARGET deucessh_test_lib PROPERTY CXX_STANDARD_REQUIRED ON)
endif()
target_compile_definitions(deucessh_test_lib PRIVATE DSSH_TESTING
	$<$<COMPILE_LANGUAGE:C>:_POSIX_C_SOURCE=200809L>
	$<$<STREQUAL:${DEUCESSH_CRYPTO_BACKEND},OpenSSL>:DSSH_CRYPTO_OPENSSL>
	$<$<STREQUAL:${DEUCESSH_CRYPTO_BACKEND},Botan>:DSSH_CRYPTO_BOTAN>)
# Consumers need the instrumented private layout without enabling allocator or
# provider macro redirections in their own fixture code.
target_compile_definitions(deucessh_test_lib PUBLIC DSSH_TEST_LAYOUT ${DSSH_TIME_SHIM_DEFINITIONS})
target_include_directories(deucessh_test_lib
	PUBLIC ${PROJECT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(DSSH_THREADS_SHIM_INCLUDE)
	target_include_directories(deucessh_test_lib BEFORE
		PUBLIC ${DSSH_THREADS_SHIM_INCLUDE})
endif()
target_include_directories(deucessh_test_lib SYSTEM
	PRIVATE ${DSSH_CRYPTO_INCLUDE})
target_link_libraries(deucessh_test_lib
	INTERFACE ${DSSH_CRYPTO_LIB_INTERFACE} ${THREADS_LINK_LIB}
		${DSSH_PRIVATE_KEY_FILE_LIB} ${DSSH_MATH_LINK_LIB})
target_compile_options(deucessh_test_lib PRIVATE
	${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
		-Wno-unused-function>)

# Mock I/O library
add_library(dssh_mock_io STATIC mock_io.c)
# Strict C17 hides SSIZE_MAX on glibc; retain the native socket extensions too.
target_compile_definitions(dssh_mock_io PRIVATE _DEFAULT_SOURCE)
set_property(TARGET dssh_mock_io PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_mock_io PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_mock_io PUBLIC
	${PROJECT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dssh_mock_io PRIVATE deucessh_test_lib)

# Library-only test allocator (macro-based, doesn't affect OpenSSL)
add_library(dssh_test_allocator STATIC dssh_test_alloc.c)
set_property(TARGET dssh_test_allocator PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_allocator PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_allocator PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR})
# Crypto + C11 threads failure injection library
add_library(dssh_test_ossl STATIC dssh_test_ossl.c)
target_compile_definitions(dssh_test_ossl PRIVATE ${DSSH_TIME_SHIM_DEFINITIONS})
if(UNIX)
	target_compile_definitions(dssh_test_ossl PRIVATE _POSIX_C_SOURCE=200809L)
endif()
set_property(TARGET dssh_test_ossl PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_ossl PROPERTY C_STANDARD_REQUIRED ON)
target_compile_options(dssh_test_ossl PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
target_include_directories(dssh_test_ossl PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dssh_test_ossl PRIVATE dssh_test_allocator)
if(DSSH_THREADS_SHIM_INCLUDE)
	target_include_directories(dssh_test_ossl BEFORE
		PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
endif()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	target_link_libraries(dssh_test_ossl PRIVATE OpenSSL::Crypto)
	target_compile_definitions(dssh_test_ossl PRIVATE DSSH_CRYPTO_OPENSSL)
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_compile_definitions(dssh_test_ossl PRIVATE DSSH_CRYPTO_BOTAN)
endif()

# Link allocator and injection into the test library
# so all test executables get the definitions library code references.
target_link_libraries(deucessh_test_lib INTERFACE
	dssh_test_allocator dssh_test_ossl)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	foreach(adapter IN ITEMS hash mac cipher)
		set(adapter_target dssh_test_botan_${adapter})
		add_library(${adapter_target} STATIC dssh_test_botan_${adapter}.cpp)
		set_property(TARGET ${adapter_target} PROPERTY CXX_STANDARD 20)
		set_property(TARGET ${adapter_target} PROPERTY CXX_STANDARD_REQUIRED ON)
		target_include_directories(${adapter_target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
		target_link_libraries(${adapter_target} PRIVATE ${DSSH_CRYPTO_LIB_PRIVATE})
		target_compile_options(${adapter_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
		target_link_libraries(deucessh_test_lib INTERFACE ${adapter_target})
	endforeach()
endif()

# External test providers compiled against public headers only.
add_library(dssh_mock_comp STATIC test_comp.c test_comp_zlib.c test_stateless.c)
set_property(TARGET dssh_mock_comp PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_mock_comp PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_mock_comp PUBLIC
	${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(dssh_mock_comp PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
target_link_libraries(dssh_mock_comp PRIVATE deucessh_test_lib)
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
	target_sources(dssh_mock_comp PRIVATE ../examples/zlib-comp.c)
	target_compile_definitions(dssh_mock_comp PRIVATE DSSH_TEST_HAVE_ZLIB DSSH_EXAMPLE_ZLIB_TESTING)
	target_link_libraries(dssh_mock_comp PRIVATE ZLIB::ZLIB)
	if(UNIX AND TARGET deucessh_static)
		# Separate production-library consumer: no DSSH_TESTING/internal headers.
		add_executable(dssh_compression_peer_server
			compression_peer_server.c test_comp_zlib.c ../examples/zlib-comp.c)
		set_property(TARGET dssh_compression_peer_server PROPERTY C_STANDARD 17)
		set_property(TARGET dssh_compression_peer_server PROPERTY C_STANDARD_REQUIRED ON)
		target_compile_definitions(dssh_compression_peer_server PRIVATE
			_POSIX_C_SOURCE=200809L DSSH_TEST_HAVE_ZLIB DSSH_EXAMPLE_ZLIB_TESTING)
		target_compile_options(dssh_compression_peer_server PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
		target_link_libraries(dssh_compression_peer_server PRIVATE deucessh_static ZLIB::ZLIB)
		find_package(PkgConfig QUIET)
		if(PkgConfig_FOUND)
			pkg_check_modules(DSSH_INTEROP_LIBSSH QUIET IMPORTED_TARGET libssh)
			if(DSSH_INTEROP_LIBSSH_FOUND)
				add_executable(dssh_compression_libssh_client compression_libssh_client.c)
				set_property(TARGET dssh_compression_libssh_client PROPERTY C_STANDARD 17)
				set_property(TARGET dssh_compression_libssh_client PROPERTY C_STANDARD_REQUIRED ON)
				target_compile_definitions(dssh_compression_libssh_client PRIVATE _POSIX_C_SOURCE=200809L)
				target_compile_options(dssh_compression_libssh_client PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
				target_link_libraries(dssh_compression_libssh_client PRIVATE PkgConfig::DSSH_INTEROP_LIBSSH)
				add_test(NAME dssh_compression_libssh
					COMMAND dssh_compression_libssh_client $<TARGET_FILE:dssh_compression_peer_server>)
				set_tests_properties(dssh_compression_libssh PROPERTIES TIMEOUT 60)
				foreach(direction IN ITEMS c2s s2c)
					add_test(NAME dssh_compression_libssh_${direction}
						COMMAND dssh_compression_libssh_client $<TARGET_FILE:dssh_compression_peer_server> ${direction})
					set_tests_properties(dssh_compression_libssh_${direction} PROPERTIES TIMEOUT 60)
				endforeach()
			endif()
		endif()
	endif()
endif()

# Mock allocator library (--wrap based, for process-wide injection).
# Apple's Mach-O linker does not support the GNU --wrap option.
if(NOT APPLE)
	add_library(dssh_mock_alloc STATIC mock_alloc.c)
	set_property(TARGET dssh_mock_alloc PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET dssh_mock_alloc PROPERTY C_STANDARD_REQUIRED ON)
	target_include_directories(dssh_mock_alloc PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR})

	set(DSSH_WRAP_FLAGS
		-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free)
endif()

# This exercises the bundled C glue directly without a provider, registry or
# library test hooks. The runner uses only standard C library facilities.
add_executable(dssh_test_acvp_io test_acvp_io.c)
set_property(TARGET dssh_test_acvp_io PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_acvp_io PROPERTY C_STANDARD_REQUIRED ON)
target_compile_options(dssh_test_acvp_io PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
dssh_add_output_test(dssh_acvp_reporting dssh_test_acvp_io
	"ACVP reporting: controls=12 valid=1")
set_tests_properties(dssh_acvp_reporting PROPERTIES TIMEOUT 30)

add_executable(dssh_test_vendor_glue test_vendor_glue.c)
set_property(TARGET dssh_test_vendor_glue PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_vendor_glue PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_vendor_glue PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(dssh_test_vendor_glue PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
add_test(NAME dssh_unit_vendor_glue COMMAND dssh_test_vendor_glue)
set_tests_properties(dssh_unit_vendor_glue PROPERTIES TIMEOUT 60)

# Matrix identities against independent coefficient-domain convolution.
add_executable(dssh_test_mlkem_poly test_mlkem_poly.c)
set_property(TARGET dssh_test_mlkem_poly PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_mlkem_poly PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_mlkem_poly PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(dssh_test_mlkem_poly PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
dssh_add_output_test(dssh_mlkem_polynomial_relations dssh_test_mlkem_poly
	"ML-KEM polynomial relations: patterns=6 fields=12288 guards=24 inputs=24 valid=1")
set_tests_properties(dssh_mlkem_polynomial_relations PROPERTIES TIMEOUT 120)

# Production field encodings against independent bit-at-a-time wire answers.
add_executable(dssh_test_mlkem_encoding test_mlkem_encoding.c)
set_property(TARGET dssh_test_mlkem_encoding PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_mlkem_encoding PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_mlkem_encoding PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(dssh_test_mlkem_encoding PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
dssh_add_output_test(dssh_mlkem_encodings dssh_test_mlkem_encoding
	"ML-KEM encodings: widths=4 cases=5578 bytes=127376 fields=89248 guards=22312 inputs=11156 valid=1")
set_tests_properties(dssh_mlkem_encodings PROPERTIES TIMEOUT 120)

# Exact comparison/selection domains and output aliases, without test hooks.
add_executable(dssh_test_mlkem_selection test_mlkem_selection.c)
set_property(TARGET dssh_test_mlkem_selection PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_mlkem_selection PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_mlkem_selection PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(dssh_test_mlkem_selection PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
dssh_add_output_test(dssh_mlkem_selection dssh_test_mlkem_selection
	"ML-KEM selection: nonzero=256 selections=3072 comparisons=8705 valid=1")
set_tests_properties(dssh_mlkem_selection PROPERTIES TIMEOUT 30)

# Vector arithmetic against independent integer formulas and alias checks.
add_executable(dssh_test_mlkem_vector test_mlkem_vector.c)
set_property(TARGET dssh_test_mlkem_vector PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_mlkem_vector PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_mlkem_vector PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(dssh_test_mlkem_vector PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
dssh_add_output_test(dssh_mlkem_vectors dssh_test_mlkem_vector
	"ML-KEM vectors: operations=18 cases=1857062 fields=29712992 guards=5571186 inputs=2720057 valid=1")
set_tests_properties(dssh_mlkem_vectors PROPERTIES TIMEOUT 120)

# Original NIST response files for the four bundled ML-KEM hash primitives.
add_executable(dssh_test_mlkem_hash test_mlkem_hash.c)
set_property(TARGET dssh_test_mlkem_hash PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_mlkem_hash PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_mlkem_hash PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(dssh_test_mlkem_hash PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
dssh_add_output_test(dssh_mlkem_hash_cavp dssh_test_mlkem_hash
	"ML-KEM hash CAVP: 3592 passed, 0 failed; files=10 guards=1"
	"${CMAKE_CURRENT_SOURCE_DIR}/vectors/sha3-cavp")
set_tests_properties(dssh_mlkem_hash_cavp PROPERTIES TIMEOUT 120)
add_executable(dssh_test_mlkem_hash_retirement test_mlkem_hash.c)
set_property(TARGET dssh_test_mlkem_hash_retirement PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_mlkem_hash_retirement PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_mlkem_hash_retirement PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_definitions(dssh_test_mlkem_hash_retirement PRIVATE DSSH_TEST_MLKEM_HASH_RETIREMENT)
target_compile_options(dssh_test_mlkem_hash_retirement PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
dssh_add_output_test(dssh_mlkem_hash_retirement dssh_test_mlkem_hash_retirement
	"ML-KEM hash CAVP: 3592 passed, 0 failed; files=10 guards=1"
	"${CMAKE_CURRENT_SOURCE_DIR}/vectors/sha3-cavp")
set_tests_properties(dssh_mlkem_hash_retirement PROPERTIES TIMEOUT 120)

# Pinned official NIST answers, shared by the actual bundled and native roots.
add_library(dssh_mlkem_acvp_data STATIC mlkem_acvp_data.c)
set_property(TARGET dssh_mlkem_acvp_data PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_mlkem_acvp_data PROPERTY C_STANDARD_REQUIRED ON)
target_compile_options(dssh_mlkem_acvp_data PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
add_executable(dssh_test_mlkem_acvp test_mlkem_acvp.c)
set_property(TARGET dssh_test_mlkem_acvp PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_mlkem_acvp PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_mlkem_acvp PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(dssh_test_mlkem_acvp PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
target_link_libraries(dssh_test_mlkem_acvp PRIVATE dssh_mlkem_acvp_data)
dssh_add_output_test(dssh_mlkem_acvp_bundled dssh_test_mlkem_acvp
	"ML-KEM ACVP bundled: 145 passed, 0 failed")
set_tests_properties(dssh_mlkem_acvp_bundled PROPERTIES TIMEOUT 120)
add_executable(dssh_test_mlkem_retirement test_mlkem_acvp.c)
set_property(TARGET dssh_test_mlkem_retirement PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_mlkem_retirement PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_mlkem_retirement PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_definitions(dssh_test_mlkem_retirement PRIVATE DSSH_TEST_MLKEM_RETIREMENT)
target_compile_options(dssh_test_mlkem_retirement PRIVATE ${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
target_link_libraries(dssh_test_mlkem_retirement PRIVATE dssh_mlkem_acvp_data)
dssh_add_output_test(dssh_mlkem_retirement dssh_test_mlkem_retirement
	"ML-KEM ACVP bundled retirement: 145 passed, 0 failed")
set_tests_properties(dssh_mlkem_retirement PROPERTIES TIMEOUT 120)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	add_executable(dssh_test_mlkem_acvp_botan test_mlkem_acvp_botan.cpp)
	set_property(TARGET dssh_test_mlkem_acvp_botan PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_mlkem_acvp_botan PROPERTY CXX_STANDARD_REQUIRED ON)
	target_compile_options(dssh_test_mlkem_acvp_botan PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_link_libraries(dssh_test_mlkem_acvp_botan PRIVATE dssh_mlkem_acvp_data ${DSSH_CRYPTO_LIB_PRIVATE})
	dssh_add_output_test(dssh_mlkem_acvp_botan dssh_test_mlkem_acvp_botan
		"ML-KEM ACVP Botan: 145 passed, 0 failed")
	set_tests_properties(dssh_mlkem_acvp_botan PROPERTIES TIMEOUT 120)
endif()

# Helper function for adding provider-backed test executables
function(dssh_add_test test_name source_file)
	add_executable(dssh_test_${test_name} ${source_file})
	# Test clocks, mkstemp(), and strdup() need the POSIX namespace in
	# strict C modes. Leave C++'s native library namespace unchanged.
	target_compile_definitions(dssh_test_${test_name} PRIVATE
		$<$<COMPILE_LANGUAGE:C>:_POSIX_C_SOURCE=200809L>)
	set_property(TARGET dssh_test_${test_name} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET dssh_test_${test_name} PROPERTY C_STANDARD_REQUIRED ON)
	target_include_directories(dssh_test_${test_name} PRIVATE
		${PROJECT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(dssh_test_${test_name} SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(dssh_test_${test_name} PRIVATE
		deucessh_test_lib ${DSSH_CRYPTO_LIB_INTERFACE} ${THREADS_LINK_LIB})
	target_compile_options(dssh_test_${test_name} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
		target_compile_definitions(dssh_test_${test_name} PRIVATE DSSH_CRYPTO_OPENSSL)
	elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
		target_compile_definitions(dssh_test_${test_name} PRIVATE DSSH_CRYPTO_BOTAN)
	endif()
endfunction()

# Compile the actual private ML-KEM wrapper with test-only endpoint names.
dssh_add_test(mlkem_wrapper test_mlkem_wrapper.c)
target_compile_options(dssh_test_mlkem_wrapper PRIVATE
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:-Wno-unused-function>)
dssh_add_output_test(dssh_mlkem_wrapper_owners dssh_test_mlkem_wrapper
	"ML-KEM wrappers: cases=223 failed=0 excluded=20 random=193 wipes=318 bytes=[0-9]+ zero_failures=0 guards=599 inputs=163 answers=200 owners=100,35,93,60,30 valid=1")
set_tests_properties(dssh_mlkem_wrapper_owners PROPERTIES TIMEOUT 120)

# Exercise the exact production ring arithmetic under unsigned size models.
foreach(model IN ITEMS native u8 u16 u32 u64)
	add_executable(dssh_test_ring_sizes_${model} test_ring_sizes.c)
	set_property(TARGET dssh_test_ring_sizes_${model} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET dssh_test_ring_sizes_${model} PROPERTY C_STANDARD_REQUIRED ON)
	target_include_directories(dssh_test_ring_sizes_${model} PRIVATE ${PROJECT_SOURCE_DIR})
	target_compile_options(dssh_test_ring_sizes_${model} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_compile_definitions(dssh_test_ring_sizes_${model} PRIVATE DSSH_RING_MODEL_NAME="${model}")
	if(NOT model STREQUAL "native")
		string(SUBSTRING ${model} 1 -1 bits)
		target_compile_definitions(dssh_test_ring_sizes_${model} PRIVATE
			DSSH_RING_SIZE_TYPE=uint${bits}_t DSSH_RING_SIZE_LIMIT=UINT${bits}_MAX
			DSSH_RING_MODEL_BITS=${bits})
	endif()
	dssh_add_output_test(dssh_ring_sizes_${model} dssh_test_ring_sizes_${model}
		"ring_sizes model=${model} width=[0-9]+ checks=[1-9][0-9]* failed=0")
endforeach()

# Exercise the exact production packet arithmetic under unsigned size models.
foreach(model IN ITEMS native u16 u32 u64)
	add_executable(dssh_test_packet_sizes_${model} test_packet_sizes.c)
	set_property(TARGET dssh_test_packet_sizes_${model} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET dssh_test_packet_sizes_${model} PROPERTY C_STANDARD_REQUIRED ON)
	target_include_directories(dssh_test_packet_sizes_${model} PRIVATE ${PROJECT_SOURCE_DIR})
	target_compile_options(dssh_test_packet_sizes_${model} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_compile_definitions(dssh_test_packet_sizes_${model} PRIVATE DSSH_PACKET_MODEL_NAME="${model}")
	if(NOT model STREQUAL "native")
		string(SUBSTRING ${model} 1 -1 bits)
		target_compile_definitions(dssh_test_packet_sizes_${model} PRIVATE
			DSSH_PACKET_SIZE_TYPE=uint${bits}_t DSSH_PACKET_SIZE_LIMIT=UINT${bits}_MAX
			DSSH_PACKET_MODEL_BITS=${bits})
	endif()
	dssh_add_output_test(dssh_packet_sizes_${model} dssh_test_packet_sizes_${model}
		"packet_sizes model=${model} width=[0-9]+ checks=[1-9][0-9]* failed=0")
endforeach()

# Exercise the exact production time arithmetic under different C type models.
foreach(model IN ITEMS i16 u16 i32 u32 i64 u64 f32 f64 fwide)
	if(model STREQUAL "i16")
		set(time_type int16_t)
		set(time_max INT16_MAX)
		set(time_min INT16_MIN)
	elseif(model STREQUAL "u16")
		set(time_type uint16_t)
		set(time_max UINT16_MAX)
		set(time_min 0)
	elseif(model STREQUAL "i32")
		set(time_type int32_t)
		set(time_max INT32_MAX)
		set(time_min INT32_MIN)
	elseif(model STREQUAL "u32")
		set(time_type uint32_t)
		set(time_max UINT32_MAX)
		set(time_min 0)
	elseif(model STREQUAL "i64")
		set(time_type int64_t)
		set(time_max INT64_MAX)
		set(time_min INT64_MIN)
	elseif(model STREQUAL "u64")
		set(time_type uint64_t)
		set(time_max UINT64_MAX)
		set(time_min 0)
	elseif(model STREQUAL "f32")
		set(time_type float)
		set(time_max FLT_MAX)
		set(time_min -FLT_MAX)
	elseif(model STREQUAL "f64")
		set(time_type double)
		set(time_max DBL_MAX)
		set(time_min -DBL_MAX)
	else()
		set(time_type "long double")
		set(time_max LDBL_MAX)
		set(time_min -LDBL_MAX)
	endif()
	dssh_add_test(time_${model} test_time.c)
	target_compile_definitions(dssh_test_time_${model} PRIVATE
		DSSH_TIME_MODEL_TYPE=${time_type}
		DSSH_TIME_MODEL_MAX=${time_max} DSSH_TIME_MODEL_MIN=${time_min})
	if(model MATCHES "^u")
		target_compile_definitions(dssh_test_time_${model} PRIVATE DSSH_TIME_MODEL_SIGNED=0)
	else()
		target_compile_definitions(dssh_test_time_${model} PRIVATE DSSH_TIME_MODEL_SIGNED=1)
	endif()
	if(model MATCHES "^f")
		target_compile_definitions(dssh_test_time_${model} PRIVATE DSSH_TIME_MODEL_FLOAT=1)
	else()
		target_compile_definitions(dssh_test_time_${model} PRIVATE DSSH_TIME_MODEL_FLOAT=0)
	endif()
	if(model MATCHES "16$")
		target_compile_definitions(dssh_test_time_${model} PRIVATE DSSH_TIME_MODEL_NARROW=1)
	else()
		target_compile_definitions(dssh_test_time_${model} PRIVATE DSSH_TIME_MODEL_NARROW=0)
	endif()
	add_test(NAME dssh_unit_time_${model} COMMAND dssh_test_time_${model})
endforeach()

# The assertion hook must run before stack-owned contexts go out of scope.
if(UNIX)
	dssh_add_test(mock_io test_mock_io.c)
	# Use the same native socket namespace as mock_io.c itself.
	get_target_property(mock_io_definitions dssh_test_mock_io COMPILE_DEFINITIONS)
	list(FILTER mock_io_definitions EXCLUDE REGEX "_POSIX_C_SOURCE")
	set_property(TARGET dssh_test_mock_io PROPERTY COMPILE_DEFINITIONS "${mock_io_definitions}")
	target_compile_definitions(dssh_test_mock_io PRIVATE _DEFAULT_SOURCE)
endif()
dssh_add_test(framework test_framework.c)
dssh_add_test(assertions test_assertions.c)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	dssh_add_test(assertions_cpp test_assertions_cpp.cpp)
	set_target_properties(dssh_test_assertions_cpp PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endif()
dssh_add_test(runner test_runner.c)
dssh_add_output_test(dssh_runner_elapsed dssh_test_runner "runner_elapsed cases=10 valid=1" elapsed)
add_test(NAME dssh_test_framework COMMAND dssh_test_framework)

# Execute the shared exact-count body with native and signed width models.
foreach(model IN ITEMS native i16 i32 i64)
	dssh_add_test(counter_${model} test_counter.c)
	target_compile_definitions(dssh_test_counter_${model} PRIVATE DSSH_TEST_COUNTER_MODEL="${model}")
	if(NOT model STREQUAL "native")
		string(SUBSTRING ${model} 1 -1 bits)
		target_compile_definitions(dssh_test_counter_${model} PRIVATE
			DSSH_TEST_COUNTER_TYPE=int${bits}_t DSSH_TEST_COUNTER_MAX=INT${bits}_MAX)
	endif()
	dssh_add_output_test(dssh_counter_${model} dssh_test_counter_${model}
		"counter model=${model} calls=1024 created=4 joined=4 boundary=1 unique=1 valid=1" normal)
endforeach()

# Exercise the five actual test-module counter bindings with real contexts.
dssh_add_test(module_counters test_module_counters.c)
add_test(NAME dssh_module_counters COMMAND dssh_test_module_counters normal)

# A shared cache needs a single writer before parallel tests start.  Unit key
# tests also load these files without going through test_generate_host_key().
dssh_add_test(key_setup test_key_setup.c)
add_test(NAME dssh_test_keys COMMAND dssh_test_key_setup)
set_tests_properties(dssh_test_keys PROPERTIES
	FIXTURES_SETUP dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}")

# Helper: register one executable × 16 KEX/key/MAC variants as CTest entries.
# Each entry runs the full executable (all tests in one process).
macro(dssh_add_variant_tests test_name executable)
	add_test(NAME ${test_name} COMMAND ${executable})
	set_tests_properties(${test_name} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_hmac256 COMMAND ${executable})
	set_tests_properties(${test_name}_hmac256 PROPERTIES
		ENVIRONMENT "DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_dhgex COMMAND ${executable})
	set_tests_properties(${test_name}_dhgex PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_rsa COMMAND ${executable})
	set_tests_properties(${test_name}_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_rsa512 COMMAND ${executable})
	set_tests_properties(${test_name}_rsa512 PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_dhgex_rsa COMMAND ${executable})
	set_tests_properties(${test_name}_dhgex_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_dhgex_rsa512 COMMAND ${executable})
	set_tests_properties(${test_name}_dhgex_rsa512 PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_sntrup COMMAND ${executable})
	set_tests_properties(${test_name}_sntrup PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=sntrup;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_sntrup_rsa COMMAND ${executable})
	set_tests_properties(${test_name}_sntrup_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_sntrup_rsa512 COMMAND ${executable})
	set_tests_properties(${test_name}_sntrup_rsa512 PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_mlkem COMMAND ${executable})
	set_tests_properties(${test_name}_mlkem PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=mlkem;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_mlkem_rsa COMMAND ${executable})
	set_tests_properties(${test_name}_mlkem_rsa PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_mlkem_rsa512 COMMAND ${executable})
	set_tests_properties(${test_name}_mlkem_rsa512 PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_rsa_hmac256 COMMAND ${executable})
	set_tests_properties(${test_name}_rsa_hmac256 PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=rsa;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_dhgex_hmac256 COMMAND ${executable})
	set_tests_properties(${test_name}_dhgex_hmac256 PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
	add_test(NAME ${test_name}_sntrup_hmac256 COMMAND ${executable})
	set_tests_properties(${test_name}_sntrup_hmac256 PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
endmacro()

# Helper: register one test per name × KEX/key/MAC variants as separate
# CTest entries.  Each entry runs one test in its own process.
# Use for test suites where per-test parallelism matters (e.g. auth
# with expensive sntrup handshakes).
macro(dssh_add_individual_tests executable prefix test_list)
	foreach(t ${${test_list}})
		add_test(NAME ${prefix}_${t}
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t} PROPERTIES
			ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_dhgex
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_dhgex PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_rsa
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_rsa512
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_rsa512 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_dhgex_rsa
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_dhgex_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_dhgex_rsa512
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_dhgex_rsa512 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_sntrup PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup_rsa
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_sntrup_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup_rsa512
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_sntrup_rsa512 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_mlkem
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_mlkem PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=mlkem;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_mlkem_rsa
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_mlkem_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_mlkem_rsa512
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_mlkem_rsa512 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_hmac256
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_hmac256 PROPERTIES
			ENVIRONMENT "DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_rsa_hmac256
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_rsa_hmac256 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEY=rsa;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_dhgex_hmac256
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_dhgex_hmac256 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup_hmac256
			COMMAND ${executable} --exact ${t})
		set_tests_properties(${prefix}_${t}_sntrup_hmac256 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
	endforeach()
endmacro()

# ================================================================
# Tier 1: Unit tests
# ================================================================

add_executable(dssh_test_portable_api test_portable_api.c)
set_target_properties(dssh_test_portable_api PROPERTIES
	C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_portable_api PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_options(dssh_test_portable_api PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
dssh_add_output_test(dssh_portable_api dssh_test_portable_api
	"portable_api spans=16777728 wipes=2145 guards=1 null_zero=1 valid=1")
set_tests_properties(dssh_portable_api PROPERTIES TIMEOUT 60)

dssh_add_test(chan test_chan.c)
add_test(NAME dssh_unit_chan COMMAND dssh_test_chan)
dssh_add_output_test(dssh_bytebuf_ring dssh_test_chan
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact bytebuf_ring_positions)


dssh_add_test(crypto test_crypto.c)
add_test(NAME dssh_unit_crypto COMMAND dssh_test_crypto)
dssh_add_test(sntrup test_sntrup.c)
foreach(mode IN ITEMS native generic)
	dssh_add_test(sntrup_deterministic_${mode} test_sntrup_arithmetic.c)
	target_compile_definitions(dssh_test_sntrup_deterministic_${mode} PRIVATE
		DSSH_SNTRUP_DETERMINISTIC
		DSSH_SNTRUP_KAT_RESPONSE="${CMAKE_CURRENT_SOURCE_DIR}/vectors/sntrup761.rsp"
		DSSH_SNTRUP_KAT_INTERMEDIATE="${CMAKE_CURRENT_SOURCE_DIR}/vectors/sntrup761.int"
		DSSH_SNTRUP_REJECTION_ANSWERS="${CMAKE_CURRENT_SOURCE_DIR}/vectors/sntrup761.reject")
	if(mode STREQUAL "generic")
		target_compile_definitions(dssh_test_sntrup_deterministic_${mode} PRIVATE DSSH_TEST_SNTRUP_GENERIC)
	endif()
	dssh_add_output_test(dssh_sntrup_deterministic_${mode} dssh_test_sntrup_deterministic_${mode}
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact kat/generation)
	set_tests_properties(dssh_sntrup_deterministic_${mode} PROPERTIES TIMEOUT 120)
	dssh_add_test(sntrup_arithmetic_${mode} test_sntrup_arithmetic.c)
	target_compile_definitions(dssh_test_sntrup_arithmetic_${mode} PRIVATE
		DSSH_SNTRUP_KAT_RESPONSE="${CMAKE_CURRENT_SOURCE_DIR}/vectors/sntrup761.rsp"
		DSSH_SNTRUP_KAT_INTERMEDIATE="${CMAKE_CURRENT_SOURCE_DIR}/vectors/sntrup761.int"
		DSSH_SNTRUP_REJECTION_ANSWERS="${CMAKE_CURRENT_SOURCE_DIR}/vectors/sntrup761.reject")
	foreach(part IN ITEMS decapsulation encoding rejection)
		dssh_add_output_test(dssh_sntrup_kat_${mode}_${part} dssh_test_sntrup_arithmetic_${mode}
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact kat/${part})
		set_tests_properties(dssh_sntrup_kat_${mode}_${part} PROPERTIES TIMEOUT 120)
	endforeach()
	if(mode STREQUAL "generic")
		target_compile_definitions(dssh_test_sntrup_arithmetic_${mode} PRIVATE DSSH_TEST_SNTRUP_GENERIC)
	endif()
	foreach(part IN ITEMS masks16 minmax_sort shifts64 field_reduction division_encoding random_words kem)
		dssh_add_output_test(dssh_sntrup_arithmetic_${mode}_${part} dssh_test_sntrup_arithmetic_${mode}
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact arithmetic/${part})
	endforeach()
endforeach()
dssh_add_output_test(dssh_sntrup_roundtrip dssh_test_sntrup "1 tests: 1 passed, 0 failed, 0 skipped"
	--exact sntrup/roundtrip)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	dssh_add_output_test(dssh_sntrup_rng_retirement dssh_test_sntrup "1 tests: 1 passed, 0 failed, 0 skipped"
		--exact sntrup/rng_retirement)
	dssh_add_output_test(dssh_sntrup_provider_errors dssh_test_sntrup "1 tests: 1 passed, 0 failed, 0 skipped"
		--exact sntrup/provider_errors)
endif()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	dssh_add_test(crypto_botan_faults test_crypto_botan_faults.cpp)
	set_property(TARGET dssh_test_crypto_botan_faults PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_crypto_botan_faults PROPERTY CXX_STANDARD_REQUIRED ON)
	add_test(NAME dssh_unit_crypto_botan_faults COMMAND dssh_test_crypto_botan_faults)
	dssh_add_test(base64_botan_faults test_base64_botan_faults.cpp)
	set_target_properties(dssh_test_base64_botan_faults PROPERTIES
		CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
	dssh_add_output_test(dssh_base64_native_faults dssh_test_base64_botan_faults
		"base64_native cases=31 failures=0")
	foreach(kind IN ITEMS static shared)
		if(TARGET deucessh_${kind})
			set(base64_target dssh_base64_storage_${kind})
			add_executable(${base64_target} test_base64_storage.cpp)
			set_property(TARGET ${base64_target} PROPERTY CXX_STANDARD 20)
			set_property(TARGET ${base64_target} PROPERTY CXX_STANDARD_REQUIRED ON)
			target_compile_options(${base64_target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			target_link_libraries(${base64_target} PRIVATE deucessh_${kind})
			dssh_add_output_test(${base64_target} ${base64_target}
				"base64_storage cases=3744 vectors=96 offsets=13 failures=0 allocation_calls=0 escapes=0")
			set_tests_properties(${base64_target} PROPERTIES TIMEOUT 60)
		endif()
	endforeach()
endif()

# Algorithm validation tests — reference helpers are backend-conditional
dssh_add_test(signature_storage test_signature_storage.c)
add_test(NAME dssh_unit_signature_storage COMMAND dssh_test_signature_storage)
if(UNIX AND NOT APPLE)
	dssh_add_test(signature_storage_gaps test_signature_storage_gaps.c)
	target_link_options(dssh_test_signature_storage_gaps PRIVATE
		-Wl,--wrap=dssh_test_malloc,--wrap=dssh_test_free)
	dssh_add_output_test(dssh_signature_storage_gaps dssh_test_signature_storage_gaps
		"4 tests: 4 passed, 0 failed, 0 skipped")
	set_tests_properties(dssh_signature_storage_gaps PROPERTIES TIMEOUT 60)
endif()

dssh_add_test(kex_workspace test_kex_workspace.c)
add_test(NAME dssh_unit_kex_workspace COMMAND dssh_test_kex_workspace)

dssh_add_test(curve_storage test_curve_storage.c)
add_test(NAME dssh_unit_curve_storage COMMAND dssh_test_curve_storage)

dssh_add_test(x25519 test_x25519.c)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_sources(dssh_test_x25519 PRIVATE x25519_fixture_botan.cpp)
	set_target_properties(dssh_test_x25519 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endif()
dssh_add_output_test(dssh_x25519_native dssh_test_x25519
	"5 tests: 5 passed, 0 failed, 0 skipped")
set_tests_properties(dssh_x25519_native PROPERTIES
	FAIL_REGULAR_EXPRESSION "AddressSanitizer;ThreadSanitizer;UndefinedBehaviorSanitizer;runtime error:")

dssh_add_test(hybrid_storage test_hybrid_storage.c)
foreach(method IN ITEMS sntrup mlkem)
	add_test(NAME dssh_unit_hybrid_storage_${method} COMMAND dssh_test_hybrid_storage)
	set_tests_properties(dssh_unit_hybrid_storage_${method} PROPERTIES
		ENVIRONMENT "DSSH_TEST_HYBRID=${method}")
endforeach()

dssh_add_test(dhgex_storage test_dhgex_storage.c)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	dssh_add_test(dh_rng test_dh_rng.c)
	target_compile_definitions(dssh_test_dh_rng PRIVATE DSSH_DH_RNG_INJECT)
	foreach(mode IN ITEMS aes128 aes192 aes256 invalid pre post)
		dssh_add_output_test(dssh_dh_rng_injected_${mode} dssh_test_dh_rng
			"dh_rng_policy mode=${mode} groups=5 attempts=10 successes=[0-9]+ refusals=[0-9]+ valid=1" ${mode})
	endforeach()
	if(TARGET deucessh_static)
		add_executable(dssh_test_dh_rng_static test_dh_rng.c)
		set_target_properties(dssh_test_dh_rng_static PROPERTIES
			C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
		target_compile_options(dssh_test_dh_rng_static PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
		target_include_directories(dssh_test_dh_rng_static PRIVATE ${PROJECT_SOURCE_DIR})
		target_link_libraries(dssh_test_dh_rng_static PRIVATE deucessh_static OpenSSL::Crypto)
		foreach(mode IN ITEMS aes128 aes192 aes256 invalid)
			dssh_add_output_test(dssh_dh_rng_static_${mode} dssh_test_dh_rng_static
				"dh_rng_policy mode=${mode} groups=5 attempts=10 successes=[0-9]+ refusals=[0-9]+ valid=1" ${mode})
		endforeach()
	endif()
endif()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	foreach(storage IN ITEMS curve_storage hybrid_storage dhgex_storage)
		target_sources(dssh_test_${storage} PRIVATE key_context_fixture_botan.cpp)
		set_target_properties(dssh_test_${storage} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
	endforeach()
endif()
add_test(NAME dssh_unit_dhgex_storage COMMAND dssh_test_dhgex_storage dh/)
set_tests_properties(dssh_unit_dhgex_storage PROPERTIES TIMEOUT 180)
set(fixed_dh_parts native_reuse failures retired_state parallel_sessions allocations bounds)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	list(APPEND fixed_dh_parts provider_failures constructor_failures)
endif()
foreach(group IN ITEMS 14 16)
	foreach(part IN LISTS fixed_dh_parts)
		add_test(NAME dssh_dh_group${group}_${part} COMMAND dssh_test_dhgex_storage --exact group${group}/${part})
		set_tests_properties(dssh_dh_group${group}_${part} PROPERTIES TIMEOUT 600)
	endforeach()
endforeach()


if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	dssh_add_test(hash_provider test_hash_provider.c)
	add_test(NAME dssh_hash_provider_lifetime COMMAND dssh_test_hash_provider)
endif()

dssh_add_test(algo_enc test_algo_enc.c)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	target_link_libraries(dssh_test_algo_enc PRIVATE OpenSSL::Crypto)
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_link_libraries(dssh_test_algo_enc PRIVATE ${DSSH_CRYPTO_LIB_PRIVATE})
endif()
add_test(NAME dssh_unit_algo_enc COMMAND dssh_test_algo_enc)
foreach(part IN ITEMS module_vectors inputs registration)
	dssh_add_output_test(dssh_cipher_cbc_${part} dssh_test_algo_enc
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact enc/aes128_cbc/${part})
endforeach()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	foreach(part IN ITEMS provider_failures module_extents)
		dssh_add_output_test(dssh_cipher_cbc_${part} dssh_test_algo_enc
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact enc/aes128_cbc/${part})
	endforeach()
endif()

dssh_add_test(algo_mac test_algo_mac.c)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	target_link_libraries(dssh_test_algo_mac PRIVATE OpenSSL::Crypto)
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_link_libraries(dssh_test_algo_mac PRIVATE ${DSSH_CRYPTO_LIB_PRIVATE})
endif()
add_test(NAME dssh_unit_algo_mac COMMAND dssh_test_algo_mac)
foreach(part IN ITEMS registration_contract module_rfc4231_vectors test_module_ownership test_module_failures)
	dssh_add_output_test(dssh_mac_${part} dssh_test_algo_mac
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact mac/${part})
endforeach()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	dssh_add_output_test(dssh_mac_native_failures dssh_test_algo_mac
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact mac/native_failures)
else()
	dssh_add_test(botan_mac_faults test_botan_mac_faults.cpp)
	set_property(TARGET dssh_test_botan_mac_faults PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_botan_mac_faults PROPERTY CXX_STANDARD_REQUIRED ON)
	dssh_add_output_test(dssh_mac_native_failures dssh_test_botan_mac_faults
		"Botan MAC boundaries: 34 passed, 0 failed")
	dssh_add_test(botan_cipher_faults test_botan_cipher_faults.cpp)
	set_property(TARGET dssh_test_botan_cipher_faults PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_botan_cipher_faults PROPERTY CXX_STANDARD_REQUIRED ON)
	dssh_add_output_test(dssh_cipher_botan_native_boundaries dssh_test_botan_cipher_faults
		"Botan cipher boundaries: 80 passed, 0 failed")
endif()

dssh_add_test(algo_key test_algo_key.c)
foreach(result_case IN ITEMS ed25519 rsa256 rsa512 concurrent)
	add_test(NAME dssh_signature_result_${result_case}
		COMMAND dssh_test_algo_key --exact result/${result_case})
	set_tests_properties(dssh_signature_result_${result_case} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
endforeach()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_sources(dssh_test_algo_key PRIVATE key_context_fixture_botan.cpp)
	set_property(TARGET dssh_test_algo_key PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_algo_key PROPERTY CXX_STANDARD_REQUIRED ON)
endif()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	target_link_libraries(dssh_test_algo_key PRIVATE OpenSSL::Crypto)
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_link_libraries(dssh_test_algo_key PRIVATE ${DSSH_CRYPTO_LIB_PRIVATE})
endif()
add_test(NAME dssh_unit_algo_key COMMAND dssh_test_algo_key)
dssh_add_test(rsa_cache test_rsa_cache.c)
dssh_add_test(key_generation test_key_generation.c)
dssh_add_test(key_publication test_key_publication.c)
foreach(part IN ITEMS initial replacement)
	add_test(NAME dssh_client_keys_fault_${part}
		COMMAND dssh_test_key_publication --exact client_${part})
endforeach()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_sources(dssh_test_key_publication PRIVATE key_publication_cpp.cpp key_file_cpp_alloc.cpp)
	set_target_properties(dssh_test_key_publication PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
	# Botan 3.13's mlock pool grows deque bookkeeping during noexcept
	# deallocation and terminates if ordinary new fails there. Exercise
	# recoverable C++ allocation failures with its optional pool disabled;
	# ordinary key tests retain the provider's default allocator.
	# Upstream: https://github.com/randombit/botan/issues/6149
	set(DSSH_TEST_CPP_FAULT_ENV "${DSSH_TEST_KEY_ENV};BOTAN_MLOCK_POOL_SIZE=0")
	add_test(NAME dssh_key_cpp_password_encrypted COMMAND dssh_test_key_publication --exact cpp_password_encrypted)
	add_test(NAME dssh_key_cpp_counts_full_password COMMAND dssh_test_key_publication --exact cpp_counts_full_password)
	set_tests_properties(dssh_key_cpp_password_encrypted PROPERTIES ENVIRONMENT "${DSSH_TEST_CPP_FAULT_ENV}" TIMEOUT 300)
	set_tests_properties(dssh_key_cpp_counts_full_password PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 300)
	foreach(part IN ITEMS generation plain encrypted)
		add_test(NAME dssh_key_cpp_counts_${part} COMMAND dssh_test_key_publication --exact cpp_counts_${part})
		set_tests_properties(dssh_key_cpp_counts_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 300)
		foreach(scope IN ITEMS prefix late context)
			add_test(NAME dssh_key_cpp_${scope}_${part} COMMAND dssh_test_key_publication --exact cpp_${scope}_${part})
			set_tests_properties(dssh_key_cpp_${scope}_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_CPP_FAULT_ENV}" TIMEOUT 1200)
		endforeach()
	endforeach()
endif()
foreach(part IN ITEMS generation plain encrypted preflight)
	add_test(NAME dssh_key_publication_${part} COMMAND dssh_test_key_publication --exact ${part})
	set_tests_properties(dssh_key_publication_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 300)
endforeach()
dssh_add_test(config_setup test_config_setup.c)
dssh_add_test(native_key_pool test_native_key_pool.c)
dssh_add_test(frozen_keys test_frozen_keys.c)
if(APPLE)
	target_compile_definitions(dssh_test_frozen_keys PRIVATE _DARWIN_C_SOURCE)
endif()
add_test(NAME dssh_frozen_keys_readers COMMAND dssh_test_frozen_keys --exact readers)
set_tests_properties(dssh_frozen_keys_readers PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 300 RUN_SERIAL TRUE)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	# The portable owner helper is a Botan production dependency. Compile the
	# same source separately for OpenSSL sanitizer ownership tests only.
	add_library(dssh_test_native_key_helper STATIC ../key_algo/native-key-pool.c)
	set_property(TARGET dssh_test_native_key_helper PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET dssh_test_native_key_helper PROPERTY C_STANDARD_REQUIRED ON)
	target_compile_definitions(dssh_test_native_key_helper PRIVATE DSSH_TESTING DSSH_CRYPTO_OPENSSL)
	target_include_directories(dssh_test_native_key_helper PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(dssh_test_native_key_helper SYSTEM PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_compile_options(dssh_test_native_key_helper PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_link_libraries(dssh_test_native_key_helper PUBLIC deucessh_test_lib)
	target_link_libraries(dssh_test_native_key_pool PRIVATE dssh_test_native_key_helper)
endif()
foreach(part IN ITEMS reuse failures retirement concurrent)
	add_test(NAME dssh_native_key_pool_${part} COMMAND dssh_test_native_key_pool --exact ${part})
	set_tests_properties(dssh_native_key_pool_${part} PROPERTIES TIMEOUT 120)
endforeach()
set_tests_properties(dssh_native_key_pool_concurrent PROPERTIES RUN_SERIAL TRUE)
foreach(part IN ITEMS allocation_failures native_failures clock_failure success callback_entry)
	add_test(NAME dssh_config_setup_${part} COMMAND dssh_test_config_setup --exact ${part})
	set_tests_properties(dssh_config_setup_${part} PROPERTIES TIMEOUT 120)
endforeach()
set(key_generation_parts invalid_sizes)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	list(APPEND key_generation_parts native_errors native_cancel)
endif()
foreach(part IN LISTS key_generation_parts)
	add_test(NAME dssh_key_generation_${part} COMMAND dssh_test_key_generation --exact ${part})
	set_tests_properties(dssh_key_generation_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
endforeach()
find_package(Python3 3.9 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
	set(ASSERTION_LANGUAGES c)
	if(TARGET dssh_test_assertions_cpp)
		list(APPEND ASSERTION_LANGUAGES cpp)
	endif()
	foreach(language IN LISTS ASSERTION_LANGUAGES)
		set(assertion_target dssh_test_assertions)
		if(language STREQUAL "cpp")
			set(assertion_target dssh_test_assertions_cpp)
		endif()
		foreach(mode IN ITEMS values eq-wide eq-mixed ne-equal unsigned-negative
			ok-wide ok-negative error-side-effect positive-error memory-different
			memory-negative memory-zero memory-null string-different string-equal
			string-null string-both-null diagnostic-error)
			add_test(NAME dssh_assertion_${language}_${mode} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/assertion_controls.py $<TARGET_FILE:${assertion_target}> ${mode})
		endforeach()
	endforeach()
endif()
if(Python3_Interpreter_FOUND AND UNIX)
	set(MOCK_IO_CONTROLS normal math queued overflow arguments disconnected zero-cap line-exact
		read-intr read-again read-zero read-over write-intr write-again write-zero write-over
		line-intr line-again line-over inject-over drain-over pair-0 pair-1)
	foreach(index RANGE 0 7)
		list(APPEND MOCK_IO_CONTROLS option-${index}-before option-${index}-after)
	endforeach()
	foreach(index RANGE 0 3)
		foreach(kind IN ITEMS close shutdown)
			list(APPEND MOCK_IO_CONTROLS ${kind}-${index}-before ${kind}-${index}-after)
		endforeach()
	endforeach()
	foreach(mode IN LISTS MOCK_IO_CONTROLS)
		add_test(NAME dssh_mock_native_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/mock_io_controls.py $<TARGET_FILE:dssh_test_mock_io> ${mode})
	endforeach()
endif()
if(Python3_Interpreter_FOUND)
	set(RUNNER_CONTROL_MODES exact all substring none missing-exact extra empty bad-option
		duplicate null-name null-function skip fail positive negative
		clock-first clock-last nano-negative nano-over reverse fraction
		write-first write-last flush-first flush-last io-real)
	if(UNIX)
		list(APPEND RUNNER_CONTROL_MODES signal-first signal-last)
	endif()
	foreach(mode IN LISTS RUNNER_CONTROL_MODES)
		add_test(NAME dssh_runner_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/runner_controls.py $<TARGET_FILE:dssh_test_runner> ${mode})
	endforeach()
endif()
if(Python3_Interpreter_FOUND AND UNIX)
	foreach(binding IN ITEMS enc-init enc-encrypt enc-decrypt mac-init mac-generate)
		foreach(mode IN ITEMS exhausted negative)
			add_test(NAME dssh_module_counter_${binding}_${mode} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/module_counter_controls.py
				$<TARGET_FILE:dssh_test_module_counters> ${binding} ${mode})
		endforeach()
	endforeach()
	foreach(model IN ITEMS native i16 i32 i64)
		foreach(mode IN ITEMS exhausted negative)
			add_test(NAME dssh_counter_${model}_${mode} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/counter_controls.py
				$<TARGET_FILE:dssh_test_counter_${model}> ${model} ${mode})
		endforeach()
	endforeach()
endif()
if(Python3_Interpreter_FOUND)
	foreach(mode IN ITEMS empty buffer-refusal buffer queue-refusal queue queue-frozen
	    params-refusal params temporary-refusal all nested normal)
		dssh_add_output_test(dssh_chan_unit_owner_${mode} Python3::Interpreter
			"chan_unit_observed mode=${mode} valid=1"
			${CMAKE_CURRENT_SOURCE_DIR}/channel_unit_controls.py $<TARGET_FILE:dssh_test_chan> ${mode})
	endforeach()
endif()
if(Python3_Interpreter_FOUND AND UNIX AND NOT APPLE)
	# Compile the canonical application sources with GNU linker controls;
	# both variants continue to link the actual production libraries.
	foreach(kind IN ITEMS static shared)
		foreach(peer IN ITEMS event accept_callback)
			set(consumer dssh_test_${peer}_peer_${kind})
			if(NOT TARGET ${consumer})
				continue()
			endif()
			set(control ${consumer}_failures)
			get_target_property(consumer_sources ${consumer} SOURCES)
			add_executable(${control} ${consumer_sources} peer_thread_failures.c)
			set_property(TARGET ${control} PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
			set_property(TARGET ${control} PROPERTY C_STANDARD_REQUIRED ON)
			target_compile_options(${control} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			target_compile_definitions(${control} PRIVATE _DEFAULT_SOURCE)
			target_link_libraries(${control} PRIVATE deucessh_${kind} ${THREADS_LINK_LIB})
			if(DSSH_THREADS_SHIM_INCLUDE)
				target_include_directories(${control} BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
			endif()
			set(create_symbol thrd_create)
			set(join_symbol thrd_join)
			if(DSSH_THREADS_SHIM_INCLUDE)
				set(create_symbol dssh_thread_thrd_create)
				set(join_symbol dssh_thread_thrd_join)
			endif()
			target_link_options(${control} PRIVATE
				"-Wl,--wrap=${create_symbol},--wrap=${join_symbol},--wrap=mock_tx_client")
			if(peer STREQUAL "event")
				set(profiles event)
			else()
				set(profiles accept nested)
			endif()
			foreach(profile IN LISTS profiles)
				set(modes plain tx)
				if(profile STREQUAL "nested")
					set(last_worker 3)
				else()
					set(last_worker 1)
				endif()
				foreach(index RANGE 0 ${last_worker})
					list(APPEND modes create-${index} join-${index}-retained join-${index}-released)
				endforeach()
				foreach(mode IN LISTS modes)
					set(name dssh_peer_ownership_${kind}_${profile}_${mode})
					add_test(NAME ${name} COMMAND ${Python3_EXECUTABLE}
						${CMAKE_CURRENT_SOURCE_DIR}/peer_thread_failures.py
						$<TARGET_FILE:${control}> ${profile} ${mode})
					set_tests_properties(${name} PROPERTIES TIMEOUT 60)
				endforeach()
			endforeach()
		endforeach()
	endforeach()
endif()
if(Python3_Interpreter_FOUND)
	dssh_add_test(key_validation test_key_validation.c)
	if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
		add_test(NAME dssh_key_validation_native_errors COMMAND dssh_test_key_validation --exact native_errors)
		set_tests_properties(dssh_key_validation_native_errors PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
	endif()
	foreach(part IN ITEMS valid invalid)
		add_test(NAME dssh_key_validation_${part} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/key_validation.py $<TARGET_FILE:dssh_test_key_validation> ${part})
		set_tests_properties(dssh_key_validation_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 300)
	endforeach()
endif()
set(rsa_cache_parts number_models layout_models)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	list(APPEND rsa_cache_parts native_numbers public_values zero_modulus zero_exponent publication_errors)
endif()
foreach(part IN LISTS rsa_cache_parts)
	add_test(NAME dssh_rsa_cache_${part} COMMAND dssh_test_rsa_cache --exact ${part})
	set_tests_properties(dssh_rsa_cache_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
endforeach()
dssh_add_test(key_text test_key_text.c)
set(key_text_parts formats file_stages extent_models chunk_boundaries)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	list(APPEND key_text_parts encoder_reports)
endif()
foreach(part IN LISTS key_text_parts)
	add_test(NAME dssh_key_text_${part} COMMAND dssh_test_key_text --exact ${part})
	set_tests_properties(dssh_key_text_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
endforeach()
dssh_add_test(key_file_import test_key_file_import.c)
if(UNIX)
	target_compile_definitions(dssh_test_key_file_import PRIVATE _XOPEN_SOURCE=700)
endif()
set(key_file_import_parts callback_replacement file_length_models close_success close_failure password_errors password_binary password_full password_empty no_implicit_password)
list(APPEND key_file_import_parts stdio_control stdio_success stdio_setup_failure stdio_close_failure)
if(UNIX)
	list(APPEND key_file_import_parts stdio_native_flush stdio_native_write_zero
		stdio_native_write_partial stdio_small_buffer_control)
endif()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	list(APPEND key_file_import_parts input_retirement stdio_short_read)
else()
	list(APPEND key_file_import_parts reference_failure)
endif()
foreach(part IN LISTS key_file_import_parts)
	add_test(NAME dssh_key_file_import_${part} COMMAND dssh_test_key_file_import --exact ${part})
	set_tests_properties(dssh_key_file_import_${part} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
endforeach()
if(UNIX)
	add_executable(dssh_key_fd_exec_probe key_fd_exec_probe.c)
	set_target_properties(dssh_key_fd_exec_probe PROPERTIES C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
	target_compile_options(dssh_key_fd_exec_probe PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_compile_definitions(dssh_key_fd_exec_probe PRIVATE _POSIX_C_SOURCE=200809L)
	dssh_add_test(key_save_policy test_key_save_policy.c)
	foreach(part IN ITEMS identity rejected nonblocking)
		add_test(NAME dssh_key_save_policy_${part} COMMAND dssh_test_key_save_policy --exact ${part})
		set_tests_properties(dssh_key_save_policy_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
	endforeach()
	if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
		foreach(part IN ITEMS acl_owner_only acl_failures acl_capability_absent acl_workspace)
			add_test(NAME dssh_key_save_policy_${part} COMMAND dssh_test_key_save_policy --exact ${part})
			set_tests_properties(dssh_key_save_policy_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
		endforeach()
	endif()
	if(APPLE)
		foreach(part IN ITEMS darwin_acl_owner_only darwin_acl_failures darwin_acl_workspace)
			add_test(NAME dssh_key_save_policy_${part} COMMAND dssh_test_key_save_policy --exact ${part})
			set_tests_properties(dssh_key_save_policy_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
		endforeach()
	endif()
	dssh_add_test(key_descriptors test_key_descriptors.c)
	if(APPLE)
		target_compile_definitions(dssh_test_key_descriptors PRIVATE _DARWIN_C_SOURCE)
	endif()
	target_compile_definitions(dssh_test_key_descriptors PRIVATE _POSIX_C_SOURCE=200809L
		DSSH_FD_EXEC_PROBE="$<TARGET_FILE:dssh_key_fd_exec_probe>")
	add_dependencies(dssh_test_key_descriptors dssh_key_fd_exec_probe)
	foreach(part IN ITEMS modes invalid_paths association_failure native_open_failure exec_controls
	    private_stages private_metadata helper_errno helper_metadata_errno native_controls)
		add_test(NAME dssh_key_descriptors_${part} COMMAND dssh_test_key_descriptors --exact ${part})
		set_tests_properties(dssh_key_descriptors_${part} PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
	endforeach()
endif()
dssh_add_test(key_wire test_key_wire.c)
foreach(part IN ITEMS known_answers ed_metadata rsa_metadata rsa_framing metadata_allocations callback_parameters bit_limits)
	add_test(NAME dssh_key_wire_${part} COMMAND dssh_test_key_wire --exact ${part})
	set_tests_properties(dssh_key_wire_${part} PROPERTIES TIMEOUT 120)
endforeach()
set_tests_properties(dssh_unit_algo_key PROPERTIES COST 20
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_targeted_dhgex_null_ka
	COMMAND dssh_test_algo_key dhgex_handler_null_ka)
add_test(NAME dssh_targeted_c25519_null_ka
	COMMAND dssh_test_algo_key c25519_handler_null_ka)
add_test(NAME dssh_targeted_sntrup761_null_ka
	COMMAND dssh_test_algo_key sntrup761_handler_null_ka)
add_test(NAME dssh_targeted_mlkem768_null_ka
	COMMAND dssh_test_algo_key mlkem768_handler_null_ka)

# Botan-specific C++ unit tests
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	dssh_add_test(mlkem_parity test_mlkem_parity.cpp)
	set_property(TARGET dssh_test_mlkem_parity PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_mlkem_parity PROPERTY CXX_STANDARD_REQUIRED ON)
	add_test(NAME dssh_unit_mlkem_parity COMMAND dssh_test_mlkem_parity)

	add_executable(dssh_test_botan_algo_key test_botan_algo_key.cpp key_file_cpp_alloc.cpp key_file_cpp_bridge.c)
	set_property(TARGET dssh_test_botan_algo_key PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET dssh_test_botan_algo_key PROPERTY C_STANDARD_REQUIRED ON)
	set_property(TARGET dssh_test_botan_algo_key PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_botan_algo_key PROPERTY CXX_STANDARD_REQUIRED ON)
	target_include_directories(dssh_test_botan_algo_key PRIVATE
		${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(dssh_test_botan_algo_key SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(dssh_test_botan_algo_key PRIVATE
		deucessh_test_lib ${DSSH_CRYPTO_LIB_PRIVATE} ${THREADS_LINK_LIB})
	target_compile_options(dssh_test_botan_algo_key PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_compile_definitions(dssh_test_botan_algo_key PRIVATE DSSH_CRYPTO_BOTAN)
	add_test(NAME dssh_unit_botan_algo_key COMMAND dssh_test_botan_algo_key)
	add_test(NAME dssh_native_key_pool_native COMMAND dssh_test_botan_algo_key --exact native_key_owners)
	set_tests_properties(dssh_native_key_pool_native PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
	add_test(NAME dssh_rsa_cache_native COMMAND dssh_test_botan_algo_key --exact rsa_cache_native)
	set_tests_properties(dssh_rsa_cache_native PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
	add_test(NAME dssh_key_text_native_reuse COMMAND dssh_test_botan_algo_key --exact key_text_reuse)
	set_tests_properties(dssh_key_text_native_reuse PROPERTIES ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 180)
	add_test(NAME dssh_key_file_import_cpp_oom COMMAND dssh_test_botan_algo_key --exact key_file_cpp_oom)
	set_tests_properties(dssh_key_file_import_cpp_oom PROPERTIES
		ENVIRONMENT "${DSSH_TEST_CPP_FAULT_ENV}" TIMEOUT 180)
	set_tests_properties(dssh_unit_botan_algo_key PROPERTIES COST 20
		ENVIRONMENT "${DSSH_TEST_CPP_FAULT_ENV}")

	add_executable(dssh_test_botan_transport test_botan_transport.cpp)
	set_property(TARGET dssh_test_botan_transport PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_botan_transport PROPERTY CXX_STANDARD_REQUIRED ON)
	target_include_directories(dssh_test_botan_transport PRIVATE
		${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(dssh_test_botan_transport SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(dssh_test_botan_transport PRIVATE
		deucessh_test_lib ${DSSH_CRYPTO_LIB_PRIVATE} ${THREADS_LINK_LIB})
	target_compile_options(dssh_test_botan_transport PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_compile_definitions(dssh_test_botan_transport PRIVATE DSSH_CRYPTO_BOTAN)
	add_test(NAME dssh_unit_botan_transport COMMAND dssh_test_botan_transport)
endif()

# ================================================================
# Tier 2: Layer tests (need mock I/O) — one process per variant
# ================================================================

dssh_add_test(transport test_transport.c)
dssh_add_test(kex_failure test_kex_failure.c)
target_link_libraries(dssh_test_kex_failure PRIVATE dssh_mock_io)
dssh_add_output_test(dssh_mock_io_shutdown dssh_test_kex_failure "1 tests: 1 passed, 0 failed, 0 skipped"
	--exact kex_failure/io_shutdown)
set_tests_properties(dssh_mock_io_shutdown PROPERTIES TIMEOUT 60
	FAIL_REGULAR_EXPRESSION "AddressSanitizer;ThreadSanitizer;UndefinedBehaviorSanitizer;runtime error:")
foreach(key IN ITEMS ed25519 rsa rsa512)
	foreach(part IN ITEMS control module inputs send_errors stopped)
		dssh_add_output_test(dssh_kex_failure_${part}_${key} dssh_test_kex_failure "1 tests: 1 passed, 0 failed, 0 skipped"
			--exact kex_failure/${part})
		set_tests_properties(dssh_kex_failure_${part}_${key} PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEY=${key};${DSSH_TEST_KEY_ENV}"
			FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 600
			FAIL_REGULAR_EXPRESSION "AddressSanitizer;ThreadSanitizer;UndefinedBehaviorSanitizer;runtime error:")
	endforeach()
endforeach()
dssh_add_test(kex_framing test_kex_framing.c)
add_test(NAME dssh_unit_kex_framing COMMAND dssh_test_kex_framing)
dssh_add_test(module_contract test_module_contract.c)
dssh_add_output_test(dssh_module_keyed_registration dssh_test_module_contract
	"keyed_admission cases=32 mismatches=0" --exact registration/keyed_callbacks)
add_test(NAME dssh_unit_module_contract COMMAND dssh_test_module_contract)
foreach(part IN ITEMS receive_extents receive_length_refusal)
	dssh_add_output_test(dssh_packet_${part} dssh_test_module_contract
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact packet/${part})
endforeach()
dssh_add_test(dhgex_contract test_dhgex_contract.c)
add_test(NAME dssh_unit_dhgex_contract COMMAND dssh_test_dhgex_contract)
target_link_libraries(dssh_test_transport PRIVATE dssh_mock_io dssh_mock_comp)
target_sources(dssh_test_transport PRIVATE dh_group_result_provider.c)
foreach(part IN ITEMS group_selection group_validation peer_validation mpint_validation provider_contract group_result_handshake group_result_reuse)
	dssh_add_output_test(dssh_dhgex_${part} dssh_test_transport "1 tests: 1 passed, 0 failed, 0 skipped"
		--exact release/dhgex_${part})
	set_tests_properties(dssh_dhgex_${part} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
dssh_add_variant_tests(dssh_transport dssh_test_transport)
set(transport_pair_leaves vex/basic vex/remote_stored
	gather/roundtrip rxline/from_rx_handshake)
foreach(suffix IN ITEMS "" _hmac256 _dhgex _rsa _rsa512 _dhgex_rsa _dhgex_rsa512
	_sntrup _sntrup_rsa _sntrup_rsa512 _mlkem _mlkem_rsa _mlkem_rsa512
	_rsa_hmac256 _dhgex_hmac256 _sntrup_hmac256)
	get_test_property(dssh_transport${suffix} ENVIRONMENT transport_pair_environment)
	dssh_add_output_test(dssh_clock_pair${suffix} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/clock_transport)
	set_tests_properties(dssh_clock_pair${suffix} PROPERTIES
		ENVIRONMENT "${transport_pair_environment}" TIMEOUT 120)
	dssh_add_output_test(dssh_offer_auth_replay${suffix} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/auth_replayed_message_state)
	set_tests_properties(dssh_offer_auth_replay${suffix} PROPERTIES
		ENVIRONMENT "${transport_pair_environment}" TIMEOUT 120)
	foreach(leaf release/auth_kbi_request_state release/auth_server_phase_gate release/auth_client_unknown_messages)
		dssh_add_output_test(dssh_auth_builder_consumer_r109_${leaf}${suffix} dssh_test_transport
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
		set_tests_properties(dssh_auth_builder_consumer_r109_${leaf}${suffix} PROPERTIES
			ENVIRONMENT "${transport_pair_environment}" TIMEOUT 120)
	endforeach()
	foreach(part IN ITEMS kex comp_s2c)
		dssh_add_output_test(dssh_negotiation_pair_${part}${suffix} dssh_test_transport
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact negotiate/no_common_${part})
		set_tests_properties(dssh_negotiation_pair_${part}${suffix} PROPERTIES
			ENVIRONMENT "${transport_pair_environment}" TIMEOUT 120)
	endforeach()
	dssh_add_output_test(dssh_language_pair${suffix} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact language/handshake_rekey)
	set_tests_properties(dssh_language_pair${suffix} PROPERTIES
		ENVIRONMENT "${transport_pair_environment}" TIMEOUT 120)
	dssh_add_output_test(dssh_transition_cipher${suffix} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/rekey_changes_algorithms)
	set_tests_properties(dssh_transition_cipher${suffix} PROPERTIES
		ENVIRONMENT "${transport_pair_environment}" TIMEOUT 120)
	dssh_add_output_test(dssh_rekey_fixture${suffix} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact rekey/owned_roundtrip)
	set_tests_properties(dssh_rekey_fixture${suffix} PROPERTIES
		ENVIRONMENT "${transport_pair_environment}" TIMEOUT 120)
	foreach(leaf IN LISTS transport_pair_leaves)
		dssh_add_output_test(dssh_transport_pair_${leaf}${suffix} dssh_test_transport
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
		set_tests_properties(dssh_transport_pair_${leaf}${suffix} PROPERTIES
			ENVIRONMENT "${transport_pair_environment}" TIMEOUT 120)
	endforeach()
endforeach()
foreach(method IN ITEMS group14 group16)
	dssh_add_output_test(dssh_clock_pair_${method} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/clock_transport)
	set_tests_properties(dssh_clock_pair_${method} PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
	dssh_add_output_test(dssh_offer_auth_replay_${method} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/auth_replayed_message_state)
	set_tests_properties(dssh_offer_auth_replay_${method} PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
	foreach(leaf release/auth_kbi_request_state release/auth_server_phase_gate release/auth_client_unknown_messages)
		dssh_add_output_test(dssh_auth_builder_consumer_r109_${leaf}_${method} dssh_test_transport
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
		set_tests_properties(dssh_auth_builder_consumer_r109_${leaf}_${method} PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
	endforeach()
	foreach(part IN ITEMS kex comp_s2c)
		dssh_add_output_test(dssh_negotiation_pair_${part}_${method} dssh_test_transport
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact negotiate/no_common_${part})
		set_tests_properties(dssh_negotiation_pair_${part}_${method} PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
	endforeach()
	dssh_add_output_test(dssh_language_pair_${method} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact language/handshake_rekey)
	set_tests_properties(dssh_language_pair_${method} PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
	dssh_add_output_test(dssh_transition_cipher_${method} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/rekey_changes_algorithms)
	set_tests_properties(dssh_transition_cipher_${method} PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
	dssh_add_output_test(dssh_rekey_fixture_${method} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact rekey/owned_roundtrip)
	set_tests_properties(dssh_rekey_fixture_${method} PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
endforeach()
dssh_add_output_test(dssh_rekey_fixture_oracle dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact rekey/sequence_oracle)
dssh_add_output_test(dssh_direct_kex_builder dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact direct_kex/builder)
dssh_add_output_test(dssh_negotiation_pair_offer dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact negotiate/offer_replace)
dssh_add_output_test(dssh_clock_constructors dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact clock/constructors)
set_tests_properties(dssh_clock_constructors PROPERTIES TIMEOUT 120)
foreach(leaf auth_state/wire_builder auth_state/queue_wire auth_state/queue_constructors)
	dssh_add_output_test(dssh_auth_builder_r109_${leaf} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
	set_tests_properties(dssh_auth_builder_r109_${leaf} PROPERTIES TIMEOUT 120)
endforeach()
set(packet_wire_leaves packet/roundtrip packet/alignment packet/min_padding packet/seq_increment
	packet/server_receives_client packet/too_large packet/bidirectional packet/minimum_payload
	packet/wire_oracle packet/wire_constructor_refusals)
foreach(leaf IN LISTS packet_wire_leaves)
	dssh_add_output_test(dssh_packet_r108_${leaf} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
	set_tests_properties(dssh_packet_r108_${leaf} PROPERTIES TIMEOUT 120)
endforeach()
foreach(leaf packet/recv_bad_padding packet/recv_too_small release/packet_framing
	packet/framing_constructor_refusals)
	dssh_add_output_test(dssh_frame_r107_${leaf} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
	set_tests_properties(dssh_frame_r107_${leaf} PROPERTIES TIMEOUT 120)
endforeach()
set(offer_parser_leaves kexinit/parse_valid kexinit/parse_control_char kexinit/parse_name_too_long
	kexinit/parse_truncated kexinit/parse_too_short kexinit/parse_first_kex_follows
	kexinit/peer_trunc_namelist release/kexinit_complete_fields kexinit/builder)
foreach(leaf IN LISTS offer_parser_leaves)
	dssh_add_output_test(dssh_offer_parser_r106_${leaf} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
	set_tests_properties(dssh_offer_parser_r106_${leaf} PROPERTIES TIMEOUT 120)
endforeach()
foreach(leaf release/full_negotiation_offers release/key_capability_selection ext_info/offers)
	dssh_add_output_test(dssh_offer_consumer_r106_${leaf} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
	set_tests_properties(dssh_offer_consumer_r106_${leaf} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
endforeach()
set(transport_state_leaves guard/blocksize_clamp_direct state/context_refusals
	callbacks/set_after_session gather/set gather/toolate getter/names_before_handshake
	session/set_global_request_cb aes256_ctr/null_ctx)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	list(APPEND transport_state_leaves aes256_ctr/encrypt_update_fail)
endif()
foreach(leaf IN LISTS transport_state_leaves)
	dssh_add_output_test(dssh_state_r102_${leaf} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
	set_tests_properties(dssh_state_r102_${leaf} PROPERTIES TIMEOUT 120)
endforeach()
foreach(leaf init/small_packet_size init/large_packet_size)
	dssh_add_output_test(dssh_packet_r103_${leaf} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
	set_tests_properties(dssh_packet_r103_${leaf} PROPERTIES TIMEOUT 120)
endforeach()
set(rekey_policy_leaves rekey/needed_false_initially rekey/needed_tx_packets
	rekey/needed_rx_packets rekey/needed_bytes rekey/needed_time rekey/seconds_disabled
	rekey/needed_below_threshold rekey/bytes_per_direction release/server_direction_limits
	guard/rekey_time_zero)
foreach(leaf IN LISTS rekey_policy_leaves)
	dssh_add_output_test(dssh_policy_r104_${leaf} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
	set_tests_properties(dssh_policy_r104_${leaf} PROPERTIES TIMEOUT 120)
endforeach()
set(direct_kex_leaves
	release/dhgex_group_validation release/dhgex_peer_validation
	dhgex/server_null_pubkey_fn dhgex/server_null_sign_fn dhgex/server_recv_fail
	dhgex/server_bad_request_type dhgex/server_short_request dhgex/server_null_provider
	dhgex/server_provider_error dhgex/server_bad_init_type dhgex/server_e_zero
	dhgex/server_recv_init_fail dhgex/server_ka_null
	c25519/server_ka_null c25519/server_null_pubkey_fn c25519/server_null_sign_fn
	c25519/server_recv_fail c25519/server_bad_init_type c25519/server_bad_qc_len
	c25519/server_qc_overrun release/c25519_peer_validation release/dhgex_provider_contract)
foreach(mac IN ITEMS hmac256 hmac512)
	foreach(kind IN ITEMS gex curve)
		dssh_add_output_test(dssh_direct_kex_${kind}_${mac} dssh_test_transport
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact direct_kex/fixture)
		set_tests_properties(dssh_direct_kex_${kind}_${mac} PROPERTIES
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_MAC=${mac};DSSH_DIRECT_KEX_KIND=${kind}" TIMEOUT 120)
	endforeach()
	foreach(leaf IN LISTS direct_kex_leaves)
		dssh_add_output_test(dssh_direct_kex_${leaf}_${mac} dssh_test_transport
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${leaf})
		set_tests_properties(dssh_direct_kex_${leaf}_${mac} PROPERTIES
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_MAC=${mac}" TIMEOUT 120)
	endforeach()
endforeach()
foreach(suffix IN ITEMS "" _hmac256 _dhgex _rsa _rsa512 _dhgex_rsa _dhgex_rsa512
	_sntrup _sntrup_rsa _sntrup_rsa512 _mlkem _mlkem_rsa _mlkem_rsa512
	_rsa_hmac256 _dhgex_hmac256 _sntrup_hmac256)
	get_test_property(dssh_transport${suffix} ENVIRONMENT registration_environment)
	dssh_add_output_test(dssh_transport_registration${suffix} dssh_test_transport
		"27 tests: 27 passed, 0 failed, 0 skipped" "register/")
	set_tests_properties(dssh_transport_registration${suffix} PROPERTIES
		ENVIRONMENT "${registration_environment}" TIMEOUT 120)
endforeach()
dssh_add_output_test(dssh_transport_registration_alloc dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/registration_module_allocation)
dssh_add_output_test(dssh_transport_test_layout dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/test_layout)
set_tests_properties(dssh_transport_test_layout PROPERTIES FIXTURES_SETUP dssh_internal_test_layout)
foreach(compression IN ITEMS none test-growth zlib)
	if(compression STREQUAL "zlib" AND NOT ZLIB_FOUND)
		continue()
	endif()
	foreach(part IN ITEMS batch batch_budget batch_failures batch_allocation batch_refill)
		dssh_add_output_test(dssh_gather_${part}_${compression} dssh_test_transport
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact gather/${part})
		set_tests_properties(dssh_gather_${part}_${compression} PROPERTIES
			ENVIRONMENT "DSSH_TEST_COMPRESSION=${compression};${DSSH_TEST_KEY_ENV}"
			FIXTURES_REQUIRED "dssh_cached_keys;dssh_internal_test_layout" TIMEOUT 120)
	endforeach()
endforeach()
foreach(method IN ITEMS curve25519 dh-gex group14 group16 sntrup mlkem)
	foreach(key IN ITEMS ed25519 rsa rsa512)
		foreach(part IN ITEMS kex auth)
			dssh_add_output_test(dssh_key_borrow_${part}_${method}_${key} dssh_test_transport
				"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/key_borrow_${part})
			set_tests_properties(dssh_key_borrow_${part}_${method}_${key} PROPERTIES
				ENVIRONMENT "DSSH_TEST_KEX=${method};DSSH_TEST_KEY=${key};${DSSH_TEST_KEY_ENV}"
				FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 120)
		endforeach()
	endforeach()
endforeach()
foreach(part IN ITEMS local bounds storage wire handshake_rekey concurrent entry_failures)
	dssh_add_output_test(dssh_language_${part} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact language/${part})
	set_tests_properties(dssh_language_${part} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 180)
endforeach()
foreach(method IN ITEMS dh-gex group14 group16 sntrup mlkem)
	dssh_add_output_test(dssh_language_handshake_rekey_${method} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact language/handshake_rekey)
	set_tests_properties(dssh_language_handshake_rekey_${method} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_KEX=${method}"
		FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 180)
endforeach()
foreach(key IN ITEMS ed25519 rsa512)
	dssh_add_output_test(dssh_cipher_cbc_rekey_${key} dssh_test_transport
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/rekey_changes_algorithms)
	set_tests_properties(dssh_cipher_cbc_rekey_${key} PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=${key};${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys
		TIMEOUT 180)
endforeach()
foreach(key IN ITEMS ed25519 rsa rsa512)
	add_test(NAME dssh_fixed_rekey_${key} COMMAND dssh_test_transport --exact release/fixed_rekey_methods)
	set_tests_properties(dssh_fixed_rekey_${key} PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEY=${key};${DSSH_TEST_KEY_ENV}"
		FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 180)
endforeach()

foreach(part IN ITEMS codec packet_bound offers advertisement receive_positions receive_rejections send_failure)
	add_test(NAME dssh_transport_ext_info_${part}
		COMMAND dssh_test_transport --exact ext_info/${part})
	set_tests_properties(dssh_transport_ext_info_${part} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys
		TIMEOUT 180)
endforeach()
foreach(part IN ITEMS notifications auth_callbacks hostkey nested demux joinable)
	dssh_add_output_test(dssh_transport_receive_entry_${part} dssh_test_transport "1 tests: 1 passed, 0 failed, 0 skipped"
		--exact receive_entry/${part})
	set_tests_properties(dssh_transport_receive_entry_${part} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys
		TIMEOUT 180)
endforeach()
dssh_add_output_test(dssh_transport_key_material dssh_test_transport "7 tests: 7 passed, 0 failed, 0 skipped"
	key_material/)
set_tests_properties(dssh_transport_key_material PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys
	TIMEOUT 180)
foreach(provider IN ITEMS none test-growth zlib)
	if(provider STREQUAL "zlib" AND NOT ZLIB_FOUND)
		continue()
	endif()
	dssh_add_output_test(dssh_transport_rekey_control_${provider} dssh_test_transport "1 tests: 1 passed, 0 failed, 0 skipped"
		--exact rekey/control_traffic)
	set_tests_properties(dssh_transport_rekey_control_${provider} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_REKEY_COMP=${provider}"
		FIXTURES_REQUIRED dssh_cached_keys
		TIMEOUT 240)
	dssh_add_output_test(dssh_transport_rekey_global_pipeline_${provider} dssh_test_transport "1 tests: 1 passed, 0 failed, 0 skipped"
		--exact rekey/global_pipeline)
	set_tests_properties(dssh_transport_rekey_global_pipeline_${provider} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_REKEY_COMP=${provider}"
		FIXTURES_REQUIRED dssh_cached_keys
		TIMEOUT 240)
endforeach()

dssh_add_output_test(dssh_transport_rekey_optout dssh_test_transport "2 tests: 2 passed, 0 failed, 0 skipped"
	rekey/optout_)
set_tests_properties(dssh_transport_rekey_optout PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 180)

dssh_add_output_test(dssh_transport_modules_stateless dssh_test_transport "1 passed, 0 failed, 0 skipped"
	--exact modules/stateless)
set_tests_properties(dssh_transport_modules_stateless PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
foreach(part IN ITEMS grammar bytes bounds setters output)
	add_test(NAME dssh_transport_identification_${part}
		COMMAND dssh_test_transport --exact identification/${part})
endforeach()
dssh_add_output_test(dssh_transport_query_arguments dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/query_arguments)
dssh_add_output_test(dssh_transport_query_snapshot dssh_test_transport "1 tests: 1 passed, 0 failed, 0 skipped"
	--exact release/query_snapshot)
set_tests_properties(dssh_transport_query_snapshot PROPERTIES TIMEOUT 120
	FAIL_REGULAR_EXPRESSION "AddressSanitizer;UndefinedBehaviorSanitizer;runtime error:;ThreadSanitizer")
dssh_add_output_test(dssh_transport_wire_trace dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact transport/wire_trace)
set_tests_properties(dssh_transport_wire_trace PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
dssh_add_output_test(dssh_kex_io_arguments dssh_test_transport
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/kex_io_arguments)
set_tests_properties(dssh_kex_io_arguments PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
foreach(part IN ITEMS expansion context_replacement auth_cleanup sensitive_failure teardown_cleanup queued_rekey scratch_failures replay_cleanup callbacks newkeys_framing mac_before_decode zlib_bounds gather_slots rekey_reuse transform_failures provider_changes setup_failures registration zlib zlib_allocations)
	add_test(NAME dssh_transport_compression_${part}
		COMMAND dssh_test_transport --exact compression/${part})
	set_tests_properties(dssh_transport_compression_${part} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()

dssh_add_test(auth test_auth.c)
dssh_add_output_test(dssh_kbi_prompt_counts dssh_test_auth
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact auth/kbi/prompt_counts)
set_tests_properties(dssh_kbi_prompt_counts PROPERTIES
	TIMEOUT 120 ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
foreach(part IN ITEMS preflight progression handshake_failure)
	dssh_add_output_test(dssh_lifecycle_${part} dssh_test_auth
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact lifecycle/${part})
	set_tests_properties(dssh_lifecycle_${part} PROPERTIES
		TIMEOUT 120 ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
endforeach()
dssh_add_output_test(dssh_auth_setup_ownership dssh_test_auth
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/auth_setup_ownership)
set_tests_properties(dssh_auth_setup_ownership PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
dssh_add_output_test(dssh_auth_nested_setup_ownership dssh_test_auth
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/auth_nested_setup_ownership)
set_tests_properties(dssh_auth_nested_setup_ownership PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
dssh_add_output_test(dssh_auth_fixture_builder dssh_test_auth
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/auth_fixture_builder)
dssh_add_output_test(dssh_auth_direct_fixture_status dssh_test_auth
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/auth_direct_fixture_status)
set_tests_properties(dssh_auth_direct_fixture_status PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 120)
add_test(NAME dssh_auth_request_capacity
	COMMAND dssh_test_auth --exact release/request_capacity)
target_link_libraries(dssh_test_auth PRIVATE dssh_mock_io dssh_mock_comp)
target_sources(dssh_test_auth PRIVATE password_result_provider.c kbi_result_provider.c signature_result_provider.c)
set(AUTH_TESTS
	auth/post_success/direct auth/post_success/wire
	auth/continuation/snapshots
	auth/continuation/storage
	auth/kbi/prompt_counts
	callbacks/banner_transaction callbacks/output_pairs callbacks/password_lifetime callbacks/password_result callbacks/signature_result callbacks/kbi_client_result callbacks/kbi_server_result
	callbacks/kbi_lifetime callbacks/client_cleanup callbacks/results callbacks/reply_failures
	framing/client_results framing/client_text framing/client_kbi framing/server_requests
	framing/server_config_update
	parse/prefix_valid parse/prefix_empty
	parse/prefix_no_service parse/prefix_trunc_service
	parse/prefix_trunc_username parse/prefix_no_method
	parse/prefix_trunc_method
	auth/password/success auth/password/failure
	auth/password/wrong_user auth/password/partial
	auth/password/empty auth/password/long
	auth/server/none_accept auth/server/none_reject
	auth/server/password_accept auth/server/password_reject
	auth/server/publickey_accept auth/server/publickey_reject
	auth/server/no_callbacks auth/server/username_captured
	auth/roundtrip/password auth/roundtrip/password_reject
	auth/roundtrip/publickey auth/roundtrip/get_methods
	auth/roundtrip/kbi auth/roundtrip/multiple_attempts
	auth/banner_delivered auth/banner_no_lang
	auth/server/password_change auth/server/publickey_probe
	auth/server/request_boundaries auth/server/request_identity auth/server/request_results
	auth/server/unknown_method auth/server/unexpected_message
	auth/kbi/zero_prompts auth/kbi/failure_response
	auth/kbi/callback_abort auth/kbi/empty_response
	auth/kbi/unexpected_message auth/kbi/banner_before_info
	auth/kbi/truncated_name_header auth/kbi/truncated_name_data
	auth/kbi/truncated_instr_header auth/kbi/truncated_instr_data
	auth/kbi/truncated_lang_header auth/kbi/truncated_lang_data
	auth/kbi/truncated_num_prompts
	auth/kbi/truncated_prompt_header auth/kbi/truncated_prompt_data
	auth/banner_truncated
	auth/get_methods_none_accepted
	auth/get_methods_fail_trunc auth/get_methods_fail_len
	auth/get_methods_fail_ctrl auth/get_methods_unexpected
	auth/get_methods_del_char auth/get_methods_small_buf
	auth/get_methods_zero_buf
	auth/server/parse_empty auth/server/parse_long_username
	auth/server/parse_no_method auth/server/parse_no_service
	auth/server/parse_not_svc_req
	auth/server/parse_trunc_method auth/server/parse_trunc_service
	auth/server/parse_trunc_user
	auth/server/parse_pw_no_bool auth/server/parse_pw_no_pass
	auth/server/parse_pw_trunc_pw
	auth/server/parse_pw_chg_no_new auth/server/parse_pw_chg_trunc
	auth/server/parse_pk_no_algo auth/server/parse_pk_no_blob
	auth/server/parse_pk_no_has_sig auth/server/parse_pk_no_sig
	auth/server/short_svc_req auth/server/tiny_svc_req
	auth/server/method_4_not_none
	auth/server/method_8_not_password
	auth/server/method_9_not_publickey
	auth/server/kbi_roundtrip
	auth/server/kbi_failure
	auth/server/kbi_multi_round
	auth/server/banner
	auth/server/set_banner_null
	auth/server/kbi_bad_response
	auth/server/kbi_trunc_response
	auth/server/kbi_trunc_lang
	auth/server/kbi_no_callback
	auth/server/kbi_with_banner
	auth/server/no_password_cb auth/server/no_publickey_cb
	auth/server/pk_probe_rejected auth/server/pk_unknown_algo
	auth/server/pk_verify_bad_sig
	auth/server/pk_sig_truncated
	auth/server/pk_sig_trunc_algo auth/server/pk_sig_trunc_blob
	auth/server/pw_change_no_cb auth/server/pw_change_rejected
	auth/server/changepw_null_prompt
	auth/server/changepw_null_prompt_chg
	auth/server/long_algo_name
	auth/server/null_username_out
	auth/server/small_username_buffer
	auth/server_send_fail/svc_accept
	auth/server_send_fail/none_success auth/server_send_fail/none_failure
	auth/server_send_fail/pw_success auth/server_send_fail/pw_failure
	auth/server_send_fail/pw_no_cb auth/server_send_fail/pw_changereq
	auth/server_send_fail/pk_success auth/server_send_fail/pk_ok
	auth/server_send_fail/pk_no_cb auth/server_send_fail/pk_unknown
	auth/server_send_fail/pk_bad_sig auth/server_send_fail/pk_reject_sig
	auth/server_send_fail/pk_probe_rej
	auth/server_send_fail/unknown_meth
	auth/server_send_fail/kbi_success auth/server_send_fail/kbi_failure
	auth/server_send_fail/kbi_info_req auth/server_send_fail/kbi_no_cb
	auth/server_send_fail/chg_success auth/server_send_fail/chg_failure
	auth/server_send_fail/chg_changereq
	auth/pw_changereq_no_cb auth/pw_changereq_loop
	auth/pw_changereq_no_lang
	auth/pw_changereq_trunc_prompt auth/pw_changereq_trunc_lang
	auth/pw_changereq_trunc_lang_data auth/pw_changereq_trunc_data
	auth/pw_unexpected_msg
	auth/service_accept_unexpected
	auth/client/svc_req_send_fail auth/client/pw_changereq_cb_error
	auth/client/pw_changereq_send_fail
	auth/client/pk_no_key auth/client/pk_pubkey_fail
	auth/client/pk_sign_fail auth/client/pk_send_fail
	auth/client/pk_recv_fail auth/client/pk_banner
	auth/client/begin_send_fail
	auth/client/kbi_send_fail auth/client/kbi_recv_fail
	direct/wrong_first_msg direct/short_svc_request
	direct/svc_slen_overflow direct/bad_prefix_in_loop
	direct/long_username direct/null_username_out_len
	direct/pw_no_bool direct/pw_no_pw_len direct/pw_overflow
	direct/pw_change_no_new direct/pw_change_overflow
	direct/pk_no_algo_len direct/pk_algo_overflow
	direct/pk_no_hassig direct/pk_no_pk_len direct/pk_pk_overflow
	direct/pk_sig_no_sig_len direct/pk_sig_overflow
	direct/pk_sig_unknown_algo
	direct/pk_verify_alloc
	dclient/wrong_svc_reply dclient/get_methods_unexpected
	dclient/get_methods_trunc_payload
	dclient/get_methods_ctrl_char dclient/get_methods_del
	dclient/changereq_no_cb dclient/changereq_trunc
	dclient/changereq_no_lang
	dclient/changereq_prompt_overflow
	dclient/changereq_lang_overflow
	dclient/get_methods_multi_banner
	dclient/changereq_cb_error
	dclient/pk_no_sign dclient/pk_alloc_fail
	dclient/kbi_alloc_fail dclient/kbi_calloc_chain
	dclient/kbi_resp_alloc
	auth/server/password_disconnect
	auth/server/none_disconnect
	auth/server/pk_probe_disconnect
	auth/server/pk_verify_disconnect
	auth/server/kbi_disconnect
	auth/server/passwd_change_disconnect
	direct/kbi_wrong_resp_type direct/kbi_resp_parse_error
	dclient/pk_no_key
	dclient/pk_banner_in_response dclient/pk_banner_parse_error
	dclient/kbi_resp_overflow dclient/kbi_resp_size_overflow
	direct/kbi_trunc_lang_hdr direct/kbi_trunc_lang_data
	direct/kbi_trunc_submethods direct/kbi_trunc_submethods_data
	auth/server/kbi_trunc_lang_hdr auth/server/kbi_trunc_submethods
	auth/server/kbi_resp_alloc auth/server/kbi_trunc_resp_body
	auth/server/kbi_excess_responses auth/client/kbi_too_many_prompts
)
dssh_add_individual_tests(dssh_test_auth dssh_auth AUTH_TESTS)

dssh_add_test(conn test_conn.c)
foreach(compression IN ITEMS none test-growth zlib)
	if(compression STREQUAL "zlib" AND NOT ZLIB_FOUND)
		continue()
	endif()
	dssh_add_output_test(dssh_gather_full_${compression} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact gather/full_batch_sha512)
	set_tests_properties(dssh_gather_full_${compression} PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=curve25519;DSSH_TEST_KEY=ed25519;DSSH_TEST_MAC=hmac512;DSSH_TEST_COMPRESSION=${compression};${DSSH_TEST_KEY_ENV}"
		FIXTURES_REQUIRED "dssh_cached_keys;dssh_internal_test_layout" TIMEOUT 120)
endforeach()
foreach(part IN ITEMS admission resize)
	dssh_add_output_test(dssh_conn_tx_slot_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/tx_slot_${part})
	set_tests_properties(dssh_conn_tx_slot_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
foreach(kind IN ITEMS key size)
	dssh_add_output_test(dssh_conn_tx_slot_refusal_${kind} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/tx_slot_refusal)
	set_tests_properties(dssh_conn_tx_slot_refusal_${kind} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
set_property(TEST dssh_conn_tx_slot_refusal_size APPEND PROPERTY ENVIRONMENT "DSSH_TEST_COMPRESSION=test-growth")
foreach(part IN ITEMS threshold inflight simultaneous failure)
	dssh_add_output_test(dssh_conn_transmit_rekey_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/transmit_rekey_${part})
	set_tests_properties(dssh_conn_transmit_rekey_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
foreach(method IN ITEMS dh-gex group14 group16 sntrup mlkem)
	dssh_add_output_test(dssh_conn_transmit_rekey_threshold_${method} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/transmit_rekey_threshold)
	set_tests_properties(dssh_conn_transmit_rekey_threshold_${method} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_KEX=${method}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
dssh_add_output_test(dssh_conn_transmit_rekey_inflight_growth dssh_test_conn
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/transmit_rekey_inflight)
set_tests_properties(dssh_conn_transmit_rekey_inflight_growth PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth" FIXTURES_REQUIRED dssh_cached_keys)
foreach(part IN ITEMS lifetime late_reply backpressure)
	dssh_add_output_test(dssh_conn_channel_id_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/channel_id_${part})
	set_tests_properties(dssh_conn_channel_id_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
foreach(part IN ITEMS queued popped confirmed late_confirmation concurrent_accept ordered_terminal)
	dssh_add_output_test(dssh_conn_peer_id_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/peer_id_${part})
	set_tests_properties(dssh_conn_peer_id_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
foreach(part IN ITEMS notification_reply_flags notification_flag_failures terminal_mode_validation terminal_mode_roundtrip terminal_mode_wire break_handler_outcomes break_handler_lifetime break_handler_failures
    break_handler_positions break_handler_queue break_handler_setup)
	foreach(profile IN ITEMS default dh-gex group14 group16 sntrup mlkem rsa rsa512 growth)
		set(extra "")
		if(profile STREQUAL "growth")
			set(extra "DSSH_TEST_COMPRESSION=test-growth")
		elseif(profile MATCHES "^rsa")
			set(extra "DSSH_TEST_KEY=${profile}")
		elseif(NOT profile STREQUAL "default")
			set(extra "DSSH_TEST_KEX=${profile}")
		endif()
		dssh_add_output_test(dssh_conn_${part}_${profile} dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/${part})
		set_tests_properties(dssh_conn_${part}_${profile} PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()
foreach(part IN ITEMS disposal_values disposal_send disposal_retirement reservation termination_callback)
	foreach(profile IN ITEMS default dh-gex group14 group16 sntrup mlkem rsa rsa512 growth)
		set(extra "")
		if(profile STREQUAL "growth")
			set(extra "DSSH_TEST_COMPRESSION=test-growth")
		elseif(profile MATCHES "^rsa")
			set(extra "DSSH_TEST_KEY=${profile}")
		elseif(NOT profile STREQUAL "default")
			set(extra "DSSH_TEST_KEX=${profile}")
		endif()
		dssh_add_output_test(dssh_conn_close_${part}_${profile} dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/close_${part})
		set_tests_properties(dssh_conn_close_${part}_${profile} PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()
foreach(part IN ITEMS wire after_eof refusal failure)
	set(control_profiles default dh-gex group14 group16 sntrup mlkem rsa rsa512)
	if(NOT part STREQUAL "refusal")
		list(APPEND control_profiles growth)
	endif()
	foreach(profile IN LISTS control_profiles)
		set(extra "")
		if(part STREQUAL "refusal" OR profile STREQUAL "growth")
			list(APPEND extra "DSSH_TEST_COMPRESSION=test-growth")
		endif()
		if(profile MATCHES "^rsa")
			list(APPEND extra "DSSH_TEST_KEY=${profile}")
		elseif(NOT profile STREQUAL "default" AND NOT profile STREQUAL "growth")
			list(APPEND extra "DSSH_TEST_KEX=${profile}")
		endif()
		dssh_add_output_test(dssh_conn_control_${part}_${profile} dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/control_${part})
		set_tests_properties(dssh_conn_control_${part}_${profile} PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()
foreach(part IN ITEMS retry concurrent failure publication)
	dssh_add_output_test(dssh_conn_half_close_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/half_close_${part})
	set_tests_properties(dssh_conn_half_close_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
	foreach(profile IN ITEMS dh-gex group14 group16 sntrup mlkem growth rsa rsa512)
		set(extra "DSSH_TEST_KEX=${profile}")
		if(profile STREQUAL "growth")
			set(extra "DSSH_TEST_COMPRESSION=test-growth")
		elseif(profile MATCHES "^rsa")
			set(extra "DSSH_TEST_KEY=${profile}")
		endif()
		dssh_add_output_test(dssh_conn_half_close_${part}_${profile} dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/half_close_${part})
		set_tests_properties(dssh_conn_half_close_${part}_${profile} PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()
foreach(part IN ITEMS direct queued buffered)
	dssh_add_output_test(dssh_conn_post_close_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/post_close_${part})
	set_tests_properties(dssh_conn_post_close_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
dssh_add_output_test(dssh_conn_post_close_capacity dssh_test_conn
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/post_close_direct)
set_tests_properties(dssh_conn_post_close_capacity PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth" FIXTURES_REQUIRED dssh_cached_keys)
dssh_add_output_test(dssh_conn_channel_worker_ownership dssh_test_conn
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/channel_worker_ownership)
set_tests_properties(dssh_conn_channel_worker_ownership PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
foreach(part IN ITEMS setup no_start)
	dssh_add_output_test(dssh_conn_${part}_ownership dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/conn_${part}_ownership)
	set_tests_properties(dssh_conn_${part}_ownership PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
foreach(consumer IN ITEMS close defaults atomic lifetime queue setup replacement incoming)
	dssh_add_output_test(dssh_conn_callback_owner_${consumer} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/callback_owner_${consumer})
	set_tests_properties(dssh_conn_callback_owner_${consumer} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
foreach(part IN ITEMS capture packet)
	dssh_add_output_test(dssh_conn_callback_${part}_bounds dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/callback_${part}_bounds)
	set_tests_properties(dssh_conn_callback_${part}_bounds PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
foreach(consumer IN ITEMS reject two accept independent concurrent terminate drop failure blocking callback credit deadline)
	dssh_add_output_test(dssh_conn_actor_owner_${consumer} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/actor_owner_${consumer})
	set_tests_properties(dssh_conn_actor_owner_${consumer} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
dssh_add_output_test(dssh_conn_zc_fixture_bounds dssh_test_conn
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/zc_fixture_bounds)
set_tests_properties(dssh_conn_zc_fixture_bounds PROPERTIES TIMEOUT 120)
foreach(consumer IN ITEMS accept client cancel close)
	dssh_add_output_test(dssh_conn_zc_owner_${consumer} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/zc_owner_${consumer})
	set_tests_properties(dssh_conn_zc_owner_${consumer} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
set(DSSH_CHANNEL_PAIR_CASES
	chan_get_pty
	channel_close_subsys
	env_callback_rejection
	env_roundtrip
	open_shell
	open_subsystem
	subsys_large_write
	subsys_multiple_writes
	subsys_poll
	subsys_poll_timeout_zero
	subsys_poll_write
	subsys_write_after_close
	subsys_write_read
	subsys_write_zero
	subsystem_roundtrip
	window_change
)
foreach(consumer IN LISTS DSSH_CHANNEL_PAIR_CASES)
	dssh_add_output_test(dssh_conn_pair_${consumer} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/pair_${consumer})
	set_tests_properties(dssh_conn_pair_${consumer} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
target_link_libraries(dssh_test_conn PRIVATE dssh_mock_io dssh_mock_comp)

# Exercise actual auth/channel fixtures with external compression. Exact-test
# output is required so a skipped setup cannot be credited as a passing case.
foreach(provider IN ITEMS test-growth zlib)
	if(provider STREQUAL "zlib" AND NOT ZLIB_FOUND)
		continue()
	endif()
	set(compressed_auth_cases
		callbacks/password_lifetime callbacks/password_result callbacks/signature_result callbacks/kbi_client_result callbacks/kbi_server_result callbacks/kbi_lifetime callbacks/client_cleanup
		callbacks/results callbacks/reply_failures framing/server_requests
		framing/client_kbi auth/roundtrip/password auth/roundtrip/kbi
		auth/server/password_change)
	set(compressed_conn_cases
		queue_retry/packet_size_refusal queue_retry/gather_size_partial
		queue_retry/packet_concurrent_key queue_retry/gather_concurrent_size
		queue_retry/packet_channel_key_partial queue_retry/gather_key_probe_write
		queue_retry/packet_key_rekey queue_retry/gather_size_rekey
		window_credit/reverse_packet_0_stream_full window_credit/reverse_gather_0_stream_full
		window_credit/reverse_packet_1_zc_full window_credit/reverse_gather_1_zc_full
		window_credit/packet_0_stream_full window_credit/gather_0_stream_full
		window_credit/packet_1_zc_full window_credit/gather_1_zc_full
		test_session_write_read test_session_read_write_reverse test_session_large_write
		test_subsys_large_write test_concurrent_write_read test_replenish_queued_accounting
		zc/open_client zc/cancel data/extended_zc_types control/unknown_replies
		lifecycle/receive_callback)
	foreach(layer IN ITEMS auth conn)
		foreach(case IN LISTS compressed_${layer}_cases)
			string(REPLACE "/" "_" case_name "${case}")
			set(test_name "dssh_${layer}_compression_${provider}_${case_name}")
			dssh_add_output_test("${test_name}" dssh_test_${layer} "1 tests: 1 passed, 0 failed, 0 skipped"
				--exact "${case}")
			set_tests_properties("${test_name}" PROPERTIES
				ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=${provider}"
				FIXTURES_REQUIRED dssh_cached_keys)
		endforeach()
	endforeach()
endforeach()
dssh_add_output_test(dssh_conn_compression_test-growth_window_rekey dssh_test_conn "1 tests: 1 passed, 0 failed, 0 skipped"
	--exact compression/window_rekey)
set_tests_properties(dssh_conn_compression_test-growth_window_rekey PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth"
	FIXTURES_REQUIRED dssh_cached_keys
	TIMEOUT 60)
set(CONN_TESTS
	release/callback_incoming_lifetime
	queue_retry/packet_stream_callback queue_retry/gather_stream_callback
	queue_retry/packet_ordinary queue_retry/packet_key_refusal queue_retry/packet_size_refusal queue_retry/packet_key_partial queue_retry/packet_size_partial queue_retry/packet_phase_pause queue_retry/packet_concurrent_key queue_retry/packet_channel_key_partial queue_retry/packet_window_size queue_retry/packet_concurrent_size queue_retry/packet_key_rekey queue_retry/packet_size_rekey queue_retry/packet_key_probe_send queue_retry/packet_key_probe_write queue_retry/packet_key_probe_slot queue_retry/packet_key_probe_rekey queue_retry/packet_size_probe_send queue_retry/packet_size_probe_write queue_retry/packet_size_probe_slot queue_retry/packet_size_probe_rekey queue_retry/gather_ordinary queue_retry/gather_key_refusal queue_retry/gather_size_refusal queue_retry/gather_key_partial queue_retry/gather_size_partial queue_retry/gather_phase_pause queue_retry/gather_concurrent_key queue_retry/gather_channel_key_partial queue_retry/gather_window_size queue_retry/gather_concurrent_size queue_retry/gather_key_rekey queue_retry/gather_size_rekey queue_retry/gather_key_probe_send queue_retry/gather_key_probe_write queue_retry/gather_key_probe_slot queue_retry/gather_key_probe_rekey queue_retry/gather_size_probe_send queue_retry/gather_size_probe_write queue_retry/gather_size_probe_slot queue_retry/gather_size_probe_rekey
	control_frame/wire_normal_window control_frame/wire_normal_eof control_frame/wire_normal_close control_frame/wire_normal_success control_frame/wire_normal_failure control_frame/wire_setup_window control_frame/wire_setup_eof control_frame/wire_setup_close control_frame/wire_setup_success control_frame/wire_setup_failure control_frame/wire_pending_success control_frame/wire_pending_failure
	control_frame/stream_normal_window control_frame/stream_normal_eof control_frame/stream_normal_close control_frame/stream_normal_success control_frame/stream_normal_failure control_frame/stream_normal_valid control_frame/stream_setup_window control_frame/stream_setup_eof control_frame/stream_setup_close control_frame/stream_setup_success control_frame/stream_setup_failure control_frame/stream_setup_valid control_frame/zc_normal_window control_frame/zc_normal_eof control_frame/zc_normal_close control_frame/zc_normal_success control_frame/zc_normal_failure control_frame/zc_normal_valid control_frame/zc_setup_window control_frame/zc_setup_eof control_frame/zc_setup_close control_frame/zc_setup_success control_frame/zc_setup_failure control_frame/zc_setup_valid control_frame/unknown control_frame/stale
	window_credit/arithmetic window_credit/atomic
	window_credit/packet_0_stream_small window_credit/packet_0_stream_full window_credit/packet_0_zc_small window_credit/packet_0_zc_full window_credit/packet_0_cancel window_credit/packet_0_oversize window_credit/packet_0_key_limit window_credit/packet_0_io_failure window_credit/packet_1_stream_small window_credit/packet_1_stream_full window_credit/packet_1_zc_small window_credit/packet_1_zc_full window_credit/packet_1_cancel window_credit/packet_1_oversize window_credit/packet_1_key_limit window_credit/packet_1_io_failure window_credit/gather_0_stream_small window_credit/gather_0_stream_full window_credit/gather_0_zc_small window_credit/gather_0_zc_full window_credit/gather_0_cancel window_credit/gather_0_oversize window_credit/gather_0_key_limit window_credit/gather_0_io_failure window_credit/gather_1_stream_small window_credit/gather_1_stream_full window_credit/gather_1_zc_small window_credit/gather_1_zc_full window_credit/gather_1_cancel window_credit/gather_1_oversize window_credit/gather_1_key_limit window_credit/gather_1_io_failure window_credit/reverse_packet_0_stream_small window_credit/reverse_packet_0_stream_full window_credit/reverse_packet_0_zc_small window_credit/reverse_packet_0_zc_full window_credit/reverse_packet_0_cancel window_credit/reverse_packet_0_oversize window_credit/reverse_packet_0_key_limit window_credit/reverse_packet_0_io_failure window_credit/reverse_packet_1_stream_small window_credit/reverse_packet_1_stream_full window_credit/reverse_packet_1_zc_small window_credit/reverse_packet_1_zc_full window_credit/reverse_packet_1_cancel window_credit/reverse_packet_1_oversize window_credit/reverse_packet_1_key_limit window_credit/reverse_packet_1_io_failure window_credit/reverse_gather_0_stream_small window_credit/reverse_gather_0_stream_full window_credit/reverse_gather_0_zc_small window_credit/reverse_gather_0_zc_full window_credit/reverse_gather_0_cancel window_credit/reverse_gather_0_oversize window_credit/reverse_gather_0_key_limit window_credit/reverse_gather_0_io_failure window_credit/reverse_gather_1_stream_small window_credit/reverse_gather_1_stream_full window_credit/reverse_gather_1_zc_small window_credit/reverse_gather_1_zc_full window_credit/reverse_gather_1_cancel window_credit/reverse_gather_1_oversize window_credit/reverse_gather_1_key_limit window_credit/reverse_gather_1_io_failure
	accept_callback/publish_before_return accept_callback/terminate_deferred accept_callback/stop_deferred accept_callback/cleanup_deferred
	accept_callback/pty_silent_positive accept_callback/pty_silent_negative accept_callback/pty_reply_positive accept_callback/pty_reply_negative accept_callback/env_silent_positive accept_callback/env_silent_negative accept_callback/env_reply_positive accept_callback/env_reply_negative accept_callback/shell_silent_positive accept_callback/shell_silent_negative accept_callback/shell_reply_positive accept_callback/shell_reply_negative accept_callback/exec_silent_positive accept_callback/exec_silent_negative accept_callback/exec_reply_positive accept_callback/exec_reply_negative accept_callback/subsystem_silent_positive accept_callback/subsystem_silent_negative accept_callback/subsystem_reply_positive accept_callback/subsystem_reply_negative accept_callback/pty_request_only accept_callback/env_request_only
	test_assertion_cleanup
	test_session_start test_session_stop test_session_start_stop
	test_session_stop_open_channel
	lifecycle/join_once lifecycle/demux_callback lifecycle/receive_callback
	lifecycle/recursive_callbacks lifecycle/tx_reservation
	clock/deadline clock/poll_accept clock/pending_waits clock/receive
	data/extended_stream_types data/extended_zc_types data/framing
	data/replenish_sync control/unknown_replies
	test_open_exec test_open_exec_command test_open_exec_rejected
	test_open_shell test_open_subsystem
	open/before_start open/type_and_queue_policy open/response_state
	open/failure_roundtrip open/sync_failures
	io/validation io/request_response_state io/sync_failures io/tx_sync_sweep
	io/registration_failures
	test_session_write_read test_session_read_write_reverse
	test_session_write_ext test_session_read_ext
	test_session_poll_read test_session_poll_timeout
	test_session_large_write test_session_multiple_writes
	test_subsys_write_read test_subsys_multiple_writes
	test_subsys_poll test_subsys_large_write
	test_send_signal test_signal_after_data test_window_change
	test_session_close_exit_code test_channel_close_subsys
	test_eof_detection test_eof_half_close test_read_nomore_vs_eof
	test_graceful_disconnect
	test_two_channels test_accept_reject test_channel_independence
	test_concurrent_write_read test_terminate_during_io
	test_connection_drop_during_read test_connection_drop_during_write
	test_cleanup_after_drop
	test_session_start_twice test_accept_timeout_zero
	test_accept_timeout_short test_session_poll_timeout_zero
	test_auto_reject_x11 test_auto_reject_forwarded_tcpip
	test_auto_reject_direct_tcpip test_auto_reject_session_server
	test_window_adjust_from_peer test_data_after_eof
	test_truncated_channel_data test_ext_data_after_eof
	test_truncated_ext_data test_channel_request_want_reply
	test_data_exceeds_window
	test_session_poll_write test_session_write_after_eof
	test_session_write_ext_after_eof test_subsys_write_after_close
	test_subsys_poll_write test_subsys_poll_timeout_zero
	test_read_event_empty test_signal_interleave_read
	test_signal_interleave_read_ext test_truncated_channel_open
	test_truncated_channel_request test_demux_short_payload
	test_session_poll_read_empty test_session_write_window_zero
	test_session_write_ext_window_zero test_accept_timeout_negative
	test_setup_exec_rejected_by_callback test_truncated_open_confirmation
	test_open_conf_unknown_channel test_channel_success_no_request
	test_double_eof_close test_data_dlen_exceeds_payload
	test_demux_chan_type_zero test_send_eof_already_sent
	test_send_close_already_sent test_replenish_after_eof
	test_replenish_low_window test_replenish_queued_accounting
	test_replenish_reserves_outstanding
	test_zc_consumption_clamped test_window_underflow_to_zero
	test_window_add_overflow
	test_chan_write_not_open test_chan_write_close_recv
	test_subsystem_roundtrip test_accept_zc test_close_during_zc_callback
	test_dispatch_sync_failures test_detach_sync_failures
	test_session_poll_nsec_overflow test_accept_nsec_overflow
	test_demux_data_truncation test_replenish_caps_buf_space
	rx_truncation/default rx_truncation/clips_to_window
	rx_bypass/pre_setup rx_bypass/disengages_post_setup
	early_data/type_lock_subsystem
	poll/terminate_surfaces_data poll/terminate_no_event_bit
	null/chan_read null/chan_write null/chan_poll null/chan_close
	null/chan_send_signal null/chan_send_window_change
	alloc/open_exec_sweep alloc/open_shell_sweep
	alloc/open_subsystem_sweep alloc/server_accept_sweep
	subsys/write_zero
	test_data_after_close test_ext_data_after_close
	test_replenish_after_close test_truncated_window_adjust
	test_poll_deadline_not_reset
	chanreq/parse_valid chanreq/parse_empty_type chanreq/parse_no_data
	chanreq/parse_truncated_type_len chanreq/parse_truncated_type
	chanreq/parse_truncated_want_reply
	zc/open_client zc/cancel null/zc_open
	event/overflow_request_notification event/overflow_eof_notification event/overflow_close_notification event/overflow_request_close_failure event/overflow_eof_close_failure event/overflow_close_close_failure event/overflow_request_normal event/overflow_eof_normal event/overflow_close_normal
	framing/name_byte_and_boolean
	framing/setup_short_length framing/setup_short_name framing/setup_short_reply framing/setup_empty framing/setup_nonascii framing/setup_zero framing/setup_comma framing/setup_long framing/normal_short_length framing/normal_short_name framing/normal_short_reply framing/normal_empty framing/normal_nonascii framing/normal_zero framing/normal_comma framing/normal_long framing/break_short framing/break_trailer framing/shell_reply framing/shell_no_reply framing/setup_unknown framing/normal_unknown framing/event_lengths_stream framing/event_lengths_zc
	params/stream_sync params/zc_sync
	params/stream_validation params/zc_validation
	params/stream_snapshot params/stream_term params/stream_command params/stream_subsystem params/stream_modes params/stream_env_array params/stream_env_name1 params/stream_env_value1 params/stream_env_name2 params/stream_env_value2 params/zc_snapshot params/zc_term params/zc_command params/zc_subsystem params/zc_modes params/zc_env_array params/zc_env_name1 params/zc_env_value1 params/zc_env_name2 params/zc_env_value2
	accept_failure/notify_failure accept_failure/success_send_failure accept_failure/window_send_failure
	accept_failure/close_deadline
	accept_failure/channel_storage accept_failure/event_storage accept_failure/param_storage accept_failure/stream_first accept_failure/stream_second accept_failure/exec_reject accept_failure/exec_no_reply accept_failure/peer_close accept_failure/shell_reject accept_failure/subsystem_reject accept_failure/buf_init accept_failure/cb_init accept_failure/cnd_init accept_failure/slot_first accept_failure/slot_second accept_failure/stream_first_no_reply accept_failure/stream_second_no_reply
	setup/confirmation_cleanup
	setup/mailbox_no_allocation setup/gap_disable setup/gap_detach setup/gap_terminate setup/gap_reacquire
	setup/mailbox_reuse setup/mailbox_growth setup/mailbox_rollback setup/mailbox_bound
	setup/terminal_storage setup/terminal_first_failure setup/terminal_second_failure
	event/text_poll_reuse
	event/text_free_observer event/text_growth_unlocked event/text_poll_race
	event/text_binary_callback event/text_binary_poll event/text_empty_callback event/text_empty_poll event/text_packet_boundary
	event/delivery_atomic_state
	event/delivery_self_disable
	event/delivery_client_defaults event/delivery_lengths event/delivery_positions event/delivery_reacquire
	event/delivery_acquire event/delivery_queue_allocation event/delivery_malformed event/delivery_close_race
	event/delivery_peer_stream event/delivery_peer_zc event/delivery_stress event/delivery_switch event/delivery_replacement event/delivery_lifecycle
	event/session_set_cb event/session_set_cb_toolate event/chan_set_cb
	event/session_set_max event/session_set_max_toolate event/chan_set_max
	pty/get_pty env/roundtrip env/callback_rejection
)
dssh_add_individual_tests(dssh_test_conn dssh_conn CONN_TESTS)
get_property(queue_retry_tests DIRECTORY PROPERTY TESTS)
foreach(case IN LISTS queue_retry_tests)
	if(case MATCHES "^dssh_conn_queue_retry/")
		set_property(TEST "${case}" APPEND PROPERTY ENVIRONMENT "DSSH_TEST_COMPRESSION=test-growth")
	endif()
endforeach()


foreach(part IN ITEMS wait_contract owner_accept_failure owner_request_frame owner_control_frame owner_accept_callback)
	dssh_add_output_test(dssh_conn_wire_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/wire_${part})
	set_tests_properties(dssh_conn_wire_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()

foreach(part IN ITEMS bytes owner_tx owner_rekey owner_window owner_queue)
	set(extra "")
	if(part STREQUAL "owner_queue")
		set(extra "DSSH_TEST_COMPRESSION=test-growth")
	endif()
	dssh_add_output_test(dssh_conn_flow_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/flow_${part})
	set_tests_properties(dssh_conn_flow_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	if(part MATCHES "^owner_(tx|rekey|window)$")
		dssh_add_output_test(dssh_conn_flow_${part}_growth dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/flow_${part})
		set_tests_properties(dssh_conn_flow_${part}_growth PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth" FIXTURES_REQUIRED dssh_cached_keys)
	endif()
endforeach()

foreach(part IN ITEMS ids peer_ids half_close setup)
	foreach(profile IN ITEMS ordinary growth)
		set(extra "")
		if(profile STREQUAL "growth")
			set(extra "DSSH_TEST_COMPRESSION=test-growth")
		endif()
		dssh_add_output_test(dssh_conn_lifecycle_owner_${part}_${profile} dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/lifecycle_owner_${part})
		set_tests_properties(dssh_conn_lifecycle_owner_${part}_${profile} PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()
dssh_add_output_test(dssh_conn_setup_packet_bounds dssh_test_conn
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact setup/packet_bounds)
set_tests_properties(dssh_conn_setup_packet_bounds PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)

foreach(part IN ITEMS text params overflow)
	foreach(profile IN ITEMS ordinary growth)
		set(extra "")
		if(profile STREQUAL "growth")
			set(extra "DSSH_TEST_COMPRESSION=test-growth")
		endif()
		dssh_add_output_test(dssh_conn_storage_owner_${part}_${profile} dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/storage_owner_${part})
		set_tests_properties(dssh_conn_storage_owner_${part}_${profile} PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()
dssh_add_output_test(dssh_conn_storage_text_pattern dssh_test_conn
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/storage_text_pattern)
set_tests_properties(dssh_conn_storage_text_pattern PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)

foreach(part IN ITEMS close reservation mode)
	foreach(profile IN ITEMS ordinary growth)
		set(extra "")
		if(profile STREQUAL "growth")
			set(extra "DSSH_TEST_COMPRESSION=test-growth")
		endif()
		dssh_add_output_test(dssh_conn_consume_owner_${part}_${profile} dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/consume_owner_${part})
		set_tests_properties(dssh_conn_consume_owner_${part}_${profile} PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()
dssh_add_output_test(dssh_conn_consume_capture_bounds dssh_test_conn
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/terminal_capture_bounds)
set_tests_properties(dssh_conn_consume_capture_bounds PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)

foreach(part IN ITEMS deferred failures retry terminal provider reservations poll concurrent)
	foreach(profile IN ITEMS ordinary growth)
		if(part STREQUAL "provider" AND profile STREQUAL "ordinary" AND DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
			continue()
		endif()
		set(extra "")
		if(profile STREQUAL "growth")
			set(extra "DSSH_TEST_COMPRESSION=test-growth")
		endif()
		dssh_add_output_test(dssh_conn_read_result_${part}_${profile} dssh_test_conn
			"1 tests: 1 passed, 0 failed, 0 skipped" --exact read_result/${part})
		set_tests_properties(dssh_conn_read_result_${part}_${profile} PROPERTIES TIMEOUT 120
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};${extra}" FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()

foreach(part IN ITEMS probe wire)
	dssh_add_output_test(dssh_conn_native_eof_${part} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/native_eof_${part})
	set_tests_properties(dssh_conn_native_eof_${part} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
endforeach()
dssh_add_output_test(dssh_conn_native_eof_wire_growth dssh_test_conn
	"1 tests: 1 passed, 0 failed, 0 skipped" --exact release/native_eof_wire)
set_tests_properties(dssh_conn_native_eof_wire_growth PROPERTIES TIMEOUT 120
	ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth" FIXTURES_REQUIRED dssh_cached_keys)

# Actual SIGABRT checks run in a fresh exec, with no inherited library workers.
if(UNIX)
	add_executable(dssh_test_fatal_driver test_fatal_driver.c)
	set_property(TARGET dssh_test_fatal_driver PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
	set_property(TARGET dssh_test_fatal_driver PROPERTY C_STANDARD_REQUIRED ON)
	target_compile_definitions(dssh_test_fatal_driver PRIVATE _POSIX_C_SOURCE=200809L)
	target_compile_options(dssh_test_fatal_driver PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	add_executable(dssh_test_fatal_child fatal_driver_child.c)
	set_target_properties(dssh_test_fatal_child PROPERTIES
		C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
	target_compile_options(dssh_test_fatal_child PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	add_test(NAME dssh_fatal_driver_policy COMMAND ${CMAKE_COMMAND}
		"-DFATAL_DRIVER=$<TARGET_FILE:dssh_test_fatal_driver>"
		"-DFATAL_CHILD=$<TARGET_FILE:dssh_test_fatal_child>"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/fatal_driver_policy.cmake")
	if(TARGET dssh_test_threads_shim_posix)
		foreach(operation IN ITEMS mutex-destroy condition-destroy)
			dssh_add_output_test(dssh_threads_shim_posix_fatal_${operation}
				dssh_test_fatal_driver "fatal_child=1"
				$<TARGET_FILE:dssh_test_threads_shim_posix> ${operation})
		endforeach()
	endif()
	set(consume_worker_counts 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0 0)
	set(consume_consumer 0)
	foreach(workers IN LISTS consume_worker_counts)
		if(workers GREATER 0)
			foreach(join RANGE 1 ${workers})
				foreach(mode IN ITEMS retained released)
					dssh_add_output_test(dssh_channel_consume_fatal_${consume_consumer}-${join}_${mode} dssh_test_fatal_driver
						"channel_join_fatal=1" $<TARGET_FILE:dssh_test_conn> --channel-fatal consume-${consume_consumer}-${join} ${mode})
					set_tests_properties(dssh_channel_consume_fatal_${consume_consumer}-${join}_${mode} PROPERTIES TIMEOUT 120
						ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth" FIXTURES_REQUIRED dssh_cached_keys)
				endforeach()
			endforeach()
		endif()
		math(EXPR consume_consumer "${consume_consumer} + 1")
	endforeach()
	set(storage_worker_counts 2 2 2 2 2 0 2 3 2 2 1 1 1 1 1 1 1 1 1 1 1 0 1 1 1 1 1 1 1 1 1 1 1 0 6 3 3 6 3 3 6 3 3)
	set(storage_consumer 0)
	foreach(workers IN LISTS storage_worker_counts)
		if(workers GREATER 0)
			foreach(join RANGE 1 ${workers})
				foreach(mode IN ITEMS retained released)
					dssh_add_output_test(dssh_channel_storage_fatal_${storage_consumer}-${join}_${mode} dssh_test_fatal_driver
						"channel_join_fatal=1" $<TARGET_FILE:dssh_test_conn> --channel-fatal storage-${storage_consumer}-${join} ${mode})
					set_tests_properties(dssh_channel_storage_fatal_${storage_consumer}-${join}_${mode} PROPERTIES TIMEOUT 120
						ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth" FIXTURES_REQUIRED dssh_cached_keys)
				endforeach()
			endforeach()
		endif()
		math(EXPR storage_consumer "${storage_consumer} + 1")
	endforeach()
	set(lifecycle_worker_counts 6 2 0 0 0 0 0 2 4 4 10 2 2 2 2 2 2 2 2 2 2 0 3 4 3 3 2)
	set(lifecycle_consumer 0)
	foreach(workers IN LISTS lifecycle_worker_counts)
		if(workers GREATER 0)
			foreach(join RANGE 1 ${workers})
				foreach(mode IN ITEMS retained released)
					dssh_add_output_test(dssh_channel_lifecycle_fatal_${lifecycle_consumer}-${join}_${mode} dssh_test_fatal_driver
						"channel_join_fatal=1" $<TARGET_FILE:dssh_test_conn> --channel-fatal lifecycle-${lifecycle_consumer}-${join} ${mode})
					set_tests_properties(dssh_channel_lifecycle_fatal_${lifecycle_consumer}-${join}_${mode} PROPERTIES TIMEOUT 120
						ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth" FIXTURES_REQUIRED dssh_cached_keys)
				endforeach()
			endforeach()
		endif()
		math(EXPR lifecycle_consumer "${lifecycle_consumer} + 1")
	endforeach()
	set(flow_worker_counts 1 4 2 1 1 1 2 2 2 2 2 2 2 1 2 2 2 2 2 2 2 1 2 2 2 2 2 2 2 1 2 2 2 2 2 2 2 1 2 2 2 2 2 2 2 1 2 2 2 2 2 2 2 1 2 2 2 2 2 2 2 1 2 2 2 2 2 2 2 1 2 2 2 2 2 2 3 2 2 3 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 3 2 2 3 2 2 2 2 2 2 2 2 2 2 2 2)
	set(flow_consumer 0)
	foreach(workers IN LISTS flow_worker_counts)
		foreach(join RANGE 1 ${workers})
			foreach(mode IN ITEMS retained released)
				dssh_add_output_test(dssh_channel_flow_fatal_${flow_consumer}-${join}_${mode} dssh_test_fatal_driver
					"channel_join_fatal=1" $<TARGET_FILE:dssh_test_conn> --channel-fatal flow-${flow_consumer}-${join} ${mode})
				set_tests_properties(dssh_channel_flow_fatal_${flow_consumer}-${join}_${mode} PROPERTIES TIMEOUT 120
					ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=test-growth" FIXTURES_REQUIRED dssh_cached_keys)
			endforeach()
		endforeach()
		math(EXPR flow_consumer "${flow_consumer} + 1")
	endforeach()
	foreach(consumer RANGE 0 79)
		foreach(join RANGE 1 2)
			foreach(mode IN ITEMS retained released)
				dssh_add_output_test(dssh_channel_wire_fatal_${consumer}-${join}_${mode} dssh_test_fatal_driver
					"channel_join_fatal=1" $<TARGET_FILE:dssh_test_conn> --channel-fatal wire-${consumer}-${join} ${mode})
				set_tests_properties(dssh_channel_wire_fatal_${consumer}-${join}_${mode} PROPERTIES TIMEOUT 120
					ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
			endforeach()
		endforeach()
	endforeach()
	foreach(operation IN ITEMS close-1 close-2 close-3 defaults-1 defaults-2 defaults-3 defaults-4 atomic-1 atomic-2 atomic-3 atomic-4 atomic-5 atomic-6 atomic-7 atomic-8 atomic-9 lifetime-1 lifetime-2 lifetime-3 lifetime-4 lifetime-5 lifetime-6 queue-1 queue-2 queue-3 queue-4 queue-5 queue-6 queue-7 queue-8 queue-9 queue-10 queue-11 queue-12 queue-13 queue-14 queue-15 queue-16 setup-1 setup-2 setup-3 setup-4 replacement-1 replacement-2 incoming-1 incoming-2 incoming-3 incoming-4 incoming-5 incoming-6 incoming-7 incoming-8 incoming-9 incoming-10 incoming-11 incoming-12)
		foreach(mode IN ITEMS retained released)
			dssh_add_output_test(dssh_channel_callback_fatal_${operation}_${mode} dssh_test_fatal_driver
				"channel_join_fatal=1" $<TARGET_FILE:dssh_test_conn> --channel-fatal callback-${operation} ${mode})
			set_tests_properties(dssh_channel_callback_fatal_${operation}_${mode} PROPERTIES TIMEOUT 120
				ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
		endforeach()
	endforeach()
	foreach(operation IN ITEMS reject-1 reject-2 two-1 accept-1 independent-1 concurrent-1 concurrent-2 concurrent-3 concurrent-4 terminate-1 terminate-2 terminate-3 drop-1 drop-2 drop-3 failure-1 blocking-1 callback-1 callback-2 credit-1 credit-2 credit-3 deadline-1 deadline-2 deadline-3)
		foreach(mode IN ITEMS retained released)
			dssh_add_output_test(dssh_channel_actor_fatal_${operation}_${mode} dssh_test_fatal_driver
				"channel_join_fatal=1" $<TARGET_FILE:dssh_test_conn> --channel-fatal actor-${operation} ${mode})
			set_tests_properties(dssh_channel_actor_fatal_${operation}_${mode} PROPERTIES TIMEOUT 120
				ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
		endforeach()
	endforeach()
	foreach(operation IN ITEMS accept-1 accept-2 client-1 client-2 cancel-1 cancel-2 close-1 close-2 close-3)
		foreach(mode IN ITEMS retained released)
			dssh_add_output_test(dssh_channel_zc_fatal_${operation}_${mode} dssh_test_fatal_driver
				"channel_join_fatal=1" $<TARGET_FILE:dssh_test_conn> --channel-fatal zc-${operation} ${mode})
			set_tests_properties(dssh_channel_zc_fatal_${operation}_${mode} PROPERTIES TIMEOUT 120
				ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
		endforeach()
	endforeach()
	foreach(consumer IN LISTS DSSH_CHANNEL_PAIR_CASES ITEMS chan_get_pty_second)
		foreach(mode IN ITEMS retained released)
			set(name dssh_channel_pair_fatal_${consumer}_${mode})
			dssh_add_output_test(${name} dssh_test_fatal_driver "channel_join_fatal=1"
				$<TARGET_FILE:dssh_test_conn> --channel-fatal pair-${consumer} ${mode})
			set_tests_properties(${name} PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
				ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
		endforeach()
	endforeach()
	foreach(operation IN ITEMS open exec shell subsystem accept)
		foreach(mode IN ITEMS retained released)
			set(name dssh_channel_worker_fatal_${operation}_${mode})
			dssh_add_output_test(${name} dssh_test_fatal_driver "channel_join_fatal=1"
				$<TARGET_FILE:dssh_test_conn> --channel-fatal ${operation} ${mode})
			set_tests_properties(${name} PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
				ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
		endforeach()
	endforeach()
	foreach(operation IN ITEMS stop cleanup)
		dssh_add_output_test(dssh_lifecycle_fatal_${operation} dssh_test_fatal_driver
			"fatal_child=1" $<TARGET_FILE:dssh_test_conn> ${operation})
		set_tests_properties(dssh_lifecycle_fatal_${operation} PROPERTIES
			FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}"
			TIMEOUT 60)
	endforeach()
endif()

# ================================================================
# Tier 1b: Alloc failure tests (need mock allocator + mock I/O)
# ossl_iterate tests are OpenSSL-only (guarded with #ifdef in source)
# The process-wide allocator injection requires the GNU --wrap linker
# option, so these tests are not available with Apple's Mach-O linker.
# ================================================================

if(NOT APPLE)
dssh_add_test(alloc test_alloc.c)
target_link_libraries(dssh_test_alloc PRIVATE dssh_mock_io dssh_mock_alloc)
target_link_options(dssh_test_alloc PRIVATE ${DSSH_WRAP_FLAGS} "-Wl,--wrap=dssh_hash_final")
dssh_add_output_test(dssh_registration_heap dssh_test_alloc
	"16 tests: 16 passed, 0 failed, 0 skipped" "alloc/register_")
add_test(NAME dssh_unit_alloc COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_dhgex COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_dhgex PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEX=dh-gex;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_rsa COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_rsa PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_dhgex_rsa COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_dhgex_rsa PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")

add_test(NAME dssh_unit_alloc_sntrup COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_sntrup PROPERTIES COST 6
	ENVIRONMENT "DSSH_TEST_KEX=sntrup;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_sntrup_rsa COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_sntrup_rsa PROPERTIES COST 7
	ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")

add_test(NAME dssh_unit_alloc_mlkem COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_mlkem PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEX=mlkem;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_mlkem_rsa COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_mlkem_rsa PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")

add_test(NAME dssh_unit_alloc_rsa512 COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_rsa512 PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_dhgex_rsa512 COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_dhgex_rsa512 PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")

# Sequential failure sweeps have exact, independently selectable completion
# controls. Keep existing aggregates; threaded/parser migration remains separate.
set(ALLOCATION_SEQUENTIAL_TESTS alloc/sweep_contract alloc/session_init)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	list(APPEND ALLOCATION_SEQUENTIAL_TESTS ossl/session_init
		ossl/key_sign_verify ossl/keygen ossl/enc ossl/mac
		ossl/key_verify ossl/key_pubkey)
endif()
foreach(allocation_key_mode IN ITEMS ed25519 rsa rsa512)
	foreach(allocation_group IN LISTS ALLOCATION_SEQUENTIAL_TESTS)
		dssh_add_output_test(dssh_alloc_sweep_${allocation_key_mode}_${allocation_group}
			dssh_test_alloc "1 tests: 1 passed, 0 failed, 0 skipped"
			--exact ${allocation_group})
		set_tests_properties(dssh_alloc_sweep_${allocation_key_mode}_${allocation_group}
			PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
			ENVIRONMENT "DSSH_TEST_KEY=${allocation_key_mode};DSSH_TEST_KEX=curve25519;${DSSH_TEST_KEY_ENV}")
	endforeach()
endforeach()

# Direct method sweeps prepare negotiation before injection and require both
# actual KEX results to agree. Setup failures never count as injected coverage.
set(ALLOCATION_DIRECT_TESTS alloc/kex_server alloc/kex_client)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	list(APPEND ALLOCATION_DIRECT_TESTS ossl/kex_server ossl/kex_client)
endif()
foreach(allocation_kex IN ITEMS curve25519 dh-gex sntrup mlkem group14 group16)
	foreach(allocation_key_mode IN ITEMS ed25519 rsa rsa512)
		foreach(allocation_group IN LISTS ALLOCATION_DIRECT_TESTS)
			dssh_add_output_test(dssh_alloc_direct_${allocation_kex}_${allocation_key_mode}_${allocation_group}
				dssh_test_alloc "1 tests: 1 passed, 0 failed, 0 skipped" --exact ${allocation_group})
			set_tests_properties(dssh_alloc_direct_${allocation_kex}_${allocation_key_mode}_${allocation_group}
				PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
				ENVIRONMENT "DSSH_TEST_KEY=${allocation_key_mode};DSSH_TEST_KEX=${allocation_kex};${DSSH_TEST_KEY_ENV}")
		endforeach()
	endforeach()
endforeach()

# Explicit allocation owners cover every supported KEX/key selection. Native
# controls operate in fresh children and never fabricate native thread handles.
set(ALLOCATION_THREADED_TESTS alloc/handshake_iterate alloc/auth_iterate alloc/conn_iterate)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	list(APPEND ALLOCATION_THREADED_TESTS ossl/handshake_iterate)
endif()
foreach(allocation_kex IN ITEMS curve25519 dh-gex sntrup mlkem group14 group16)
	foreach(allocation_key_mode IN ITEMS ed25519 rsa rsa512)
		foreach(allocation_group IN LISTS ALLOCATION_THREADED_TESTS)
			dssh_add_output_test(dssh_alloc_threaded_${allocation_kex}_${allocation_key_mode}_${allocation_group}
				dssh_test_alloc "1 tests: 1 passed, 0 failed, 0 skipped" --exact ${allocation_group})
			set_tests_properties(dssh_alloc_threaded_${allocation_kex}_${allocation_key_mode}_${allocation_group}
				PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
				ENVIRONMENT "DSSH_TEST_KEY=${allocation_key_mode};DSSH_TEST_KEX=${allocation_kex};${DSSH_TEST_KEY_ENV}")
		endforeach()
	endforeach()
endforeach()
foreach(allocation_group IN ITEMS alloc/fixture_sync alloc/fixture_watchdog alloc/fixture_owner)
	dssh_add_output_test(dssh_alloc_fixture_${allocation_group} dssh_test_alloc
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${allocation_group})
	set_tests_properties(dssh_alloc_fixture_${allocation_group} PROPERTIES
		FIXTURES_REQUIRED dssh_cached_keys
		ENVIRONMENT "DSSH_TEST_KEY=ed25519;DSSH_TEST_KEX=curve25519;${DSSH_TEST_KEY_ENV}")
endforeach()
if(Python3_Interpreter_FOUND AND UNIX)
	dssh_add_test(allocation_owner test_alloc.c)
	target_sources(dssh_test_allocation_owner PRIVATE peer_thread_failures.c)
	target_link_libraries(dssh_test_allocation_owner PRIVATE dssh_mock_io dssh_mock_alloc)
	set(allocation_create_symbol thrd_create)
	set(allocation_join_symbol thrd_join)
	if(DSSH_THREADS_SHIM_INCLUDE)
		set(allocation_create_symbol dssh_thread_thrd_create)
		set(allocation_join_symbol dssh_thread_thrd_join)
	endif()
	target_link_options(dssh_test_allocation_owner PRIVATE ${DSSH_WRAP_FLAGS} "-Wl,--wrap=dssh_hash_final"
		"-Wl,--wrap=${allocation_create_symbol},--wrap=${allocation_join_symbol},--wrap=mock_tx_client")
	set(allocation_direct_modes plain tx create-0 create-1 create-2 assert-0 assert-1 assert-2 assert-3)
	foreach(index RANGE 0 2)
		list(APPEND allocation_direct_modes join-${index}-retained join-${index}-released)
	endforeach()
	foreach(mode IN LISTS allocation_direct_modes)
		dssh_add_output_test(dssh_alloc_direct_owner_${mode} Python3::Interpreter
			"allocation_direct_observed mode=${mode} valid=1"
			${CMAKE_CURRENT_SOURCE_DIR}/allocation_direct_controls.py $<TARGET_FILE:dssh_test_allocation_owner> ${mode} direct)
		set_tests_properties(dssh_alloc_direct_owner_${mode} PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
			ENVIRONMENT "DSSH_TEST_KEY=ed25519;DSSH_TEST_KEX=curve25519;${DSSH_TEST_KEY_ENV}")
	endforeach()
	set(allocation_parser_modes ${allocation_direct_modes} skip-reply bad-request provider-error)
	foreach(mode IN LISTS allocation_parser_modes)
		dssh_add_output_test(dssh_alloc_parser_owner_${mode} Python3::Interpreter
			"allocation_parser_observed mode=${mode} valid=1"
			${CMAKE_CURRENT_SOURCE_DIR}/allocation_direct_controls.py $<TARGET_FILE:dssh_test_allocation_owner> ${mode} parser)
		set(parser_method curve25519)
		if(mode STREQUAL "provider-error")
			set(parser_method dh-gex)
		endif()
		set_tests_properties(dssh_alloc_parser_owner_${mode} PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
			ENVIRONMENT "DSSH_TEST_KEY=ed25519;DSSH_TEST_KEX=${parser_method};${DSSH_TEST_KEY_ENV}")
	endforeach()
	set(allocation_owner_modes plain tx create-0 create-1 create-2 assert-0 assert-1 assert-2 assert-3)
	foreach(index RANGE 0 2)
		list(APPEND allocation_owner_modes join-${index}-retained join-${index}-released)
	endforeach()
	foreach(site IN ITEMS gate.enter_lock gate.arrive_notify gate.enter_wait
	    gate.ready_lock gate.ready_wait gate.release_lock gate.release_notify
	    gate.cancel_lock gate.cancel_notify watch.enter_lock watch.enter_wait
	    watch.cancel_lock watch.cancel_notify)
		list(APPEND allocation_owner_modes sync-${site}-error)
	endforeach()
	foreach(site IN ITEMS gate.enter_unlock gate.ready_unlock gate.release_unlock
	    gate.cancel_unlock watch.enter_unlock watch.cancel_unlock)
		list(APPEND allocation_owner_modes sync-${site}-retained sync-${site}-released)
	endforeach()
	foreach(mode IN LISTS allocation_owner_modes)
		add_test(NAME dssh_alloc_owner_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/allocation_owner_controls.py
			$<TARGET_FILE:dssh_test_allocation_owner> ${mode})
		set_tests_properties(dssh_alloc_owner_${mode} PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
			ENVIRONMENT "DSSH_TEST_KEY=ed25519;DSSH_TEST_KEX=curve25519;${DSSH_TEST_KEY_ENV}")
	endforeach()
endif()

# Private verification-context models require actual native H agreement and
# before/after full KEX controls for every supported method and key family.
foreach(method IN ITEMS curve25519 dh-gex group14 group16 sntrup mlkem)
	foreach(key IN ITEMS ed25519 rsa rsa512)
		foreach(part IN ITEMS kex_missing_key kex_missing_verify)
			dssh_add_output_test(dssh_alloc_guard_${method}_${key}_${part} dssh_test_alloc
				"1 tests: 1 passed, 0 failed, 0 skipped" --exact guard/${part})
			set_tests_properties(dssh_alloc_guard_${method}_${key}_${part} PROPERTIES
				FIXTURES_REQUIRED dssh_cached_keys
				ENVIRONMENT "DSSH_TEST_KEY=${key};DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}")
		endforeach()
	endforeach()
endforeach()
set(ALLOCATION_STANDALONE_TESTS guard/kbi_null_callback guard/acceptqueue alloc/kexinit)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_curve25519_sha256)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_dh_gex_sha256)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_dh_group14_sha256)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_dh_group16_sha512)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_sntrup761x25519_sha512)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_mlkem768x25519_sha256)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_ssh_ed25519)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_rsa_sha2_256)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_rsa_sha2_512)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_aes256_ctr)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_aes128_cbc)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_none_enc)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_hmac_sha2_256)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_hmac_sha2_512)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_none_mac)
list(APPEND ALLOCATION_STANDALONE_TESTS alloc/register_none_comp)
foreach(group IN LISTS ALLOCATION_STANDALONE_TESTS)
	dssh_add_output_test(dssh_alloc_standalone_${group} dssh_test_alloc
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${group})
	set_tests_properties(dssh_alloc_standalone_${group} PROPERTIES FIXTURES_REQUIRED dssh_cached_keys
		ENVIRONMENT "DSSH_TEST_KEY=ed25519;DSSH_TEST_KEX=curve25519;${DSSH_TEST_KEY_ENV}")
endforeach()

# Each parser contract has exact method/key bindings and rejects fixture errors.
set(ALLOCATION_CURVE_PARSE_TESTS
	c25519/client_recv_reply c25519/client_short_ks c25519/client_trunc_ks
	c25519/client_short_qs c25519/client_bad_qs_len c25519/client_qs_overrun
	c25519/client_short_sig c25519/client_trunc_sig c25519/server_truncated_init
	c25519/client_bad_qs_length)
set(ALLOCATION_GEX_PARSE_TESTS
	dhgex/client_recv_group_fail dhgex/client_group_empty dhgex/client_group_no_g
	dhgex/client_group_too_small dhgex/client_bad_reply_type dhgex/client_short_ks
	dhgex/client_trunc_ks dhgex/client_reply_f_zero dhgex/client_short_sig dhgex/client_trunc_sig)
foreach(method IN ITEMS curve25519 dh-gex)
	set(parser_groups ${ALLOCATION_CURVE_PARSE_TESTS})
	if(method STREQUAL "dh-gex")
		set(parser_groups ${ALLOCATION_GEX_PARSE_TESTS})
	endif()
	foreach(key IN ITEMS ed25519 rsa rsa512)
		foreach(group IN LISTS parser_groups)
			dssh_add_output_test(dssh_alloc_parser_${method}_${key}_${group} dssh_test_alloc
				"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${group})
			set_tests_properties(dssh_alloc_parser_${method}_${key}_${group} PROPERTIES
				FIXTURES_REQUIRED dssh_cached_keys
				ENVIRONMENT "DSSH_TEST_KEY=${key};DSSH_TEST_KEX=${method};${DSSH_TEST_KEY_ENV}")
		endforeach()
	endforeach()
endforeach()
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	foreach(method IN ITEMS dh-gex group14 group16)
		foreach(key IN ITEMS ed25519 rsa rsa512)
			dssh_add_output_test(dssh_dh_rng_method_${method}_${key} dssh_test_alloc
				"1 tests: 1 passed, 0 failed, 0 skipped" --exact dh/private_rng_fail)
			set_tests_properties(dssh_dh_rng_method_${method}_${key} PROPERTIES
				FIXTURES_REQUIRED dssh_cached_keys
				ENVIRONMENT "DSSH_TEST_KEX=${method};DSSH_TEST_KEY=${key};${DSSH_TEST_KEY_ENV}")
		endforeach()
	endforeach()
endif()
endif()

# ================================================================
# Test algorithm modules (enc/mac with failure injection)
# ================================================================

add_library(dssh_test_algos STATIC test_enc.c test_mac.c)
set_property(TARGET dssh_test_algos PROPERTY C_STANDARD ${DEUCESSH_C_STANDARD})
set_property(TARGET dssh_test_algos PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_algos PUBLIC
	${PROJECT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(dssh_test_algos PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
target_link_libraries(dssh_test_algos PRIVATE deucessh_test_lib)
target_link_libraries(dssh_test_algo_mac PRIVATE dssh_test_algos)

# ================================================================
# Tier 2b: Transport error tests (need test enc/mac + mock I/O)
# ================================================================

dssh_add_test(transport_errors test_transport_errors.c)
target_link_libraries(dssh_test_transport_errors PRIVATE
	dssh_mock_io dssh_test_algos)
set(TRANSPORT_ERROR_TESTS
	fixture/setup_allocations fixture/ownership
	test_modules/roundtrip
	send/encrypt_error send/mac_error send/mac_overflow_rejected
	send/sensitive_success send/sensitive_gather_success
	send/sensitive_encrypt_error send/sensitive_too_long
	send/channel_slot_drain_fatal send/channel_slot_gather_fatal
	recv/decrypt_error_first_block recv/decrypt_error_remaining
	recv/mac_generate_error recv/mac_mismatch recv/mac_too_large
	newkeys/enc_init_failure newkeys/mac_init_failure
)
foreach(t ${TRANSPORT_ERROR_TESTS})
	dssh_add_output_test(dssh_transport_errors_${t} dssh_test_transport_errors
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact ${t})
	set_tests_properties(dssh_transport_errors_${t} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}"
		FAIL_REGULAR_EXPRESSION "AddressSanitizer;ThreadSanitizer;UndefinedBehaviorSanitizer;runtime error:")
endforeach()
if(Python3_Interpreter_FOUND AND UNIX AND NOT APPLE)
	dssh_add_test(transport_fixture_owner test_transport_errors.c)
	target_sources(dssh_test_transport_fixture_owner PRIVATE peer_thread_failures.c)
	target_link_libraries(dssh_test_transport_fixture_owner PRIVATE dssh_mock_io dssh_test_algos)
	set(create_symbol thrd_create)
	set(join_symbol thrd_join)
	if(DSSH_THREADS_SHIM_INCLUDE)
		set(create_symbol dssh_thread_thrd_create)
		set(join_symbol dssh_thread_thrd_join)
	endif()
	target_link_options(dssh_test_transport_fixture_owner PRIVATE
		"-Wl,--wrap=${create_symbol},--wrap=${join_symbol},--wrap=mock_tx_client")
	set(owner_modes plain tx create-0 create-1 assert-0 assert-1 assert-2)
	foreach(index RANGE 0 1)
		list(APPEND owner_modes join-${index}-retained join-${index}-released)
	endforeach()
	foreach(mode IN LISTS owner_modes)
		add_test(NAME dssh_fixture_owner_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/fixture_owner_controls.py
			$<TARGET_FILE:dssh_test_transport_fixture_owner> ${mode})
	endforeach()
	dssh_add_test(transport_pair_owner test_transport.c)
	target_sources(dssh_test_transport_pair_owner PRIVATE peer_thread_failures.c dh_group_result_provider.c)
	target_compile_definitions(dssh_test_transport_pair_owner PRIVATE DSSH_TRANSPORT_PAIR_CONTROLS)
	target_link_libraries(dssh_test_transport_pair_owner PRIVATE dssh_mock_io dssh_mock_comp)
	target_link_options(dssh_test_transport_pair_owner PRIVATE
		"-Wl,--wrap=${create_symbol},--wrap=${join_symbol},--wrap=mock_tx_client")
	foreach(kind RANGE 0 3)
		foreach(mode IN LISTS owner_modes)
			add_test(NAME dssh_transport_pair_owner_${kind}_${mode} COMMAND ${CMAKE_COMMAND} -E env
				"DSSH_TRANSPORT_PAIR_KIND=${kind}" ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/fixture_owner_controls.py
				$<TARGET_FILE:dssh_test_transport_pair_owner> ${mode})
			set_tests_properties(dssh_transport_pair_owner_${kind}_${mode} PROPERTIES
				ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
		endforeach()
	endforeach()
	dssh_add_test(transport_rekey_owner test_transport.c)
	target_sources(dssh_test_transport_rekey_owner PRIVATE peer_thread_failures.c dh_group_result_provider.c)
	target_compile_definitions(dssh_test_transport_rekey_owner PRIVATE
		DSSH_TRANSPORT_REKEY_CONTROLS DSSH_PEER_DEFER_CONTROL)
	target_link_libraries(dssh_test_transport_rekey_owner PRIVATE dssh_mock_io dssh_mock_comp)
	target_link_options(dssh_test_transport_rekey_owner PRIVATE
		"-Wl,--wrap=${create_symbol},--wrap=${join_symbol},--wrap=mock_tx_client")
	foreach(mode IN LISTS owner_modes)
		add_test(NAME dssh_language_owner_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/fixture_owner_controls.py
			$<TARGET_FILE:dssh_test_transport_rekey_owner> ${mode})
		set_tests_properties(dssh_language_owner_${mode} PROPERTIES
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_LANGUAGE_OWNER=1" TIMEOUT 120)
		add_test(NAME dssh_rekey_fixture_owner_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/fixture_owner_controls.py
			$<TARGET_FILE:dssh_test_transport_rekey_owner> ${mode})
		set_tests_properties(dssh_rekey_fixture_owner_${mode} PROPERTIES
			ENVIRONMENT "${DSSH_TEST_KEY_ENV}" TIMEOUT 120)
	endforeach()
	foreach(kind IN ITEMS gex curve)
		foreach(mode IN LISTS owner_modes)
			add_test(NAME dssh_direct_kex_owner_${kind}_${mode} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/fixture_owner_controls.py
				$<TARGET_FILE:dssh_test_transport_pair_owner> ${mode})
			set_tests_properties(dssh_direct_kex_owner_${kind}_${mode} PROPERTIES
				ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_DIRECT_KEX_KIND=${kind}" TIMEOUT 120)
		endforeach()
	endforeach()
	foreach(kind IN ITEMS kex comp)
		foreach(mode IN LISTS owner_modes)
			add_test(NAME dssh_negotiation_pair_owner_${kind}_${mode} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/fixture_owner_controls.py
				$<TARGET_FILE:dssh_test_transport_pair_owner> ${mode})
			set_tests_properties(dssh_negotiation_pair_owner_${kind}_${mode} PROPERTIES
				ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_NEGOTIATION_KIND=${kind}" TIMEOUT 120)
		endforeach()
	endforeach()
	foreach(mode IN LISTS owner_modes)
		add_test(NAME dssh_clock_pair_owner_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/fixture_owner_controls.py
			$<TARGET_FILE:dssh_test_transport_pair_owner> ${mode})
		set_tests_properties(dssh_clock_pair_owner_${mode} PROPERTIES
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_CLOCK_PAIR_OWNER=1" TIMEOUT 120)
	endforeach()
	foreach(kind IN ITEMS cipher fixed)
		if(kind STREQUAL "fixed")
			set(transition_rounds 0 1 2)
		else()
			set(transition_rounds 0 1)
		endif()
		foreach(round IN LISTS transition_rounds)
			foreach(mode IN LISTS owner_modes)
				add_test(NAME dssh_transition_owner_${kind}_${round}_${mode} COMMAND ${Python3_EXECUTABLE}
					${CMAKE_CURRENT_SOURCE_DIR}/fixture_owner_controls.py
					$<TARGET_FILE:dssh_test_transport_rekey_owner> ${mode})
				set_tests_properties(dssh_transition_owner_${kind}_${round}_${mode} PROPERTIES
					ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TRANSITION_KIND=${kind};DSSH_TRANSITION_ROUND=${round}"
					TIMEOUT 120)
			endforeach()
		endforeach()
	endforeach()
endif()
set_tests_properties(
	dssh_transport_errors_send/channel_slot_drain_fatal
	dssh_transport_errors_send/channel_slot_gather_fatal
	PROPERTIES TIMEOUT 15)

# ================================================================
# Tier 2c: Thread error tests (lock/unlock/condvar failure injection)
# ================================================================

dssh_add_test(thread_errors test_thread_errors.c)
target_link_libraries(dssh_test_thread_errors PRIVATE dssh_mock_io)
dssh_add_variant_tests(dssh_thrd dssh_test_thread_errors)
set(WAIT_CONTRACT_TESTS thrd/wait_contract_errors thrd/wait_contract_spurious
	thrd/wait_contract_timeout_errors thrd/wait_contract_timeouts)
dssh_add_individual_tests(dssh_test_thread_errors dssh_wait WAIT_CONTRACT_TESTS)
if(UNIX)
	foreach(path IN ITEMS primitive event limit eof close log-queue log-empty log-data wa-idle wa-zero wa-send)
		foreach(mode IN ITEMS held released)
			dssh_add_output_test(dssh_unlock_fatal_${path}_${mode} dssh_test_fatal_driver
				"fatal_child=1" $<TARGET_FILE:dssh_test_thread_errors>
				--unlock-fatal ${path} ${mode})
			set_tests_properties(dssh_unlock_fatal_${path}_${mode} PROPERTIES
				FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}"
				TIMEOUT 60)
		endforeach()
	endforeach()
	foreach(path IN ITEMS lock-tx lock-slot lock-accept lock-registry lock-channel
		notify-tx notify-slot notify-accept notify-channel retry-notify)
		foreach(mode IN ITEMS before after)
			dssh_add_output_test(dssh_wake_fatal_${path}_${mode} dssh_test_fatal_driver
				"fatal_child=1" $<TARGET_FILE:dssh_test_thread_errors>
				--wake-fatal ${path} ${mode})
			set_tests_properties(dssh_wake_fatal_${path}_${mode} PROPERTIES TIMEOUT 30)
		endforeach()
	endforeach()
endif()

# ================================================================
# Tier 3a: Self-test (integration) — one process per case and variant
# ================================================================

dssh_add_test(selftest test_selftest.c)
target_link_libraries(dssh_test_selftest PRIVATE dssh_mock_comp)
if(Python3_Interpreter_FOUND AND UNIX AND NOT APPLE)
	dssh_add_test(selftest_owner test_selftest_owner.c)
	target_link_libraries(dssh_test_selftest_owner PRIVATE dssh_mock_comp)
	foreach(mode IN ITEMS rekey-data-native rekey-data-omitted rekey-manual-native
		rekey-manual-omitted rekey-channels-native rekey-channels-omitted
		rekey-inflight-native rekey-inflight-omitted rekey-boundaries)
		add_test(NAME dssh_selftest_rekey_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/selftest_rekey_controls.py
			$<TARGET_FILE:dssh_test_selftest_owner> ${mode})
		set_tests_properties(dssh_selftest_rekey_${mode} PROPERTIES
			FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=none")
	endforeach()
	foreach(mode IN ITEMS metadata-rekey-native metadata-rekey-corrupt metadata-rekey-worker
		metadata-sid-native metadata-sid-zero metadata-sid-null metadata-sid-oversize
		metadata-sid-corrupt metadata-sid-length metadata-clock-start metadata-clock-end
		metadata-getter-kex metadata-getter-key metadata-getter-enc metadata-getter-mac
		metadata-getter-comp metadata-getter-direction metadata-boundaries)
		add_test(NAME dssh_selftest_metadata_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/selftest_metadata_controls.py
			$<TARGET_FILE:dssh_test_selftest_owner> ${mode})
		set_tests_properties(dssh_selftest_metadata_${mode} PROPERTIES
			FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
	endforeach()
	target_link_options(dssh_test_selftest_owner PRIVATE "-Wl,--wrap=send_commit"
		"-Wl,--wrap=recv_packet")
	set(selftest_constructor_mutex_destroy mtx_destroy)
	set(selftest_constructor_condition_destroy cnd_destroy)
	if(DSSH_THREADS_SHIM_INCLUDE)
		set(selftest_constructor_mutex_destroy dssh_thread_mtx_destroy)
		set(selftest_constructor_condition_destroy dssh_thread_cnd_destroy)
	endif()
	foreach(symbol IN ITEMS dssh_test_malloc dssh_test_realloc dssh_test_free
		dssh_test_mtx_init dssh_test_cnd_init dssh_test_channel_observe_end
		dssh_test_wait_observe_end ${selftest_constructor_mutex_destroy}
		${selftest_constructor_condition_destroy})
		target_link_options(dssh_test_selftest_owner PRIVATE "-Wl,--wrap=${symbol}")
	endforeach()
	foreach(side IN ITEMS open accept)
		if(side STREQUAL "open")
			set(selftest_constructor_malloc_last 6)
		else()
			set(selftest_constructor_malloc_last 2)
		endif()
		set(selftest_constructor_modes constructor-${side}-condition-0)
		foreach(index RANGE 0 ${selftest_constructor_malloc_last})
			list(APPEND selftest_constructor_modes constructor-${side}-malloc-${index})
		endforeach()
		foreach(index RANGE 0 2)
			list(APPEND selftest_constructor_modes constructor-${side}-realloc-${index})
		endforeach()
		foreach(index RANGE 0 1)
			list(APPEND selftest_constructor_modes constructor-${side}-mutex-${index})
		endforeach()
		foreach(mode IN LISTS selftest_constructor_modes)
			add_test(NAME dssh_selftest_constructor_${mode} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/selftest_constructor_controls.py
				$<TARGET_FILE:dssh_test_selftest_owner> ${mode})
			set_tests_properties(dssh_selftest_constructor_${mode} PROPERTIES
				FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
		endforeach()
	endforeach()
	foreach(mode IN ITEMS wire-env-native wire-env-omitted wire-env-reply
		wire-break-native wire-break-duplicate)
		add_test(NAME dssh_selftest_wire_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/selftest_wire_controls.py
			$<TARGET_FILE:dssh_test_selftest_owner> ${mode})
		set_tests_properties(dssh_selftest_wire_${mode} PROPERTIES
			FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
	endforeach()
	set(selftest_socket_modes callback-unlock-native callback-unlock-retained
		callback-unlock-released socket-recv-eof)
	foreach(kind IN ITEMS native eintr error-before error-after zero oversize short short-error)
		list(APPEND selftest_socket_modes socket-send-${kind} socket-recv-${kind})
	endforeach()
	foreach(kind IN ITEMS native eintr error-before error-after zero count nval hup err other terminated)
		list(APPEND selftest_socket_modes socket-poll-${kind})
	endforeach()
	foreach(kind IN ITEMS native eintr error-before error-after zero oversize full capacity eof empty-eof)
		list(APPEND selftest_socket_modes socket-line-${kind})
	endforeach()
	foreach(mode IN LISTS selftest_socket_modes)
		add_test(NAME dssh_selftest_socket_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/selftest_socket_controls.py
			$<TARGET_FILE:dssh_test_selftest_owner> ${mode})
	endforeach()
	foreach(mode IN ITEMS standalone-log standalone-clock standalone-terminate)
		add_test(NAME dssh_selftest_completion_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/selftest_completion_controls.py
			$<TARGET_FILE:dssh_test_selftest_owner> ${mode})
	endforeach()
	dssh_add_test(wait_observation test_wait_observation.c)
	set(selftest_wait_symbol cnd_timedwait)
	if(DSSH_THREADS_SHIM_INCLUDE)
		set(selftest_wait_symbol dssh_thread_cnd_timedwait)
	endif()
	target_link_options(dssh_test_wait_observation PRIVATE "-Wl,--wrap=${selftest_wait_symbol}")
	dssh_add_output_test(dssh_wait_observation dssh_test_wait_observation
		"wait_observation cases=10 native_success=1 native_timeout=1 reported_error=1 excluded=1 other=1 injection=1 disabled=1 thread_reset=1 full_reset=1 wildcard=1 valid=1")
	set(selftest_owner_modes plain assert-body)
	foreach(index RANGE 0 4)
		list(APPEND selftest_owner_modes create-${index}
			join-${index}-retained join-${index}-released)
	endforeach()
	foreach(index RANGE 0 3)
		list(APPEND selftest_owner_modes setter-${index}-before setter-${index}-after)
	endforeach()
	foreach(stage IN ITEMS hs auth)
		foreach(index RANGE 0 2)
			list(APPEND selftest_owner_modes assert-${stage}-${index})
		endforeach()
	endforeach()
	foreach(mode IN LISTS selftest_owner_modes)
		add_test(NAME dssh_selftest_owner_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/selftest_owner_controls.py
			$<TARGET_FILE:dssh_test_selftest_owner> ${mode})
		set_tests_properties(dssh_selftest_owner_${mode} PROPERTIES
			FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
	endforeach()
	set(selftest_stream_modes assert-params assert-channel extent-boundaries
		pattern-tail stream-partial stream-corrupt
		stream-write-zero stream-write-negative stream-write-oversize
		stream-read-eof stream-read-negative stream-read-oversize)
	foreach(kind IN ITEMS exec shell)
		foreach(index RANGE 0 2)
			list(APPEND selftest_stream_modes builder-${kind}-${index}-before
				builder-${kind}-${index}-after)
		endforeach()
	endforeach()
	foreach(operation IN ITEMS read write)
		foreach(kind IN ITEMS zero negative wrong)
			list(APPEND selftest_stream_modes stream-${operation}-poll-${kind})
		endforeach()
	endforeach()
	foreach(mode IN LISTS selftest_stream_modes)
		add_test(NAME dssh_selftest_stream_${mode} COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/selftest_stream_controls.py
			$<TARGET_FILE:dssh_test_selftest_owner> ${mode})
		set_tests_properties(dssh_selftest_stream_${mode} PROPERTIES
			FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
	endforeach()
endif()
foreach(provider IN ITEMS test-growth zlib)
	if(provider STREQUAL "zlib" AND NOT ZLIB_FOUND)
		continue()
	endif()
	foreach(case IN ITEMS test_self_rekey_during_data test_self_rekey_manual
		test_self_rekey_preserves_channels test_self_rekey_inflight_data
		test_self_window_adjust_race)
		set(test_name "dssh_self_compression_${provider}_${case}")
		dssh_add_output_test("${test_name}" dssh_test_selftest "1 tests: 1 passed, 0 failed, 0 skipped"
			--exact "${case}")
		set_tests_properties("${test_name}" PROPERTIES
			ENVIRONMENT "${DSSH_TEST_KEY_ENV};DSSH_TEST_COMPRESSION=${provider}"
			FIXTURES_REQUIRED dssh_cached_keys)
	endforeach()
endforeach()
set(SELFTEST_TESTS
	test_assertion_cleanup
	test_self_handshake_curve25519
	test_self_auth_password
	test_self_exec_echo
	test_self_window_adjust_race
	test_self_exec_exit_code
	test_self_shell_echo
	test_self_shell_large_data
	test_self_signal
	test_self_window_change
	test_self_multiple_channels
	test_self_graceful_disconnect
	test_self_eof_half_close
	test_self_rekey_during_data
	test_self_rekey_manual
	test_self_rekey_preserves_channels
	test_self_rekey_inflight_data
	test_self_connection_drop
	test_self_socket_boundaries
	test_null_session_api
	test_log_api
	test_set_timeout
	test_set_terminate_cb
	test_open_channel_timeout
	test_setup_recv_timeout
	test_rekey_send_timeout
	test_self_chan_exec_echo
	test_self_chan_shell_pty
	test_self_chan_accept_exec
	test_self_chan_accept_shell_pty
	test_self_chan_event_exit_status
	test_self_chan_read_peek
	test_self_chan_shutwr_then_close
	test_self_chan_close_exit_code
	test_self_chan_close_no_exit_code
	test_self_chan_send_break
	test_self_chan_getters_after_accept
	test_self_chan_accept_env_reject
	test_self_io_callback_contract
)
dssh_add_individual_tests(dssh_test_selftest dssh_self SELFTEST_TESTS)
foreach(group IN ITEMS 14 16)
	foreach(case IN LISTS SELFTEST_TESTS)
		add_test(NAME dssh_fixed_group${group}_${case} COMMAND dssh_test_selftest --exact ${case})
		set_tests_properties(dssh_fixed_group${group}_${case} PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=group${group};${DSSH_TEST_KEY_ENV}"
			FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 300)
	endforeach()
	add_test(NAME dssh_fixed_group${group}_transport COMMAND dssh_test_transport)
	set_tests_properties(dssh_fixed_group${group}_transport PROPERTIES
		ENVIRONMENT "DSSH_TEST_KEX=group${group};${DSSH_TEST_KEY_ENV}"
		FIXTURES_REQUIRED dssh_cached_keys TIMEOUT 900)
endforeach()


# ================================================================
# Tier 3b: Asymmetric MAC negotiation (fork-based, separate registries)
# ================================================================

dssh_add_test(asymmetric_mac test_asymmetric_mac.c)
dssh_add_output_test(dssh_asymmetric_mac dssh_test_asymmetric_mac
	"4 tests: 4 passed, 0 failed, 0 skipped")
set_tests_properties(dssh_asymmetric_mac PROPERTIES TIMEOUT 60)

# ================================================================
# Tier 3c/3d: Optional interop tests
# ================================================================

if(DEUCESSH_TEST_OPENSSH)
	if(NOT UNIX)
		message(FATAL_ERROR "OpenSSH stdio fixtures currently require POSIX process/I/O support")
	endif()
	find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
	find_program(DSSH_OPENSSH_EXECUTABLE NAMES ssh REQUIRED)
	find_program(DSSH_OPENSSH_DAEMON NAMES sshd PATHS /usr/sbin /usr/local/sbin REQUIRED)
	find_program(DSSH_OPENSSH_KEYGEN NAMES ssh-keygen REQUIRED)
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(DSSH_MAC_LIBSSH QUIET IMPORTED_TARGET "libssh>=0.12.0")
		if(DSSH_MAC_LIBSSH_FOUND)
			add_executable(dssh_libssh_example_server libssh_example_server.c)
			set_property(TARGET dssh_libssh_example_server PROPERTY C_STANDARD 17)
			set_property(TARGET dssh_libssh_example_server PROPERTY C_STANDARD_REQUIRED ON)
			target_compile_definitions(dssh_libssh_example_server PRIVATE _POSIX_C_SOURCE=200809L)
			target_compile_options(dssh_libssh_example_server PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			target_link_libraries(dssh_libssh_example_server PRIVATE PkgConfig::DSSH_MAC_LIBSSH)
			add_executable(dssh_libssh_mac_peer libssh_mac_peer.c)
			set_property(TARGET dssh_libssh_mac_peer PROPERTY C_STANDARD 17)
			set_property(TARGET dssh_libssh_mac_peer PROPERTY C_STANDARD_REQUIRED ON)
			target_compile_definitions(dssh_libssh_mac_peer PRIVATE _POSIX_C_SOURCE=200809L)
			target_compile_options(dssh_libssh_mac_peer PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			target_link_libraries(dssh_libssh_mac_peer PRIVATE PkgConfig::DSSH_MAC_LIBSSH)
		endif()
	endif()
	# Actual client main, linked to each production form, with a foreign libssh
	# server and independently computed pins. Reuse the existing key fixture.
	if(TARGET dssh_libssh_example_server AND TARGET dssh_test_example_client_static)
		function(dssh_client_peer_case name target algorithm auth mode)
			if(algorithm STREQUAL "rsa-sha2-256")
				set(key "${CMAKE_CURRENT_BINARY_DIR}/test_rsa.pem")
			else()
				set(key "${CMAKE_CURRENT_BINARY_DIR}/test_ed25519.pem")
			endif()
			add_test(NAME ${name} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/client_example_interop.py
				--client $<TARGET_FILE:${target}> --server $<TARGET_FILE:dssh_libssh_example_server>
				--key ${key} --algorithm ${algorithm} --auth ${auth} --mode ${mode}
				--evidence ${CMAKE_CURRENT_BINARY_DIR}/client-example-evidence/${name} ${ARGN})
			set_tests_properties(${name} PROPERTIES TIMEOUT 90 LABELS "libssh;client-example"
				FIXTURES_REQUIRED dssh_cached_keys ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
		endfunction()
		foreach(form IN ITEMS static shared)
			foreach(algorithm IN ITEMS rsa-sha2-256 ssh-ed25519)
				foreach(auth IN ITEMS password kbi fallback partial)
					foreach(mode IN ITEMS exec shell)
						dssh_client_peer_case(dssh_client_peer_${form}_${algorithm}_${auth}_${mode}
							dssh_test_example_client_${form} ${algorithm} ${auth} ${mode})
					endforeach()
				endforeach()
				dssh_client_peer_case(dssh_client_peer_${form}_${algorithm}_wrong_pin
					dssh_test_example_client_${form} ${algorithm} password exec --negative-pin)
			endforeach()
			dssh_client_peer_case(dssh_client_peer_${form}_idle dssh_test_example_client_${form}
				ssh-ed25519 kbi shell-idle --host localhost)
			dssh_client_peer_case(dssh_client_peer_${form}_wrong_password dssh_test_example_client_${form}
				rsa-sha2-256 password exec --wrong-password)
			dssh_client_peer_case(dssh_client_peer_${form}_rejected_command dssh_test_example_client_${form}
				rsa-sha2-256 password exec --reject-command)
			foreach(fault IN ITEMS registration init terminate_cb verify_cb handshake auth_query start
					params_init command pty term size open worker output stdin poll close join)
				set(mode exec)
				if(fault MATCHES "^(term|size|worker|stdin|join)$")
					set(mode shell)
				endif()
				dssh_client_peer_case(dssh_client_fault_${form}_${fault} dssh_client_driver_${form}
					rsa-sha2-256 password ${mode} --client-fault ${fault})
			endforeach()
		endforeach()
		if(TARGET client)
			dssh_client_peer_case(dssh_client_peer_standalone client ssh-ed25519 kbi shell --direct)
		endif()
	endif()
	foreach(form IN ITEMS static shared)
		if(TARGET deucessh_${form})
			# Deliberately link the production target, without DSSH_TESTING,
			# private headers, allocator hooks or replacement algorithms.
			add_executable(dssh_openssh_${form} openssh_peer.c)
			target_compile_definitions(dssh_openssh_${form} PRIVATE _POSIX_C_SOURCE=200809L)
			set_property(TARGET dssh_openssh_${form} PROPERTY C_STANDARD 17)
			set_property(TARGET dssh_openssh_${form} PROPERTY C_STANDARD_REQUIRED ON)
			target_compile_options(dssh_openssh_${form} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
			target_link_libraries(dssh_openssh_${form} PRIVATE deucessh_${form})
			if(TARGET dssh_libssh_mac_peer)
				foreach(role IN ITEMS client server)
					foreach(first IN ITEMS 256 512)
						set(mac_case dssh_libssh_mac_${form}_${role}_${first})
						add_test(NAME ${mac_case} COMMAND ${Python3_EXECUTABLE}
							${CMAKE_CURRENT_SOURCE_DIR}/libssh_mac_interop.py
							--peer $<TARGET_FILE:dssh_openssh_${form}>
							--libssh $<TARGET_FILE:dssh_libssh_mac_peer>
							--role ${role} --c2s ${first} --keygen ${DSSH_OPENSSH_KEYGEN}
							--evidence ${CMAKE_CURRENT_BINARY_DIR}/libssh-mac-evidence/${mac_case})
						set_tests_properties(${mac_case} PROPERTIES TIMEOUT 90 LABELS libssh)
					endforeach()
					foreach(first IN ITEMS cbc ctr)
						foreach(mac_first IN ITEMS 256 512)
							foreach(mode IN ITEMS keep swap)
								set(cipher_case dssh_libssh_cipher_${form}_${role}_${first}_${mac_first}_${mode})
								set(cipher_command ${Python3_EXECUTABLE}
									${CMAKE_CURRENT_SOURCE_DIR}/libssh_mac_interop.py
									--peer $<TARGET_FILE:dssh_openssh_${form}>
									--libssh $<TARGET_FILE:dssh_libssh_mac_peer>
									--role ${role} --c2s ${mac_first} --cipher-c2s ${first}
									--keygen ${DSSH_OPENSSH_KEYGEN}
									--evidence ${CMAKE_CURRENT_BINARY_DIR}/libssh-cipher-evidence/${cipher_case})
								if(mode STREQUAL "swap")
									list(APPEND cipher_command --swap-rekey)
								endif()
								add_test(NAME ${cipher_case} COMMAND ${cipher_command})
								set_tests_properties(${cipher_case} PROPERTIES TIMEOUT 90 LABELS libssh)
							endforeach()
						endforeach()
					endforeach()
				endforeach()
			endif()
			foreach(method IN ITEMS curve25519-sha256 diffie-hellman-group-exchange-sha256
					sntrup761x25519-sha512 mlkem768x25519-sha256
					diffie-hellman-group14-sha256 diffie-hellman-group16-sha512)
				foreach(role IN ITEMS client server)
					set(interop_name dssh_openssh_${form}_${role}_${method})
					set(interop_extra_args)
					set(interop_timeout 210)
					if(method STREQUAL "diffie-hellman-group-exchange-sha256"
					    AND role STREQUAL client AND DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL"
					    AND CMAKE_C_FLAGS MATCHES "-fsanitize=[^ ]*thread")
						get_target_property(interop_crypto OpenSSL::Crypto IMPORTED_LOCATION)
						if(interop_crypto MATCHES "[.]a$")
							# Instrumented no-assembly 4096-bit p/q validation costs
							# about 105s per exchange on the qualified native host.
							# Retain system moduli and full initial/rekey checks.
							set(interop_extra_args --client-exchange-seconds 540)
							set(interop_timeout 660)
						endif()
					endif()
					add_test(NAME ${interop_name} COMMAND ${Python3_EXECUTABLE}
						${CMAKE_CURRENT_SOURCE_DIR}/openssh_interop.py run
						--peer $<TARGET_FILE:dssh_openssh_${form}> --role ${role} --algorithm ${method}
						--ssh ${DSSH_OPENSSH_EXECUTABLE} --sshd ${DSSH_OPENSSH_DAEMON}
						--keygen ${DSSH_OPENSSH_KEYGEN}
						--evidence ${CMAKE_CURRENT_BINARY_DIR}/openssh-evidence/${interop_name}
						${interop_extra_args})
					set_tests_properties(${interop_name} PROPERTIES TIMEOUT ${interop_timeout} LABELS openssh)
				endforeach()
			endforeach()
			set(cbc_name dssh_openssh_${form}_client_aes128-cbc)
			foreach(role IN ITEMS client server)
				set(mac_name dssh_openssh_${form}_${role}_hmac-sha2-512)
				add_test(NAME ${mac_name} COMMAND ${Python3_EXECUTABLE}
					${CMAKE_CURRENT_SOURCE_DIR}/openssh_interop.py run
					--peer $<TARGET_FILE:dssh_openssh_${form}> --role ${role} --algorithm curve25519-sha256
					--ssh ${DSSH_OPENSSH_EXECUTABLE} --sshd ${DSSH_OPENSSH_DAEMON}
					--keygen ${DSSH_OPENSSH_KEYGEN}
					--evidence ${CMAKE_CURRENT_BINARY_DIR}/openssh-evidence/${mac_name})
				set_tests_properties(${mac_name} PROPERTIES TIMEOUT 210 LABELS openssh
					ENVIRONMENT "DSSH_TEST_MAC=hmac-sha2-512")
			endforeach()
			foreach(role IN ITEMS client server)
				set(language_name dssh_openssh_${form}_${role}_languages)
				add_test(NAME ${language_name} COMMAND ${Python3_EXECUTABLE}
					${CMAKE_CURRENT_SOURCE_DIR}/openssh_interop.py run
					--peer $<TARGET_FILE:dssh_openssh_${form}> --role ${role} --algorithm curve25519-sha256
					--ssh ${DSSH_OPENSSH_EXECUTABLE} --sshd ${DSSH_OPENSSH_DAEMON}
					--keygen ${DSSH_OPENSSH_KEYGEN}
					--evidence ${CMAKE_CURRENT_BINARY_DIR}/openssh-evidence/${language_name})
				set_tests_properties(${language_name} PROPERTIES TIMEOUT 210 LABELS openssh
					ENVIRONMENT "DSSH_TEST_LANGUAGE_FIELDS=1")
			endforeach()
			add_test(NAME ${cbc_name} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/openssh_interop.py run
				--peer $<TARGET_FILE:dssh_openssh_${form}> --role client --algorithm curve25519-sha256
				--ssh ${DSSH_OPENSSH_EXECUTABLE} --sshd ${DSSH_OPENSSH_DAEMON}
				--keygen ${DSSH_OPENSSH_KEYGEN}
				--evidence ${CMAKE_CURRENT_BINARY_DIR}/openssh-evidence/${cbc_name})
			set_tests_properties(${cbc_name} PROPERTIES TIMEOUT 210 LABELS openssh
				ENVIRONMENT "DSSH_TEST_ENC=aes128-cbc")
			set(reject_name dssh_openssh_${form}_reject_host)
			add_test(NAME ${reject_name} COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/openssh_interop.py run
				--peer $<TARGET_FILE:dssh_openssh_${form}> --role client --algorithm curve25519-sha256
				--ssh ${DSSH_OPENSSH_EXECUTABLE} --sshd ${DSSH_OPENSSH_DAEMON}
				--keygen ${DSSH_OPENSSH_KEYGEN} --negative-host
				--evidence ${CMAKE_CURRENT_BINARY_DIR}/openssh-evidence/${reject_name})
			set_tests_properties(${reject_name} PROPERTIES TIMEOUT 210 LABELS openssh)
		endif()
	endforeach()
endif()

if(DEUCESSH_TEST_SYNCHRONET)
	dssh_add_test(synchronet test_synchronet.c)
	add_test(NAME dssh_integration_synchronet COMMAND dssh_test_synchronet)
endif()

# Include consumers registered by either helper and the standalone unit tests.
get_property(dssh_registered_tests DIRECTORY PROPERTY TESTS)
foreach(test_name IN LISTS dssh_registered_tests)
	get_test_property("${test_name}" ENVIRONMENT test_environment)
	if(NOT test_name STREQUAL "dssh_test_keys"
			AND "${test_environment}" MATCHES "DSSH_TEST_RSA_KEY=")
		get_test_property("${test_name}" FIXTURES_REQUIRED test_fixtures)
		if(NOT "dssh_cached_keys" IN_LIST test_fixtures)
			set_property(TEST "${test_name}" APPEND PROPERTY FIXTURES_REQUIRED dssh_cached_keys)
		endif()
	endif()
endforeach()

# Optional independent SSH implementation, built separately without compiling
# the C API as another language. Normal library/tests need no Go dependency.
set(DEUCESSH_BREAK_SSH_PEER "" CACHE FILEPATH "Prebuilt independent SSH peer for BREAK acknowledgement checks")
if(DEUCESSH_BREAK_SSH_PEER AND Python3_Interpreter_FOUND
    AND TARGET dssh_openssh_static AND TARGET dssh_openssh_shared)
	if(NOT EXISTS "${DEUCESSH_BREAK_SSH_PEER}")
		message(FATAL_ERROR "DEUCESSH_BREAK_SSH_PEER does not exist")
	endif()
	foreach(form IN ITEMS static shared)
		set(name dssh_break_independent_${form})
		dssh_add_output_test(${name} Python3::Interpreter
			"independent_break status=PASSED cases=14"
			${CMAKE_CURRENT_SOURCE_DIR}/break_ssh_interop.py
			--peer $<TARGET_FILE:dssh_openssh_${form}>
			--foreign "${DEUCESSH_BREAK_SSH_PEER}"
			--evidence ${CMAKE_CURRENT_BINARY_DIR}/break-evidence/${name})
		set_tests_properties(${name} PROPERTIES TIMEOUT 90 LABELS "interop;break"
			FAIL_REGULAR_EXPRESSION "independent_break status=FAILED;AddressSanitizer;LeakSanitizer;ThreadSanitizer;UndefinedBehaviorSanitizer;runtime error:")
		set(notification_name dssh_notification_flags_independent_${form})
		dssh_add_output_test(${notification_name} Python3::Interpreter
			"independent_break status=PASSED cases=14"
			${CMAKE_CURRENT_SOURCE_DIR}/break_ssh_interop.py
			--peer $<TARGET_FILE:dssh_openssh_${form}>
			--foreign "${DEUCESSH_BREAK_SSH_PEER}" --notification-flags
			--evidence ${CMAKE_CURRENT_BINARY_DIR}/break-evidence/${notification_name})
		set_tests_properties(${notification_name} PROPERTIES TIMEOUT 90 LABELS "interop;notifications"
			FAIL_REGULAR_EXPRESSION "independent_break status=FAILED;AddressSanitizer;LeakSanitizer;ThreadSanitizer;UndefinedBehaviorSanitizer;runtime error:")
		foreach(mode IN ITEMS none 0 1)
			set(mode_name dssh_terminal_mode_independent_${form}_${mode})
			dssh_add_output_test(${mode_name} Python3::Interpreter
				"independent_break status=PASSED cases=14"
				${CMAKE_CURRENT_SOURCE_DIR}/break_ssh_interop.py
				--peer $<TARGET_FILE:dssh_openssh_${form}>
				--foreign "${DEUCESSH_BREAK_SSH_PEER}" --terminal-mode ${mode}
				--evidence ${CMAKE_CURRENT_BINARY_DIR}/break-evidence/${mode_name})
			set_tests_properties(${mode_name} PROPERTIES TIMEOUT 90 LABELS "interop;terminal-modes"
				FAIL_REGULAR_EXPRESSION "independent_break status=FAILED;AddressSanitizer;LeakSanitizer;ThreadSanitizer;UndefinedBehaviorSanitizer;runtime error:")
		endforeach()
	endforeach()
endif()

foreach(profile IN ITEMS ordinary growth)
	dssh_add_output_test(dssh_conn_stream_body_${profile} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact stream_body/controls)
	set_tests_properties(dssh_conn_stream_body_${profile} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
	if(profile STREQUAL "growth")
		set_property(TEST dssh_conn_stream_body_${profile} APPEND PROPERTY
			ENVIRONMENT "DSSH_TEST_COMPRESSION=test-growth")
	endif()
endforeach()

foreach(profile IN ITEMS ordinary growth)
	dssh_add_output_test(dssh_conn_event_body_${profile} dssh_test_conn
		"1 tests: 1 passed, 0 failed, 0 skipped" --exact event_body/controls)
	set_tests_properties(dssh_conn_event_body_${profile} PROPERTIES TIMEOUT 120
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}" FIXTURES_REQUIRED dssh_cached_keys)
	if(profile STREQUAL "growth")
		set_property(TEST dssh_conn_event_body_${profile} APPEND PROPERTY
			ENVIRONMENT "DSSH_TEST_COMPRESSION=test-growth")
	endif()
endforeach()
