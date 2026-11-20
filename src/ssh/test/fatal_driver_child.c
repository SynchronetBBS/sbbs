/* Controlled diagnostics for the real fatal driver. No invalid memory access
 * is used: sanitizer names are literal test input followed by an actual abort. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
	const char *scenario = getenv("DSSH_FATAL_POLICY_CASE");
	if (scenario == NULL || (argc != 3 && argc != 4))
		return 2;
	const char *marker;
	if (strcmp(argv[1], "--unlock-fatal") == 0)
		marker = "DSSH_TEST_UNLOCK_FAILURE\n";
	else if (strcmp(argv[1], "--wake-fatal") == 0)
		marker = "DSSH_TEST_WAKE_FAILURE\n";
	else if (strcmp(argv[1], "--channel-fatal") == 0)
		marker = "DSSH_TEST_CHANNEL_JOIN_FAILURE\n";
	else if (strcmp(argv[1], "--join-fatal") == 0 && strcmp(argv[2], "stop") == 0)
		marker = "DSSH_TEST_JOIN_FAILURE\n";
	else if (strcmp(argv[1], "--join-fatal") == 0 && strcmp(argv[2], "mutex-destroy") == 0)
		marker = "DSSH_TEST_SHIM_DESTROY_FAILURE\n";
	else
		return 2;
	if (strcmp(scenario, "missing") == 0)
		marker = "different-marker\n";
	if (fputs(marker, stderr) < 0)
		return 3;
	/* More than the driver's old prefix buffer and native read chunk. */
	static const char part[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	for (unsigned i = 0; i < 64; i++) {
		if (fwrite(part, 1, sizeof(part) - 1, stderr) != sizeof(part) - 1)
			return 3;
	}
	if (fputs("\ndiagnostic_tail=1\n", stderr) < 0)
		return 3;
	if (strcmp(scenario, "AddressSanitizer") == 0
	    || strcmp(scenario, "LeakSanitizer") == 0
	    || strcmp(scenario, "ThreadSanitizer") == 0
	    || strcmp(scenario, "UndefinedBehaviorSanitizer") == 0
	    || strcmp(scenario, "runtime error:") == 0) {
		if (fprintf(stderr, "%s synthetic-control\n", scenario) < 0)
			return 3;
	}
	else if (strcmp(scenario, "fake-exit") != 0 && strcmp(scenario, "wrong-signal") != 0
	    && strcmp(scenario, "missing") != 0 && strcmp(scenario, "clean") != 0)
		return 2;
	if (fflush(stderr) != 0)
		return 3;
	if (strcmp(scenario, "fake-exit") == 0) {
		if (puts("fatal_child=1") == EOF || fflush(stdout) != 0)
			return 3;
		return 0;
	}
	if (strcmp(scenario, "wrong-signal") == 0) {
		if (signal(SIGTERM, SIG_DFL) == SIG_ERR || raise(SIGTERM) != 0)
			return 3;
		return 4;
	}
	abort();
}
