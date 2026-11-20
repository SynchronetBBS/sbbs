/* Private observation of completed exchanges on both still-owned peers. */
#ifndef SELFTEST_REKEY_H
#define SELFTEST_REKEY_H
#include <limits.h>
#include "dssh_test_internal.h"
#include "dssh_test_wait.h"

static bool
selftest_rekey_advanced(unsigned client, unsigned server, const unsigned before[2])
{
	return before[0] < UINT_MAX && before[1] < UINT_MAX
	    && client > before[0] && server > before[1];
}

static bool
selftest_rekey_snapshot(dssh_session client, dssh_session server, unsigned before[2])
{
	unsigned c = dssh_test_rekeys_completed(client);
	unsigned s = dssh_test_rekeys_completed(server);

	if (c == UINT_MAX || s == UINT_MAX)
		return false;
	before[0] = c;
	before[1] = s;
	return true;
}

static bool
selftest_rekey_wait(dssh_session client, dssh_session server, const unsigned before[2])
{
	struct timespec start;

	if (before[0] == UINT_MAX || before[1] == UINT_MAX
	    || clock_gettime(CLOCK_MONOTONIC, &start) != 0
	    || start.tv_nsec < 0 || start.tv_nsec >= 1000000000L)
		return false;
	for (;;) {
		if (dssh_session_is_terminated(client) || dssh_session_is_terminated(server))
			return false;
		if (selftest_rekey_advanced(dssh_test_rekeys_completed(client),
		    dssh_test_rekeys_completed(server), before))
			return true;
		if (!dssh_test_wait_tick(&start))
			return false;
	}
}
#endif
