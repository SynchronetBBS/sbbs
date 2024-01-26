#include <stdio.h>

#include "CuTest.h"

CuSuite* root_window_get_test_suite();

void RunAllTests(void) {
	CuString *output = CuStringNew();
	CuSuite* suite = CuSuiteNew();

	CuSuiteAddSuite(suite, root_window_get_test_suite());

	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
}

int
main(int argc, char **argv) {
	RunAllTests();
	return 0;
}
