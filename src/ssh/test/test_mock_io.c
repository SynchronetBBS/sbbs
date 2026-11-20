/* Compile the actual mock implementation with typed native boundary controls. */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "mock_io.h"

static int native_pair(int, int, int, int[2]);
static int native_option(int, int, int, const void *, socklen_t);
static int native_close(int);
static int native_shutdown(int, int);
static ssize_t native_send(int, const void *, size_t, int);
static ssize_t native_read(int, void *, size_t);
static ssize_t native_recv(int, void *, size_t, int);
#define socketpair native_pair
#define setsockopt native_option
#define close native_close
#define shutdown native_shutdown
#define send native_send
#define read native_read
#define recv native_recv
#include "mock_io.c"
#undef socketpair
#undef setsockopt
#undef close
#undef shutdown
#undef send
#undef read
#undef recv

static const char *scenario;
static unsigned target;
static bool after;
static bool io_fault;
static unsigned pair_calls, option_calls, close_calls, shutdown_calls, send_calls, read_calls, recv_calls;
static unsigned produced, released;
static int descriptors[4];
static bool owned[4];

static void
journal(const char *kind, unsigned index, unsigned slot, bool completed, bool injected)
{
	if (printf("mock_native kind=%s index=%u slot=%u completed=%u injected=%u\n",
	    kind, index, slot, completed ? 1U : 0U, injected ? 1U : 0U) < 0 || fflush(stdout) != 0)
		_Exit(7);
}

static unsigned
slot_for(int fd)
{
	for (unsigned i = 0; i < produced; i++) {
		if (descriptors[i] == fd && owned[i])
			return i;
	}
	abort();
}

static int
native_pair(int domain, int type, int protocol, int fds[2])
{
	unsigned index = pair_calls++;
	if (strcmp(scenario, "pair") == 0 && index == target) {
		journal("pair", index, produced, false, true);
		errno = EMFILE;
		return -1;
	}
	if (produced > 2)
		abort();
	int ret = socketpair(domain, type, protocol, fds);
	if (ret == 0) {
		for (unsigned i = 0; i < 2; i++) {
			descriptors[produced] = fds[i];
			owned[produced] = true;
			produced++;
		}
	}
	journal("pair", index, produced, ret == 0, false);
	return ret;
}

static int
native_option(int fd, int level, int option, const void *value, socklen_t size)
{
	unsigned index = option_calls++;
	bool selected = strcmp(scenario, "option") == 0 && index == target;
	int ret = selected && !after ? -1 : setsockopt(fd, level, option, value, size);
	journal("option", index, slot_for(fd), (!selected || after) && ret == 0, selected);
	if (selected) {
		errno = EIO;
		return -1;
	}
	return ret;
}

static int
native_close(int fd)
{
	unsigned index = close_calls++;
	unsigned slot = slot_for(fd);
	bool selected = strcmp(scenario, "close") == 0 && index == target;
	int ret = selected && !after ? -1 : close(fd);
	bool completed = (!selected || after) && ret == 0;
	if (completed) {
		owned[slot] = false;
		released++;
	}
	journal("close", index, slot, completed, selected);
	if (selected) {
		errno = EINTR;
		return -1;
	}
	return ret;
}

static int
native_shutdown(int fd, int how)
{
	unsigned index = shutdown_calls++;
	bool selected = strcmp(scenario, "shutdown") == 0 && index == target;
	int ret = selected && !after ? -1 : shutdown(fd, how);
	int saved_errno = errno;
	bool completed = (!selected || after) && (ret == 0 || saved_errno == ENOTCONN);
	journal("shutdown", index, slot_for(fd), completed, selected);
	if (selected || strcmp(scenario, "disconnected") == 0) {
		errno = selected ? EIO : ENOTCONN;
		return -1;
	}
	errno = saved_errno;
	return ret;
}

static bool
fault_is(const char *kind, const char *suffix)
{
	char name[40];
	int length = snprintf(name, sizeof(name), "%s-%s", kind, suffix);
	if (length < 0 || length >= 40)
		abort();
	return strcmp(scenario, name) == 0;
}

static ssize_t
transfer(int fd, void *buffer, const void *input, size_t size, int flags, unsigned kind)
{
	const char *name = kind == 0 ? "write" : kind == 1 ? "read" : kind == 2 ? "inject" : "drain";
	unsigned *calls = kind == 0 || kind == 2 ? &send_calls : kind == 1 ? &read_calls : &recv_calls;
	if (*calls == UINT_MAX)
		abort();
	(*calls)++;
	bool line = kind == 1 && strncmp(scenario, "line-", 5) == 0;
	const char *fault_name = line ? "line" : name;
	bool selected = !io_fault && (fault_is(fault_name, "intr") || fault_is(fault_name, "again")
	    || fault_is(fault_name, "zero") || fault_is(fault_name, "over"));
	if (selected) {
		io_fault = true;
		if (fault_is(fault_name, "intr") || fault_is(fault_name, "again")) {
			errno = fault_is(fault_name, "intr") ? EINTR : EAGAIN;
			return -1;
		}
		if (fault_is(fault_name, "zero"))
			return 0;
	}
	ssize_t ret = kind == 0 || kind == 2 ? send(fd, input, size, flags)
	    : kind == 1 ? read(fd, buffer, size) : recv(fd, buffer, size, flags);
	if (selected) {
		if (ret <= 0 || size != 1)
			abort();
		return 2; /* Metadata only; native work used the real one-byte extent. */
	}
	return ret;
}

static ssize_t
native_send(int fd, const void *buffer, size_t size, int flags)
{
	unsigned kind = strncmp(scenario, "inject-", 7) == 0 ? 2U : 0U;
	return transfer(fd, NULL, buffer, size, flags, kind);
}

static ssize_t
native_read(int fd, void *buffer, size_t size)
{
	return transfer(fd, buffer, NULL, size, 0, 1);
}

static ssize_t
native_recv(int fd, void *buffer, size_t size, int flags)
{
	return transfer(fd, buffer, NULL, size, flags, 3);
}

static bool
retired(const struct mock_io_state *io)
{
	return atomic_load(&io->c2s.rfd) == -1 && atomic_load(&io->c2s.wfd) == -1
	    && atomic_load(&io->s2c.rfd) == -1 && atomic_load(&io->s2c.wfd) == -1
	    && atomic_load(&io->c2s.read_closed) && atomic_load(&io->c2s.write_closed)
	    && atomic_load(&io->s2c.read_closed) && atomic_load(&io->s2c.write_closed);
}

static bool
exercise(struct mock_io_state *io)
{
	uint8_t output[8];
	memset(output, 0x5a, sizeof(output));
	static const uint8_t bytes[] = "ABC\nXY";
	bool valid = true;
	if (strcmp(scenario, "arguments") == 0) {
		size_t received = 77;
		valid = mock_tx_client(NULL, 1, NULL, io) == -1 && mock_rx_client(NULL, 1, NULL, io) == -1
		    && mock_rxline_client(NULL, 1, &received, NULL, io) == -1 && received == 0
		    && mock_rxline_client(output, 1, NULL, NULL, io) == -1
		    && mock_tx_client(output, 1, NULL, NULL) == -1
		    && mock_rx_server(output, 1, NULL, NULL) == -1
		    && mock_io_inject(&io->c2s, NULL, 1) == 0 && mock_io_drain(&io->s2c, NULL, 1) == 0
		    && send_calls == 0 && read_calls == 0 && recv_calls == 0;
	}
	else if (strncmp(scenario, "write-", 6) == 0 || strncmp(scenario, "inject-", 7) == 0) {
		int ret = strncmp(scenario, "write-", 6) == 0 ? mock_tx_client(output, 1, NULL, io)
		    : mock_io_inject(&io->c2s, output, 1) == 1 ? 0 : -1;
		valid = io_fault && ret == (strstr(scenario, "over") || strstr(scenario, "zero") ? -1 : 0);
	}
	else {
		bool line = strncmp(scenario, "line-", 5) == 0;
		size_t size = line ? 4 : strcmp(scenario, "overflow") == 0 ? 6 : 1;
		if (strcmp(scenario, "line-exact") == 0)
			size = 6;
		if (mock_io_inject(&io->s2c, bytes, size) != size)
			return false;
		if (strcmp(scenario, "zero-cap") == 0) {
			size_t received = 77;
			return mock_rxline_client(NULL, 0, &received, NULL, io) == 0 && received == 1;
		}
		if (strcmp(scenario, "queued") == 0)
			mock_io_close_s2c_write(io);
		size_t received = 77;
		if (line || strcmp(scenario, "overflow") == 0) {
			size_t capacity = strcmp(scenario, "line-exact") == 0 ? 4 : line ? 1 : 2;
			int ret = mock_rxline_client(output, capacity, &received, NULL, io);
			bool over = strcmp(scenario, "line-over") == 0;
			valid = valid && ret == (over ? -1 : 0) && received == (over ? 0 : capacity == 4 ? 4 : capacity + 1)
			    && output[capacity] == 0x5a;
			if (!over)
				valid = valid && memcmp(output, bytes, capacity) == 0;
			if (strcmp(scenario, "overflow") == 0 || strcmp(scenario, "line-exact") == 0) {
				uint8_t pending[4];
				size_t consumed = strcmp(scenario, "overflow") == 0 ? 3 : 4;
				size_t remaining = size - consumed;
				valid = mock_io_drain(&io->s2c, pending, remaining) == remaining
				    && memcmp(pending, bytes + consumed, remaining) == 0 && valid;
			}
		}
		else if (strncmp(scenario, "drain-", 6) == 0) {
			valid = valid && mock_io_drain(&io->s2c, output, 1) == 0 && io_fault;
		}
		else {
			int ret = mock_rx_client(output, 1, NULL, io);
			bool rejected = fault_is("read", "over") || fault_is("read", "zero");
			valid = valid && ret == (rejected ? -1 : 0) && output[1] == 0x5a;
			if (!rejected)
				valid = valid && output[0] == bytes[0];
			if (strcmp(scenario, "queued") == 0) {
				uint8_t extra;
				valid = atomic_load(&io->s2c.wfd) >= 0 && atomic_load(&io->s2c.rfd) >= 0
				    && atomic_load(&io->s2c.write_closed) && !atomic_load(&io->s2c.read_closed)
				    && mock_rx_client(&extra, 1, NULL, io) == -1 && valid;
			}
			if (strcmp(scenario, "normal") == 0) {
				valid = mock_tx_client(output, 1, NULL, io) == 0
				    && mock_rx_server(output, 1, NULL, io) == 0
				    && mock_tx_server(output, 1, NULL, io) == 0
				    && mock_rx_client(output, 1, NULL, io) == 0 && output[0] == bytes[0] && valid;
			}
		}
	}
	return valid;
}

static bool
math_cases(void)
{
	size_t value = 7;
	bool valid = !pipe_count(-1, 1, &value) && value == 7;
	valid = !pipe_count(0, 1, &value) && value == 7 && valid;
	valid = !pipe_count(2, 1, &value) && value == 7 && valid;
	valid = pipe_count(1, 1, &value) && value == 1 && valid;
	valid = pipe_line_next(0, &value) && value == 1 && valid;
	valid = pipe_line_next(SIZE_MAX - 1, &value) && value == SIZE_MAX && valid;
	value = 7;
	valid = !pipe_line_next(SIZE_MAX, &value) && value == 7 && valid;
	uintmax_t maximum = (uintmax_t)SSIZE_MAX;
	uintmax_t bounded = maximum < SIZE_MAX ? maximum : SIZE_MAX;
	valid = pipe_request(0) == 0 && pipe_request(1) == 1 && pipe_request(SIZE_MAX) == bounded && valid;
	return valid;
}

int
main(int argc, char **argv)
{
	if (argc < 2 || argc > 4)
		return 2;
	scenario = argv[1];
	if (argc > 2) {
		if (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '7')
			return 2;
		target = (unsigned)(argv[2][0] - '0');
	}
	if (argc == 4) {
		if (strcmp(argv[3], "before") != 0 && strcmp(argv[3], "after") != 0)
			return 2;
		after = strcmp(argv[3], "after") == 0;
	}
	if (strcmp(scenario, "math") == 0) {
		bool valid = math_cases();
		if (printf("mock_io_math cases=10 valid=%u\n", valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
			return 1;
		return valid ? 0 : 1;
	}
	struct mock_io_state io;
	int result = mock_io_init(&io, 0);
	bool failed_init = strcmp(scenario, "pair") == 0 || strcmp(scenario, "option") == 0;
	bool valid = result == (failed_init ? -1 : 0);
	if (result == 0) {
		if (strcmp(scenario, "close") != 0 && strcmp(scenario, "shutdown") != 0)
			valid = exercise(&io) && valid;
		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
		valid = shutdown_calls == 4 && valid;
	}
	mock_io_free(&io);
	mock_io_free(&io);
	valid = retired(&io) && released == produced && close_calls == produced && valid;
	if (printf("mock_io_control mode=%s target=%u after=%u result=%d pairs=%u options=%u produced=%u released=%u closes=%u shutdowns=%u sends=%u reads=%u recvs=%u fault=%u retired=%u valid=%u\n",
	    scenario, target, after ? 1U : 0U, result, pair_calls, option_calls, produced, released, close_calls,
	    shutdown_calls, send_calls, read_calls, recv_calls, io_fault ? 1U : 0U,
	    retired(&io) ? 1U : 0U, valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
		return 1;
	return valid ? 0 : 1;
}
