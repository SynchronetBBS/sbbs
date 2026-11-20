/* Internal fixture assertion shared by auth/channel tests. External algorithm
 * fixture modules do not include this header or any private library header. */
#ifndef DSSH_COMPRESSION_FIXTURE_H
#define DSSH_COMPRESSION_FIXTURE_H

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "ssh-internal.h"
#include "dssh_test_wait.h"
#include "test_comp.h"

static bool
compression_fixture_selected(dssh_session client, dssh_session server)
{
	const char *method = getenv("DSSH_TEST_COMPRESSION");
	if (method == NULL)
		return true;
	dssh_session peers[] = { client, server };
	for (size_t i = 0; i < sizeof(peers) / sizeof(peers[0]); i++) {
		dssh_session sess = peers[i];
		if (sess == NULL || sess->trans.comp_c2s_active == NULL || sess->trans.comp_s2c_active == NULL
		    || strcmp(sess->trans.comp_c2s_active->name, method) != 0
		    || strcmp(sess->trans.comp_s2c_active->name, method) != 0)
			return false;
		if (strcmp(method, "none") != 0
		    && (sess->trans.comp_tx_packet == NULL || sess->trans.comp_rx_input == NULL
		        || sess->trans.comp_c2s_ctx == NULL || sess->trans.comp_s2c_ctx == NULL
		        || sess->trans.comp_tx_payload_limit == 0))
			return false;
	}
	return true;
}

static inline unsigned
compression_fixture_reset_count(void)
{
	const char *method = getenv("DSSH_TEST_COMPRESSION");
	if (method != NULL && strcmp(method, "zlib") == 0)
		return dssh_test_zlib_resets();
	struct dssh_test_comp_stats stats;
	dssh_test_comp_stats(&stats);
	return stats.resets;
}

/* Both endpoints must complete their directional context resets. Merely
 * receiving echoed data before deferred rekey runs is insufficient evidence. */
static inline bool
compression_fixture_wait_reset(unsigned before)
{
	const char *method = getenv("DSSH_TEST_COMPRESSION");
	if (method == NULL || strcmp(method, "none") == 0)
		return true;
	if (before > UINT_MAX - 4)
		return false;
	struct timespec start;
	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
		return false;
	while (compression_fixture_reset_count() < before + 4) {
		if (!dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

#endif
