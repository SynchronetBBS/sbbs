#ifndef DSSH_EXAMPLE_SOCKET_IO_H
#define DSSH_EXAMPLE_SOCKET_IO_H

/* Private POSIX example code, not part of the library or installed SDK.
 * The application owns a nonblocking socket and its SIGPIPE policy. */
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <time.h>
#include "deucessh.h"
#include "deucessh-conn.h"

/* Reset only during serialized connection setup/authentication. Once the
 * connection thread starts, callbacks only read this application-owned state. */
struct example_socket_io {
	int descriptor;
	struct timespec activity;
	bool timed;
};

static inline int
example_socket_reset(struct example_socket_io *io)
{
	struct timespec now;
	if (io == NULL || clock_gettime(CLOCK_MONOTONIC, &now) != 0
	    || now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L)
		return DSSH_ERROR_INIT;
	io->activity = now;
	io->timed = true;
	return 0;
}

static inline int
example_socket_wait(const struct example_socket_io *io, int *milliseconds)
{
	struct timespec now;
	if (!io->timed || clock_gettime(CLOCK_MONOTONIC, &now) != 0
	    || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L || now.tv_sec < io->activity.tv_sec)
		return DSSH_ERROR_INIT;
	/* difftime avoids overflowing time_t subtraction. Both values originate
	 * from the POSIX monotonic clock; no calendar interpretation is needed. */
	double seconds = difftime(now.tv_sec, io->activity.tv_sec);
	if (seconds == 0 && now.tv_nsec < io->activity.tv_nsec)
		return DSSH_ERROR_INIT;
	if (seconds > 60 || (seconds == 60 && now.tv_nsec >= io->activity.tv_nsec))
		return DSSH_ERROR_TIMEOUT;
	long remaining = 1000000000L;
	if (seconds == 60)
		remaining = io->activity.tv_nsec - now.tv_nsec;
	else if (seconds == 59 && now.tv_nsec >= io->activity.tv_nsec)
		remaining -= now.tv_nsec - io->activity.tv_nsec;
	/* remaining is 1..10^9, so this sum fits the POSIX long minimum and the
	 * rounded result is 1..1000, within the C int minimum. */
	long rounded = (remaining + 999999L) / 1000000L;
	int wait = (int)rounded;
	*milliseconds = wait;
	return 0;
}

static inline int
example_socket_transfer(const struct example_socket_io *io, uint8_t *buffer, size_t length,
    dssh_session session, bool transmit)
{
	if (io == NULL || io->descriptor < 0 || session == NULL || (length > 0 && buffer == NULL))
		return DSSH_ERROR_INVALID;
	size_t offset = 0;
	while (offset < length) {
		if (dssh_session_is_terminated(session))
			return DSSH_ERROR_TERMINATED;
		int milliseconds;
		int ret = example_socket_wait(io, &milliseconds);
		if (ret < 0)
			return ret;
		short wanted = POLLIN;
		if (transmit)
			wanted = POLLOUT;
		struct pollfd item = { .fd = io->descriptor, .events = wanted };
		int ready = poll(&item, 1, milliseconds);
		if (dssh_session_is_terminated(session))
			return DSSH_ERROR_TERMINATED;
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			return DSSH_ERROR_INIT;
		}
		if (ready == 0)
			continue;
		ret = example_socket_wait(io, &milliseconds);
		if (ret < 0)
			return ret;
		if (ready != 1 || (item.revents & POLLNVAL) != 0)
			return DSSH_ERROR_INIT;
		/* Read available final bytes before reporting a peer hangup. */
		if (transmit && (item.revents & (POLLHUP | POLLERR)) != 0)
			return DSSH_ERROR_INIT;
		if ((item.revents & wanted) == 0) {
			if ((item.revents & (POLLHUP | POLLERR)) != 0)
				return DSSH_ERROR_INIT;
			continue;
		}
		size_t count = length - offset;
		if (count > 4096)
			count = 4096;
		ssize_t transferred = transmit
		    ? send(io->descriptor, &buffer[offset], count, 0)
		    : recv(io->descriptor, &buffer[offset], count, 0);
		if (transferred > 0) {
			if (transferred > 4096)
				return DSSH_ERROR_INVALID;
			size_t advanced = (size_t)transferred;
			if (advanced > count)
				return DSSH_ERROR_INVALID;
			offset += advanced;
		}
		else if (transferred == 0)
			return DSSH_ERROR_INIT;
		else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static inline int
example_socket_line(const struct example_socket_io *io, uint8_t *buffer, size_t capacity,
    size_t *received, dssh_session session)
{
	if (buffer == NULL || capacity == 0 || received == NULL || session == NULL
	    || io == NULL || io->descriptor < 0)
		return DSSH_ERROR_INVALID;
	*received = 0;
	bool previous_cr = false;
	for (size_t offset = 0; offset < capacity; offset++) {
		int ret = example_socket_transfer(io, &buffer[offset], 1, session, false);
		if (ret < 0)
			return ret;
		if (buffer[offset] == '\n' && previous_cr) {
			*received = offset + 1;
			return 0;
		}
		previous_cr = buffer[offset] == '\r';
	}
	return DSSH_ERROR_TOOLONG;
}

static inline int
example_channel_write(dssh_channel channel, int stream, const uint8_t *bytes, size_t length)
{
	if (channel == NULL || stream < 0 || stream > 1 || (length > 0 && bytes == NULL))
		return DSSH_ERROR_INVALID;
	size_t offset = 0;
	while (offset < length) {
		size_t count = length - offset;
		if (count > 4096)
			count = 4096;
		int64_t written = dssh_chan_write(channel, stream, &bytes[offset], count);
		if (written == DSSH_ERROR_NOMORE) {
			int ready = dssh_chan_poll(channel, DSSH_POLL_WRITE, 1000);
			if (ready < 0)
				return ready;
			continue;
		}
		if (written < 0) {
			if (written < INT_MIN)
				return DSSH_ERROR_INVALID;
			int error = (int)written;
			return error;
		}
		if (written == 0 || written > 4096)
			return DSSH_ERROR_INVALID;
		size_t advanced = (size_t)written;
		if (advanced > count)
			return DSSH_ERROR_INVALID;
		offset += advanced;
	}
	return 0;
}

#endif
