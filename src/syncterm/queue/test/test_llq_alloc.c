#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>

extern void _test_free(void* const ptr, const char* file, const int line);
void
test_free(void *ptr) {
	_test_free(ptr, "Unknown", -1);
}

#include <cmocka.h>

#include "llq.h"

int
mtx_init(mtx_t *mtx, int type)
{
	return mock();
}

void
mtx_destroy(mtx_t *mtx)
{
	function_called();
}

int
cnd_init(cnd_t *cnd)
{
	return mock();
}

void
cnd_destroy(cnd_t *cnd)
{
	function_called();
}

int
tss_create(tss_t *key, void (*dtor)(void *))
{
	return mock();
}

void
tss_delete(tss_t key)
{
	function_called();
}

static int assert_count = -1;
static void
my_mock_assert(const int result, const char* const expression,
                 const char * const file, const int line)
{
	if (assert_count == -1)
		fail();
	assert_count++;
}

static int malloc_count = -1;
static void *
my_test_calloc(size_t number, const size_t size, const char *file, const int line)
{
	if (malloc_count == 0) {
		errno = ENOMEM;
		return nullptr;
	}
	if (malloc_count > 0)
		malloc_count--;
	return _test_calloc(number, size, file, line);
}

void
test_setup()
{
	assert_count = -1;
	malloc_count = -1;
}

#define llq_test_err(atype, flags, err) \
	llq_err_t e;                     \
	void *ret;                        \
	e = (llq_err_t)&e;                 \
	assert_count = 0;                   \
	atype(ret = llq_alloc(&e, flags));   \
	free(ret);                            \
	assert_true(e == err)

#define llq_test_noerr(atype, flags)  \
	void *ret;                     \
	assert_count = 0;               \
	ret = llq_alloc(nullptr, flags); \
	atype(ret);                       \
	free(ret);

void
illegal_flags_error(void **state)
{
	test_setup();
	llq_test_err(assert_null, 1, llq_err_badflags);
	assert_true(assert_count == 1);
}

void
illegal_flags_noerror(void **state)
{
	test_setup();
	llq_test_noerr(assert_null, 1);
	assert_true(assert_count == 1);
}

void
calloc_failed_error(void **state)
{
	test_setup();
	malloc_count = 0;
	llq_test_err(assert_null, 0, llq_err_calloc);
	assert_true(assert_count == 1);
}

void
calloc_failed_noerror(void **state)
{
	test_setup();
	malloc_count = 0;
	llq_test_noerr(assert_null, 0);
	assert_true(assert_count == 1);
}

void
mtx_init_failed_error(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_error);
	llq_test_err(assert_null, 0, llq_err_mtx_init);
	assert_true(assert_count == 1);
}

void
mtx_init_failed_noerror(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_error);
	llq_test_noerr(assert_null, 0);
	assert_true(assert_count == 1);
}

void
cnd_init_failed_error(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_success);
	will_return(cnd_init, thrd_error);
	expect_function_call(mtx_destroy);
	llq_test_err(assert_null, 0, llq_err_cnd_init);
	assert_true(assert_count == 1);
}

void
cnd_init_failed_noerror(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_success);
	will_return(cnd_init, thrd_error);
	expect_function_call(mtx_destroy);
	llq_test_noerr(assert_null, 0);
	assert_true(assert_count == 1);
}

void
tss_create_failed_error(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_success);
	will_return(cnd_init, thrd_success);
	will_return(tss_create, thrd_error);
	expect_function_call(cnd_destroy);
	expect_function_call(mtx_destroy);
	llq_test_err(assert_null, 0, llq_err_tss_create);
	assert_true(assert_count == 1);
}

void
tss_create_failed_noerror(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_success);
	will_return(cnd_init, thrd_success);
	will_return(tss_create, thrd_error);
	expect_function_call(cnd_destroy);
	expect_function_call(mtx_destroy);
	llq_test_noerr(assert_null, 0);
	assert_true(assert_count == 1);
}

void
tss_create_failed_error2(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_success);
	will_return(cnd_init, thrd_success);
	will_return(tss_create, thrd_success);
	will_return(tss_create, thrd_error);
	expect_function_call(tss_delete);
	expect_function_call(cnd_destroy);
	expect_function_call(mtx_destroy);
	llq_test_err(assert_null, 0, llq_err_tss_create);
	assert_true(assert_count == 1);
}

void
tss_create_failed_noerror2(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_success);
	will_return(cnd_init, thrd_success);
	will_return(tss_create, thrd_success);
	will_return(tss_create, thrd_error);
	expect_function_call(tss_delete);
	expect_function_call(cnd_destroy);
	expect_function_call(mtx_destroy);
	llq_test_noerr(assert_null, 0);
	assert_true(assert_count == 1);
}

void
success_error(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_success);
	will_return(cnd_init, thrd_success);
	will_return(tss_create, thrd_success);
	will_return(tss_create, thrd_success);
	llq_test_err(assert_non_null, 0, llq_err_none);
	assert_true(assert_count == 0);
}

void
success_noerror(void **state)
{
	test_setup();
	will_return(mtx_init, thrd_success);
	will_return(cnd_init, thrd_success);
	will_return(tss_create, thrd_success);
	will_return(tss_create, thrd_success);
	llq_test_noerr(assert_non_null, 0);
	assert_true(assert_count == 0);
}

int
main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(illegal_flags_error),
		cmocka_unit_test(illegal_flags_noerror),
		cmocka_unit_test(calloc_failed_error),
		cmocka_unit_test(calloc_failed_noerror),
		cmocka_unit_test(mtx_init_failed_error),
		cmocka_unit_test(mtx_init_failed_noerror),
		cmocka_unit_test(cnd_init_failed_error),
		cmocka_unit_test(cnd_init_failed_noerror),
		cmocka_unit_test(tss_create_failed_error),
		cmocka_unit_test(tss_create_failed_noerror),
		cmocka_unit_test(tss_create_failed_error2),
		cmocka_unit_test(tss_create_failed_noerror2),
		cmocka_unit_test(success_error),
		cmocka_unit_test(success_noerror),
	};

	return cmocka_run_group_tests(tests, nullptr, nullptr);
}

#undef test_calloc
#define test_calloc(number, size) my_test_calloc(number, size, __FILE__, __LINE__)
#include "../llq_alloc.c"
