/* scores_lock.c -- see scores_lock.h. */
#include <stdio.h>
#include "filewrap.h"       /* also <fcntl.h> + <unistd.h>/<io.h> */
#include "scores_lock.h"

static char lock_path[1024];
static int  lock_fd = -1;

void scores_lock_init(const char *data_dir)
{
	snprintf(lock_path, sizeof lock_path, "%s/SCORES.lck", data_dir);
}

int scores_lock_acquire(void)
{
	lock_fd = open(lock_path, O_RDWR | O_CREAT | O_BINARY, DEFFILEMODE);
	if (lock_fd < 0)
		return -1;
	if (xp_lockfile(lock_fd, 0, 1, true) != 0) {
		close(lock_fd);
		lock_fd = -1;
		return -1;
	}
	return 0;
}

void scores_lock_release(void)
{
	if (lock_fd < 0)
		return;
	unlock(lock_fd, 0, 1);
	close(lock_fd);
	lock_fd = -1;
}
