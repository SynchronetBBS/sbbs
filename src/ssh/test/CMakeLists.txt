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
target_compile_definitions(deucessh_test_lib PRIVATE DSSH_TESTING)
target_include_directories(deucessh_test_lib
	PUBLIC ${PROJECT_SOURCE_DIR}
	PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(deucessh_test_lib
	INTERFACE ${OPENSSL_CRYPTO_LIBRARIES} ${THREADS_LINK_LIB})
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
# OpenSSL + C11 threads failure injection library
add_library(dssh_test_ossl STATIC dssh_test_ossl.c)
set_property(TARGET dssh_test_ossl PROPERTY C_STANDARD 17)
set_property(TARGET dssh_test_ossl PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(dssh_test_ossl PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dssh_test_ossl PRIVATE OpenSSL::Crypto)

# Link both allocator and ossl injection into the test library
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
		${CMAKE_CURRENT_SOURCE_DIR}
		${OPENSSL_INCLUDE_DIR})
	target_link_libraries(dssh_test_${test_name} PRIVATE
		deucessh_test_lib ${OPENSSL_CRYPTO_LIBRARIES} ${THREADS_LINK_LIB})
	target_compile_options(dssh_test_${test_name} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
endfunction()

# Helper: register one test per name × 6 env variants
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
		add_test(NAME ${prefix}_${t}_dhgex_rsa
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_dhgex_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_sntrup PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_sntrup_rsa
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_sntrup_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_mlkem
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_mlkem PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=mlkem;${DSSH_TEST_KEY_ENV}")
		add_test(NAME ${prefix}_${t}_mlkem_rsa
			COMMAND ${executable} ${t})
		set_tests_properties(${prefix}_${t}_mlkem_rsa PROPERTIES
			ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
	endforeach()
endmacro()

# ================================================================
# Tier 1: Unit tests
# ================================================================

dssh_add_test(arch test_arch.c)
add_test(NAME dssh_unit_arch COMMAND dssh_test_arch)

dssh_add_test(chan test_chan.c)
add_test(NAME dssh_unit_chan COMMAND dssh_test_chan)

dssh_add_test(algo_enc test_algo_enc.c)
add_test(NAME dssh_unit_algo_enc COMMAND dssh_test_algo_enc)

dssh_add_test(algo_mac test_algo_mac.c)
add_test(NAME dssh_unit_algo_mac COMMAND dssh_test_algo_mac)

dssh_add_test(algo_key test_algo_key.c)
add_test(NAME dssh_unit_algo_key COMMAND dssh_test_algo_key)
add_test(NAME dssh_targeted_dhgex_null_ka
	COMMAND dssh_test_algo_key dhgex_handler_null_ka)
add_test(NAME dssh_targeted_c25519_null_ka
	COMMAND dssh_test_algo_key c25519_handler_null_ka)

# ================================================================
# Tier 2: Layer tests (need mock I/O) — one process per test
# ================================================================

dssh_add_test(transport test_transport.c)
target_link_libraries(dssh_test_transport PRIVATE dssh_mock_io)
set(TRANSPORT_TESTS
	version/has_nulls_true version/has_nulls_false
	version/missing_crlf_short version/missing_crlf_valid
	version/missing_crlf_no_cr version/missing_crlf_no_lf
	version/is_version_line_true version/is_version_line_false_short
	version/is_version_line_false_get version/is_version_ss_not_ssh
	version/has_non_ascii_false version/has_non_ascii_high
	version/has_non_ascii_ctrl
	version/is_20_true version/is_20_true_199
	version/is_20_false_short version/is_20_false_10
	version/is_20_bad_minor version/is_20_199_valid
	version/is_20_199_short_buf version/is_20_199_partial
	version/is_20_199_no_dash version/is_20_199_bad_minor_d
	version/rx_non_ascii version/rx_too_long
	algo/negotiate_first_client_match algo/negotiate_single_match
	algo/negotiate_client_priority algo/negotiate_no_match
	algo/negotiate_not_registered
	algo/build_namelist_single algo/build_namelist_multiple
	algo/build_namelist_overflow algo/build_namelist_truncation
	algo/build_namelist_empty
	derive/deterministic derive/different_letters
	derive/extension_loop derive/bad_hash
	derive_key/ossl_init derive_key/ossl_shared_secret
	derive_key/ossl_hash derive_key/ossl_letter
	derive_key/ossl_session_id derive_key/ossl_final
	derive_key/ossl_extension_loop
	vex/basic vex/remote_stored vex/rx_error
	vex/extra_lines vex/extra_line_error
	vex/accept_199 vex/reject_non_20 vex/reject_non_ascii
	vex/terminate_mid_rx vex/with_comment
	vex/tx_toolong_version vex/tx_toolong_comment
	packet/roundtrip packet/alignment packet/min_padding
	packet/empty_payload packet/too_large packet/seq_increment
	packet/bidirectional packet/server_receives_client
	packet/recv_too_small packet/recv_bad_padding
	transport/ignore_skipped transport/multiple_ignore
	transport/disconnect_terminates
	transport/unimplemented_callback
	transport/debug_callback
	handshake/completes handshake/negotiated_algorithms
	handshake/session_id_set handshake/encrypted_roundtrip
	handshake/mac_active
	rekey/needed_false_initially rekey/needed_tx_packets
	rekey/needed_rx_packets rekey/needed_bytes
	rekey/needed_time rekey/needed_below_threshold
	rekey/after_handshake rekey/encrypted_roundtrip
	rekey/seq_preserved rekey/session_id_stable
	rekey/hard_limit_send rekey/hard_limit_recv
	session/init_cleanup session/terminate
	session/cleanup_null session/set_global_request_cb
	set_version/valid set_version/valid_with_comment
	set_version/empty_version set_version/empty_comment
	set_version/null_version set_version/null_with_comment
	set_version/space_in_version set_version/space_in_comment
	set_version/ctrl_in_version set_version/ctrl_in_comment
	set_version/high_byte set_version/high_byte_version
	set_version/high_byte_comment set_version/too_long
	set_version/long_version_string set_version/after_session
	register/two_kex register/two_comp register/two_lang
	register/kex_toolong register/kex_empty_name
	register/kex_toomany register/kex_toolate
	register/kex_next_not_null
	register/enc_toolong register/enc_toomany
	register/enc_toolate register/enc_next_not_null
	register/mac_toolong register/mac_toomany
	register/mac_toolate register/mac_next_not_null
	register/comp_toolong register/comp_empty_name
	register/comp_toomany register/comp_toolate
	register/comp_next_not_null
	register/key_algo_toolong register/key_algo_toomany
	register/key_algo_toolate register/key_algo_next_not_null
	register/lang_basic register/lang_empty_name
	register/lang_toolong register/lang_toomany
	register/lang_toolate register/lang_next_not_null
	init/small_packet_size init/large_packet_size
	cleanup/no_handshake
	callbacks/set_after_session
	getter/names_before_handshake
	negotiate/no_common_kex negotiate/no_common_comp_s2c
	kexinit/peer_trunc_namelist
	none/enc none/mac none/comp
	disconnect/long_description
	debug/no_callback debug/truncated_payload
	debug/msg_len_exceeds_payload
	unimplemented/short_payload
	global_request/no_callback global_request/rejected
	global_request/with_reply global_request/no_reply
	global_request/truncated global_request/name_exceeds
	aes256_ctr/null_ctx aes256_ctr/ctx_member_null
	aes256_ctr/alloc_fail aes256_ctr/encrypt_update_fail
	hmac_sha2_256/null_ctx hmac_sha2_256/alloc_fail
	hmac_sha2_256/fetch_failure hmac_sha2_256/mac_init_failure
	hmac_sha2_256/generate_failure hmac_sha2_256/reinit_failure
	hmac_sha2_256/cleanup_null
	guard/first_name_basic guard/first_name_single
	guard/first_name_small_buf
	guard/blocksize_lt8 guard/blocksize_clamp_direct
	guard/rekey_time_zero
	guard/ed25519_sign_small_buf guard/ed25519_pubkey_small_buf
	guard/ed25519_haskey_wrong_type
	guard/rsa_sign_small_buf guard/rsa_pubkey_small_buf
	guard/rsa_haskey_wrong_type
	guard/cleanup_null_cleanup_fn
	guard/remote_languages_cleanup
	guard/bn_mpint_small_buf
	dhgex/parse_bn_mpint_valid dhgex/parse_bn_mpint_short
	dhgex/parse_bn_mpint_truncated
	dhgex/serialize_zero_bn dhgex/serialize_alloc_fail
	dhgex/dh_value_ok dhgex/dh_value_zero
	dhgex/dh_value_one dhgex/dh_value_negative
	dhgex/dh_value_equal_p dhgex/dh_value_greater_p
	dhgex/dh_value_p_minus_one
	dhgex/exchange_hash_alloc_iter
	dhgex/server_recv_fail dhgex/server_bad_request_type
	dhgex/server_short_request
	dhgex/server_null_provider dhgex/server_provider_error
	dhgex/server_recv_init_fail dhgex/server_bad_init_type
	dhgex/server_e_zero
	dhgex/server_ka_null dhgex/server_null_pubkey_fn
	dhgex/server_null_sign_fn
	c25519/x25519_exchange_alloc
	c25519/encode_ss_leading_zeros c25519/encode_ss_alloc_fail
	c25519/server_recv_fail c25519/server_bad_init_type
	c25519/server_bad_qc_len c25519/server_qc_overrun
	c25519/server_ka_null c25519/server_null_pubkey_fn
	c25519/server_null_sign_fn
)
dssh_add_individual_tests(dssh_test_transport dssh_transport TRANSPORT_TESTS)

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
	auth/server_send_fail/svc_accept
	auth/server_send_fail/none_success auth/server_send_fail/none_failure
	auth/server_send_fail/pw_success auth/server_send_fail/pw_failure
	auth/server_send_fail/pw_no_cb auth/server_send_fail/pw_changereq
	auth/server_send_fail/pk_success auth/server_send_fail/pk_ok
	auth/server_send_fail/pk_no_cb auth/server_send_fail/pk_unknown
	auth/server_send_fail/pk_bad_sig auth/server_send_fail/pk_reject_sig
	auth/server_send_fail/pk_probe_rej
	auth/server_send_fail/unknown_meth
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
	direct/pk_ok_alloc direct/pk_verify_alloc
	direct/changereq_alloc
	dclient/wrong_svc_reply dclient/get_methods_unexpected
	dclient/get_methods_trunc_payload
	dclient/get_methods_ctrl_char dclient/get_methods_del
	dclient/get_methods_alloc dclient/get_methods_malloc
	dclient/get_methods_msg_alloc
	dclient/changereq_no_cb dclient/changereq_trunc
	dclient/changereq_no_lang
	dclient/changereq_prompt_overflow
	dclient/changereq_lang_overflow
	dclient/changereq_cb_error
	dclient/pk_no_sign dclient/pk_alloc_fail
	dclient/kbi_alloc_fail dclient/kbi_calloc_chain
	dclient/kbi_resp_alloc
)
dssh_add_individual_tests(dssh_test_auth dssh_auth AUTH_TESTS)

dssh_add_test(conn test_conn.c)
target_link_libraries(dssh_test_conn PRIVATE dssh_mock_io)
set(CONN_TESTS
	parse/pty_req_valid parse/pty_req_no_modes
	parse/pty_req_modes_truncated parse/pty_req_truncated
	parse/pty_req_empty
	parse/env_valid parse/env_truncated_name
	parse/env_truncated_value parse/env_empty
	parse/env_missing_value_header
	parse/exec_valid parse/exec_truncated
	parse/exec_empty parse/exec_zero_length
	parse/subsystem_valid parse/subsystem_truncated
	parse/subsystem_empty
	test_session_start test_session_stop
	test_session_start_stop test_session_start_twice
	test_open_exec test_open_exec_command
	test_open_exec_rejected test_open_shell test_open_subsystem
	test_session_write_read test_session_read_write_reverse
	test_session_write_ext test_session_read_ext
	test_session_poll_read test_session_poll_timeout
	test_session_large_write test_session_multiple_writes
	test_subsys_write_read test_subsys_multiple_writes
	test_subsys_poll test_subsys_large_write
	test_send_signal test_signal_after_data test_window_change
	test_session_close_exit_code test_channel_close_raw
	test_eof_detection test_eof_half_close
	test_graceful_disconnect
	test_two_channels test_accept_reject
	test_channel_independence
	test_concurrent_write_read test_terminate_during_io
	test_connection_drop_during_read
	test_connection_drop_during_write test_cleanup_after_drop
	test_accept_timeout_zero test_accept_timeout_short
	test_reject_null_description
	test_session_poll_timeout_zero
	test_accept_raw test_subsystem_roundtrip test_accept_nsec_overflow
	test_accept_timeout_negative
	test_session_poll_nsec_overflow
	test_session_write_not_open test_session_write_after_eof
	test_session_write_window_zero
	test_session_write_ext_not_open
	test_session_write_ext_after_eof
	test_session_write_ext_window_zero
	test_session_read_ext_empty
	test_session_poll_read_empty test_session_poll_write
	test_session_poll_close_read test_session_poll_close_readext
	test_session_write_close_recv
	test_session_write_ext_close_recv
	test_channel_write_not_open test_channel_write_close_recv
	test_channel_read_empty_queue
	test_channel_poll_close_received test_channel_poll_eof
	test_chan_poll_infinite_ready test_sess_poll_infinite_ready
	test_raw_channel_poll_timeout_zero
	test_raw_channel_poll_write
	test_raw_channel_write_after_close
	test_raw_channel_write_toolong
	test_send_eof_already_sent test_send_close_already_sent
	test_double_eof_close test_data_after_eof
	test_ext_data_after_eof
	test_read_signal_empty
	test_reject_long_description
	test_setup_exec_rejected_by_callback
	test_channel_request_want_reply
	test_channel_success_no_request
	test_demux_short_payload test_demux_chan_type_zero
	test_data_dlen_exceeds_payload test_data_exceeds_window
	test_truncated_channel_data test_truncated_ext_data
	test_truncated_channel_open test_truncated_open_confirmation
	test_truncated_channel_request
	test_open_conf_unknown_channel
	test_window_adjust_from_peer test_window_add_overflow
	test_window_underflow_to_zero
	test_replenish_low_window test_replenish_after_eof
	test_send_ext_data_toolong
	test_signal_interleave_read test_signal_interleave_read_ext
	test_stdout_signal_mark_consumed
	test_stderr_signal_mark_consumed
	test_stderr_signal_truncate
	test_auto_reject_x11 test_auto_reject_direct_tcpip
	test_auto_reject_forwarded_tcpip
	test_auto_reject_session_server
)
dssh_add_individual_tests(dssh_test_conn dssh_conn CONN_TESTS)

# ================================================================
# Tier 1b: Alloc failure tests (need mock allocator + mock I/O)
# ================================================================

dssh_add_test(alloc test_alloc.c)
target_link_libraries(dssh_test_alloc PRIVATE dssh_mock_io dssh_mock_alloc)
target_link_options(dssh_test_alloc PRIVATE ${DSSH_WRAP_FLAGS})
add_test(NAME dssh_unit_alloc COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc PROPERTIES
	COST 100
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_dhgex COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_dhgex PROPERTIES
	COST 200 TIMEOUT 120
	ENVIRONMENT "DSSH_TEST_KEX=dh-gex;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_rsa COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_rsa PROPERTIES
	COST 200 TIMEOUT 120
	ENVIRONMENT "DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_dhgex_rsa COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_dhgex_rsa PROPERTIES
	COST 300 TIMEOUT 600
	ENVIRONMENT "DSSH_TEST_KEX=dh-gex;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")

add_test(NAME dssh_unit_alloc_sntrup COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_sntrup PROPERTIES
	COST 400 TIMEOUT 600
	ENVIRONMENT "DSSH_TEST_KEX=sntrup;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_sntrup_rsa COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_sntrup_rsa PROPERTIES
	COST 400 TIMEOUT 600
	ENVIRONMENT "DSSH_TEST_KEX=sntrup;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")

add_test(NAME dssh_unit_alloc_mlkem COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_mlkem PROPERTIES
	COST 300 TIMEOUT 600
	ENVIRONMENT "DSSH_TEST_KEX=mlkem;${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_unit_alloc_mlkem_rsa COMMAND dssh_test_alloc)
set_tests_properties(dssh_unit_alloc_mlkem_rsa PROPERTIES
	COST 300 TIMEOUT 600
	ENVIRONMENT "DSSH_TEST_KEX=mlkem;DSSH_TEST_KEY=rsa;${DSSH_TEST_KEY_ENV}")

# Targeted coverage tests — run in their own process for clean state
add_test(NAME dssh_targeted_c25519_server_trunc
	COMMAND dssh_test_alloc c25519/server_truncated_init)
set_tests_properties(dssh_targeted_c25519_server_trunc PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_targeted_c25519_client_bad_qs
	COMMAND dssh_test_alloc c25519/client_bad_qs_length)
set_tests_properties(dssh_targeted_c25519_client_bad_qs PROPERTIES
	ENVIRONMENT "${DSSH_TEST_KEY_ENV}")
add_test(NAME dssh_targeted_dhgex_bn_rand_fail
	COMMAND dssh_test_alloc dhgex/server_bn_rand_fail)
set_tests_properties(dssh_targeted_dhgex_bn_rand_fail PROPERTIES
	ENVIRONMENT "DSSH_TEST_KEX=dh-gex;${DSSH_TEST_KEY_ENV}")

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
# Tier 3a: Self-test (integration) — one process per test
# ================================================================

dssh_add_test(selftest test_selftest.c)
set(SELFTEST_TESTS
	test_self_handshake_curve25519
	test_self_auth_password
	test_self_exec_echo
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
	test_self_connection_drop
)
dssh_add_individual_tests(dssh_test_selftest dssh_self SELFTEST_TESTS)

# ================================================================
# Tier 3b/3c: Optional interop tests
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
