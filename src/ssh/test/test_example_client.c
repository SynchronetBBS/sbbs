/* Exercise the actual POSIX example against production libraries. Native I/O
 * faults are local to the included application; library calls are not wrapped. */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "dssh_test.h"

struct example_io {
	bool enabled;
	bool interrupt;
	short events;
	unsigned polls, sends, receives, writes;
	int transfer_errno, write_errno;
	bool zero_transfer, zero_write;
	uint8_t bytes[64];
	size_t length, offset;
};
static struct example_io io;
static bool deny_resolution;
static unsigned resolutions;
struct credential_io {
	bool enabled, interrupt, again, read_error, close_error, close_interrupt, alloc_error, poll_interrupt, poll_error;
	int descriptor;
	size_t chunk;
	void *allocation;
	unsigned allocations, frees, closes, reads, polls;
	bool valid;
};
static struct credential_io credential;
static atomic_bool credential_writer_ready = ATOMIC_VAR_INIT(false);
static bool credential_native_pipe;
static ssize_t example_read(int, void *, size_t);
static int example_close(int);
static int example_start(dssh_session);
static void *example_malloc(size_t);
static void example_free(void *);

static int example_poll(struct pollfd *, nfds_t, int);
static ssize_t example_send(int, const void *, size_t, int);
static ssize_t example_recv(int, void *, size_t, int);
static ssize_t example_write(int, const void *, size_t);
static int example_getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **);

#define read example_read
#define close example_close
#define malloc example_malloc
#define free example_free
#define dssh_session_start example_start
#define main client_example_main
#define poll example_poll
#define send example_send
#define recv example_recv
#define write example_write
#define getaddrinfo example_getaddrinfo
#include "../examples/client.c"
#undef free
#undef malloc
#undef close
#undef read
#undef getaddrinfo
#undef write
#undef recv
#undef send
#undef poll
#undef main
#undef dssh_session_start

static void *
example_malloc(size_t size)
{
	if (credential.enabled && size == PASSWORD_LIMIT + 3) {
		credential.allocations++;
		if (credential.alloc_error)
			return NULL;
		if (credential.allocation != NULL)
			credential.valid = false;
		credential.allocation = malloc(size);
		return credential.allocation;
	}
	return malloc(size);
}

static void
example_free(void *value)
{
	if (credential.enabled && value != NULL && value == credential.allocation) {
		const unsigned char *bytes = value;
		for (size_t i = 0; i < PASSWORD_LIMIT + 3; i++) {
			if (bytes[i] != 0)
				credential.valid = false;
		}
		credential.frees++;
		credential.allocation = NULL;
	}
	free(value);
}

static ssize_t
example_read(int fd, void *bytes, size_t size)
{
	if (credential.enabled && fd == credential.descriptor) {
		credential.reads++;
		if (credential.interrupt) {
			credential.interrupt = false;
			errno = EINTR;
			return -1;
		}
		if (credential.again) {
			credential.again = false;
			errno = EAGAIN;
			return -1;
		}
		if (credential.read_error) {
			errno = EIO;
			return -1;
		}
		if (credential.chunk > 0 && size > credential.chunk)
			size = credential.chunk;
	}
	ssize_t result = read(fd, bytes, size);
	if (credential_native_pipe && fd == credential.descriptor && result < 0 && errno == EAGAIN)
		atomic_store_explicit(&credential_writer_ready, true, memory_order_release);
	return result;
}

static int
example_start(dssh_session session)
{
	if (credential.enabled && (credential.allocation != NULL || credential.frees != 1))
		credential.valid = false;
	return dssh_session_start(session);
}

static int
example_close(int fd)
{
	int ret = close(fd);
	if (credential.enabled && fd == credential.descriptor && credential.closes == 0) {
		credential.closes++;
		if (ret != 0)
			credential.valid = false;
		if (credential.close_error || credential.close_interrupt) {
			errno = credential.close_interrupt ? EINTR : EIO;
			return -1;
		}
	}
	return ret;
}

static int
example_poll(struct pollfd *fds, nfds_t count, int timeout)
{
	if (credential.enabled && count == 1 && fds[0].fd == credential.descriptor) {
		credential.polls++;
		if (credential.poll_interrupt) {
			credential.poll_interrupt = false;
			errno = EINTR;
			return -1;
		}
		if (credential.poll_error) {
			errno = EIO;
			return -1;
		}
	}
	if (!io.enabled)
		return poll(fds, count, timeout);
	io.polls++;
	if (io.interrupt) {
		io.interrupt = false;
		errno = EINTR;
		return -1;
	}
	if (count != 1 || fds[0].fd != sock) {
		errno = EINVAL;
		return -1;
	}
	fds[0].revents = io.events;
	return 1;
}

static ssize_t
example_send(int fd, const void *buf, size_t length, int flags)
{
	if (!io.enabled)
		return send(fd, buf, length, flags);
	io.sends++;
	if (io.transfer_errno != 0) {
		errno = io.transfer_errno;
		io.transfer_errno = 0;
		return -1;
	}
	if (io.zero_transfer)
		return 0;
	if (fd != sock || length > sizeof(io.bytes) - io.length) {
		errno = EIO;
		return -1;
	}
	/* Force successful short writes; the application must retain the suffix. */
	size_t count = length > 3 ? 3 : length;
	memcpy(&io.bytes[io.length], buf, count);
	io.length += count;
	ssize_t result = (ssize_t)count;
	return result;
}

static ssize_t
example_recv(int fd, void *buf, size_t length, int flags)
{
	if (!io.enabled)
		return recv(fd, buf, length, flags);
	io.receives++;
	if (io.transfer_errno != 0) {
		errno = io.transfer_errno;
		io.transfer_errno = 0;
		return -1;
	}
	if (io.zero_transfer)
		return 0;
	if (fd != sock || io.offset > io.length) {
		errno = EIO;
		return -1;
	}
	size_t remaining = io.length - io.offset;
	size_t count = length < remaining ? length : remaining;
	if (count > 3)
		count = 3;
	if (count > 0)
		memcpy(buf, &io.bytes[io.offset], count);
	io.offset += count;
	ssize_t result = (ssize_t)count;
	return result;
}

static ssize_t
example_write(int fd, const void *buf, size_t length)
{
	if (!io.enabled)
		return write(fd, buf, length);
	io.writes++;
	if (io.write_errno != 0) {
		errno = io.write_errno;
		io.write_errno = 0;
		return -1;
	}
	if (io.zero_write)
		return 0;
	if (fd != sock || io.length > sizeof(io.bytes) || length > sizeof(io.bytes) - io.length) {
		errno = EIO;
		return -1;
	}
	size_t count = length > 3 ? 3 : length;
	memcpy(&io.bytes[io.length], buf, count);
	io.length += count;
	ssize_t result = (ssize_t)count;
	return result;
}

static int
example_getaddrinfo(const char *host, const char *port, const struct addrinfo *hints,
    struct addrinfo **result)
{
	if (credential.enabled && (credential.closes != 1 || credential.allocation == NULL))
		credential.valid = false;
	if (!deny_resolution)
		return getaddrinfo(host, port, hints, result);
	resolutions++;
	*result = NULL;
	return EAI_FAIL;
}

static int
begin_io(short events)
{
	io = (struct example_io){ .enabled = true, .events = events };
	sock = 17; /* Never used by a native syscall while io.enabled is true. */
	sess = dssh_session_init(true, 0);
	return sess != NULL ? 0 : -1;
}

static void
end_io(void)
{
	dssh_session_cleanup(sess);
	sess = NULL;
	sock = -1;
	io.enabled = false;
}

static int
test_host_pin(void)
{
	static const char matching[] = "SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	static const char different[] = "SHA256:BAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	uint8_t hash[32] = {0};
	uint8_t blob = 1;
	/* These are callback-format controls; native peer tests supply real keys. */
	void *matching_data = (void *)matching;
	void *different_data = (void *)different;
	dssh_hostkey_decision accepted = verify_hostkey("ssh-ed25519", 256, hash, &blob, 1, matching_data);
	dssh_hostkey_decision rejected = verify_hostkey("rsa-sha2-256", 2048, hash, &blob, 1, different_data);
	dssh_hostkey_decision missing = verify_hostkey("rsa-sha2-512", 2048, hash, &blob, 1, NULL);
	ASSERT_EQ(accepted, DSSH_HOSTKEY_ACCEPT);
	ASSERT_EQ(rejected, DSSH_HOSTKEY_REJECT);
	ASSERT_EQ(missing, DSSH_HOSTKEY_REJECT);
	return TEST_PASS;
}

static int
test_tx_interruption(void)
{
	ASSERT_OK(begin_io(POLLOUT));
	io.interrupt = true;
	uint8_t bytes[] = {1, 2, 3, 4, 5, 6, 7};
	int ret = tx(bytes, sizeof(bytes), sess, NULL);
	bool valid = ret == 0 && io.polls == 4 && io.sends == 3
	    && io.length == sizeof(bytes) && memcmp(io.bytes, bytes, sizeof(bytes)) == 0;
	end_io();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_rx_interruption(void)
{
	ASSERT_OK(begin_io(POLLIN));
	io.interrupt = true;
	io.length = 7;
	for (size_t i = 0; i < io.length; i++)
		io.bytes[i] = (uint8_t)i;
	uint8_t bytes[7];
	int ret = rx(bytes, sizeof(bytes), sess, NULL);
	bool valid = ret == 0 && io.polls == 4 && io.receives == 3
	    && io.offset == sizeof(bytes) && memcmp(io.bytes, bytes, sizeof(bytes)) == 0;
	end_io();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_empty_line_buffer(void)
{
	ASSERT_OK(begin_io(POLLIN));
	io.bytes[0] = 'x';
	io.length = 1;
	uint8_t guard = 0xa5;
	size_t received = 0;
	int ret = rxline(&guard, 0, &received, sess, NULL);
	bool valid = ret == DSSH_ERROR_INVALID && guard == 0xa5 && received == 0
	    && io.polls == 0 && io.receives == 0;
	end_io();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_invalid_poll_descriptor(void)
{
	ASSERT_OK(begin_io(POLLNVAL));
	uint8_t byte = 1;
	int ret = tx(&byte, 1, sess, NULL);
	bool valid = ret == DSSH_ERROR_INIT && io.polls == 1 && io.sends == 0;
	io.polls = 0;
	ret = rx(&byte, 1, sess, NULL);
	valid = valid && ret == DSSH_ERROR_INIT && io.polls == 1 && io.receives == 0;
	end_io();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* Fixed vectors were independently generated with Python's standard base64
 * encoder for 31 zero bytes followed by values 0..15. */
static int
test_fingerprint_classes(void)
{
	static const char *const vectors[] = {
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAI",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAM",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQ",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAU",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAY",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAc",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAg",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAk",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAo",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAs",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAw",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA4",
		"SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA8",
	};
	bool valid = true;
	uint8_t hash[32] = {0}, blob = 1;
	for (size_t i = 0; i < 16; i++) {
		uint8_t tail = (uint8_t)i;
		hash[31] = tail;
		void *pin = (void *)vectors[i];
		valid = valid_fingerprint(vectors[i]) && valid;
		valid = verify_hostkey("ssh-ed25519", 256, hash, &blob, 1, pin) == DSSH_HOSTKEY_ACCEPT && valid;
		char noncanonical[51];
		memcpy(noncanonical, vectors[i], sizeof(noncanonical));
		noncanonical[49]++;
		valid = !valid_fingerprint(noncanonical) && valid;
	}
	static const char *const rejected[] = {"", "SHA1:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
	    "SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
	    "SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA-",
	    "sha256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"};
	for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++)
		valid = !valid_fingerprint(rejected[i]) && valid;
	void *pin = (void *)vectors[0];
	valid = !valid_fingerprint(NULL) && valid;
	valid = verify_hostkey(NULL, 256, hash, &blob, 1, pin) == DSSH_HOSTKEY_REJECT && valid;
	valid = verify_hostkey("ssh-ed25519", 256, NULL, &blob, 1, pin) == DSSH_HOSTKEY_REJECT && valid;
	valid = verify_hostkey("ssh-ed25519", 256, hash, NULL, 1, pin) == DSSH_HOSTKEY_REJECT && valid;
	valid = verify_hostkey("ssh-ed25519", 256, hash, &blob, 0, pin) == DSSH_HOSTKEY_REJECT && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_port_values(void)
{
	static const struct { const char *text; unsigned expected; } good[] = {
		{"1", 1}, {"22", 22}, {"00022", 22}, {"65535", 65535},
	};
	bool valid = true;
	for (size_t i = 0; i < sizeof(good) / sizeof(good[0]); i++) {
		uint16_t port = 0;
		valid = parse_port(good[i].text, &port) == 0 && port == good[i].expected && valid;
	}
	static const char *const bad[] = {NULL, "", "0", "65536", "99999999999999999999999",
	    "-1", "+22", " 22", "22 ", "22x"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		uint16_t port = 123;
		valid = parse_port(bad[i], &port) != 0 && port == 123 && valid;
	}
	valid = parse_port("22", NULL) != 0 && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_method_tokens(void)
{
	ASSERT_TRUE(offered_method("publickey,password,keyboard-interactive", "password"));
	ASSERT_TRUE(offered_method("publickey,password,keyboard-interactive", "keyboard-interactive"));
	ASSERT_TRUE(offered_method("password", "password"));
	ASSERT_FALSE(offered_method("notpassword,password2", "password"));
	ASSERT_FALSE(offered_method("keyboard-interactive-other", "keyboard-interactive"));
	ASSERT_FALSE(offered_method("", "password"));
	ASSERT_FALSE(offered_method(",,,", "password"));
	return TEST_PASS;
}

/* Duplicate a native temporary-file descriptor so the application's close can
 * be checked independently of the FILE owner. No large prefilled pipe can block. */
static int
credential_file(const void *bytes, size_t size)
{
	FILE *file = tmpfile();
	if (file == NULL)
		return -1;
	bool valid = size == 0 || fwrite(bytes, 1, size, file) == size;
	valid = fflush(file) == 0 && valid;
	valid = fseek(file, 0, SEEK_SET) == 0 && valid;
	int fd = valid ? fcntl(fileno(file), F_DUPFD, 3) : -1;
	if (fclose(file) != 0) {
		if (fd >= 0 && close(fd) != 0)
			abort();
		return -1;
	}
	return fd;
}

static int
test_argument_validation(void)
{
	char pin[] = "SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	char descriptor[32];
	int fd = credential_file("password\n", 9);
	ASSERT_TRUE(fd >= 3);
	int written = snprintf(descriptor, sizeof(descriptor), "%d", fd);
	bool valid = written > 0 && written < 32;
	char *args[] = {"client", pin, "audit", descriptor, "127.0.0.1", "22", "cmd", NULL};
	deny_resolution = true;
	const int bad_counts[] = {0, 1, 2, 3, 8};
	for (size_t i = 0; i < sizeof(bad_counts) / sizeof(bad_counts[0]); i++) {
		resolutions = 0;
		valid = client_example_main(bad_counts[i], args) == 1 && resolutions == 0 && valid;
	}
	resolutions = 0;
	args[1] = "bad";
	valid = client_example_main(7, args) == 1 && resolutions == 0 && valid;
	args[1] = pin;
	args[2] = "";
	valid = client_example_main(7, args) == 1 && resolutions == 0 && valid;
	args[2] = "audit";
	args[4] = "";
	valid = client_example_main(7, args) == 1 && resolutions == 0 && valid;
	args[4] = "127.0.0.1";
	static const char *const bad_ports[] = {"0", "65536", "-1", "+22", "22x", ""};
	for (size_t i = 0; i < sizeof(bad_ports) / sizeof(bad_ports[0]); i++) {
		char *text = (char *)bad_ports[i];
		args[5] = text;
		valid = client_example_main(7, args) == 1 && resolutions == 0 && valid;
	}
	args[5] = "65535";
	/* Rejected argument sets do not consume or close the supplied descriptor. */
	valid = fcntl(fd, F_GETFD) >= 0 && valid;
	valid = client_example_main(7, args) == 1 && resolutions == 1 && valid;
	valid = fcntl(fd, F_GETFD) == -1 && errno == EBADF && valid;
	fd = credential_file("", 0);
	written = snprintf(descriptor, sizeof(descriptor), "%d", fd);
	valid = fd >= 3 && written > 0 && written < 32 && valid;
	resolutions = 0;
	valid = client_example_main(4, args) == 1 && resolutions == 1 && valid;
	valid = fcntl(fd, F_GETFD) == -1 && errno == EBADF && valid;
	deny_resolution = false;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_password_descriptor(void)
{
	const char *bad[] = {NULL, "", "0", "1", "2", "-3", "+3", " 3", "3 ", "3x", "999999999999999999999999999999999"};
	int fd = -1;
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		ASSERT_TRUE(parse_password_descriptor(bad[i], &fd) != 0);
		ASSERT_EQ(fd, -1);
	}
	ASSERT_OK(parse_password_descriptor("3", &fd));
	ASSERT_EQ(fd, 3);
	ASSERT_OK(parse_password_descriptor("003", &fd));
	ASSERT_EQ(fd, 3);
	char text[64];
	int n = snprintf(text, sizeof(text), "%d", INT_MAX);
	ASSERT_TRUE(n > 0 && n < 64);
	ASSERT_OK(parse_password_descriptor(text, &fd));
	ASSERT_EQ(fd, INT_MAX);
	uintmax_t over = INT_MAX;
	over++;
	n = snprintf(text, sizeof(text), "%ju", over);
	ASSERT_TRUE(n > 0 && n < 64);
	ASSERT_TRUE(parse_password_descriptor(text, &fd) != 0);
	return TEST_PASS;
}

static bool
credential_case(const void *bytes, size_t size, const void *expected, size_t length,
    bool accept, unsigned mode)
{
	int fd = credential_file(bytes, size);
	if (fd < 3)
		return false;
	credential = (struct credential_io){.enabled = true, .descriptor = fd, .valid = true};
	credential.chunk = mode == 1 ? 1 : 0;
	credential.interrupt = mode == 2;
	credential.again = mode == 3 || mode == 8 || mode == 9;
	credential.read_error = mode == 4;
	credential.close_error = mode == 5;
	credential.alloc_error = mode == 6;
	credential.close_interrupt = mode == 7;
	credential.poll_interrupt = mode == 8;
	credential.poll_error = mode == 9;
	struct password_input input = {0};
	int ret = password_read(fd, &input);
	bool valid = (ret == 0) == accept && credential.closes == 1;
	if (accept) {
		valid = input.text != NULL && strlen(input.text) == length
		    && memcmp(input.text, expected, length) == 0 && valid;
	}
	else
		valid = input.text == NULL && input.capacity == 0 && valid;
	password_clear(&input);
	password_clear(&input);
	valid = credential.valid && credential.allocations == 1
	    && credential.frees == (mode == 6 ? 0U : 1U) && credential.allocation == NULL
	    && fcntl(fd, F_GETFD) == -1 && errno == EBADF && valid;
	if (mode == 2)
		valid = credential.reads >= 2 && valid;
	if (mode == 3 || mode == 8 || mode == 9)
		valid = credential.polls == (mode == 8 ? 2U : 1U) && valid;
	credential.enabled = false;
	return valid;
}

static int
test_password_lines(void)
{
	ASSERT_TRUE(credential_case("", 0, "", 0, true, 0));
	ASSERT_TRUE(credential_case("\n", 1, "", 0, true, 0));
	ASSERT_TRUE(credential_case("\r\n", 2, "", 0, true, 0));
	static const char bytes[] = " space \303\251\377\r";
	ASSERT_TRUE(credential_case(bytes, sizeof(bytes) - 1, bytes, sizeof(bytes) - 1, true, 0));
	ASSERT_TRUE(credential_case("pass\r\nignored\0bytes", 19, "pass", 4, true, 0));
	ASSERT_TRUE(credential_case("pass\n", 5, "pass", 4, true, 1));
	ASSERT_TRUE(credential_case("a\0b\n", 4, NULL, 0, false, 0));
	ASSERT_TRUE(credential_case("a\0b", 3, NULL, 0, false, 0));
	return TEST_PASS;
}

static int
test_password_bounds(void)
{
	char *bytes = malloc(PASSWORD_LIMIT + 4);
	ASSERT_NOT_NULL(bytes);
	memset(bytes, 'x', PASSWORD_LIMIT + 4);
	bool valid = credential_case(bytes, PASSWORD_LIMIT, bytes, PASSWORD_LIMIT, true, 0);
	bytes[PASSWORD_LIMIT] = '\n';
	valid = credential_case(bytes, PASSWORD_LIMIT + 1, bytes, PASSWORD_LIMIT, true, 0) && valid;
	bytes[PASSWORD_LIMIT] = '\r';
	bytes[PASSWORD_LIMIT + 1] = '\n';
	valid = credential_case(bytes, PASSWORD_LIMIT + 2, bytes, PASSWORD_LIMIT, true, 0) && valid;
	valid = credential_case(bytes, PASSWORD_LIMIT + 1, NULL, 0, false, 0) && valid;
	memset(bytes, 'x', PASSWORD_LIMIT + 4);
	valid = credential_case(bytes, PASSWORD_LIMIT + 1, NULL, 0, false, 0) && valid;
	bytes[PASSWORD_LIMIT + 1] = '\n';
	valid = credential_case(bytes, PASSWORD_LIMIT + 2, NULL, 0, false, 0) && valid;
	bytes[PASSWORD_LIMIT + 1] = '\r';
	bytes[PASSWORD_LIMIT + 2] = '\n';
	valid = credential_case(bytes, PASSWORD_LIMIT + 3, NULL, 0, false, 0) && valid;
	free(bytes);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_password_failures(void)
{
	for (unsigned mode = 2; mode <= 9; mode++) {
		bool accept = mode == 2 || mode == 3 || mode == 8;
		ASSERT_TRUE(credential_case("password\n", 9, "password", 8, accept, mode));
	}
	struct password_input input = {0};
	int unreadable = open("/dev/null", O_WRONLY);
	ASSERT_TRUE(unreadable >= 3);
	/* A descriptor opened write-only rejects native read on POSIX platforms. */
	ASSERT_TRUE(password_read(unreadable, &input) != 0);
	ASSERT_NULL(input.text);
	ASSERT_TRUE(fcntl(unreadable, F_GETFD) == -1 && errno == EBADF);
	int closed = credential_file("", 0);
	ASSERT_TRUE(closed >= 3);
	ASSERT_OK(close(closed));
	ASSERT_TRUE(password_read(closed, &input) != 0);
	ASSERT_NULL(input.text);
	return TEST_PASS;
}

static int
credential_pipe_writer(void *data)
{
	const int *descriptor = data;
	while (!atomic_load_explicit(&credential_writer_ready, memory_order_acquire))
		thrd_yield();
	const char text[] = "pipe password\r\n";
	size_t used = 0;
	int result = 0;
	while (used < sizeof(text) - 1) {
		ssize_t n = write(*descriptor, &text[used], sizeof(text) - 1 - used);
		if (n > 0 && n <= 15) {
			size_t count = (size_t)n;
			if (count > sizeof(text) - 1 - used) {
				result = -1;
				break;
			}
			used += count;
		}
		else if (n < 0 && errno == EINTR)
			continue;
		else {
			result = -1;
			break;
		}
	}
	if (close(*descriptor) != 0)
		result = -1;
	return result;
}

static int
test_password_native_pipe(void)
{
	int pipefd[2];
	ASSERT_OK(pipe(pipefd));
	int flags = fcntl(pipefd[0], F_GETFL);
	bool valid = flags >= 0 && fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) == 0;
	int fd = fcntl(pipefd[0], F_DUPFD, 3);
	if (!valid || fd < 3) {
		if (fd >= 0 && close(fd) != 0)
			abort();
		if (close(pipefd[0]) != 0 || close(pipefd[1]) != 0)
			abort();
		return TEST_FAIL;
	}
	credential = (struct credential_io){.enabled = true, .descriptor = fd, .valid = true};
	atomic_store_explicit(&credential_writer_ready, false, memory_order_release);
	credential_native_pipe = true;
	thrd_t writer;
	int created = thrd_create(&writer, credential_pipe_writer, &pipefd[1]);
	struct password_input input = {0};
	if (created == thrd_success) {
		int ret = password_read(fd, &input);
		/* Release the writer even if an allocation failed before the native read. */
		atomic_store_explicit(&credential_writer_ready, true, memory_order_release);
		int result;
		if (thrd_join(writer, &result) != thrd_success)
			abort();
		valid = ret == 0 && result == 0 && input.text != NULL
		    && strcmp(input.text, "pipe password") == 0 && credential.polls >= 1 && valid;
	}
	else {
		valid = false;
		if (close(fd) != 0 || close(pipefd[1]) != 0)
			abort();
	}
	password_clear(&input);
	int after = fcntl(pipefd[0], F_GETFL);
	valid = after == (flags | O_NONBLOCK) && credential.valid && credential.closes == 1
	    && credential.allocations == 1 && credential.frees == 1 && valid;
	credential_native_pipe = false;
	credential.enabled = false;
	valid = close(pipefd[0]) == 0 && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_password_setup(void)
{
	char pin[] = "SHA256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	char descriptor[32];
	char *args[] = {"client", pin, "audit", descriptor, NULL};
	bool valid = true;
	deny_resolution = true;
	for (unsigned mode = 0; mode < 5; mode++) {
		int fd = credential_file(mode == 1 ? "a\0b" : "pass\n", mode == 1 ? 3 : 5);
		if (fd < 3) {
			valid = false;
			break;
		}
		int n = snprintf(descriptor, sizeof(descriptor), "%d", fd);
		if (n < 0 || n >= 32) {
			if (close(fd) != 0)
				abort();
			valid = false;
			break;
		}
		credential = (struct credential_io){.enabled = true, .descriptor = fd, .valid = true};
		credential.close_error = mode == 2;
		credential.read_error = mode == 3;
		credential.alloc_error = mode == 4;
		resolutions = 0;
		int ret = client_example_main(4, args);
		valid = ret == 1 && credential.valid && credential.closes == 1
		    && credential.allocations == 1 && credential.frees == (mode == 4 ? 0U : 1U) && credential.allocation == NULL
		    && resolutions == (mode == 0 ? 1U : 0U) && valid;
		credential.enabled = false;
	}
	deny_resolution = false;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_output_transfers(void)
{
	const uint8_t bytes[] = {1, 2, 3, 4, 5, 6, 7};
	const int errors[] = {0, EINTR, EAGAIN, EIO};
	for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
		ASSERT_OK(begin_io(POLLOUT));
		io.write_errno = errors[i];
		int ret = write_output(sock, bytes, sizeof(bytes));
		bool valid = errors[i] == EIO ? ret != 0 && io.length == 0 && io.writes == 1
		    : ret == 0 && io.length == sizeof(bytes) && memcmp(io.bytes, bytes, sizeof(bytes)) == 0;
		valid = valid && io.polls == (errors[i] == EAGAIN ? 1U : 0U);
		end_io();
		ASSERT_TRUE(valid);
	}
	ASSERT_OK(begin_io(POLLOUT));
	io.zero_write = true;
	int ret = write_output(sock, bytes, sizeof(bytes));
	bool valid = ret != 0 && io.writes == 1 && io.length == 0;
	end_io();
	ASSERT_TRUE(valid);
	ASSERT_OK(begin_io(POLLNVAL));
	io.write_errno = EAGAIN;
	ret = write_output(sock, bytes, sizeof(bytes));
	valid = ret != 0 && io.writes == 1 && io.polls == 1 && io.length == 0;
	end_io();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_native_transfer_errors(void)
{
	const int errors[] = {EINTR, EAGAIN, EIO};
	for (unsigned receiving = 0; receiving < 2; receiving++) {
		for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
			ASSERT_OK(begin_io(receiving != 0 ? POLLIN : POLLOUT));
			io.transfer_errno = errors[i];
			uint8_t bytes[7] = {1, 2, 3, 4, 5, 6, 7};
			if (receiving != 0) {
				memcpy(io.bytes, bytes, sizeof(bytes));
				io.length = sizeof(bytes);
			}
			int ret = receiving != 0 ? rx(bytes, sizeof(bytes), sess, NULL) : tx(bytes, sizeof(bytes), sess, NULL);
			bool valid = errors[i] == EIO ? ret == DSSH_ERROR_INIT : ret == 0;
			valid = valid && (errors[i] == EIO || memcmp(io.bytes, bytes, sizeof(bytes)) == 0);
			end_io();
			ASSERT_TRUE(valid);
		}
		ASSERT_OK(begin_io(receiving != 0 ? POLLIN : POLLOUT));
		io.zero_transfer = true;
		uint8_t byte = 1;
		int ret = receiving != 0 ? rx(&byte, 1, sess, NULL) : tx(&byte, 1, sess, NULL);
		end_io();
		ASSERT_EQ(ret, DSSH_ERROR_INIT);
	}
	return TEST_PASS;
}

static int
test_line_framing(void)
{
	static const uint8_t input[] = "a\rx\nb\r\n";
	for (size_t capacity = sizeof(input) - 2; capacity < sizeof(input); capacity++) {
		ASSERT_OK(begin_io(POLLIN));
		memcpy(io.bytes, input, sizeof(input) - 1);
		io.length = sizeof(input) - 1;
		uint8_t bytes[9];
		memset(bytes, 0xa5, sizeof(bytes));
		size_t received = 0;
		int ret = rxline(bytes, capacity, &received, sess, NULL);
		bool complete = capacity == sizeof(input) - 1;
		bool valid = bytes[capacity] == 0xa5 && (complete
		    ? ret == 0 && received == sizeof(input) - 1 && memcmp(bytes, input, received) == 0
		    : ret == DSSH_ERROR_TOOLONG && received == 0);
		end_io();
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_termination(void)
{
	ASSERT_OK(begin_io(POLLIN));
	bool valid = dssh_session_terminate(sess);
	uint8_t byte = 1;
	valid = tx(&byte, 1, sess, NULL) == DSSH_ERROR_TERMINATED && valid;
	valid = rx(&byte, 1, sess, NULL) == DSSH_ERROR_TERMINATED && valid;
	valid = tx(NULL, 0, sess, NULL) == 0 && rx(NULL, 0, sess, NULL) == 0 && valid;
	valid = io.polls == 0 && io.sends == 0 && io.receives == 0 && valid;
	end_io();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_invalid_callback_spans(void)
{
	ASSERT_OK(begin_io(POLLIN));
	uint8_t byte = 1;
	size_t received = 0;
	bool valid = tx(NULL, 1, sess, NULL) == DSSH_ERROR_INVALID;
	valid = rx(NULL, 1, sess, NULL) == DSSH_ERROR_INVALID && valid;
	valid = tx(&byte, 1, NULL, NULL) == DSSH_ERROR_INVALID && valid;
	valid = rx(&byte, 1, NULL, NULL) == DSSH_ERROR_INVALID && valid;
	valid = rxline(NULL, 1, &received, sess, NULL) == DSSH_ERROR_INVALID && valid;
	valid = rxline(&byte, 1, NULL, sess, NULL) == DSSH_ERROR_INVALID && valid;
	valid = io.polls == 0 && io.sends == 0 && io.receives == 0 && valid;
	end_io();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static const struct dssh_test_entry tests[] = {
	{"client/host_pin", test_host_pin},
	{"client/tx_interruption", test_tx_interruption},
	{"client/rx_interruption", test_rx_interruption},
	{"client/empty_line_buffer", test_empty_line_buffer},
	{"client/invalid_poll_descriptor", test_invalid_poll_descriptor},
	{"client/fingerprint_classes", test_fingerprint_classes},
	{"client/port_values", test_port_values},
	{"client/method_tokens", test_method_tokens},
	{"client/argument_validation", test_argument_validation},
	{"client/password_descriptor", test_password_descriptor},
	{"client/password_lines", test_password_lines},
	{"client/password_bounds", test_password_bounds},
	{"client/password_failures", test_password_failures},
	{"client/password_native_pipe", test_password_native_pipe},
	{"client/password_setup", test_password_setup},
	{"client/output_transfers", test_output_transfers},
	{"client/native_transfer_errors", test_native_transfer_errors},
	{"client/line_framing", test_line_framing},
	{"client/termination", test_termination},
	{"client/invalid_callback_spans", test_invalid_callback_spans},
};
void
dssh_test_failure_cleanup(void)
{
	if (sess != NULL)
		end_io();
	deny_resolution = false;
	credential.enabled = false;
}

#define main component_main
DSSH_TEST_MAIN(tests)
#undef main

int
main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "--run-client") == 0) {
		int descriptor;
		if (argc < 5 || parse_password_descriptor(argv[4], &descriptor) != 0)
			return 2;
		credential = (struct credential_io){.enabled = true, .descriptor = descriptor, .valid = true};
		int ret = client_example_main(argc - 1, &argv[1]);
		bool valid = credential.valid && credential.closes == 1 && credential.allocations == 1
		    && credential.frees == 1 && credential.allocation == NULL;
		if (fprintf(stderr, "example_credential closed=%u allocated=%u cleared=%u valid=%u\n",
		    credential.closes, credential.allocations, credential.frees, valid ? 1U : 0U) < 0)
			valid = false;
		return valid ? ret : 2;
	}
	return component_main(argc, argv);
}
