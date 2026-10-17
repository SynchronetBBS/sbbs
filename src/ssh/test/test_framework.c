/* Assertion cleanup must run while the failed test's stack is still live. */
#include "dssh_test.h"

static int *active_local;
static bool in_scope;
static bool cleaned_in_scope;

void
dssh_test_failure_cleanup(void)
{
	if (active_local != NULL && in_scope) {
		cleaned_in_scope = *active_local == 42;
		*active_local = 0;
		active_local = NULL;
	}
}

static int
fail_with_live_context(void)
{
	int local = 42;

	active_local = &local;
	in_scope = true;
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
test_assertion_cleanup(void)
{
	int result = fail_with_live_context();

	in_scope = false;
	ASSERT_EQ(result, TEST_FAIL);
	ASSERT_NULL(active_local);
	ASSERT_TRUE(cleaned_in_scope);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "assertion_cleanup", test_assertion_cleanup },
};

DSSH_TEST_MAIN(tests)
