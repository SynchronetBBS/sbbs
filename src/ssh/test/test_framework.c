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
unused_worker(void *argument)
{
	(void)argument;
	return 0;
}

static unsigned create_refusals;

static int
assertion_create_refusal(thrd_t *thread, thrd_start_t start, void *argument)
{
	if (thread == NULL || start != unused_worker || argument != NULL)
		abort();
	create_refusals++;
	return thrd_error;
}

/* The macro must clean up on a reported creation error; no native worker or
 * handle is created by this local return-contract control. */
#define thrd_create assertion_create_refusal
static int
fail_with_live_context(unsigned kind)
{
	int local = 42;
	thrd_t thread;
	static const unsigned char first[] = {1, 2};
	static const unsigned char last[] = {1, 3};

	active_local = &local;
	in_scope = true;
	cleaned_in_scope = false;
	switch (kind) {
		case 0:
			ASSERT_TRUE(false);
			break;
		case 1:
			ASSERT_FALSE(true);
			break;
		case 2:
			ASSERT_EQ(1, 2);
			break;
		case 3:
			ASSERT_NE(1, 1);
			break;
		case 4:
			ASSERT_EQ_U(1U, 2U);
			break;
		case 5:
			ASSERT_OK(DSSH_ERROR_INVALID);
			break;
		case 6:
			ASSERT_ERR(0, DSSH_ERROR_INVALID);
			break;
		case 7:
			ASSERT_NULL(&local);
			break;
		case 8:
			ASSERT_NOT_NULL(NULL);
			break;
		case 9:
			ASSERT_MEM_EQ(first, last, sizeof(first));
			break;
		case 10:
			ASSERT_STR_EQ("first", "last");
			break;
		case 11:
			ASSERT_THRD_CREATE(&thread, unused_worker, NULL);
			break;
	}
	/* Keep the outer oracle safe if an assertion unexpectedly returns normally. */
	active_local = NULL;
	in_scope = false;
	return TEST_PASS;
}
#undef thrd_create

static int
test_assertion_cleanup(void)
{
	bool valid = true;
	for (unsigned kind = 0; kind < 12; kind++) {
		int result = fail_with_live_context(kind);
		valid = result == TEST_FAIL && active_local == NULL && cleaned_in_scope
		    && in_scope && valid;
		in_scope = false;
	}
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(create_refusals, 1);
	if (printf("assertion_cleanup kinds=12 live=12 valid=1\n") < 0)
		return TEST_FAIL;
	return TEST_PASS;
}

static int
test_byte_views(void)
{
	static const unsigned char data[] = {1, 2};
	struct dssh_bytes bytes = {data, sizeof(data)};
	ASSERT_TRUE(dssh_test_bytes_equal(bytes, data, sizeof(data)));
	ASSERT_FALSE(dssh_test_bytes_equal(bytes, NULL, sizeof(data)));
	ASSERT_FALSE(dssh_test_bytes_equal(bytes, data, 1));
	bytes.data = NULL;
	ASSERT_FALSE(dssh_test_bytes_equal(bytes, data, sizeof(data)));
	bytes.len = 0;
	ASSERT_TRUE(dssh_test_bytes_equal(bytes, NULL, 0));
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "assertion_cleanup", test_assertion_cleanup },
	{ "byte_views", test_byte_views },
};

DSSH_TEST_MAIN(tests)
