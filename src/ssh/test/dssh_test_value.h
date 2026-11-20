/* Exact private assertion values. No narrowing through signed long long. */
#ifndef DSSH_TEST_VALUE_H
#define DSSH_TEST_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdio.h>

struct dssh_test_integer {
	bool negative;
	intmax_t signed_value;
	uintmax_t unsigned_value;
};

static inline struct dssh_test_integer
dssh_test_integer_signed(intmax_t value)
{
	struct dssh_test_integer result = {value < 0, value, 0};
	if (value >= 0) {
		uintmax_t positive = (uintmax_t)value;
		result.unsigned_value = positive;
	}
	return result;
}

static inline struct dssh_test_integer
dssh_test_integer_unsigned(uintmax_t value)
{
	struct dssh_test_integer result = {false, 0, value};
	return result;
}

static inline bool
dssh_test_integer_equal(struct dssh_test_integer first, struct dssh_test_integer last)
{
	if (first.negative != last.negative)
		return false;
	return first.negative ? first.signed_value == last.signed_value
	    : first.unsigned_value == last.unsigned_value;
}

static inline bool
dssh_test_integer_within(struct dssh_test_integer value, uintmax_t maximum)
{
	return !value.negative && value.unsigned_value <= maximum;
}

static inline bool
dssh_test_integer_size(struct dssh_test_integer value, size_t *out)
{
	if (!dssh_test_integer_within(value, SIZE_MAX))
		return false;
	size_t safe = (size_t)value.unsigned_value;
	*out = safe;
	return true;
}

#ifdef __cplusplus
extern "C++" {
#include <type_traits>

template <typename T>
static inline struct dssh_test_integer
dssh_test_number(T value)
{
	static_assert(std::is_integral_v<T> || std::is_enum_v<T>, "integer assertion required");
	if constexpr (std::is_enum_v<T>) {
		using underlying_type = std::underlying_type_t<T>;
		underlying_type underlying = static_cast<underlying_type>(value);
		return dssh_test_number(underlying);
	}
	else if constexpr (std::is_signed_v<T>)
		return dssh_test_integer_signed(value);
	else
		return dssh_test_integer_unsigned(value);
}
}
#define DSSH_TEST_NUMBER(value) dssh_test_number(value)
#else
/* Controlling expressions are not evaluated. Promotions retain every standard
 * integer value; a selected unsupported-type function rejects floats/pointers. */
static inline struct dssh_test_integer
dssh_test_integer_unsupported(void)
{
	struct dssh_test_integer result = {false, 0, 0};
	return result;
}
#define DSSH_TEST_NUMBER(value) \
	_Generic(+(value), \
	    int: dssh_test_integer_signed, unsigned int: dssh_test_integer_unsigned, \
	    long: dssh_test_integer_signed, unsigned long: dssh_test_integer_unsigned, \
	    long long: dssh_test_integer_signed, unsigned long long: dssh_test_integer_unsigned, \
	    default: _Generic(+(value), intmax_t: dssh_test_integer_signed, \
	        uintmax_t: dssh_test_integer_unsigned, default: dssh_test_integer_unsupported))((value))
#endif

static inline bool
dssh_test_integer_print(struct dssh_test_integer value)
{
	return value.negative ? fprintf(stderr, "%" PRIdMAX, value.signed_value) >= 0
	    : fprintf(stderr, "%" PRIuMAX, value.unsigned_value) >= 0;
}

/* Diagnostics are best effort after an assertion has already failed. The
 * caller always invokes its live-frame cleanup and returns TEST_FAIL. */
static inline void
dssh_test_report_integer(const char *name, const char *first, const char *last,
    struct dssh_test_integer actual, struct dssh_test_integer expected,
    const char *file, int line)
{
	if (fprintf(stderr, "  %s failed: %s == ", name, first) < 0
	    || !dssh_test_integer_print(actual)
	    || fprintf(stderr, ", expected %s == ", last) < 0
	    || !dssh_test_integer_print(expected)
	    || fprintf(stderr, "\n    at %s:%d\n", file, line) < 0 || fflush(stderr) != 0)
		return;
}

static inline void
dssh_test_report_memory(const char *first, const char *last,
    struct dssh_test_integer length, const char *file, int line)
{
	if (fprintf(stderr, "  ASSERT_MEM_EQ failed: %s != %s (len ", first, last) < 0
	    || !dssh_test_integer_print(length)
	    || fprintf(stderr, ")\n    at %s:%d\n", file, line) < 0 || fflush(stderr) != 0)
		return;
}

static inline void
dssh_test_report_string(const char *first, const char *last, const char *file, int line)
{
	const char *shown_first = first == NULL ? "(null)" : first;
	const char *shown_last = last == NULL ? "(null)" : last;
	if (fprintf(stderr, "  ASSERT_STR_EQ failed: \"%s\" != \"%s\"\n    at %s:%d\n",
	    shown_first, shown_last, file, line) < 0 || fflush(stderr) != 0)
		return;
}

#endif
