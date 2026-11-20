/* Compile each actual example in this application fixture. Native socketpair
 * traffic and bounded system-call substitutions exercise its real callbacks;
 * the production library itself is never substituted. */
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "deucessh.h"
#include "deucessh-conn.h"

enum io_case {
	TX_FULL, RX_FULL, TX_CANCEL, RX_CANCEL, TX_ZERO, RX_EOF,
	POLL_INTERRUPT, POLL_INVALID, POLL_NOT_READY, TX_AGAIN, RX_AGAIN,
	TX_BAD_EXTENT, RX_BAD_EXTENT, LINE_COMPLETE, LINE_ADJACENCY,
	LINE_SHORT, LINE_ZERO, TIME_EXPIRED, TIME_WAIT, TIME_FRACTION,
	TIME_INVALID, TIME_FAILURE, TIME_BACKWARD, TIME_AFTER_POLL,
	TIME_RESET_FAILURE, TIME_RESET, IO_CASES,
};
static enum io_case active_case;
static atomic_uint send_calls, recv_calls, poll_calls, clock_calls;
static atomic_int last_wait;
static atomic_bool bounded;
static bool cap_channel_writes;
static unsigned application_writes;
enum exchange_case {
	EXEC_NATIVE, EXEC_CAPPED, SHELL_LINES, SHELL_CRLF, SHELL_LF,
	SHELL_PARTIAL, SHELL_LONG, SHELL_BINARY, SHELL_READ_ERROR,
	SHELL_POLL_ERROR, SHELL_NOMORE, SHELL_POLL_ZERO, SHELL_BAD_EXTENT,
	SHELL_PARENT, SHELL_PARENT_ERROR,
	EXCHANGE_CASES,
};
static enum exchange_case exchange;
static bool server_process;
static unsigned application_reads, application_polls, application_signals;
static bool signal_arguments_valid;
static int observed_kill(pid_t, int);
static int64_t observed_channel_read(dssh_channel, int, uint8_t *, size_t);
static int observed_channel_poll(dssh_channel, int, int);
static ssize_t observed_send(int, const void *, size_t, int);
static ssize_t observed_recv(int, void *, size_t, int);
static int observed_poll(struct pollfd *, nfds_t, int);
static int observed_clock(clockid_t, struct timespec *);
static int64_t observed_channel_write(dssh_channel, int, const uint8_t *, size_t);

#define main example_server_main
#define send observed_send
#define recv observed_recv
#define poll observed_poll
#define clock_gettime observed_clock
#define dssh_chan_write observed_channel_write
#define dssh_chan_read observed_channel_read
#define dssh_chan_poll observed_channel_poll
#define kill observed_kill
#include "../examples/server-io.h"
#include "../examples/server-shell.h"
/* Other example paths retain their independent review. These diagnostics are
 * scoped to the existing, otherwise unused application code included here. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#ifdef DSSH_GAME_SERVER
#include "../examples/kbi-game-server.c"
#define EXAMPLE_NAME "game"
#else
#include "../examples/server.c"
#define EXAMPLE_NAME "server"
#endif
#pragma GCC diagnostic pop
#undef poll
#undef clock_gettime
#undef dssh_chan_write
#undef dssh_chan_read
#undef dssh_chan_poll
#undef kill
#undef recv
#undef send
#undef main

static ssize_t
observed_send(int descriptor, const void *data, size_t count, int flags)
{
	send_calls++;
	if (count > 4096)
		atomic_store(&bounded, false);
	if (active_case == TX_ZERO) {
		if (send_calls == 1)
			return 0;
		errno = EIO;
		return -1;
	}
	if (active_case == TX_AGAIN && send_calls == 1) {
		errno = EWOULDBLOCK;
		return -1;
	}
	if (active_case == TX_BAD_EXTENT) {
		if (count > 8192)
			abort();
		ssize_t oversized = (ssize_t)(count + 1);
		return oversized;
	}
	size_t amount = active_case == TX_CANCEL ? 1 : count;
	ssize_t result = send(descriptor, data, amount, flags);
	if (active_case == TX_CANCEL && result > 0)
		dssh_session_terminate(sess);
	return result;
}

static ssize_t
observed_recv(int descriptor, void *data, size_t count, int flags)
{
	recv_calls++;
	if (count > 4096)
		atomic_store(&bounded, false);
	if (active_case == RX_AGAIN && recv_calls == 1) {
		errno = EWOULDBLOCK;
		return -1;
	}
	if (active_case == RX_BAD_EXTENT) {
		if (count > 8192)
			abort();
		ssize_t oversized = (ssize_t)(count + 1);
		return oversized;
	}
	size_t amount = active_case == RX_CANCEL ? 1 : count;
	ssize_t result = recv(descriptor, data, amount, flags);
	if (active_case == RX_CANCEL && result > 0)
		dssh_session_terminate(sess);
	return result;
}

static int
observed_poll(struct pollfd *items, nfds_t count, int timeout)
{
	poll_calls++;
	last_wait = timeout;
	if (timeout <= 0 || timeout > 1000)
		atomic_store(&bounded, false);
	if (poll_calls > 10000) {
		errno = EIO;
		return -1;
	}
	if (active_case == POLL_INTERRUPT && poll_calls == 1) {
		errno = EINTR;
		return -1;
	}
	if (active_case == POLL_INVALID) {
		items[0].revents = POLLNVAL;
		return 1;
	}
	if (active_case == POLL_NOT_READY && poll_calls == 1) {
		items[0].revents = 0;
		return 1;
	}
	return poll(items, count, timeout);
}

static int
observed_clock(clockid_t clock, struct timespec *output)
{
	clock_calls++;
	if (active_case < TIME_EXPIRED)
		return clock_gettime(clock, output);
	if (clock != CLOCK_MONOTONIC)
		abort();
	if (active_case == TIME_RESET_FAILURE || (active_case == TIME_FAILURE && clock_calls > 1)) {
		errno = EIO;
		return -1;
	}
	*output = (struct timespec){ .tv_sec = 10, .tv_nsec = 500000000L };
	if (clock_calls == 1)
		return 0;
	if (active_case == TIME_INVALID)
		output->tv_nsec = 1000000000L;
	else if (active_case == TIME_BACKWARD)
		output->tv_sec = 9;
	else if (active_case == TIME_WAIT) {
		output->tv_sec = 69;
		output->tv_nsec = 750000000L;
	}
	else if (active_case == TIME_FRACTION) {
		output->tv_sec = 70;
		output->tv_nsec = 250000000L;
	}
	else if (active_case == TIME_RESET)
		output->tv_sec = clock_calls == 2 ? 69 : 70;
	else if (active_case != TIME_AFTER_POLL || clock_calls > 2)
		output->tv_sec = 70;
	return 0;
}

static int64_t
observed_channel_write(dssh_channel channel, int stream, const uint8_t *bytes, size_t length)
{
	application_writes++;
	if (cap_channel_writes) {
		if (application_writes == 1)
			return DSSH_ERROR_NOMORE;
		if (length > 2)
			length = 2;
	}
	return dssh_chan_write(channel, stream, bytes, length);
}

static int64_t
observed_channel_read(dssh_channel channel, int stream, uint8_t *data, size_t capacity)
{
	if (server_process) {
		application_reads++;
		if (exchange == SHELL_READ_ERROR)
			return DSSH_ERROR_INIT;
		if (exchange == SHELL_BAD_EXTENT)
			return INT64_MAX;
		if (exchange == SHELL_NOMORE && application_reads == 1)
			return DSSH_ERROR_NOMORE;
		if (exchange == SHELL_CRLF && capacity > 1)
			capacity = 1;
	}
	return dssh_chan_read(channel, stream, data, capacity);
}

static int
observed_channel_poll(dssh_channel channel, int events, int timeout)
{
	if (server_process && events == DSSH_POLL_READ) {
		application_polls++;
		if (exchange == SHELL_POLL_ERROR)
			return DSSH_ERROR_INIT;
		if (exchange == SHELL_POLL_ZERO && application_polls == 1)
			return 0;
	}
	return dssh_chan_poll(channel, events, timeout);
}

static int
observed_kill(pid_t target, int number)
{
	application_signals++;
	signal_arguments_valid &= server_process && target == getppid() && number == SIGTERM;
	if (exchange == SHELL_PARENT_ERROR) {
		errno = EACCES;
		return -1;
	}
	return 0;
}

static bool
one_case(enum io_case which)
{
	int descriptors[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) != 0)
		return false;
	conn_fd = descriptors[0];
	bool valid = true;
	for (unsigned i = 0; i < 2; i++) {
		int flags = fcntl(descriptors[i], F_GETFL);
		valid &= flags >= 0 && fcntl(descriptors[i], F_SETFL, flags | O_NONBLOCK) == 0;
	}
	sess = dssh_session_init(false, 0);
	valid &= sess != NULL;
	uint8_t payload[8192], output[8192];
	memset(payload, 'p', sizeof(payload));
	memset(output, 'X', sizeof(output));
	bool line = which >= LINE_COMPLETE && which <= LINE_ZERO;
	bool receive = which == RX_FULL || which == RX_CANCEL || which == RX_EOF
	    || which == POLL_INTERRUPT || which == POLL_INVALID || which == POLL_NOT_READY
	    || which == RX_AGAIN || which == RX_BAD_EXTENT || line;
	size_t length = which == TX_FULL || which == RX_FULL ? sizeof(payload)
	    : which == TX_CANCEL || which == RX_CANCEL ? 2 : 1;
	if (line) {
		const char *line = which == LINE_ADJACENCY ? "a\rb\nc\r\nZ" : "a\r\nZ";
		length = strlen(line);
		memcpy(payload, line, length);
	}
	if (valid && receive && which != RX_EOF) {
		ssize_t prepared = send(descriptors[1], payload, length, 0);
		valid &= prepared >= 0;
		if (prepared >= 0) {
			size_t actual = (size_t)prepared;
			valid &= actual == length;
		}
	}
	if (which == RX_EOF) {
		valid &= close(descriptors[1]) == 0;
		descriptors[1] = -1;
	}
	active_case = which;
	send_calls = recv_calls = poll_calls = 0;
	clock_calls = 0;
	last_wait = 0;
	bounded = true;
	connection_io = (struct example_socket_io){ .descriptor = conn_fd };
	int initialized = example_socket_reset(&connection_io);
	int result = DSSH_ERROR_INIT;
	if (which == TIME_RESET_FAILURE)
		valid &= initialized == DSSH_ERROR_INIT && !connection_io.timed;
	else
		valid &= initialized == 0;
	if (valid && which == TIME_RESET)
		valid &= example_socket_reset(&connection_io) == 0;
	if (valid && line) {
		size_t received = 99;
		size_t capacity = which == LINE_SHORT ? 2 : which == LINE_ZERO ? 0 : sizeof(output);
		result = rxline(output, capacity, &received, sess, NULL);
		if (which == LINE_ZERO)
			valid &= result == DSSH_ERROR_INVALID && received == 99 && output[0] == 'X';
		else if (which == LINE_SHORT)
			valid &= result == DSSH_ERROR_TOOLONG && received == 0;
		else {
			size_t expected = which == LINE_ADJACENCY ? 7 : 3;
			uint8_t next;
			valid &= result == 0 && received == expected && memcmp(output, payload, expected) == 0;
			valid &= recv(conn_fd, &next, 1, 0) == 1 && next == 'Z';
		}
	}
	else if (valid && which != TIME_RESET_FAILURE) {
		result = receive ? rx(output, length, sess, NULL) : tx(payload, length, sess, NULL);
		if (which == TX_CANCEL || which == RX_CANCEL)
			valid &= result == DSSH_ERROR_TERMINATED;
		else if (which == TX_ZERO)
			valid &= result == DSSH_ERROR_INIT && send_calls == 1;
		else if (which == RX_EOF || which == POLL_INVALID)
			valid &= result == DSSH_ERROR_INIT;
		else if (which == TX_BAD_EXTENT || which == RX_BAD_EXTENT)
			valid &= result == DSSH_ERROR_INVALID;
		else if (which == TIME_EXPIRED || which == TIME_AFTER_POLL)
			valid &= result == DSSH_ERROR_TIMEOUT && send_calls == 0;
		else if (which == TIME_INVALID || which == TIME_FAILURE || which == TIME_BACKWARD)
			valid &= result == DSSH_ERROR_INIT && send_calls == 0;
		else {
			valid &= result == 0;
			if (receive)
				valid &= memcmp(output, payload, length) == 0;
			else {
				ssize_t read_count = recv(descriptors[1], output, length, 0);
				valid &= read_count >= 0;
				if (read_count >= 0) {
					size_t actual = (size_t)read_count;
					valid &= actual == length && memcmp(output, payload, length) == 0;
				}
			}
			if (which == POLL_INTERRUPT || which == POLL_NOT_READY || which == TX_AGAIN || which == RX_AGAIN)
				valid &= poll_calls >= 2;
			if (which == TIME_WAIT || which == TIME_FRACTION)
				valid &= last_wait == (which == TIME_WAIT ? 750 : 250);
		}
	}
	valid &= bounded;
	dssh_session_cleanup(sess);
	sess = NULL;
	valid &= close(conn_fd) == 0;
	conn_fd = -1;
	if (descriptors[1] >= 0)
		valid &= close(descriptors[1]) == 0;
	printf("server_io example=%s case=%d result=%d send=%u recv=%u poll=%u clocks=%u wait=%d valid=%d\n",
	    EXAMPLE_NAME, which, result, send_calls, recv_calls, poll_calls, clock_calls, last_wait, valid);
	return valid;
}

struct game_client {
	unsigned round;
	struct dssh_bytes answer;
};

static void
game_client_release(void *opaque)
{
	(void)opaque;
}

static int
game_client_prompt(const uint8_t *name, size_t name_len, const uint8_t *instruction,
    size_t instruction_len, uint32_t count, const uint8_t **prompts,
    const size_t *lengths, const bool *echo, struct dssh_auth_responses *out, void *opaque)
{
	(void)name;
	(void)name_len;
	(void)instruction;
	(void)instruction_len;
	(void)prompts;
	(void)lengths;
	(void)echo;
	struct game_client *client = opaque;
	static const char *const commands[] = { "n", "answer map", "take key", "s", "use key" };
	if (count != 1 || client->round >= sizeof(commands) / sizeof(commands[0]))
		return DSSH_ERROR_INVALID;
	const char *command = commands[client->round++];
	const uint8_t *bytes = (const uint8_t *)command;
	client->answer = (struct dssh_bytes){ .data = bytes, .len = strlen(command) };
	out->items = &client->answer;
	out->count = 1;
	out->lease.release = game_client_release;
	return 0;
}

/* This fixture communicates only over its own inherited socketpair. */
static dssh_hostkey_decision
game_client_trust(const char *algorithm, unsigned bits, const uint8_t *hash,
    const uint8_t *key, size_t key_len, void *opaque)
{
	(void)algorithm;
	(void)bits;
	(void)hash;
	(void)key;
	(void)key_len;
	(void)opaque;
	return DSSH_HOSTKEY_ACCEPT;
}

static bool
game_roundtrip(enum exchange_case which)
{
	/* Sanitizer exit interceptors may flush inherited stdio buffers. */
	if (fflush(NULL) != 0)
		return false;
	int descriptors[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) != 0)
		return false;
	active_case = TX_FULL;
	exchange = which;
	bool capped = which == EXEC_CAPPED;
	bool shell = which >= SHELL_LINES;
	bool failure = which == SHELL_READ_ERROR || which == SHELL_POLL_ERROR || which == SHELL_BAD_EXTENT || which == SHELL_PARENT_ERROR;
	cap_channel_writes = capped;
	application_reads = application_polls = application_signals = 0;
	signal_arguments_valid = true;
	application_writes = 0;
	send_calls = recv_calls = poll_calls = clock_calls = 0;
	pid_t child = fork();
	if (child < 0) {
		if (close(descriptors[0]) != 0 || close(descriptors[1]) != 0)
			abort();
		return false;
	}
	if (child == 0) {
		if (close(descriptors[0]) != 0)
			_exit(2);
		conn_fd = descriptors[1];
		server_process = true;
		int status = handle_connection();
		bool parent_command = which == SHELL_PARENT || which == SHELL_PARENT_ERROR;
		bool complete = status == (failure ? 1 : 0) && (!capped || application_writes >= 7)
		    && signal_arguments_valid && application_signals == (parent_command ? 1u : 0u);
		if (fprintf(stderr, "server_io_application example=%s profile=%d writes=%u status=%d signals=%u signal_args=%d complete=%d\n",
		    EXAMPLE_NAME, which, application_writes, status, application_signals, signal_arguments_valid, complete) < 0)
			complete = false;
		_exit(complete ? 0 : 1);
	}
	bool valid = close(descriptors[1]) == 0;
	conn_fd = descriptors[0];
	int flags = fcntl(conn_fd, F_GETFL);
	valid &= flags >= 0 && fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK) == 0;
	connection_io = (struct example_socket_io){ .descriptor = conn_fd };
	valid &= example_socket_reset(&connection_io) == 0;
	sess = dssh_session_init(true, 0);
	valid &= sess != NULL;
	dssh_channel channel = NULL;
	struct dssh_chan_params params;
	bool params_owned = false;
	struct game_client client = {0};
	if (valid)
		valid = dssh_session_set_terminate_cb(sess, on_terminate, NULL) == 0
		    && dssh_session_set_hostkey_verify_cb(sess, game_client_trust, NULL) == 0
		    && dssh_transport_handshake(sess) == 0;
	if (valid)
		valid = dssh_auth_keyboard_interactive(sess, "fixture", game_client_prompt, &client) == DSSH_AUTH_SUCCESS
		    && client.round == 5 && dssh_session_start(sess) == 0;
	if (valid) {
		params_owned = dssh_chan_params_init(&params, shell ? DSSH_CHAN_SHELL : DSSH_CHAN_EXEC) == 0;
		valid = params_owned && (shell || dssh_chan_params_set_command(&params, "ping") == 0);
	}
	if (valid) {
		channel = dssh_chan_open(sess, &params);
		valid = channel != NULL;
	}
	if (valid && shell) {
		static const uint8_t lines[] = "ping\rquit\r";
		static const uint8_t crlf[] = "ping\r\nquit\r\n";
		static const uint8_t lf[] = "ping\nexit\n";
		static const uint8_t partial[] = "pin";
		static const uint8_t binary[] = "ping\0ignored\rquit\r";
		static const uint8_t normal_output[] = "> ping\r\npong\r\n> quit\r\nbye\r\n";
		static const uint8_t lf_output[] = "> ping\npong\r\n> exit\nbye\r\n";
		static const uint8_t binary_output[] = "> ping\0ignored\r\ncommands: ping, quit, exit\r\n> quit\r\nbye\r\n";
		uint8_t input[8208], expected[16384], output[16384];
		const uint8_t *bytes = lines;
		size_t length = sizeof(lines) - 1;
		const uint8_t *answer = normal_output;
		size_t answer_length = sizeof(normal_output) - 1;
		if (which == SHELL_CRLF) {
			bytes = crlf;
			length = sizeof(crlf) - 1;
		}
		else if (which == SHELL_LF) {
			bytes = lf;
			length = sizeof(lf) - 1;
			answer = lf_output;
			answer_length = sizeof(lf_output) - 1;
		}
		else if (which == SHELL_PARTIAL) {
			bytes = partial;
			length = sizeof(partial) - 1;
			answer = (const uint8_t *)"> pin";
			answer_length = 5;
		}
		else if (which == SHELL_BINARY) {
			bytes = binary;
			length = sizeof(binary) - 1;
			answer = binary_output;
			answer_length = sizeof(binary_output) - 1;
		}
		else if (which == SHELL_LONG) {
			memset(input, 'x', 8192);
			static const uint8_t suffix[] = "\rping\rquit\r";
			memcpy(input + 8192, suffix, sizeof(suffix) - 1);
			length = 8192 + sizeof(suffix) - 1;
			bytes = input;
			memcpy(expected, "> ", 2);
			memset(expected + 2, 'x', 8192);
			static const uint8_t ending[] = "\r\ncommands: ping, quit, exit\r\n> ping\r\npong\r\n> quit\r\nbye\r\n";
			memcpy(expected + 8194, ending, sizeof(ending) - 1);
			answer_length = 8194 + sizeof(ending) - 1;
			answer = expected;
		}
		if (failure && which != SHELL_PARENT_ERROR) {
			answer = (const uint8_t *)"> ";
			answer_length = 2;
		}
		if (which == SHELL_PARENT || which == SHELL_PARENT_ERROR) {
			bytes = (const uint8_t *)"diediedie\r";
			length = 10;
			answer = (const uint8_t *)"> diediedie\r\ndying...\r\n";
			answer_length = 23;
		}
		/* The injected poll error closes the channel before accepting input.
		 * Sending commands here races that close and can fail legitimately;
		 * this case checks the server's prompt, error status and EOF instead. */
		if (which != SHELL_POLL_ERROR)
			valid = example_channel_write(channel, 0, bytes, length) == 0
			    && dssh_chan_shutwr(channel) == 0;
		size_t used = 0;
		bool eof = false;
		for (unsigned attempt = 0; attempt < 32768 && valid && !eof; attempt++) {
			int64_t received = dssh_chan_read(channel, 0, output + used, sizeof(output) - used);
			if (received == DSSH_ERROR_NOMORE)
				valid = dssh_chan_poll(channel, DSSH_POLL_READ, 100) >= 0;
			else if (received < 0 || received > 16384)
				valid = false;
			else if (received == 0)
				eof = true;
			else {
				size_t count = (size_t)received;
				valid = count <= sizeof(output) - used;
				if (valid)
					used += count;
			}
		}
		valid &= eof && used == answer_length && memcmp(output, answer, answer_length) == 0;
		bool exit_seen = false;
		for (unsigned attempt = 0; attempt < 20 && !exit_seen && valid; attempt++) {
			int events = dssh_chan_poll(channel, DSSH_POLL_EVENT, 100);
			valid = events >= 0;
			if ((events & DSSH_POLL_EVENT) != 0) {
				struct dssh_chan_event event;
				valid = dssh_chan_read_event(channel, &event) == 0;
				if (valid && event.type == DSSH_EVENT_EXIT_STATUS) {
					exit_seen = true;
					valid = event.exit_status.exit_code == (failure ? 1u : 0u);
				}
			}
		}
		valid &= exit_seen;
		printf("server_io_shell example=%s profile=%d input=%zu output=%zu expected=%zu eof=%d exit=%d valid=%d\n",
		    EXAMPLE_NAME, which, length, used, answer_length, eof, exit_seen, valid);
	}
	for (int stream = 0; stream < 2 && valid && !shell; stream++) {
		uint8_t output[5];
		size_t used = 0;
		for (unsigned attempt = 0; attempt < 50 && used < sizeof(output) && valid; attempt++) {
			int64_t received = dssh_chan_read(channel, stream, &output[used], sizeof(output) - used);
			printf("server_io_read example=%s stream=%d result=%" PRId64 " used=%zu\n",
			    EXAMPLE_NAME, stream, received, used);
			if (received == DSSH_ERROR_NOMORE)
				valid = dssh_chan_poll(channel, stream == 0 ? DSSH_POLL_READ : DSSH_POLL_READEXT, 100) >= 0;
			else if (received <= 0 || received > 5)
				valid = false;
			else if (received > 0) {
				size_t advanced = (size_t)received;
				valid = advanced <= sizeof(output) - used;
				if (valid)
					used += advanced;
			}
		}
		valid &= used == sizeof(output) && memcmp(output, stream == 0 ? "pong\n" : "gnop\n", sizeof(output)) == 0;
	}
	if (params_owned)
		dssh_chan_params_free(&params);
	if (channel != NULL) {
		int closed = dssh_chan_close(channel, -1);
		printf("server_io_close example=%s result=%d valid_before=%d\n", EXAMPLE_NAME, closed, valid);
		valid &= closed == 0;
	}
	if (shutdown(conn_fd, SHUT_RDWR) != 0 && errno != ENOTCONN)
		valid = false;
	dssh_session_cleanup(sess);
	sess = NULL;
	valid &= close(conn_fd) == 0;
	conn_fd = -1;
	int status;
	pid_t waited;
	do {
		waited = waitpid(child, &status, 0);
	} while (waited < 0 && errno == EINTR);
	valid &= waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
	if (waited == child)
		printf("server_io_child example=%s status=%d\n", EXAMPLE_NAME, status);
	intmax_t child_id = (intmax_t)waited;
	printf("server_io_roundtrip example=%s profile=%d rounds=%u child=%" PRIdMAX " valid=%d\n",
	    EXAMPLE_NAME, which, client.round, child_id, valid);
	return valid;
}

static bool
shell_parser_cases(void)
{
	bool valid = true;
	for (unsigned value = 0; value < 256; value++) {
		uint8_t byte = (uint8_t)value;
		struct example_shell_line line = EXAMPLE_SHELL_INITIALIZER;
		enum example_shell_event first = example_shell_byte(&line, byte);
		bool newline = byte == '\r' || byte == '\n';
		valid &= first == (newline ? EXAMPLE_SHELL_EMPTY : EXAMPLE_SHELL_DATA);
		valid &= example_shell_byte(&line, '\r') == (newline ? EXAMPLE_SHELL_EMPTY : EXAMPLE_SHELL_UNKNOWN);
		valid &= example_shell_byte(&line, '\n') == EXAMPLE_SHELL_SKIP_LF;
	}
	static const struct {
		const uint8_t *bytes;
		size_t length;
		enum example_shell_event expected;
	} cases[] = {
		{ (const uint8_t *)"ping", 4, EXAMPLE_SHELL_PING },
		{ (const uint8_t *)"quit", 4, EXAMPLE_SHELL_QUIT },
		{ (const uint8_t *)"exit", 4, EXAMPLE_SHELL_QUIT },
		{ (const uint8_t *)"diediedie", 9, EXAMPLE_SHELL_STOP_PARENT },
		{ (const uint8_t *)"pingx", 5, EXAMPLE_SHELL_UNKNOWN },
		{ (const uint8_t *)"ping\0x", 6, EXAMPLE_SHELL_UNKNOWN },
		{ (const uint8_t *)"\0ping", 5, EXAMPLE_SHELL_UNKNOWN },
		{ (const uint8_t *)"", 0, EXAMPLE_SHELL_EMPTY },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct example_shell_line line = EXAMPLE_SHELL_INITIALIZER;
		for (size_t j = 0; j < cases[i].length; j++)
			valid &= example_shell_byte(&line, cases[i].bytes[j]) == EXAMPLE_SHELL_DATA;
		valid &= example_shell_byte(&line, '\n') == cases[i].expected;
	}
	struct example_shell_line line = EXAMPLE_SHELL_INITIALIZER;
	for (uint32_t i = 0; i < UINT32_C(65536); i++) {
		valid &= example_shell_byte(&line, 'x') == EXAMPLE_SHELL_DATA;
		valid &= line.position == 0 && line.candidates == 0;
	}
	valid &= example_shell_byte(&line, '\n') == EXAMPLE_SHELL_UNKNOWN;
	const uint8_t ping[] = "ping\r\n";
	for (size_t i = 0; i < sizeof(ping) - 1; i++) {
		enum example_shell_event expected = i < 4 ? EXAMPLE_SHELL_DATA
		    : i == 4 ? EXAMPLE_SHELL_PING : EXAMPLE_SHELL_SKIP_LF;
		valid &= example_shell_byte(&line, ping[i]) == expected;
	}
	printf("server_shell_parser octets=256 long_bytes=65536 valid=%d\n", valid);
	return valid;
}

int
main(int argc, char **argv)
{
	bool baseline = argc == 2 && strcmp(argv[1], "--baseline") == 0;
	if (argc > 1 && !baseline)
		return 2;
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || dssh_transport_set_callbacks(tx, rx, rxline, NULL) != 0
	    || dssh_register_curve25519_sha256() != 0 || dssh_register_ssh_ed25519() != 0
	    || dssh_register_aes256_ctr() != 0 || dssh_register_hmac_sha2_256() != 0
	    || dssh_register_none_comp() != 0 || dssh_ed25519_generate_key() != 0)
		return 2;
	unsigned failures = shell_parser_cases() ? 0u : 1u;
	for (enum io_case which = TX_FULL; which < IO_CASES; which++) {
		if (!one_case(which))
			failures++;
	}
	bool integration = true;
	unsigned exchanges = 0;
	for (enum exchange_case which = EXEC_NATIVE; which < EXCHANGE_CASES; which++) {
		/* Do not feed an impossible oversized read result to the old parser. */
		if (baseline && which == SHELL_BAD_EXTENT)
			continue;
		integration &= game_roundtrip(which);
		exchanges++;
	}
	printf("server_io_exchanges example=%s cases=%u valid=%d\n", EXAMPLE_NAME, exchanges, integration);
	if (!integration)
		failures++;
	printf("server_io example=%s cases=%d failures=%u integration=%d\n", EXAMPLE_NAME, IO_CASES, failures, integration);
	return failures == 0 ? 0 : 1;
}
