#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include "llq.h"

static llq llq;



main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
	};

	return cmocka_run_group_tests(tests, nullptr, nullptr);
}
