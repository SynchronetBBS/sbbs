/* Same assertion contracts in C17 and the private C++ test inclusion context. */
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "dssh_test.h"
#ifdef __cplusplus
}
#endif

static unsigned actual_calls, expected_calls, length_calls, memory_calls, string_calls;
static unsigned cleanups;
static int *borrowed;
static bool cleaned_live;
static const char *mode;

static void
count(unsigned *value)
{
	if (*value == UINT_MAX)
		abort();
	(*value)++;
}

void
dssh_test_failure_cleanup(void)
{
	count(&cleanups);
	if (borrowed != NULL) {
		cleaned_live = *borrowed == 42;
		*borrowed = 0;
		borrowed = NULL;
	}
}

static intmax_t
actual_negative(void)
{
	count(&actual_calls);
	return strcmp(mode, "ok-negative") == 0 ? INTMAX_MIN : -1;
}

static uintmax_t
actual_unsigned(void)
{
	count(&actual_calls);
	return UINTMAX_MAX;
}

static uintmax_t
expected_unsigned(void)
{
	count(&expected_calls);
	return UINTMAX_MAX;
}

static intmax_t
expected_negative(void)
{
	count(&expected_calls);
	return strcmp(mode, "unsigned-negative") == 0 ? -1 : -2;
}

static const void *
actual_memory(void)
{
	static const unsigned char bytes[] = {1, 2};
	count(&actual_calls);
	return strcmp(mode, "memory-null") == 0 || strcmp(mode, "memory-zero") == 0 ? NULL : bytes;
}

static const void *
expected_memory(void)
{
	static const unsigned char bytes[] = {1, 3};
	count(&expected_calls);
	return strcmp(mode, "memory-zero") == 0 ? NULL : bytes;
}

static intmax_t
memory_length(void)
{
	count(&length_calls);
	if (strcmp(mode, "memory-negative") == 0)
		return -1;
	return strcmp(mode, "memory-zero") == 0 ? 0 : 2;
}

static const char *
actual_string(void)
{
	count(&actual_calls);
	return strstr(mode, "null") != NULL ? NULL : "first";
}

static const char *
expected_string(void)
{
	count(&expected_calls);
	return strcmp(mode, "string-both-null") == 0 ? NULL
	    : strcmp(mode, "string-equal") == 0 ? "first" : "last";
}

static int
observed_memcmp(const void *first, const void *last, size_t size)
{
	count(&memory_calls);
	return memcmp(first, last, size);
}

static int
observed_strcmp(const char *first, const char *last)
{
	count(&string_calls);
	return strcmp(first, last);
}

/* Only the actual assertion calls are redirected, not fixture selection or
 * diagnostics. Invalid/empty spans must not reach either native comparator. */
#define memcmp observed_memcmp
#define strcmp observed_strcmp
static int
assertion_model(void)
{
	int local = 42;
	borrowed = &local;
	if (mode[0] == 'e' && mode[1] == 'q') {
		if (mode[3] == 'm')
			ASSERT_EQ(actual_negative(), expected_unsigned());
		else
			ASSERT_EQ(actual_unsigned(), expected_unsigned());
	}
	else if (mode[0] == 'n')
		ASSERT_NE(actual_unsigned(), expected_unsigned());
	else if (mode[0] == 'u')
		ASSERT_EQ_U(actual_negative(), expected_negative());
	else if (mode[0] == 'o') {
		if (mode[3] == 'n')
			ASSERT_OK(actual_negative());
		else
			ASSERT_OK(actual_unsigned());
	}
	else if (mode[0] == 'e')
		ASSERT_ERR(actual_negative(), expected_negative());
	else if (mode[0] == 'p')
		ASSERT_ERR(0, 0);
	else if (mode[0] == 'm')
		ASSERT_MEM_EQ(actual_memory(), expected_memory(), memory_length());
	else
		ASSERT_STR_EQ(actual_string(), expected_string());
	/* Never retain the successful path's stack borrow past this return. */
	borrowed = NULL;
	return TEST_PASS;
}
#undef memcmp
#undef strcmp

static int
values(void)
{
	enum value_id {zero_id, one_id, negative_id, min_id, max_id, unsigned_id};
	enum small_enum {enum_zero = 0, enum_one = 1, enum_negative = -1};
	struct answer {struct dssh_test_integer value; enum value_id id;};
	bool yes = true, no = false;
	char c = 1;
	signed char sc = 1;
	unsigned char uc = 1;
	short sh = 1;
	unsigned short ush = 1;
	int in = 1;
	unsigned int uin = 1;
	long lo = 1;
	unsigned long ulo = 1;
	long long ll = 1;
	unsigned long long ull = 1;
	struct answer answers[] = {
		{DSSH_TEST_NUMBER(no), zero_id}, {DSSH_TEST_NUMBER(0), zero_id},
		{DSSH_TEST_NUMBER((uintmax_t)0), zero_id}, {DSSH_TEST_NUMBER(enum_zero), zero_id},
		{DSSH_TEST_NUMBER(yes), one_id}, {DSSH_TEST_NUMBER(c), one_id},
		{DSSH_TEST_NUMBER(sc), one_id}, {DSSH_TEST_NUMBER(uc), one_id},
		{DSSH_TEST_NUMBER(sh), one_id}, {DSSH_TEST_NUMBER(ush), one_id},
		{DSSH_TEST_NUMBER(in), one_id}, {DSSH_TEST_NUMBER(uin), one_id},
		{DSSH_TEST_NUMBER(lo), one_id}, {DSSH_TEST_NUMBER(ulo), one_id},
		{DSSH_TEST_NUMBER(ll), one_id}, {DSSH_TEST_NUMBER(ull), one_id},
		{DSSH_TEST_NUMBER(enum_one), one_id}, {DSSH_TEST_NUMBER(-1), negative_id},
		{DSSH_TEST_NUMBER((intmax_t)-1), negative_id}, {DSSH_TEST_NUMBER(enum_negative), negative_id},
		{DSSH_TEST_NUMBER(INTMAX_MIN), min_id}, {DSSH_TEST_NUMBER(INTMAX_MAX), max_id},
		{DSSH_TEST_NUMBER((uintmax_t)INTMAX_MAX), max_id},
		{DSSH_TEST_NUMBER(UINTMAX_MAX), unsigned_id}
	};
	bool valid = true;
	size_t count_answers = sizeof(answers) / sizeof(answers[0]);
	unsigned pairs = 0;
	for (size_t i = 0; i < count_answers; i++) {
		for (size_t j = 0; j < count_answers; j++) {
			bool equal = answers[i].id == answers[j].id;
			valid = dssh_test_integer_equal(answers[i].value, answers[j].value) == equal && valid;
			count(&pairs);
		}
	}
	static const uintmax_t limits[] = {UINT8_MAX, UINT16_MAX, UINT32_MAX, UINT64_MAX};
	for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); i++) {
		valid = dssh_test_integer_within(DSSH_TEST_NUMBER(limits[i]), limits[i])
		    && !dssh_test_integer_within(DSSH_TEST_NUMBER(-1), limits[i]) && valid;
		if (limits[i] < UINTMAX_MAX) {
			uintmax_t excess = limits[i] + 1;
			valid = !dssh_test_integer_within(DSSH_TEST_NUMBER(excess), limits[i]) && valid;
		}
	}
	size_t output = 7;
	valid = !dssh_test_integer_size(DSSH_TEST_NUMBER(-1), &output) && output == 7 && valid;
	valid = dssh_test_integer_size(DSSH_TEST_NUMBER(SIZE_MAX), &output) && output == SIZE_MAX && valid;
	if (UINTMAX_MAX > SIZE_MAX) {
		output = 7;
		valid = !dssh_test_integer_size(DSSH_TEST_NUMBER(UINTMAX_MAX), &output) && output == 7 && valid;
	}
	if (printf("assertion_values cases=%zu pairs=%u bounds=4 size_guard=1 valid=%u\n", count_answers, pairs, valid ? 1U : 0U) < 0
	    || fflush(stdout) != 0)
		return 1;
	return valid ? 0 : 1;
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		return 3;
	mode = argv[1];
	if (strcmp(mode, "values") == 0)
		return values();
	if (printf("assertion_limits signed_min=%" PRIdMAX " signed_max=%" PRIdMAX " unsigned_max=%" PRIuMAX "\n",
	    INTMAX_MIN, INTMAX_MAX, UINTMAX_MAX) < 0 || fflush(stdout) != 0)
		return 3;
	int result = assertion_model();
	if (printf("assertion_observed mode=%s result=%d actual=%u expected=%u length=%u memory=%u string=%u cleanups=%u live=%u retired=%u\n",
	    mode, result, actual_calls, expected_calls, length_calls, memory_calls, string_calls,
	    cleanups, cleaned_live ? 1U : 0U, borrowed == NULL ? 1U : 0U) < 0 || fflush(stdout) != 0)
		return 3;
	return 0; /* The parent judges these observations independently. */
}
