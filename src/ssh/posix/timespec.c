/* C11 timespec_get fallback for POSIX SDKs that lack its declaration or API. */
#include "../ssh-timespec.h"

DSSH_PRIVATE int
dssh_timespec_get(struct timespec *ts, int base)
{
	if (ts == NULL || base != TIME_UTC)
		return 0;
	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) != 0)
		return 0;
	*ts = now;
	return TIME_UTC;
}
