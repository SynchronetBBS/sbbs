/* Exercise the actual common runner with controlled native boundaries. */
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int controlled_clock(struct timespec *, int);
static void (*controlled_signal(int, void (*)(int)))(int);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
static int controlled_printf(const char *, ...);
static int controlled_flush(FILE *);
#define DSSH_TEST_RUNNER_CLOCK controlled_clock
#define DSSH_TEST_RUNNER_SIGNAL controlled_signal
#define DSSH_TEST_RUNNER_PRINTF controlled_printf
#define DSSH_TEST_RUNNER_FLUSH controlled_flush
#include "dssh_test.h"

DSSH_TEST_NO_CLEANUP

static const char *mode;
static unsigned leaves, clocks, signals, writes, flushes;
static bool native_valid = true;

static void
observe(unsigned *counter)
{
	if (*counter == UINT_MAX)
		abort();
	(*counter)++;
}

static int
controlled_clock(struct timespec *value, int base)
{
	observe(&clocks);
	if (strcmp(mode, "clock-first") == 0 && clocks == 1)
		return 0;
	int ret = timespec_get(value, base);
	native_valid = ret == TIME_UTC && native_valid;
	if (ret != TIME_UTC)
		return ret;
	if (strcmp(mode, "clock-last") == 0 && clocks == 2)
		return 0;
	if (strcmp(mode, "nano-negative") == 0)
		value->tv_nsec = -1;
	else if (strcmp(mode, "nano-over") == 0)
		value->tv_nsec = 1000000000L;
	else if (strcmp(mode, "reverse") == 0) {
		value->tv_sec = clocks == 1 ? 100 : 99;
		value->tv_nsec = 0;
	}
	else if (strcmp(mode, "fraction") == 0) {
		value->tv_sec = clocks == 1 ? 0 : 1;
		value->tv_nsec = clocks == 1 ? 900000000L : 100000000L;
	}
	return ret;
}

static void (*
controlled_signal(int number, void (*handler)(int)))(int)
{
	observe(&signals);
	if (strcmp(mode, "signal-first") == 0)
		return SIG_ERR;
	void (*previous)(int) = signal(number, handler);
	native_valid = previous != SIG_ERR && native_valid;
	if (strcmp(mode, "signal-last") == 0)
		return SIG_ERR;
	return previous;
}

static int
controlled_printf(const char *format, ...)
{
	observe(&writes);
	if ((strcmp(mode, "write-first") == 0 && writes == 1)
	    || (strcmp(mode, "write-last") == 0 && writes == 2))
		return -1;
	va_list arguments;
	va_start(arguments, format);
	int ret = vprintf(format, arguments);
	va_end(arguments);
	return ret;
}

static int
controlled_flush(FILE *stream)
{
	observe(&flushes);
	int ret = fflush(stream);
	if ((strcmp(mode, "flush-first") == 0 && flushes == 1)
	    || (strcmp(mode, "flush-last") == 0 && flushes == 2)) {
		native_valid = ret == 0 && native_valid;
		return EOF;
	}
	return ret;
}

static int
pass(void)
{
	observe(&leaves);
	return TEST_PASS;
}

static int
skip(void)
{
	observe(&leaves);
	return TEST_SKIP;
}

static int
fail(void)
{
	observe(&leaves);
	return TEST_FAIL;
}

static int
invalid_positive(void)
{
	observe(&leaves);
	return 2;
}

static int
invalid_negative(void)
{
	observe(&leaves);
	return -2;
}

static int
elapsed(void)
{
	double value = -1;
	bool valid = dssh_test_runner_elapsed(1, 900000000L, 100000000L, &value) && value == 200;
	valid = dssh_test_runner_elapsed(0, 0, 99999999L, &value) && value < 100 && valid;
	static const double rejected[] = {-1, INFINITY, -INFINITY, NAN, DBL_MAX};
	for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++) {
		value = -1;
		valid = !dssh_test_runner_elapsed(rejected[i], 0, 0, &value) && value == -1 && valid;
	}
	valid = !dssh_test_runner_elapsed(0, 1, 0, &value) && valid;
	valid = !dssh_test_runner_elapsed(0, -1, 0, &value) && valid;
	valid = !dssh_test_runner_elapsed(0, 0, 1000000000L, &value) && valid;
	return valid ? 0 : 3;
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		return 3;
	mode = argv[1];
	if (strcmp(mode, "elapsed") == 0) {
		int ret = elapsed();
		if (printf("runner_elapsed cases=10 valid=%u\n", ret == 0 ? 1U : 0U) < 0
		    || fflush(stdout) != 0)
			return 3;
		return ret;
	}
	struct dssh_test_entry table[] = {{"alpha", pass}, {"beta", pass}, {"skip", skip}};
	size_t count = 1;
	char *arguments[] = {argv[0], "--exact", "alpha", NULL};
	int argument_count = 3;
	if (strcmp(mode, "all") == 0) {
		count = 3;
		argument_count = 1;
	}
	else if (strcmp(mode, "substring") == 0) {
		count = 3;
		argument_count = 2;
		arguments[1] = "a";
	}
	else if (strcmp(mode, "none") == 0)
		arguments[2] = "absent";
	else if (strcmp(mode, "missing-exact") == 0)
		argument_count = 2;
	else if (strcmp(mode, "extra") == 0)
		argument_count = 4;
	else if (strcmp(mode, "empty") == 0)
		arguments[2] = "";
	else if (strcmp(mode, "bad-option") == 0)
		arguments[1] = "other";
	else if (strcmp(mode, "duplicate") == 0) {
		count = 2;
		table[1].name = "alpha";
	}
	else if (strcmp(mode, "null-name") == 0)
		table[0].name = NULL;
	else if (strcmp(mode, "null-function") == 0)
		table[0].fn = NULL;
	else if (strcmp(mode, "skip") == 0)
		table[0].fn = skip;
	else if (strcmp(mode, "fail") == 0)
		table[0].fn = fail;
	else if (strcmp(mode, "positive") == 0)
		table[0].fn = invalid_positive;
	else if (strcmp(mode, "negative") == 0)
		table[0].fn = invalid_negative;
	int ret = dssh_test_run(table, count, argument_count, arguments);
	if (fprintf(stderr, "runner_observed mode=%s result=%d leaves=%u clocks=%u signals=%u writes=%u flushes=%u native=%u\n",
	    mode, ret, leaves, clocks, signals, writes, flushes, native_valid ? 1U : 0U) < 0
	    || fflush(stderr) != 0)
		return 3;
	return ret;
}
