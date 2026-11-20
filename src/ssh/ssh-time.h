/* Internal, standard C17 arithmetic shared by deadline users and tests. */
#ifndef DSSH_TIME_H
#define DSSH_TIME_H

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Internal durations include the documented default even when int is 16-bit.
 * intmax_t also preserves every value accepted by the public int setters. */
#define DSSH_DEFAULT_TIMEOUT_MS INTMAX_C(75000)

/* Compare a whole-second interval without overflowing subtraction, including
 * transitions across zero and timestamps near either representation bound. */
static inline bool
dssh_time_interval_reached(time_t now, time_t start, uint32_t seconds)
{
	long double current = (long double)now;
	long double previous = (long double)start;
	if (!isfinite(current) || !isfinite(previous) || now < start)
		return false;
	if (_Generic((time_t)0, float: true, double: true, long double: true, default: false)) {
		long double interval = seconds;
		if (previous < 0 && current >= 0) {
			/* Only subtract across zero if both magnitudes are small;
			 * otherwise either one already establishes the interval. */
			if (current >= interval || -previous >= interval)
				return true;
		}
		return current - previous >= interval;
	}
	if (previous >= 0 || current < 0) {
		/* Both values have the same sign, so this nonnegative difference
		 * fits the original type, even when its minimum is asymmetric. */
		time_t difference = (time_t)(now - start);
		uintmax_t elapsed = (uintmax_t)difference;
		return elapsed >= seconds;
	}
	uintmax_t positive = (uintmax_t)now;
	if (positive >= seconds)
		return true;
	uintmax_t negative_less_one = (uintmax_t)(-(start + 1));
	/* seconds > positive, so the final subtraction cannot underflow. */
	return negative_less_one >= seconds - positive - 1;
}

/* Convert a nonnegative relative duration to a bounded whole-millisecond
 * wait. Normalize the fractional borrow before applying the limit. */
static inline bool
dssh_relative_wait_ms(uint64_t seconds, long nanos, uint32_t limit, uint32_t *out)
{
	if (nanos < -999999999L || nanos > 999999999L)
		return false;
	if (nanos < 0) {
		if (seconds == 0)
			return false;
		seconds--;
		nanos += 1000000000L;
	}
	if (seconds > limit / 1000) {
		*out = limit;
		return true;
	}
	/* The comparison bounds whole by limit before narrowing. Both the
	 * fractional numerator and its rounded result fit even 32-bit long. */
	uint32_t whole = (uint32_t)(seconds * 1000);
	uint32_t fraction = (uint32_t)((nanos + 999999L) / 1000000L);
	*out = fraction > limit - whole ? limit : whole + fraction;
	return true;
}

/* Add a positive millisecond interval without assuming time_t's signedness,
 * width or representation. Publish the result only after all checks pass. */
static inline bool
dssh_timespec_add_ms(struct timespec *ts, intmax_t timeout_ms)
{
	if (timeout_ms <= 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000L)
		return false;
	long double base = (long double)ts->tv_sec;
	if (!isfinite(base))
		return false;
	uintmax_t seconds = (uintmax_t)(timeout_ms / 1000);
	/* Each summand is below 10^9, so this sum fits even a 32-bit long.
	 * seconds + 1 fits uintmax_t: seconds <= INTMAX_MAX / 1000.
	 * The positive remainder is below 1000 before narrowing to long. */
	long milliseconds = (long)(timeout_ms % 1000);
	long nanos = ts->tv_nsec + milliseconds * 1000000L;
	if (nanos >= 1000000000L) {
		nanos -= 1000000000L;
		seconds++;
	}
	time_t result;
	if (_Generic((time_t)0, float: true, double: true, long double: true, default: false)) {
		const time_t maximum = _Generic((time_t)0,
		    float: FLT_MAX, double: DBL_MAX, long double: LDBL_MAX, default: 0);
		/* Supported integer widths are at most 64 bits; the interval is
		 * <= INTMAX_MAX / 1000 + 1, well within every standard floating type.
		 * The reverse conversion also fits uintmax_t, including rounding. */
		time_t increment = (time_t)seconds;
		uintmax_t integral = (uintmax_t)increment;
		if (integral != seconds)
			return false;
		if (base > 0 && increment > maximum - ts->tv_sec)
			return false;
		time_t sum = (time_t)(ts->tv_sec + increment);
		if (sum - ts->tv_sec != increment)
			return false;
		result = sum;
	}
	else {
		const uintmax_t maximum = _Generic((time_t)0,
		    _Bool: 1, char: CHAR_MAX,
		    signed char: SCHAR_MAX, unsigned char: UCHAR_MAX,
		    short: SHRT_MAX, unsigned short: USHRT_MAX,
		    int: INT_MAX, unsigned int: UINT_MAX,
		    long: LONG_MAX, unsigned long: ULONG_MAX,
		    long long: LLONG_MAX, unsigned long long: ULLONG_MAX,
		    default: 0);
		if (maximum == 0)
			return false;
		if (base < 0) {
			/* Do not negate the most negative value. This also works
			 * for sign-magnitude and ones' complement representations. */
			uintmax_t magnitude_less_one = (uintmax_t)(-(ts->tv_sec + 1));
			if (magnitude_less_one == UINTMAX_MAX)
				return false;
			uintmax_t magnitude = magnitude_less_one + 1;
			if (seconds < magnitude) {
				uintmax_t remaining_less_one = magnitude - seconds - 1;
				if (remaining_less_one > maximum)
					return false;
				time_t remaining = (time_t)remaining_less_one;
				/* Its magnitude cannot exceed that of the original,
				 * representable negative value. */
				time_t negative = (time_t)(-remaining - 1);
				result = negative;
			}
			else {
				seconds -= magnitude;
				if (seconds > maximum)
					return false;
				time_t positive = (time_t)seconds;
				result = positive;
			}
		}
		else {
			uintmax_t original = (uintmax_t)ts->tv_sec;
			if (original > maximum || seconds > maximum - original)
				return false;
			uintmax_t total = original + seconds;
			time_t positive = (time_t)total;
			result = positive;
		}
	}
	ts->tv_sec = result;
	ts->tv_nsec = nanos;
	return true;
}

#endif
