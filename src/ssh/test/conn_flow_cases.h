/* Failure controls invoke complete original hosts with real created handles. */
#ifndef DSSH_TEST_CONN_FLOW_CASES_H
#define DSSH_TEST_CONN_FLOW_CASES_H

struct flow_control_stage {
	enum flow_stage stage;
	unsigned ordinal, created, retired;
};
static const struct {
	const char *name;
	int (*function)(void);
	unsigned group, workers, owners, sizes[4], stages;
	struct flow_control_stage checkpoints[8];
} flow_consumers[] = {
	{"tx_slot_resize", test_tx_slot_resize, 0, 1, 1, {1}, 2,
	    {{FLOW_TX_STARTED, 0, 1, 1}, {FLOW_TX_WAITING, 0, 1, 1}}},
	{"tx_slot_refusal", test_tx_slot_refusal, 0, 4, 4, {1, 1, 1, 1}, 8,
	    {{FLOW_REFUSAL_STARTED, 0, 1, 1}, {FLOW_REFUSAL_WAITING, 0, 1, 1}, {FLOW_REFUSAL_STARTED, 1, 2, 2}, {FLOW_REFUSAL_WAITING, 1, 2, 2}, {FLOW_REFUSAL_STARTED, 2, 3, 3}, {FLOW_REFUSAL_WAITING, 2, 3, 3}, {FLOW_REFUSAL_STARTED, 3, 4, 4}, {FLOW_REFUSAL_WAITING, 3, 4, 4}}},
	{"transmit_rekey_threshold", test_transmit_rekey_threshold, 1, 2, 2, {1, 1}, 4,
	    {{FLOW_REKEY_PREPARED, 0, 0, 1}, {FLOW_REKEY_STARTED, 0, 1, 1}, {FLOW_REKEY_PREPARED, 1, 1, 2}, {FLOW_REKEY_STARTED, 1, 2, 2}}},
	{"transmit_rekey_inflight", test_transmit_rekey_inflight, 1, 1, 1, {1}, 2,
	    {{FLOW_REKEY_PREPARED, 0, 0, 1}, {FLOW_REKEY_STARTED, 0, 1, 1}}},
	{"transmit_rekey_simultaneous", test_transmit_rekey_simultaneous, 1, 1, 1, {1}, 2,
	    {{FLOW_REKEY_PREPARED, 0, 0, 1}, {FLOW_REKEY_STARTED, 0, 1, 1}}},
	{"transmit_rekey_failure", test_transmit_rekey_failure, 1, 1, 3, {0, 0, 1}, 5,
	    {{FLOW_REKEY_PREPARED, 0, 0, 1}, {FLOW_REKEY_PREPARED, 1, 0, 2}, {FLOW_REKEY_PREPARED, 2, 0, 3}, {FLOW_REKEY_STARTED, 0, 1, 3}, {FLOW_REKEY_WAITING, 0, 1, 3}}},
	{"window_credit_packet_0_stream_small", test_window_credit_packet_0_stream_small, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_packet_0_stream_full", test_window_credit_packet_0_stream_full, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_packet_0_zc_small", test_window_credit_packet_0_zc_small, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_packet_0_zc_full", test_window_credit_packet_0_zc_full, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_packet_0_cancel", test_window_credit_packet_0_cancel, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_packet_0_oversize", test_window_credit_packet_0_oversize, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_packet_0_key_limit", test_window_credit_packet_0_key_limit, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_packet_0_io_failure", test_window_credit_packet_0_io_failure, 2, 1, 1, {1}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}}},
	{"window_credit_packet_1_stream_small", test_window_credit_packet_1_stream_small, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_packet_1_stream_full", test_window_credit_packet_1_stream_full, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_packet_1_zc_small", test_window_credit_packet_1_zc_small, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_packet_1_zc_full", test_window_credit_packet_1_zc_full, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_packet_1_cancel", test_window_credit_packet_1_cancel, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_packet_1_oversize", test_window_credit_packet_1_oversize, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_packet_1_key_limit", test_window_credit_packet_1_key_limit, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_packet_1_io_failure", test_window_credit_packet_1_io_failure, 2, 1, 1, {1}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}}},
	{"window_credit_gather_0_stream_small", test_window_credit_gather_0_stream_small, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_gather_0_stream_full", test_window_credit_gather_0_stream_full, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_gather_0_zc_small", test_window_credit_gather_0_zc_small, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_gather_0_zc_full", test_window_credit_gather_0_zc_full, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_gather_0_cancel", test_window_credit_gather_0_cancel, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_gather_0_oversize", test_window_credit_gather_0_oversize, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_gather_0_key_limit", test_window_credit_gather_0_key_limit, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_gather_0_io_failure", test_window_credit_gather_0_io_failure, 2, 1, 1, {1}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}}},
	{"window_credit_gather_1_stream_small", test_window_credit_gather_1_stream_small, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_gather_1_stream_full", test_window_credit_gather_1_stream_full, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_gather_1_zc_small", test_window_credit_gather_1_zc_small, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_gather_1_zc_full", test_window_credit_gather_1_zc_full, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_gather_1_cancel", test_window_credit_gather_1_cancel, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_gather_1_oversize", test_window_credit_gather_1_oversize, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_gather_1_key_limit", test_window_credit_gather_1_key_limit, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_gather_1_io_failure", test_window_credit_gather_1_io_failure, 2, 1, 1, {1}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}}},
	{"window_credit_reverse_packet_0_stream_small", test_window_credit_reverse_packet_0_stream_small, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_reverse_packet_0_stream_full", test_window_credit_reverse_packet_0_stream_full, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_reverse_packet_0_zc_small", test_window_credit_reverse_packet_0_zc_small, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_reverse_packet_0_zc_full", test_window_credit_reverse_packet_0_zc_full, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_reverse_packet_0_cancel", test_window_credit_reverse_packet_0_cancel, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_packet_0_oversize", test_window_credit_reverse_packet_0_oversize, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_packet_0_key_limit", test_window_credit_reverse_packet_0_key_limit, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_packet_0_io_failure", test_window_credit_reverse_packet_0_io_failure, 2, 1, 1, {1}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}}},
	{"window_credit_reverse_packet_1_stream_small", test_window_credit_reverse_packet_1_stream_small, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_reverse_packet_1_stream_full", test_window_credit_reverse_packet_1_stream_full, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_reverse_packet_1_zc_small", test_window_credit_reverse_packet_1_zc_small, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_reverse_packet_1_zc_full", test_window_credit_reverse_packet_1_zc_full, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_reverse_packet_1_cancel", test_window_credit_reverse_packet_1_cancel, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_packet_1_oversize", test_window_credit_reverse_packet_1_oversize, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_packet_1_key_limit", test_window_credit_reverse_packet_1_key_limit, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_packet_1_io_failure", test_window_credit_reverse_packet_1_io_failure, 2, 1, 1, {1}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}}},
	{"window_credit_reverse_gather_0_stream_small", test_window_credit_reverse_gather_0_stream_small, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_reverse_gather_0_stream_full", test_window_credit_reverse_gather_0_stream_full, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_reverse_gather_0_zc_small", test_window_credit_reverse_gather_0_zc_small, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_reverse_gather_0_zc_full", test_window_credit_reverse_gather_0_zc_full, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_reverse_gather_0_cancel", test_window_credit_reverse_gather_0_cancel, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_gather_0_oversize", test_window_credit_reverse_gather_0_oversize, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_gather_0_key_limit", test_window_credit_reverse_gather_0_key_limit, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_gather_0_io_failure", test_window_credit_reverse_gather_0_io_failure, 2, 1, 1, {1}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}}},
	{"window_credit_reverse_gather_1_stream_small", test_window_credit_reverse_gather_1_stream_small, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_reverse_gather_1_stream_full", test_window_credit_reverse_gather_1_stream_full, 2, 2, 1, {2}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}}},
	{"window_credit_reverse_gather_1_zc_small", test_window_credit_reverse_gather_1_zc_small, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_reverse_gather_1_zc_full", test_window_credit_reverse_gather_1_zc_full, 2, 2, 1, {2}, 4,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 0, 2, 1}}},
	{"window_credit_reverse_gather_1_cancel", test_window_credit_reverse_gather_1_cancel, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_gather_1_oversize", test_window_credit_reverse_gather_1_oversize, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_gather_1_key_limit", test_window_credit_reverse_gather_1_key_limit, 2, 2, 1, {2}, 5,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}, {FLOW_WINDOW_MARKER, 0, 2, 1}, {FLOW_WINDOW_RESERVED, 1, 2, 1}}},
	{"window_credit_reverse_gather_1_io_failure", test_window_credit_reverse_gather_1_io_failure, 2, 1, 1, {1}, 3,
	    {{FLOW_WINDOW_STARTED, 0, 1, 1}, {FLOW_WINDOW_ACCEPTED, 0, 1, 1}, {FLOW_WINDOW_RESERVED, 0, 1, 1}}},
	{"queue_retry_packet_ordinary", test_queue_retry_packet_ordinary, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_key_refusal", test_queue_retry_packet_key_refusal, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_size_refusal", test_queue_retry_packet_size_refusal, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_key_partial", test_queue_retry_packet_key_partial, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_size_partial", test_queue_retry_packet_size_partial, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_phase_pause", test_queue_retry_packet_phase_pause, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_concurrent_key", test_queue_retry_packet_concurrent_key, 3, 3, 1, {3}, 5,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_MERGER, 0, 3, 1}, {FLOW_QUEUE_REFUSED, 0, 3, 1}}},
	{"queue_retry_packet_channel_key_partial", test_queue_retry_packet_channel_key_partial, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_window_size", test_queue_retry_packet_window_size, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_concurrent_size", test_queue_retry_packet_concurrent_size, 3, 3, 1, {3}, 5,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_MERGER, 0, 3, 1}, {FLOW_QUEUE_REFUSED, 0, 3, 1}}},
	{"queue_retry_packet_key_rekey", test_queue_retry_packet_key_rekey, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_size_rekey", test_queue_retry_packet_size_rekey, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_key_probe_send", test_queue_retry_packet_key_probe_send, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_key_probe_write", test_queue_retry_packet_key_probe_write, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_key_probe_slot", test_queue_retry_packet_key_probe_slot, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_key_probe_rekey", test_queue_retry_packet_key_probe_rekey, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_size_probe_send", test_queue_retry_packet_size_probe_send, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_size_probe_write", test_queue_retry_packet_size_probe_write, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_size_probe_slot", test_queue_retry_packet_size_probe_slot, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_size_probe_rekey", test_queue_retry_packet_size_probe_rekey, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_ordinary", test_queue_retry_gather_ordinary, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_key_refusal", test_queue_retry_gather_key_refusal, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_size_refusal", test_queue_retry_gather_size_refusal, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_key_partial", test_queue_retry_gather_key_partial, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_size_partial", test_queue_retry_gather_size_partial, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_phase_pause", test_queue_retry_gather_phase_pause, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_concurrent_key", test_queue_retry_gather_concurrent_key, 3, 3, 1, {3}, 5,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_MERGER, 0, 3, 1}, {FLOW_QUEUE_REFUSED, 0, 3, 1}}},
	{"queue_retry_gather_channel_key_partial", test_queue_retry_gather_channel_key_partial, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_window_size", test_queue_retry_gather_window_size, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_concurrent_size", test_queue_retry_gather_concurrent_size, 3, 3, 1, {3}, 5,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_MERGER, 0, 3, 1}, {FLOW_QUEUE_REFUSED, 0, 3, 1}}},
	{"queue_retry_gather_key_rekey", test_queue_retry_gather_key_rekey, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_size_rekey", test_queue_retry_gather_size_rekey, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_key_probe_send", test_queue_retry_gather_key_probe_send, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_key_probe_write", test_queue_retry_gather_key_probe_write, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_key_probe_slot", test_queue_retry_gather_key_probe_slot, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_key_probe_rekey", test_queue_retry_gather_key_probe_rekey, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_size_probe_send", test_queue_retry_gather_size_probe_send, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_size_probe_write", test_queue_retry_gather_size_probe_write, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_size_probe_slot", test_queue_retry_gather_size_probe_slot, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_size_probe_rekey", test_queue_retry_gather_size_probe_rekey, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_packet_stream_callback", test_queue_retry_packet_stream_callback, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
	{"queue_retry_gather_stream_callback", test_queue_retry_gather_stream_callback, 3, 2, 1, {2}, 4,
	    {{FLOW_QUEUE_STARTED, 0, 2, 1}, {FLOW_QUEUE_OWNED, 0, 2, 1}, {FLOW_QUEUE_READY, 0, 2, 1}, {FLOW_QUEUE_REFUSED, 0, 2, 1}}},
};

enum flow_fault {FLOW_NATIVE, FLOW_CREATE, FLOW_STAGE};
static enum flow_fault flow_fault;
static size_t flow_selected_consumer;
static unsigned flow_parameter, flow_seen, flow_retired, flow_cancels;
static bool flow_fired, flow_valid_order, flow_valid_retirement;

static bool
flow_control_checkpoint(enum flow_stage stage)
{
	if (flow_seen >= flow_consumers[flow_selected_consumer].stages) {
		flow_valid_order = false;
		return true;
	}
	const struct flow_control_stage *expected =
	    &flow_consumers[flow_selected_consumer].checkpoints[flow_seen];
	flow_valid_order = flow_valid_order && stage == expected->stage;
	unsigned index = flow_seen++;
	if (flow_fault == FLOW_STAGE && index == flow_parameter) {
		flow_fired = true;
		return false;
	}
	return true;
}

static void
flow_control_retire(const struct channel_workers *workers)
{
	if (flow_retired >= 4 || workers->release != flow_release)
		abort();
	flow_retired++;
	const struct flow_fixture *flow = workers->resource;
	flow_valid_retirement = flow_valid_retirement && !workers->created[0]
	    && !workers->created[1] && workers->owner->workers == NULL
	    && workers->owner->client != NULL && workers->owner->server != NULL
	    && !flow->tx.held && flow->reserved == NULL && !flow->watching && !flow->paused
	    && !tx_owner_is_current(workers->owner->client)
	    && !tx_owner_is_current(workers->owner->server);
	if (flow->cancels > UINT_MAX - flow_cancels)
		abort();
	flow_cancels += flow->cancels;
	if (flow->unblock == transmit_rekey_release) {
		const struct transmit_rekey_fixture *fixture = flow->data;
		flow_valid_retirement = flow_valid_retirement && atomic_load(&fixture->gate.resume);
	}
	if (flow->unblock == queue_retry_release) {
		const struct queue_retry_fixture *fixture = flow->data;
		flow_valid_retirement = flow_valid_retirement && !atomic_load(&fixture->merge_expired);
	}
	if (flow->unblock == queue_retry_release && flow_fault != FLOW_NATIVE) {
		const struct queue_retry_fixture *fixture = flow->data;
		flow_valid_retirement = flow_valid_retirement && atomic_load(&fixture->cancelled);
		flow_valid_retirement = flow_valid_retirement && atomic_load(&fixture->owner_ready)
		    && atomic_load(&fixture->flush_ready) && atomic_load(&fixture->second_release)
		    && atomic_load(&fixture->rekey_go) && atomic_load(&fixture->marker_release)
		    && atomic_load(&fixture->stream_release);
	}
}

static bool
flow_schedule(size_t consumer, enum flow_fault fault, unsigned parameter)
{
	if (consumer >= sizeof(flow_consumers) / sizeof(flow_consumers[0]))
		return false;
	flow_selected_consumer = consumer;
	flow_fault = fault;
	flow_parameter = parameter;
	flow_fired = false;
	flow_seen = 0;
	flow_retired = 0;
	flow_cancels = 0;
	flow_valid_order = true;
	flow_valid_retirement = true;
	flow_stage_control = flow_control_checkpoint;
	conn_actor_retirement_observer = flow_control_retire;
	struct channel_worker_probe probe = {.fail_at = fault == FLOW_CREATE ? parameter : UINT_MAX,
	    .cancelled_before_join = true};
	channel_worker_probe_begin(&probe);
	int host = flow_consumers[consumer].function();
	channel_worker_probe_end();
	flow_stage_control = NULL;
	conn_actor_retirement_observer = NULL;
	unsigned expected = flow_consumers[consumer].workers;
	unsigned retired = flow_consumers[consumer].owners;
	unsigned stages = flow_consumers[consumer].stages, cancels = 0;
	if (fault == FLOW_STAGE) {
		const struct flow_control_stage *stage = &flow_consumers[consumer].checkpoints[parameter];
		expected = stage->created;
		retired = stage->retired;
		stages = parameter + 1;
		cancels = stage->stage == FLOW_WINDOW_RESERVED ? 1U : 0U;
	}
	if (fault == FLOW_CREATE) {
		expected = parameter;
		unsigned starts = 0;
		retired = 0;
		for (unsigned i = 0; i < flow_consumers[consumer].owners; i++) {
			unsigned size = flow_consumers[consumer].sizes[i];
			if (size > CHANNEL_WORKER_PROBE_CAP - starts)
				abort();
			if (parameter < starts + size) {
				retired = i + 1;
				break;
			}
			starts += size;
		}
		stages = 0;
		while (stages < flow_consumers[consumer].stages
		    && flow_consumers[consumer].checkpoints[stages].created <= parameter)
			stages++;
	}
	bool valid = host == (fault == FLOW_NATIVE ? TEST_PASS : TEST_FAIL)
	    && flow_valid_order && flow_seen == stages && flow_valid_retirement
	    && flow_retired == retired && retired != 0 && flow_cancels == cancels
	    && probe.created == expected && probe.joined == expected
	    && probe.cancelled_before_join && channel_fixture_owner == NULL && g_active_ctx == NULL
	    && conn_rx_gate == NULL && flow_fired == (fault == FLOW_STAGE)
	    && probe.creation_failed == (fault == FLOW_CREATE)
	    && probe.calls == expected + (fault == FLOW_CREATE ? 1U : 0U);
	for (unsigned i = 0; i < probe.created; i++)
		valid = valid && atomic_load(&probe.jobs[i].done);
	unsigned fault_number = (unsigned)fault;
	if (printf("flow_schedule consumer=%s fault=%u parameter=%u host=%d created=%u joined=%u retired=%u stages=%u cancels=%u valid=%u\n",
	    flow_consumers[consumer].name, fault_number, parameter, host, probe.created,
	    probe.joined, flow_retired, flow_seen, flow_cancels, valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static int
flow_owner_cases(unsigned group)
{
	bool valid = true;
	unsigned schedules = 0, consumers = 0;
	for (size_t i = 0; i < sizeof(flow_consumers) / sizeof(flow_consumers[0]); i++) {
		if (flow_consumers[i].group != group)
			continue;
		consumers++;
		valid = flow_schedule(i, FLOW_NATIVE, 0) && valid;
		schedules++;
		for (unsigned fail = 0; fail < flow_consumers[i].workers; fail++) {
			valid = flow_schedule(i, FLOW_CREATE, fail) && valid;
			schedules++;
		}
		for (unsigned stage = 0; stage < flow_consumers[i].stages; stage++) {
			valid = flow_schedule(i, FLOW_STAGE, stage) && valid;
			schedules++;
		}
	}
	if (printf("flow_control group=%u consumers=%u schedules=%u valid=%u\n",
	    group, consumers, schedules, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(consumers != 0 && valid);
	return TEST_PASS;
}

static int
flow_fatal_child(const char *operation, const char *mode)
{
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(flow_consumers) / sizeof(flow_consumers[0]); i++) {
		for (unsigned join = 1; join <= flow_consumers[i].workers; join++) {
			char name[32];
			int length = snprintf(name, sizeof(name), "%zu-%u", i, join);
			if (length < 0)
				return 2;
			unsigned length_u = (unsigned)length;
			if (length_u >= sizeof(name))
				return 2;
			if (strcmp(operation, name) != 0)
				continue;
			struct channel_worker_probe probe = {.fail_at = UINT_MAX,
			    .cancelled_before_join = true, .fatal_join = join,
			    .fatal_released = strcmp(mode, "released") == 0};
			channel_worker_probe_begin(&probe);
			flow_consumers[i].function();
			channel_worker_probe_end();
			return 4;
		}
	}
	return 2;
}

static int
test_flow_bytes(void)
{
	uint8_t bytes[24];
	memset(bytes, 0x6d, sizeof(bytes));
	const size_t sizes[] = {0, 1, 8, 10, 24};
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		size_t length = sizes[i];
		ASSERT_TRUE(flow_bytes(bytes, length, length, 0x6d));
		if (length != 0) {
			ASSERT_FALSE(flow_bytes(NULL, length, length, 0x6d));
			for (size_t j = 0; j < length; j++) {
				bytes[j] = 0;
				ASSERT_FALSE(flow_bytes(bytes, length, length, 0x6d));
				bytes[j] = 0x6d;
			}
		}
		ASSERT_FALSE(flow_bytes(bytes, length + 1, length, 0x6d));
	}
	ASSERT_TRUE(flow_bytes(NULL, 0, 0, 0x6d));
	ASSERT_FALSE(flow_bytes(bytes, 25, 25, 0x6d));
	ASSERT_FALSE(flow_bytes(bytes, SIZE_MAX, SIZE_MAX, 0x6d));
	return TEST_PASS;
}

static int
test_flow_owner_tx(void)
{
	return flow_owner_cases(0);
}
static int
test_flow_owner_rekey(void)
{
	return flow_owner_cases(1);
}
static int
test_flow_owner_window(void)
{
	return flow_owner_cases(2);
}
static int
test_flow_owner_queue(void)
{
	return flow_owner_cases(3);
}

#endif
