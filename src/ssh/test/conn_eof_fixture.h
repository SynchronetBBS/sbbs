/* Decisive observation for a quiescent native test reader. */
#ifndef DSSH_TEST_CONN_EOF_FIXTURE_H
#define DSSH_TEST_CONN_EOF_FIXTURE_H

#include <poll.h>
#include <sys/socket.h>

/* The caller owns the live descriptors and excludes other readers.
 * 0: actual native EOF; 1: an unexpected byte (consumed); -1: any failure.
 * A timeout/EAGAIN, local shutdown or invalid descriptor is never EOF. */
static int
conn_native_eof(struct mock_io_pipe *pipe)
{
	if (pipe == NULL || atomic_load(&pipe->read_closed))
		return -1;
	int fd = atomic_load(&pipe->rfd);
	if (fd < 0)
		return -1;
	struct pollfd ready = {.fd = fd, .events = POLLIN, .revents = 0};
	int polled = poll(&ready, 1, 0);
	if (polled != 1 || (ready.revents & (POLLERR | POLLNVAL)) != 0
	    || (ready.revents & (POLLIN | POLLHUP)) == 0)
		return -1;
	/* With no competing reader or descriptor retirement, readiness cannot
	 * be consumed between this poll and the one-byte receive. */
	uint8_t byte;
	ssize_t result = recv(fd, &byte, 1, 0);
	if (result == 0)
		return 0;
	if (result == 1)
		return 1;
	return -1;
}

#endif
