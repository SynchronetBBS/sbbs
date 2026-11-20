/* Exercise the real runner with deterministic reporting failures. */
#include <stdio.h>
#include <string.h>

#include "mlkem_acvp.h"
#include "mlkem_acvp_vectors.h"

static const struct dssh_acvp_case *selected_cases[145];

enum failure { IO_NONE, IO_PRINT, IO_DIAGNOSTIC, IO_FLUSH_OUT, IO_FLUSH_ERR };
static enum failure failure;
static size_t prints;
static size_t diagnostics;
static size_t out_flushes;
static size_t err_flushes;
static size_t evaluations;
static bool evaluation_matches;

static int
report_print(const char *format, ...)
{
	(void)format;
	prints++;
	return failure == IO_PRINT ? -1 : 1;
}

static int
report_diagnostic(FILE *stream, const char *format, ...)
{
	(void)format;
	if (stream != stderr)
		return -1;
	diagnostics++;
	return failure == IO_DIAGNOSTIC ? -1 : 1;
}

static int
report_flush(FILE *stream)
{
	if (stream == stdout) {
		out_flushes++;
		return failure == IO_FLUSH_OUT ? EOF : 0;
	}
	if (stream == stderr) {
		err_flushes++;
		return failure == IO_FLUSH_ERR ? EOF : 0;
	}
	return EOF;
}

/* The ordinary runner and its full decoder compile unchanged. Redirect only
 * its standard-I/O endpoints and its fixture pointer table in this executable. */
#define DSSH_ACVP_CASES selected_cases
#define DSSH_ACVP_PRINTF report_print
#define DSSH_ACVP_FPRINTF report_diagnostic
#define DSSH_ACVP_FFLUSH report_flush
#include "mlkem_acvp_data.c"
#undef DSSH_ACVP_FFLUSH
#undef DSSH_ACVP_FPRINTF
#undef DSSH_ACVP_PRINTF
#undef DSSH_ACVP_CASES

static bool
evaluate_case(const struct dssh_acvp_case *test, const struct dssh_acvp_decoded *bytes)
{
	(void)test;
	(void)bytes;
	evaluations++;
	return evaluation_matches;
}

struct scenario {
	enum failure failure;
	bool matched;
	bool negative;
	bool bad_fixture;
	bool no_callback;
	bool bad_option;
	int result;
	size_t evaluations;
	size_t prints;
	size_t diagnostics;
	size_t out_flushes;
	size_t err_flushes;
};

int
main(int argc, char **argv)
{
	bool negative = argc == 2 && strcmp(argv[1], "--negative") == 0;
	if (argc != 1 && !negative)
		return 2;
	static const struct scenario cases[] = {
		{IO_NONE, true, false, false, false, false, 0, 145, 1, 0, 1, 0},
		{IO_PRINT, true, false, false, false, false, 2, 145, 1, 0, 0, 0},
		{IO_FLUSH_OUT, true, false, false, false, false, 2, 145, 1, 0, 1, 0},
		{IO_NONE, false, true, false, false, false, 1, 1, 0, 1, 0, 1},
		{IO_DIAGNOSTIC, false, true, false, false, false, 2, 1, 0, 1, 0, 0},
		{IO_FLUSH_ERR, false, true, false, false, false, 2, 1, 0, 1, 0, 1},
		{IO_NONE, false, false, false, false, false, 1, 145, 1, 145, 1, 145},
		{IO_NONE, true, false, true, false, false, 2, 0, 0, 1, 0, 1},
		{IO_DIAGNOSTIC, true, false, true, false, false, 2, 0, 0, 1, 0, 0},
		{IO_FLUSH_ERR, true, false, true, false, false, 2, 0, 0, 1, 0, 1},
		{IO_NONE, true, false, false, true, false, 2, 0, 0, 0, 0, 0},
		{IO_NONE, true, false, false, false, true, 2, 0, 0, 0, 0, 0},
	};
	bool valid = true;
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const struct scenario *test = &cases[i];
		for (size_t j = 0; j < 145; j++)
			selected_cases[j] = acvp_cases[j];
		struct dssh_acvp_case invalid = *acvp_cases[0];
		invalid.kind = DSSH_ACVP_KIND_COUNT;
		if (test->bad_fixture)
			selected_cases[0] = &invalid;
		failure = test->failure;
		evaluation_matches = test->matched;
		prints = diagnostics = out_flushes = err_flushes = evaluations = 0;
		char name[] = "reporting";
		char flag[] = "--negative";
		char label[] = "keygen-private";
		char unknown[] = "--unknown";
		char *arguments[] = {name, flag, label, NULL};
		int count = test->negative ? 3 : 1;
		if (test->bad_option) {
			arguments[1] = unknown;
			count = 2;
		}
		dssh_acvp_evaluate callback = test->no_callback ? NULL : evaluate_case;
		int result = dssh_acvp_run(count, arguments, "reporting", callback);
		int expected = negative && i == 0 ? 1 : test->result;
		if (result != expected || evaluations != test->evaluations || prints != test->prints
		    || diagnostics != test->diagnostics || out_flushes != test->out_flushes
		    || err_flushes != test->err_flushes) {
			valid = false;
			if (fprintf(stderr, "ACVP reporting control %zu failed\n", i) < 0
			    || fflush(stderr) != 0)
				return 2;
		}
	}
	if (printf("ACVP reporting: controls=12 valid=%u\n", valid ? 1U : 0U) < 0
	    || fflush(stdout) != 0)
		return 2;
	return valid ? 0 : 1;
}
