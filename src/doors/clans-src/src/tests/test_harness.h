/*
 * Minimal unit-test harness shared by all test_*.c files.
 *
 * Each test_*.c file is a single translation unit that #includes the C file
 * under test directly ("include the C file" pattern).  Defining these as
 * static means every translation unit gets its own copy -- which is correct
 * because each test binary IS one translation unit.
 */

#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <setjmp.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */
static int g_tests_run;
static int g_tests_failed;

/*
 * Optional per-test setup and teardown hooks.  Assign non-NULL function
 * pointers in main() before the first RUN() call.
 */
static void (*g_test_setup)(void)    = NULL;
static void (*g_test_teardown)(void) = NULL;

/* -------------------------------------------------------------------------
 * ASSERT_EQ  --  compare two integer-typed values
 * ------------------------------------------------------------------------- */
#define ASSERT_EQ(a, b)                                                 \
	do {                                                            \
		long long _a_ = (long long)(a);                         \
		long long _b_ = (long long)(b);                         \
		if (_a_ != _b_) {                                       \
			fprintf(stderr, "  FAIL %s:%d: %lld != %lld\n",\
			        __FILE__, __LINE__, _a_, _b_);          \
			g_tests_failed++;                               \
			return;                                         \
		}                                                       \
	} while (0)

/* -------------------------------------------------------------------------
 * ASSERT_FATAL  --  verify that expr triggers a fatal longjmp
 *
 * Requires the test file to declare:
 *   static jmp_buf g_fatal_jmp;
 * and configure a mock that calls longjmp(g_fatal_jmp, 1) instead of
 * terminating (e.g. by redefining exit() or System_Error()).
 * The jmp_buf is re-armed by each ASSERT_FATAL invocation.
 * ------------------------------------------------------------------------- */
#define ASSERT_FATAL(expr)                                              \
	do {                                                            \
		if (setjmp(g_fatal_jmp) == 0) {                         \
			(void)(expr);                                   \
			fprintf(stderr, "  FAIL %s:%d: "               \
			        "fatal call not made\n",                \
			        __FILE__, __LINE__);                    \
			g_tests_failed++;                               \
			return;                                         \
		}                                                       \
	} while (0)

/* -------------------------------------------------------------------------
 * RUN  --  run a named test function and report PASS/FAIL
 *
 * Calls g_test_setup() before and g_test_teardown() after the test
 * function if those pointers are non-NULL.
 * ------------------------------------------------------------------------- */
#define RUN(name)                                                       \
	do {                                                            \
		int _before_ = g_tests_failed;                          \
		g_tests_run++;                                           \
		if (g_test_setup)    g_test_setup();                    \
		test_##name();                                           \
		if (g_test_teardown) g_test_teardown();                 \
		printf("%s  %s\n",                                      \
		       (g_tests_failed == _before_) ? "PASS" : "FAIL", \
		       #name);                                          \
	} while (0)

#endif /* TEST_HARNESS_H */
