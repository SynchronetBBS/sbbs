/*
 * dssh_test.h -- Minimal test framework for DeuceSSH.
 *
 * Each test is a function returning int: 1=PASS, 0=FAIL, -1=SKIP.
 * Define the test table and call DSSH_TEST_MAIN() from main().
 */

#ifndef DSSH_TEST_H
#define DSSH_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define TEST_PASS  1
#define TEST_FAIL  0
#define TEST_SKIP -1

typedef int (*dssh_test_fn)(void);

struct dssh_test_entry {
	const char *name;
	dssh_test_fn fn;
};

#define ASSERT_TRUE(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "  ASSERT_TRUE failed: %s\n    at %s:%d\n", \
		    #cond, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_FALSE(cond) do { \
	if ((cond)) { \
		fprintf(stderr, "  ASSERT_FALSE failed: %s\n    at %s:%d\n", \
		    #cond, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_EQ(a, b) do { \
	long long _a = (long long)(a); \
	long long _b = (long long)(b); \
	if (_a != _b) { \
		fprintf(stderr, "  ASSERT_EQ failed: %s == %lld, expected %s == %lld\n    at %s:%d\n", \
		    #a, _a, #b, _b, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_NE(a, b) do { \
	long long _a = (long long)(a); \
	long long _b = (long long)(b); \
	if (_a == _b) { \
		fprintf(stderr, "  ASSERT_NE failed: %s == %s == %lld\n    at %s:%d\n", \
		    #a, #b, _a, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

/* For unsigned comparisons (size_t, uint32_t, etc.) */
#define ASSERT_EQ_U(a, b) do { \
	unsigned long long _a = (unsigned long long)(a); \
	unsigned long long _b = (unsigned long long)(b); \
	if (_a != _b) { \
		fprintf(stderr, "  ASSERT_EQ_U failed: %s == %llu, expected %s == %llu\n    at %s:%d\n", \
		    #a, _a, #b, _b, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_OK(expr) do { \
	int64_t _r = (int64_t)(expr); \
	if (_r < 0) { \
		fprintf(stderr, "  ASSERT_OK failed: %s returned %" PRId64 "\n    at %s:%d\n", \
		    #expr, _r, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_ERR(expr, code) do { \
	int64_t _r = (int64_t)(expr); \
	if (_r != (int64_t)(code)) { \
		fprintf(stderr, "  ASSERT_ERR failed: %s returned %" PRId64 ", expected %d (%s)\n    at %s:%d\n", \
		    #expr, _r, (int)(code), #code, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_NULL(ptr) do { \
	if ((ptr) != NULL) { \
		fprintf(stderr, "  ASSERT_NULL failed: %s is not NULL\n    at %s:%d\n", \
		    #ptr, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_NOT_NULL(ptr) do { \
	if ((ptr) == NULL) { \
		fprintf(stderr, "  ASSERT_NOT_NULL failed: %s is NULL\n    at %s:%d\n", \
		    #ptr, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_MEM_EQ(a, b, n) do { \
	if (memcmp((a), (b), (n)) != 0) { \
		fprintf(stderr, "  ASSERT_MEM_EQ failed: %s != %s (len %zu)\n    at %s:%d\n", \
		    #a, #b, (size_t)(n), __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#define ASSERT_STR_EQ(a, b) do { \
	if (strcmp((a), (b)) != 0) { \
		fprintf(stderr, "  ASSERT_STR_EQ failed: \"%s\" != \"%s\"\n    at %s:%d\n", \
		    (a), (b), __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

#include <threads.h>
#define ASSERT_THRD_CREATE(thr, func, arg) do { \
	if (thrd_create((thr), (func), (arg)) != thrd_success) { \
		fprintf(stderr, "  thrd_create failed: %s\n    at %s:%d\n", \
		    #func, __FILE__, __LINE__); \
		return TEST_FAIL; \
	} \
} while (0)

/*
 * Main loop macro.  Define a tests[] array of dssh_test_entry before
 * calling this.  Usage:
 *
 *   static struct dssh_test_entry tests[] = {
 *       { "test_foo", test_foo },
 *       { "test_bar", test_bar },
 *   };
 *   DSSH_TEST_MAIN(tests)
 */
/*
 * Optional per-test cleanup hook.  If the test file defines
 * dssh_test_after_each(int result) before DSSH_TEST_MAIN, it is
 * called after every test — passing the result (PASS/FAIL/SKIP).
 * Use it to clean up leaked threads/sessions when an ASSERT bails
 * out of a test without running the normal cleanup path.
 * Files that don't need it define a no-op via DSSH_TEST_NO_CLEANUP.
 */
#define DSSH_TEST_NO_CLEANUP \
	static void dssh_test_after_each(int r) { (void)r; }

#include <signal.h>
#include <time.h>
#define DSSH_TEST_MAIN(test_table) \
int main(int argc, char *argv[]) \
{ \
	signal(SIGPIPE, SIG_IGN); \
	const char *filter = (argc > 1) ? argv[1] : NULL; \
	int total = 0, passed = 0, failed = 0, skipped = 0; \
	size_t count = sizeof(test_table) / sizeof(test_table[0]); \
	for (size_t i = 0; i < count; i++) { \
		if (filter && !strstr(test_table[i].name, filter)) \
			continue; \
		total++; \
		printf("%-60s ", test_table[i].name); \
		fflush(stdout); \
		struct timespec _t0, _t1; \
		clock_gettime(CLOCK_MONOTONIC, &_t0); \
		int result = test_table[i].fn(); \
		clock_gettime(CLOCK_MONOTONIC, &_t1); \
		dssh_test_after_each(result); \
		long _ms = (_t1.tv_sec - _t0.tv_sec) * 1000L \
		    + (_t1.tv_nsec - _t0.tv_nsec) / 1000000L; \
		const char *_label = result > 0 ? "PASS" \
		    : result == 0 ? "FAIL" : "SKIP"; \
		if (_ms >= 100) \
			printf("%s (%ldms)\n", _label, _ms); \
		else \
			printf("%s\n", _label); \
		if (result > 0) passed++; \
		else if (result == 0) failed++; \
		else skipped++; \
	} \
	printf("\n%d tests: %d passed, %d failed, %d skipped\n", \
	    total, passed, failed, skipped); \
	return failed > 0 ? 1 : 0; \
}

#endif
