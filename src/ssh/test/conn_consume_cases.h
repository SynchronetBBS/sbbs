/* Per-case controls reuse the bodies called by the ordinary matrices. */
#ifndef DSSH_TEST_CONN_CONSUME_CASES_H
#define DSSH_TEST_CONN_CONSUME_CASES_H

static int
consume_validation_host(unsigned a, unsigned b, unsigned c, unsigned d)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	return test_terminal_mode_validation();
}
static int
consume_capture_host(unsigned a, unsigned b, unsigned c, unsigned d)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	return test_terminal_capture_bounds();
}

struct consume_control_stage {
	enum consume_stage stage;
	unsigned created, retired;
};
static const struct {
	const char *name;
	int (*function)(unsigned, unsigned, unsigned, unsigned);
	unsigned args[4], group, workers, owners, sizes[8], stages;
	bool growth_only;
	struct consume_control_stage checkpoints[24];
} consume_consumers[] = {
	{"close_0_0_zero", close_disposal_case, {0, 0, 0, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_max_u32", close_disposal_case, {0, 0, 1, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_omit", close_disposal_case, {0, 0, 2, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_min_i64", close_disposal_case, {0, 0, 3, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_over_u32", close_disposal_case, {0, 0, 4, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_max_i64", close_disposal_case, {0, 0, 5, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_status_io", close_disposal_case, {0, 0, 6, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_eof_io", close_disposal_case, {0, 0, 7, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_close_io", close_disposal_case, {0, 0, 8, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_terminated", close_disposal_case, {0, 0, 9, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_peer_closed", close_disposal_case, {0, 0, 10, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_status_capacity", close_disposal_case, {0, 0, 11, 0}, 0, 1, 2, {1, 0}, 6, true,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_LIMIT, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_terminal_capacity", close_disposal_case, {0, 0, 12, 0}, 0, 1, 2, {1, 0}, 6, true,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_LIMIT, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_hard_counter", close_disposal_case, {0, 0, 13, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_detach_acquisition", close_disposal_case, {0, 0, 14, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_local_closed", close_disposal_case, {0, 0, 15, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_close_publication", close_disposal_case, {0, 0, 16, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_0_send_invalid", close_disposal_case, {0, 0, 17, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_zero", close_disposal_case, {0, 1, 0, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_max_u32", close_disposal_case, {0, 1, 1, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_omit", close_disposal_case, {0, 1, 2, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_min_i64", close_disposal_case, {0, 1, 3, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_over_u32", close_disposal_case, {0, 1, 4, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_max_i64", close_disposal_case, {0, 1, 5, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_status_io", close_disposal_case, {0, 1, 6, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_eof_io", close_disposal_case, {0, 1, 7, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_close_io", close_disposal_case, {0, 1, 8, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_terminated", close_disposal_case, {0, 1, 9, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_peer_closed", close_disposal_case, {0, 1, 10, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_status_capacity", close_disposal_case, {0, 1, 11, 0}, 0, 1, 2, {1, 0}, 6, true,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_LIMIT, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_terminal_capacity", close_disposal_case, {0, 1, 12, 0}, 0, 1, 2, {1, 0}, 6, true,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_LIMIT, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_hard_counter", close_disposal_case, {0, 1, 13, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_detach_acquisition", close_disposal_case, {0, 1, 14, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_local_closed", close_disposal_case, {0, 1, 15, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_close_publication", close_disposal_case, {0, 1, 16, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_0_1_send_invalid", close_disposal_case, {0, 1, 17, 0}, 0, 1, 2, {1, 0}, 5, false,
	    {{CO_OPEN_STARTED, 1, 1}, {CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_zero", close_disposal_case, {1, 0, 0, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_max_u32", close_disposal_case, {1, 0, 1, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_omit", close_disposal_case, {1, 0, 2, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_min_i64", close_disposal_case, {1, 0, 3, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_over_u32", close_disposal_case, {1, 0, 4, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_max_i64", close_disposal_case, {1, 0, 5, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_eof_io", close_disposal_case, {1, 0, 7, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_close_io", close_disposal_case, {1, 0, 8, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_terminated", close_disposal_case, {1, 0, 9, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_peer_closed", close_disposal_case, {1, 0, 10, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_status_capacity", close_disposal_case, {1, 0, 11, 0}, 0, 1, 2, {1, 0}, 5, true,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_LIMIT, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_terminal_capacity", close_disposal_case, {1, 0, 12, 0}, 0, 1, 2, {1, 0}, 5, true,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_LIMIT, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_hard_counter", close_disposal_case, {1, 0, 13, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_detach_acquisition", close_disposal_case, {1, 0, 14, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_local_closed", close_disposal_case, {1, 0, 15, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_close_publication", close_disposal_case, {1, 0, 16, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_0_send_invalid", close_disposal_case, {1, 0, 17, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_zero", close_disposal_case, {1, 1, 0, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_max_u32", close_disposal_case, {1, 1, 1, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_omit", close_disposal_case, {1, 1, 2, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_min_i64", close_disposal_case, {1, 1, 3, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_over_u32", close_disposal_case, {1, 1, 4, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_max_i64", close_disposal_case, {1, 1, 5, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_eof_io", close_disposal_case, {1, 1, 7, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_close_io", close_disposal_case, {1, 1, 8, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_terminated", close_disposal_case, {1, 1, 9, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_peer_closed", close_disposal_case, {1, 1, 10, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_status_capacity", close_disposal_case, {1, 1, 11, 0}, 0, 1, 2, {1, 0}, 5, true,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_LIMIT, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_terminal_capacity", close_disposal_case, {1, 1, 12, 0}, 0, 1, 2, {1, 0}, 5, true,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_LIMIT, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_hard_counter", close_disposal_case, {1, 1, 13, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_detach_acquisition", close_disposal_case, {1, 1, 14, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_local_closed", close_disposal_case, {1, 1, 15, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_close_publication", close_disposal_case, {1, 1, 16, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"close_1_1_send_invalid", close_disposal_case, {1, 1, 17, 0}, 0, 1, 2, {1, 0}, 4, false,
	    {{CO_CLOSE_LIVE, 1, 2}, {CO_CLOSE_PREPARED, 1, 2}, {CO_CLOSE_CONSUMED, 1, 2}, {CO_CLOSE_CHECKED, 1, 2}}},
	{"reservation_0_0_0_0", close_reservation_case, {0, 0, 0, 0}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_0_0_0_1", close_reservation_case, {0, 0, 0, 1}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_0_0_1_0", close_reservation_case, {0, 0, 1, 0}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_0_0_1_1", close_reservation_case, {0, 0, 1, 1}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_0_1_0_0", close_reservation_case, {0, 1, 0, 0}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_0_1_0_1", close_reservation_case, {0, 1, 0, 1}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_0_1_1_0", close_reservation_case, {0, 1, 1, 0}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_0_1_1_1", close_reservation_case, {0, 1, 1, 1}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_1_0_0_0", close_reservation_case, {1, 0, 0, 0}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_1_0_0_1", close_reservation_case, {1, 0, 0, 1}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_1_0_1_0", close_reservation_case, {1, 0, 1, 0}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_1_0_1_1", close_reservation_case, {1, 0, 1, 1}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_1_1_0_0", close_reservation_case, {1, 1, 0, 0}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_1_1_0_1", close_reservation_case, {1, 1, 0, 1}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_1_1_1_0", close_reservation_case, {1, 1, 1, 0}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"reservation_1_1_1_1", close_reservation_case, {1, 1, 1, 1}, 1, 5, 3, {2, 2, 1}, 6, false,
	    {{CO_RES_PAIRS, 4, 2}, {CO_RES_PREPARED, 4, 3}, {CO_RES_STARTED, 5, 3}, {CO_RES_HELD, 5, 3}, {CO_RES_REJECTED, 5, 3}, {CO_RES_CHECKED, 5, 3}}},
	{"mode_0_0_0_0", terminal_mode_roundtrip_case, {0, 0, 0, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_0_0_1", terminal_mode_roundtrip_case, {0, 0, 1, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_0_0_2", terminal_mode_roundtrip_case, {0, 0, 2, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_0_0_3", terminal_mode_roundtrip_case, {0, 0, 3, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_0_1_0", terminal_mode_roundtrip_case, {0, 1, 0, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_0_1_1", terminal_mode_roundtrip_case, {0, 1, 1, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_0_1_2", terminal_mode_roundtrip_case, {0, 1, 2, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_0_1_3", terminal_mode_roundtrip_case, {0, 1, 3, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_1_0_0", terminal_mode_roundtrip_case, {1, 0, 0, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_1_0_1", terminal_mode_roundtrip_case, {1, 0, 1, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_1_0_2", terminal_mode_roundtrip_case, {1, 0, 2, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_1_0_3", terminal_mode_roundtrip_case, {1, 0, 3, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_1_1_0", terminal_mode_roundtrip_case, {1, 1, 0, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_1_1_1", terminal_mode_roundtrip_case, {1, 1, 1, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_1_1_2", terminal_mode_roundtrip_case, {1, 1, 2, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_0_1_1_3", terminal_mode_roundtrip_case, {1, 1, 3, 0}, 2, 2, 1, {2}, 9, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_FIRST, 1, 1}, {CO_MODE_STARTED, 2, 1}, {CO_MODE_JOINED, 2, 1}, {CO_MODE_COPY, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DATA, 2, 1}, {CO_MODE_DONE, 2, 1}}},
	{"mode_1_0_0_0", terminal_mode_wire_case, {0, 0, 0, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_0_0_1", terminal_mode_wire_case, {0, 0, 1, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_0_0_2", terminal_mode_wire_case, {0, 0, 2, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_0_0_3", terminal_mode_wire_case, {0, 0, 3, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_0_1_0", terminal_mode_wire_case, {0, 1, 0, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_0_1_1", terminal_mode_wire_case, {0, 1, 1, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_0_1_2", terminal_mode_wire_case, {0, 1, 2, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_0_1_3", terminal_mode_wire_case, {0, 1, 3, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_1_0_0", terminal_mode_wire_case, {1, 0, 0, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_1_0_1", terminal_mode_wire_case, {1, 0, 1, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_1_0_2", terminal_mode_wire_case, {1, 0, 2, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_1_0_3", terminal_mode_wire_case, {1, 0, 3, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_1_1_0", terminal_mode_wire_case, {1, 1, 0, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_1_1_1", terminal_mode_wire_case, {1, 1, 1, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_1_1_2", terminal_mode_wire_case, {1, 1, 2, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_1_1_1_3", terminal_mode_wire_case, {1, 1, 3, 0}, 2, 1, 1, {1}, 6, false,
	    {{CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_STARTED, 1, 1}, {CO_MODE_WIRE, 1, 1}, {CO_MODE_JOINED, 1, 1}, {CO_MODE_DONE, 1, 1}}},
	{"mode_validation", consume_validation_host, {0, 0, 0, 0}, 2, 0, 1, {0}, 6, false,
	    {{CO_MODE_VALIDATED, 0, 0}, {CO_MODE_VALIDATED, 0, 0}, {CO_MODE_VALIDATED, 0, 0}, {CO_MODE_EMPTY, 0, 1}, {CO_MODE_BUILT, 0, 1}, {CO_MODE_COPY, 0, 1}}},
	{"mode_capture", consume_capture_host, {0, 0, 0, 0}, 2, 0, 0, {0}, 1, false,
	    {{CO_MODE_CAPTURE, 0, 0}}},
};

enum consume_fault {CONSUME_NATIVE, CONSUME_CREATE, CONSUME_STAGE};
static enum consume_fault consume_fault;
static size_t consume_selected_consumer;
static unsigned consume_parameter, consume_seen, consume_retired, consume_acquired, consume_cancels, consume_builders, consume_contexts, consume_watched, consume_frees;
static bool consume_fired, consume_valid_order, consume_valid_retirement;

static bool
consume_control_checkpoint(enum consume_stage stage)
{
	if (consume_seen >= consume_consumers[consume_selected_consumer].stages) {
		consume_valid_order = false;
		return true;
	}
	const struct consume_control_stage *expected =
	    &consume_consumers[consume_selected_consumer].checkpoints[consume_seen];
	consume_valid_order = consume_valid_order && stage == expected->stage
	    && channel_worker_probe != NULL && channel_worker_probe->created == expected->created;
	unsigned active = channel_fixture_owner != NULL ? 1U : 0U;
	consume_valid_order = consume_valid_order && consume_retired <= UINT_MAX - active
	    && consume_retired + active == expected->retired;
	unsigned index = consume_seen++;
	if (consume_fault == CONSUME_STAGE && index == consume_parameter) {
		consume_fired = true;
		return false;
	}
	return true;
}

static void
consume_control_retire(const struct channel_workers *workers)
{
	if (consume_retired >= 8)
		abort();
	consume_retired++;
	consume_valid_retirement = consume_valid_retirement && !workers->created[0]
	    && !workers->created[1] && workers->owner->workers == NULL
	    && workers->owner->client != NULL && workers->owner->server != NULL
	    && !tx_owner_is_current(workers->owner->client)
	    && !tx_owner_is_current(workers->owner->server);
	if (workers->release == NULL)
		return;
	if (workers->release != flow_release)
		abort();
	const struct flow_fixture *flow = workers->resource;
	consume_valid_retirement = consume_valid_retirement && !flow->tx.held
	    && flow->reserved == NULL && !flow->watching && !flow->paused && flow->cancels == 0;
	if (workers->retire == close_outcome_retire)
		consume_valid_retirement = consume_valid_retirement && setup_tx_test_hook == NULL
		    && window_tx_return_test_hook == NULL && close_outcome_publication_session == NULL;
	if (workers->retire == consume_params_retire) {
		const struct consume_params_fixture *fixture = flow->data;
		consume_valid_retirement = consume_valid_retirement && fixture->retired
		    && fixture->params.term == NULL && fixture->params.command == NULL
		    && fixture->params.subsystem == NULL && fixture->params.env == NULL
		    && fixture->params.env_count == 0 && fixture->params.env_capacity == 0
		    && fixture->params.modes == NULL && fixture->params.mode_count == 0
		    && fixture->params.mode_capacity == 0;
		consume_builders++;
	}
	if (flow->unblock == close_reservation_release) {
		const struct close_reservation_job *job = flow->data;
		bool acquired = atomic_load(&job->acquired);
		consume_valid_retirement = consume_valid_retirement && atomic_load(&job->resume)
		    && atomic_load(&job->finish) && !atomic_load(&job->reserved) && job->cancels <= 1
		    && atomic_load(&job->done) == (channel_worker_probe->created == 5);
		if (consume_fired || channel_worker_probe->creation_failed)
			consume_valid_retirement = consume_valid_retirement && atomic_load(&job->cancelled)
			    && job->cancels == (acquired ? 1U : 0U);
		consume_acquired += acquired ? 1U : 0U;
		consume_cancels += job->cancels;
	}
}

static void
consume_close_retired(const struct conn_ctx *ctx, const struct close_outcome_lifetime *watch)
{
	consume_contexts++;
	consume_valid_retirement = consume_valid_retirement && ctx->client == NULL
	    && ctx->server == NULL && !ctx->io_initialized && watch->handle == NULL
	    && watch->frees == (watch->watched ? 1U : 0U);
	consume_watched += watch->watched ? 1U : 0U;
	consume_frees += watch->frees;
}

static bool
consume_schedule(size_t consumer, enum consume_fault fault, unsigned parameter)
{
	if (consumer >= sizeof(consume_consumers) / sizeof(consume_consumers[0]))
		return false;
	consume_selected_consumer = consumer;
	consume_fault = fault;
	consume_parameter = parameter;
	consume_fired = false;
	consume_seen = 0;
	consume_retired = 0;
	consume_acquired = 0;
	consume_cancels = 0;
	consume_contexts = 0;
	consume_watched = 0;
	consume_frees = 0;
	consume_builders = 0;
	consume_valid_order = true;
	consume_valid_retirement = true;
	consume_stage_control = consume_control_checkpoint;
	conn_actor_retirement_observer = consume_control_retire;
	consume_close_retirement_observer = consume_close_retired;
	struct channel_worker_probe probe = {.fail_at = fault == CONSUME_CREATE ? parameter : UINT_MAX,
	    .cancelled_before_join = true};
	channel_worker_probe_begin(&probe);
	int host = consume_consumers[consumer].function(consume_consumers[consumer].args[0],
	    consume_consumers[consumer].args[1], consume_consumers[consumer].args[2], consume_consumers[consumer].args[3]);
	channel_worker_probe_end();
	consume_stage_control = NULL;
	conn_actor_retirement_observer = NULL;
	consume_close_retirement_observer = NULL;
	unsigned expected = consume_consumers[consumer].workers;
	unsigned retired = consume_consumers[consumer].owners;
	unsigned stages = consume_consumers[consumer].stages;
	unsigned watched = consume_consumers[consumer].group == 0 ? 1U : 0U;
	if (fault == CONSUME_STAGE) {
		const struct consume_control_stage *stage = &consume_consumers[consumer].checkpoints[parameter];
		expected = stage->created;
		retired = stage->retired;
		stages = parameter + 1;
		if (stage->stage == CO_OPEN_STARTED)
			watched = 0;
	}
	if (fault == CONSUME_CREATE) {
		expected = parameter;
		watched = 0;
		unsigned starts = 0;
		retired = 0;
		for (unsigned i = 0; i < consume_consumers[consumer].owners; i++) {
			unsigned size = consume_consumers[consumer].sizes[i];
			if (size > CHANNEL_WORKER_PROBE_CAP - starts)
				abort();
			if (parameter < starts + size) {
				retired = i + 1;
				break;
			}
			starts += size;
		}
		stages = 0;
		while (stages < consume_consumers[consumer].stages
		    && consume_consumers[consumer].checkpoints[stages].created <= parameter)
			stages++;
	}
	unsigned contexts = consume_consumers[consumer].group == 0 ? 1U : 0U;
	unsigned builders = consume_consumers[consumer].group == 2 ? retired : 0;
	bool reservation = consume_acquired == 0 && consume_cancels == 0;
	if (consume_consumers[consumer].group == 1) {
		unsigned cancels = consume_consumers[consumer].args[3] == 0 ? 1U : 0U;
		if (fault == CONSUME_NATIVE)
			reservation = consume_acquired == 1 && consume_cancels == cancels;
		if (fault == CONSUME_STAGE) {
			enum consume_stage stage = consume_consumers[consumer].checkpoints[parameter].stage;
			if (stage == CO_RES_STARTED)
				reservation = consume_acquired <= 1 && consume_cancels == consume_acquired;
			if (stage == CO_RES_HELD || stage == CO_RES_REJECTED)
				reservation = consume_acquired == 1 && consume_cancels == 1;
			if (stage == CO_RES_CHECKED)
				reservation = consume_acquired == 1 && consume_cancels == cancels;
		}
	}
	bool valid = host == (fault == CONSUME_NATIVE ? TEST_PASS : TEST_FAIL)
	    && consume_valid_order && consume_seen == stages && consume_valid_retirement
	    && consume_retired == retired && consume_builders == builders && reservation
	    && consume_contexts == contexts && consume_watched == watched && consume_frees == watched
	    && probe.created == expected && probe.joined == expected
	    && probe.cancelled_before_join && channel_fixture_owner == NULL && g_active_ctx == NULL
	    && conn_rx_gate == NULL && consume_fired == (fault == CONSUME_STAGE)
	    && probe.creation_failed == (fault == CONSUME_CREATE)
	    && probe.calls == expected + (fault == CONSUME_CREATE ? 1U : 0U);
	for (unsigned i = 0; i < probe.created; i++)
		valid = valid && atomic_load(&probe.jobs[i].done);
	unsigned fault_number = (unsigned)fault;
	if (printf("consume_schedule consumer=%s fault=%u parameter=%u host=%d created=%u joined=%u retired=%u stages=%u acquired=%u cancels=%u builders=%u contexts=%u watched=%u frees=%u valid=%u\n",
	    consume_consumers[consumer].name, fault_number, parameter, host, probe.created,
	    probe.joined, consume_retired, consume_seen, consume_acquired, consume_cancels, consume_builders, consume_contexts, consume_watched, consume_frees, valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static int
consume_owner_cases(unsigned group)
{
	bool valid = true;
	unsigned schedules = 0, consumers = 0;
	for (size_t i = 0; i < sizeof(consume_consumers) / sizeof(consume_consumers[0]); i++) {
		if (consume_consumers[i].group != group)
			continue;
		const char *compression = getenv("DSSH_TEST_COMPRESSION");
		bool growth = compression != NULL && strcmp(compression, "test-growth") == 0;
		if (consume_consumers[i].growth_only && !growth)
			continue;
		consumers++;
		valid = consume_schedule(i, CONSUME_NATIVE, 0);
		if (!valid)
			return TEST_FAIL;
		schedules++;
		for (unsigned fail = 0; fail < consume_consumers[i].workers; fail++) {
			valid = consume_schedule(i, CONSUME_CREATE, fail);
			if (!valid)
				return TEST_FAIL;
			schedules++;
		}
		for (unsigned stage = 0; stage < consume_consumers[i].stages; stage++) {
			valid = consume_schedule(i, CONSUME_STAGE, stage);
			if (!valid)
				return TEST_FAIL;
			schedules++;
		}
	}
	if (printf("consume_control group=%u consumers=%u schedules=%u valid=%u\n",
	    group, consumers, schedules, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(consumers != 0 && valid);
	return TEST_PASS;
}

static int
consume_fatal_child(const char *operation, const char *mode)
{
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(consume_consumers) / sizeof(consume_consumers[0]); i++) {
		for (unsigned join = 1; join <= consume_consumers[i].workers; join++) {
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
			consume_consumers[i].function(consume_consumers[i].args[0],
			    consume_consumers[i].args[1], consume_consumers[i].args[2], consume_consumers[i].args[3]);
			channel_worker_probe_end();
			return 4;
		}
	}
	return 2;
}

static int
test_consume_owner_close(void)
{
	return consume_owner_cases(0);
}
static int
test_consume_owner_reservation(void)
{
	return consume_owner_cases(1);
}
static int
test_consume_owner_mode(void)
{
	return consume_owner_cases(2);
}

#endif
