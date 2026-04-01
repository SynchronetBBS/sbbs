# Cached key files for test runs — avoids repeated RSA keygen.
# These are stored in the build directory and reused across runs.
set(DSSH_TEST_KEY_ENV
	"DSSH_TEST_ED25519_KEY=${CMAKE_CURRENT_BINARY_DIR}/test_ed25519.pem"
	"DSSH_TEST_RSA_KEY=${CMAKE_CURRENT_BINARY_DIR}/test_rsa.pem")

# Prepend project root to source paths (they're relative to ssh/, not test/)
set(DSSH_TEST_LIB_SOURCES "")
foreach(src ${DEUCESSH_SOURCES})
	list(APPEND DSSH_TEST_LIB_SOURCES "${PROJECT_SOURCE_DIR}/${src}")
endforeach()

# Test-instrumented static library (exposes DSSH_TESTABLE functions)
add_library(deucessh_test_lib STATIC ${DSSH_TEST_LIB_SOURCES})
set_property(TARGET deucessh_test_lib PROPERTY C_STANDARD 17)
set_property(TARGET deucessh_test_lib PROPERTY C_STANDARD_REQUIRED ON)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	set_property(TARGET deucessh_test_lib PROPERTY CXX_STANDARD 20)
	set_property(TARGET deucessh_test_lib PROPERTY CXX_STANDARD_REQUIRED ON)
endif()
target_compile_definitions(deucessh_test_lib PRIVATE DSSH_TESTING
	$<$<STREQUAL:${DEUCESSH_CRYPTO_BACKEND},OpenSSL>:DSSH_CRYPTO_OPENSSL>
	$<$<STREQUAL:${DEUCESSH_CRYPTO_BACKEND},Botan>:DSSH_CRYPTO_BOTAN>)
target_include_directories(deucessh_test_lib
	PUBLIC ${PROJECT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(deucessh_test_lib SYSTEM
	PRIVATE ${DSSH_CRYPTO_INCLUDE})
target_link_libraries(deucessh_test_lib
	INTERFACE ${DSSH_CRYPTO_LIB_INTERFACE} ${THREADS_LINK_LIB})
target_compile_options(deucessh_test_lib PRIVATE
	${DEUCESSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
		-Wno-unused-function>)

# Mock I/O library
add_library(dssh_mock_io STATIC mock_io.c)
set_property(TARGET dssh_mock_io PROPERTY C_STANDARD 17)
set_property(TARGET dssh_mock_io PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_mock_io PUBLIC
	${PROJECT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dssh_mock_io PRIVATE deucessh_test_lib)

# Library-only test allocator (macro-based, doesn't affect OpenSSL)
add_library(dssh_test_allocator STATIC dssh_test_alloc.c)
set_property(TARGET dssh_test_allocator PROPERTY C_STANDARD 17)
set_property(TARGET dssh_test_allocator PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_allocator PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR})
# Crypto + C11 threads failure injection library
add_library(dssh_test_ossl STATIC dssh_test_ossl.c)
set_property(TARGET dssh_test_ossl PROPERTY C_STANDARD 17)
set_property(TARGET dssh_test_ossl PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_ossl PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR})
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

# Mock allocator library (--wrap based, for process-wide injection)
add_library(dssh_mock_alloc STATIC mock_alloc.c)
set_property(TARGET dssh_mock_alloc PROPERTY C_STANDARD 17)
set_property(TARGET dssh_mock_alloc PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_mock_alloc PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR})

set(DSSH_WRAP_FLAGS
	-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free)

# Helper function for adding test executables
function(dssh_add_test test_name source_file)
	add_executable(dssh_test_${test_name} ${source_file})
	set_property(TARGET dssh_test_${test_name} PROPERTY C_STANDARD 17)
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
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t} PROPERTIES
			ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_dhgex
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_dhgex PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_rsa
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_rsa512
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_rsa512 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_dhgex_rsa
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_dhgex_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_dhgex_rsa512
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_dhgex_rsa512 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_sntrup PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup_rsa
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_sntrup_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup_rsa512
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_sntrup_rsa512 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_mlkem
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_mlkem PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=mlkem;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_mlkem_rsa
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_mlkem_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_mlkem_rsa512
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_mlkem_rsa512 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa512;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_hmac256
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_hmac256 PROPERTIES
			ENVIRONMENT "DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_rsa_hmac256
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_rsa_hmac256 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEY=rsa;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_dhgex_hmac256
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_dhgex_hmac256 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup_hmac256
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_sntrup_hmac256 PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_MAC=hmac256;${DSSH_TEST_KEY_ENV}")
	endforeach()
endmacro()

# ================================================================
# Tier 1: Unit tests
# ================================================================

dssh_add_test(chan test_chan.c)
add_test(NAME dssh_unit_chan COMMAND dssh_test_chan)

dssh_add_test(crypto test_crypto.c)
add_test(NAME dssh_unit_crypto COMMAND dssh_test_crypto)

# Algorithm validation tests — reference helpers are backend-conditional
dssh_add_test(algo_enc test_algo_enc.c)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	target_link_libraries(dssh_test_algo_enc PRIVATE OpenSSL::Crypto)
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_link_libraries(dssh_test_algo_enc PRIVATE PkgConfig::BOTAN3)
endif()
add_test(NAME dssh_unit_algo_enc COMMAND dssh_test_algo_enc)

dssh_add_test(algo_mac test_algo_mac.c)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	target_link_libraries(dssh_test_algo_mac PRIVATE OpenSSL::Crypto)
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_link_libraries(dssh_test_algo_mac PRIVATE PkgConfig::BOTAN3)
endif()
add_test(NAME dssh_unit_algo_mac COMMAND dssh_test_algo_mac)

dssh_add_test(algo_key test_algo_key.c)
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
	target_link_libraries(dssh_test_algo_key PRIVATE OpenSSL::Crypto)
elseif(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
	target_link_libraries(dssh_test_algo_key PRIVATE PkgConfig::BOTAN3)
endif()
add_test(NAME dssh_unit_algo_key COMMAND dssh_test_algo_key)
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
	add_executable(dssh_test_botan_algo_key test_botan_algo_key.cpp)
	set_property(TARGET dssh_test_botan_algo_key PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_botan_algo_key PROPERTY CXX_STANDARD_REQUIRED ON)
	target_include_directories(dssh_test_botan_algo_key PRIVATE
		${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(dssh_test_botan_algo_key SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(dssh_test_botan_algo_key PRIVATE
		deucessh_test_lib PkgConfig::BOTAN3 ${THREADS_LINK_LIB})
	target_compile_options(dssh_test_botan_algo_key PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_compile_definitions(dssh_test_botan_algo_key PRIVATE DSSH_CRYPTO_BOTAN)
	add_test(NAME dssh_unit_botan_algo_key COMMAND dssh_test_botan_algo_key)
	set_tests_properties(dssh_unit_botan_algo_key PROPERTIES COST 20
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}")

	add_executable(dssh_test_botan_transport test_botan_transport.cpp)
	set_property(TARGET dssh_test_botan_transport PROPERTY CXX_STANDARD 20)
	set_property(TARGET dssh_test_botan_transport PROPERTY CXX_STANDARD_REQUIRED ON)
	target_include_directories(dssh_test_botan_transport PRIVATE
		${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(dssh_test_botan_transport SYSTEM
		PRIVATE ${DSSH_CRYPTO_INCLUDE})
	target_link_libraries(dssh_test_botan_transport PRIVATE
		deucessh_test_lib PkgConfig::BOTAN3 ${THREADS_LINK_LIB})
	target_compile_options(dssh_test_botan_transport PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
	target_compile_definitions(dssh_test_botan_transport PRIVATE DSSH_CRYPTO_BOTAN)
	add_test(NAME dssh_unit_botan_transport COMMAND dssh_test_botan_transport)
endif()

# ================================================================
# Tier 2: Layer tests (need mock I/O) — one process per variant
# ================================================================

dssh_add_test(transport test_transport.c)
target_link_libraries(dssh_test_transport PRIVATE dssh_mock_io)
dssh_add_variant_tests(dssh_transport dssh_test_transport)

dssh_add_test(auth test_auth.c)
target_link_libraries(dssh_test_auth PRIVATE dssh_mock_io)
set(AUTH_TESTS
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
	auth/client/get_methods_send_fail
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
)
dssh_add_individual_tests(dssh_test_auth dssh_auth AUTH_TESTS)

dssh_add_test(conn test_conn.c)
target_link_libraries(dssh_test_conn PRIVATE dssh_mock_io)
dssh_add_variant_tests(dssh_conn dssh_test_conn)
set_tests_properties(dssh_conn_sntrup PROPERTIES COST 12)
set_tests_properties(dssh_conn_sntrup_rsa PROPERTIES COST 12)

# ================================================================
# Tier 1b: Alloc failure tests (need mock allocator + mock I/O)
# ossl_iterate tests are OpenSSL-only (guarded with #ifdef in source)
# ================================================================

dssh_add_test(alloc test_alloc.c)
target_link_libraries(dssh_test_alloc PRIVATE dssh_mock_io dssh_mock_alloc)
target_link_options(dssh_test_alloc PRIVATE ${DSSH_WRAP_FLAGS})
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

# Targeted coverage tests — run in their own process for clean state
add_test(NAME dssh_targeted_c25519_server_trunc
	COMMAND dssh_test_alloc c25519/server_truncated_init)
set_tests_properties(dssh_targeted_c25519_server_trunc PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_targeted_c25519_client_bad_qs
	COMMAND dssh_test_alloc c25519/client_bad_qs_length)
set_tests_properties(dssh_targeted_c25519_client_bad_qs PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
if(DEUCESSH_CRYPTO_BACKEND STREQUAL "OpenSSL")
add_test(NAME dssh_targeted_dhgex_bn_rand_fail
	COMMAND dssh_test_alloc dhgex/server_bn_rand_fail)
set_tests_properties(dssh_targeted_dhgex_bn_rand_fail PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEX=dh-gex;${DSSH_TEST_KEY_ENV}")
endif()

# ================================================================
# Test algorithm modules (enc/mac with failure injection)
# ================================================================

add_library(dssh_test_algos STATIC test_enc.c test_mac.c)
set_property(TARGET dssh_test_algos PROPERTY C_STANDARD 17)
set_property(TARGET dssh_test_algos PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_algos PUBLIC
	${PROJECT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dssh_test_algos PRIVATE deucessh_test_lib)

# ================================================================
# Tier 2b: Transport error tests (need test enc/mac + mock I/O)
# ================================================================

dssh_add_test(transport_errors test_transport_errors.c)
target_link_libraries(dssh_test_transport_errors PRIVATE
	dssh_mock_io dssh_test_algos)
set(TRANSPORT_ERROR_TESTS
	test_modules/roundtrip
	send/encrypt_error send/mac_error send/mac_overflow_rejected
	recv/decrypt_error_first_block recv/decrypt_error_remaining
	recv/mac_generate_error recv/mac_mismatch recv/mac_too_large
	newkeys/enc_init_failure newkeys/mac_init_failure
)
foreach(t ${TRANSPORT_ERROR_TESTS})
	add_test(NAME dssh_transport_errors_${t}
		COMMAND dssh_test_transport_errors ${t})
	set_tests_properties(dssh_transport_errors_${t} PROPERTIES
		ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
endforeach()

# ================================================================
# Tier 2c: Thread error tests (lock/unlock/condvar failure injection)
# ================================================================

dssh_add_test(thread_errors test_thread_errors.c)
target_link_libraries(dssh_test_thread_errors PRIVATE dssh_mock_io)
dssh_add_variant_tests(dssh_thrd dssh_test_thread_errors)

# ================================================================
# Tier 3a: Self-test (integration) — one process per variant
# ================================================================

dssh_add_test(selftest test_selftest.c)
dssh_add_variant_tests(dssh_self dssh_test_selftest)

# ================================================================
# Tier 3b: Asymmetric MAC negotiation (fork-based, separate registries)
# ================================================================

dssh_add_test(asymmetric_mac test_asymmetric_mac.c)
add_test(NAME dssh_asymmetric_mac COMMAND dssh_test_asymmetric_mac)

# ================================================================
# Tier 3c/3d: Optional interop tests
# ================================================================

if(DEUCESSH_TEST_OPENSSH)
	dssh_add_test(openssh_server test_openssh_server.c)
	dssh_add_test(openssh_client test_openssh_client.c)
	add_test(NAME dssh_integration_openssh
		COMMAND ${PROJECT_SOURCE_DIR}/test/test_openssh_interop.sh
			${CMAKE_CURRENT_BINARY_DIR}/dssh_test_openssh_server
			${CMAKE_CURRENT_BINARY_DIR}/dssh_test_openssh_client)
endif()

if(DEUCESSH_TEST_SYNCHRONET)
	dssh_add_test(synchronet test_synchronet.c)
	add_test(NAME dssh_integration_synchronet COMMAND dssh_test_synchronet)
endif()
