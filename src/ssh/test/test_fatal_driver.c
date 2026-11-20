/* POSIX test driver: require the intended failure marker and actual SIGABRT.
 * Full child diagnostics remain available to the strict output runner. */
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	if (argc != 3 && argc != 5)
		return 2;
	bool traced = argc == 5;
	bool wake = traced && strcmp(argv[2], "--wake-fatal") == 0;
	bool channel = traced && strcmp(argv[2], "--channel-fatal") == 0;
	if (traced && !wake && !channel && strcmp(argv[2], "--unlock-fatal") != 0)
		return 2;
	const char *marker;
	if (traced)
		marker = channel ? "DSSH_TEST_CHANNEL_JOIN_FAILURE\n"
		    : wake ? "DSSH_TEST_WAKE_FAILURE\n" : "DSSH_TEST_UNLOCK_FAILURE\n";
	else if (strcmp(argv[2], "stop") == 0 || strcmp(argv[2], "cleanup") == 0)
		marker = "DSSH_TEST_JOIN_FAILURE\n";
	else if (strcmp(argv[2], "mutex-destroy") == 0 || strcmp(argv[2], "condition-destroy") == 0)
		marker = "DSSH_TEST_SHIM_DESTROY_FAILURE\n";
	else
		return 2;
	size_t marker_len = strlen(marker);
	int descriptors[2];
	if (pipe(descriptors) != 0)
		return 3;
	struct rlimit core = {0, 0};
	if (setrlimit(RLIMIT_CORE, &core) != 0)
		return 3;
	pid_t child = fork();
	if (child < 0)
		return 4;
	if (child == 0) {
		if (close(descriptors[0]) != 0)
			_exit(127);
		if (dup2(descriptors[1], STDERR_FILENO) < 0)
			_exit(127);
		if (descriptors[1] != STDERR_FILENO && close(descriptors[1]) != 0)
			_exit(127);
		char *arguments[] = {argv[1], traced ? argv[2] : "--join-fatal",
		    traced ? argv[3] : argv[2], traced ? argv[4] : NULL, NULL};
		execv(argv[1], arguments);
		_exit(127);
	}
	if (close(descriptors[1]) != 0)
		return 5;
	size_t used = 0;
	bool prefix_valid = true;
	for (;;) {
		char part[256];
		ssize_t got = read(descriptors[0], part, sizeof(part));
		if (got < 0 && errno == EINTR)
			continue;
		if (got < 0)
			return 5;
		if (got == 0)
			break;
		/* A successful native read is bounded by its requested capacity. */
		size_t bytes = (size_t)got;
		if (fwrite(part, 1, bytes, stderr) != bytes)
			return 5;
		size_t remaining = marker_len - used;
		size_t keep = bytes < remaining ? bytes : remaining;
		if (memcmp(part, marker + used, keep) != 0)
			prefix_valid = false;
		used += keep; /* keep <= marker_len - used proves this fits. */
	}
	if (close(descriptors[0]) != 0)
		return 5;
	int status;
	pid_t waited;
	do {
		waited = waitpid(child, &status, 0);
	} while (waited < 0 && errno == EINTR);
	if (waited != child)
		return 5;
	if (used != marker_len || !prefix_valid)
		return 8;
	if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
		if (fprintf(stderr, "fatal child wait status %d; expected SIGABRT\n", status) < 0)
			return 5;
		return 6;
	}
	if (channel && puts("channel_join_fatal=1") == EOF)
		return 5;
	if (puts("fatal_child=1") == EOF || fflush(stdout) != 0 || fflush(stderr) != 0)
		return 5;
	return 0;
}
