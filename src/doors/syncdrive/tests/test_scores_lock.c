/* Unit test for scores_lock.c: a second process blocks until the first
 * releases. POSIX only (fork). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "scores_lock.h"

static long now_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

int main(void)
{
	char  dir[] = "/tmp/syncdrive_lockXXXXXX";
	pid_t pid;
	int   status;

	assert(mkdtemp(dir) != NULL);
	scores_lock_init(dir);
	assert(scores_lock_acquire() == 0);

	pid = fork();
	assert(pid >= 0);
	if (pid == 0) {
		long t0 = now_ms();

		scores_lock_init(dir);
		if (scores_lock_acquire() != 0)
			_exit(2);
		scores_lock_release();
		_exit(now_ms() - t0 >= 250 ? 0 : 1);
	}
	usleep(400 * 1000);
	scores_lock_release();
	assert(waitpid(pid, &status, 0) == pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	return 0;
}
