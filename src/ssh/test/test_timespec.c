/* Compile the exact private fallback with an observed native-clock boundary.
 * This tests the fallback even when the host already provides timespec_get. */
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

static unsigned clock_calls;
static unsigned clock_mode;
static bool realtime_only = true;

static int
observed_clock_gettime(clockid_t clock, struct timespec *now)
{
	clock_calls++;
	realtime_only &= clock == CLOCK_REALTIME;
	if (clock_mode == 1)
		return -1;
	if (clock_mode == 2) {
		*now = (struct timespec){ .tv_sec = 7, .tv_nsec = 8 };
		return -1;
	}
	if (clock_mode == 3) {
		*now = (struct timespec){ .tv_sec = 123, .tv_nsec = 456 };
		return 0;
	}
	return clock_gettime(clock, now);
}

#define clock_gettime observed_clock_gettime
#include "../posix/timespec.c"
#undef clock_gettime

static bool
unchanged(const struct timespec *value)
{
	return value->tv_sec == 11 && value->tv_nsec == 12;
}

int
main(void)
{
	struct timespec value = { .tv_sec = 11, .tv_nsec = 12 };
	bool valid = dssh_timespec_get(&value, 0) == 0;
	valid &= unchanged(&value) && clock_calls == 0;
	valid &= dssh_timespec_get(NULL, TIME_UTC) == 0;
	valid &= clock_calls == 0;
	clock_mode = 1;
	valid &= dssh_timespec_get(&value, TIME_UTC) == 0;
	valid &= unchanged(&value) && clock_calls == 1;
	clock_mode = 2;
	valid &= dssh_timespec_get(&value, TIME_UTC) == 0;
	valid &= unchanged(&value) && clock_calls == 2;
	clock_mode = 3;
	valid &= dssh_timespec_get(&value, TIME_UTC) == TIME_UTC;
	valid &= value.tv_sec == 123 && value.tv_nsec == 456 && clock_calls == 3;
	clock_mode = 0;
	valid &= dssh_timespec_get(&value, TIME_UTC) == TIME_UTC;
	valid &= value.tv_nsec >= 0 && value.tv_nsec < 1000000000L
	    && clock_calls == 4 && realtime_only;
	printf("timespec_shim cases=6 calls=%u valid=%d\n", clock_calls, valid);
	return valid ? 0 : 1;
}
