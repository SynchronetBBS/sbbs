#include <stdio.h>

#include "CuTest.h"

void test_root_window(CuTest *ct);
void test_label(CuTest *ct);
void test_api(CuTest *ct);

void RunAllTests(void) {
	CuString *output = CuStringNew();
	CuSuite* suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_root_window);
	SUITE_ADD_TEST(suite, test_label);
	SUITE_ADD_TEST(suite, test_api);

	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
}

int
main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	RunAllTests();
	return 0;
}
