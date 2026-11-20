# Native Windows acceptance reuses production-library C17 consumers. POSIX
# fork/socket descriptors and GNU --wrap probes remain in the Unix test suite.
function(dssh_windows_consumer target library)
	add_executable(${target} ${ARGN})
	foreach(shim IN LISTS DSSH_THREADS_SHIM_SOURCES)
		target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/${shim}")
	endforeach()
	set_target_properties(${target} PROPERTIES
		C_STANDARD ${DEUCESSH_C_STANDARD} C_STANDARD_REQUIRED ON)
	target_compile_options(${target} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_link_libraries(${target} PRIVATE ${library} ${THREADS_LINK_LIB})
	if(library STREQUAL "deucessh_shared")
		target_compile_definitions(${target} PRIVATE DSSH_SHARED_IMPORT)
	endif()
	if(DSSH_THREADS_SHIM_INCLUDE)
		target_include_directories(${target} BEFORE PRIVATE ${DSSH_THREADS_SHIM_INCLUDE})
	endif()
endfunction()

foreach(kind IN ITEMS static shared)
	if(NOT TARGET deucessh_${kind})
		continue()
	endif()
	set(target dssh_auth_actions_${kind})
	dssh_windows_consumer(${target} deucessh_${kind} test_auth_actions.c mock_io.c)
	target_link_libraries(${target} PRIVATE ws2_32)
	foreach(mode IN ITEMS begin direct)
		dssh_add_output_test(dssh_auth_actions_${kind}_${mode} ${target} "valid=1" ${mode})
	endforeach()
	set(target dssh_test_event_peer_${kind})
	dssh_windows_consumer(${target} deucessh_${kind} test_event_peer.c mock_io.c)
	target_link_libraries(${target} PRIVATE ws2_32)
	dssh_add_output_test(dssh_event_peer_${kind} ${target}
		"events=130 queue=0 positions=3/2 production=1")
	set(target dssh_test_accept_callback_peer_${kind})
	dssh_windows_consumer(${target} deucessh_${kind} test_accept_callback_peer.c mock_io.c)
	target_link_libraries(${target} PRIVATE ws2_32)
	foreach(mode RANGE 0 5)
		dssh_add_output_test(dssh_accept_callback_peer_${kind}_${mode} ${target}
			"setup_contract=1 kind=${mode}.*production=1" ${mode})
	endforeach()
	set(target dssh_test_crypto_${kind})
	dssh_windows_consumer(${target} deucessh_${kind} test_crypto.c)
	dssh_add_output_test(dssh_unit_crypto_${kind} ${target}
		"[0-9]+ tests: [0-9]+ passed, 0 failed, 0 skipped")
	set(target dssh_test_key_permissions_${kind})
	dssh_windows_consumer(${target} deucessh_${kind} test_key_permissions_win32.c)
	target_link_libraries(${target} PRIVATE advapi32)
	dssh_add_output_test(dssh_key_permissions_${kind} ${target}
		"key_permissions owner=1 protected=1 same_file=1 rejected=3 valid=1"
		"${CMAKE_CURRENT_BINARY_DIR}/permissions-${kind}.pem")
endforeach()

# The existing key fixture is portable and supplies the provider round trips.
if(TARGET deucessh_static)
	set(key_library deucessh_static)
else()
	set(key_library deucessh_shared)
endif()
dssh_windows_consumer(dssh_test_key_setup ${key_library} test_key_setup.c)
add_test(NAME dssh_test_keys COMMAND dssh_test_key_setup)
set_tests_properties(dssh_test_keys PROPERTIES FIXTURES_SETUP dssh_cached_keys
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}")

include("${CMAKE_CURRENT_SOURCE_DIR}/client-keys.cmake")

# Generator expressions preserve the library DLL path in multi-config builds.
# CI also adds the vcpkg provider runtime bin directory to PATH.
get_property(windows_tests DIRECTORY PROPERTY TESTS)
foreach(test IN LISTS windows_tests)
	set_property(TEST ${test} PROPERTY TIMEOUT 120)
	if(TARGET deucessh_shared)
		set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
			"PATH=path_list_prepend:$<TARGET_FILE_DIR:deucessh_shared>")
	endif()

endforeach()
