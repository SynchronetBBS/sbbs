/* Caller-side failure/ownership controls for the actual POSIX example.
 * The production library and its internal calls are never substituted. */
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-algorithms.h"

static const char *fault;
static atomic_uint hits = ATOMIC_VAR_INIT(0);
static unsigned sessions, cleanups, builders, frees, channels, closes, workers, joins, sockets, retirements;
static bool invalid;
static int credential_fd = -1;
static unsigned credential_closes, secret_allocations, secret_frees;
static void *secret;
static void *app_malloc(size_t);
static void app_free(void *);
static bool fail(const char *name);
static int report(int status);
static int app_socket(int, int, int);
static int app_close(int);
static ssize_t app_write(int, const void *, size_t);
static ssize_t app_read(int, void *, size_t);
static dssh_session app_init(bool, size_t);
static void app_cleanup(dssh_session);
static int app_terminate_cb(dssh_session, dssh_terminate_cb, void *);
static int app_verify_cb(dssh_session, dssh_hostkey_verify_cb, void *);
static int app_registration(void);
static int app_handshake(dssh_session);
static int app_auth_begin(dssh_session, const char *);
static int app_start(dssh_session);
static int app_params_init(struct dssh_chan_params *, enum dssh_chan_type);
static void app_params_free(struct dssh_chan_params *);
static int app_command(struct dssh_chan_params *, const char *);
static int app_pty(struct dssh_chan_params *, bool);
static int app_term(struct dssh_chan_params *, const char *);
static int app_size(struct dssh_chan_params *, uint32_t, uint32_t, uint32_t, uint32_t);
static dssh_channel app_open(dssh_session, const struct dssh_chan_params *);
static int app_chan_close(dssh_channel, int64_t);
static int app_create(thrd_t *, thrd_start_t, void *);
static int app_join(thrd_t, int *);
static int app_chan_poll(dssh_channel, int, int);

#define main client_example_main
#define malloc app_malloc
#define free app_free
#define socket app_socket
#define close app_close
#define write app_write
#define read app_read
#define dssh_session_init app_init
#define dssh_session_cleanup app_cleanup
#define dssh_session_set_terminate_cb app_terminate_cb
#define dssh_session_set_hostkey_verify_cb app_verify_cb
#define dssh_register_none_comp app_registration
#define dssh_transport_handshake app_handshake
#define dssh_auth_begin app_auth_begin
#define dssh_session_start app_start
#define dssh_chan_params_init app_params_init
#define dssh_chan_params_free app_params_free
#define dssh_chan_params_set_command app_command
#define dssh_chan_params_set_pty app_pty
#define dssh_chan_params_set_term app_term
#define dssh_chan_params_set_size app_size
#define dssh_chan_open app_open
#define dssh_chan_close app_chan_close
#define thrd_create app_create
#define thrd_join app_join
#define dssh_chan_poll app_chan_poll
#include "../examples/client.c"
#undef dssh_chan_poll
#undef thrd_join
#undef thrd_create
#undef dssh_chan_close
#undef dssh_chan_open
#undef dssh_chan_params_set_size
#undef dssh_chan_params_set_term
#undef dssh_chan_params_set_pty
#undef dssh_chan_params_set_command
#undef dssh_chan_params_free
#undef dssh_chan_params_init
#undef dssh_session_start
#undef dssh_auth_begin
#undef dssh_transport_handshake
#undef dssh_register_none_comp
#undef dssh_session_set_hostkey_verify_cb
#undef dssh_session_set_terminate_cb
#undef dssh_session_cleanup
#undef dssh_session_init
#undef read
#undef write
#undef close
#undef socket
#undef main
#undef free
#undef malloc

static void *
app_malloc(size_t size)
{
	void *result = malloc(size);
	if (size == PASSWORD_LIMIT + 3) {
		if (secret != NULL || secret_allocations != 0)
			invalid = true;
		secret = result;
		if (result != NULL)
			secret_allocations++;
	}
	return result;
}

static void
app_free(void *value)
{
	if (value != NULL && value == secret) {
		const unsigned char *bytes = value;
		for (size_t i = 0; i < PASSWORD_LIMIT + 3; i++) {
			if (bytes[i] != 0)
				invalid = true;
		}
		secret_frees++;
		secret = NULL;
	}
	free(value);
}

static bool
fail(const char *name)
{
	if (strcmp(fault, name) != 0)
		return false;
	atomic_fetch_add_explicit(&hits, 1, memory_order_relaxed);
	return true;
}

static int
report(int status)
{
	unsigned count = atomic_load_explicit(&hits, memory_order_relaxed);
	bool valid = !invalid && count == 1 && sessions == cleanups && builders == frees
	    && channels == closes && workers == joins && sockets == retirements
	    && credential_closes == 1 && secret_allocations == 1 && secret_frees == 1 && secret == NULL;
	if (fprintf(stderr, "example_credential closed=%u allocated=%u cleared=%u valid=%u\n",
	    credential_closes, secret_allocations, secret_frees, valid ? 1U : 0U) < 0)
		valid = false;
	if (fprintf(stderr, "example_ownership fault=%s hits=%u status=%d sessions=%u/%u builders=%u/%u channels=%u/%u workers=%u/%u sockets=%u/%u valid=%u\n",
	    fault, count, status, sessions, cleanups, builders, frees, channels, closes,
	    workers, joins, sockets, retirements, valid ? 1U : 0U) < 0)
		valid = false;
	return valid ? status : 2;
}

static int
app_socket(int domain, int type, int protocol)
{
	if (credential_closes != 1)
		invalid = true;
	int fd = socket(domain, type, protocol);
	if (fd >= 0)
		sockets++;
	return fd;
}

static int
app_close(int fd)
{
	int ret = close(fd);
	if (ret == 0) {
		if (fd == credential_fd && credential_closes == 0)
			credential_closes++;
		else
			retirements++;
	}
	else
		invalid = true;
	return ret;
}

static ssize_t
app_write(int fd, const void *bytes, size_t size)
{
	if (fail("output")) {
		errno = EIO;
		return -1;
	}
	return write(fd, bytes, size);
}

static ssize_t
app_read(int fd, void *bytes, size_t size)
{
	if (fd == STDIN_FILENO && fail("stdin")) {
		errno = EIO;
		return -1;
	}
	return read(fd, bytes, size);
}

static dssh_session
app_init(bool client, size_t size)
{
	if (fail("init"))
		return NULL;
	dssh_session s = dssh_session_init(client, size);
	if (s != NULL)
		sessions++;
	return s;
}

static void
app_cleanup(dssh_session s)
{
	if (s == NULL || workers != joins || builders != frees || channels != closes)
		invalid = true;
	dssh_session_cleanup(s);
	cleanups++;
}

#define CHECKED(name, stage, declaration, call) \
static int name declaration \
{ \
	if (fail(stage)) \
		return DSSH_ERROR_ALLOC; \
	return call; \
}
CHECKED(app_terminate_cb, "terminate_cb", (dssh_session s, dssh_terminate_cb cb, void *data), dssh_session_set_terminate_cb(s, cb, data))
CHECKED(app_verify_cb, "verify_cb", (dssh_session s, dssh_hostkey_verify_cb cb, void *data), dssh_session_set_hostkey_verify_cb(s, cb, data))
CHECKED(app_registration, "registration", (void), dssh_register_none_comp())
CHECKED(app_handshake, "handshake", (dssh_session s), dssh_transport_handshake(s))
CHECKED(app_auth_begin, "auth_query", (dssh_session s, const char *user), dssh_auth_begin(s, user))
static int
app_start(dssh_session s)
{
	if (secret != NULL || secret_frees != 1)
		invalid = true;
	if (fail("start"))
		return DSSH_ERROR_ALLOC;
	return dssh_session_start(s);
}
CHECKED(app_command, "command", (struct dssh_chan_params *p, const char *text), dssh_chan_params_set_command(p, text))
CHECKED(app_pty, "pty", (struct dssh_chan_params *p, bool enable), dssh_chan_params_set_pty(p, enable))
CHECKED(app_term, "term", (struct dssh_chan_params *p, const char *text), dssh_chan_params_set_term(p, text))
CHECKED(app_size, "size", (struct dssh_chan_params *p, uint32_t c, uint32_t r, uint32_t x, uint32_t y), dssh_chan_params_set_size(p, c, r, x, y))
#undef CHECKED

static int
app_params_init(struct dssh_chan_params *p, enum dssh_chan_type type)
{
	if (fail("params_init"))
		return DSSH_ERROR_ALLOC;
	int ret = dssh_chan_params_init(p, type);
	if (ret == 0)
		builders++;
	return ret;
}

static void
app_params_free(struct dssh_chan_params *p)
{
	if (frees >= builders)
		invalid = true;
	dssh_chan_params_free(p);
	if (p->command != NULL || p->term != NULL || p->subsystem != NULL || p->env != NULL || p->modes != NULL)
		invalid = true;
	frees++;
}

static dssh_channel
app_open(dssh_session s, const struct dssh_chan_params *p)
{
	if (fail("open"))
		return NULL;
	dssh_channel ch = dssh_chan_open(s, p);
	if (ch != NULL)
		channels++;
	return ch;
}

static int
app_chan_close(dssh_channel ch, int64_t code)
{
	if (workers != joins || closes >= channels || code != -1)
		invalid = true;
	int ret = dssh_chan_close(ch, code);
	closes++;
	/* A synthetic error after retirement checks the caller's returned status;
	 * the opaque handle is never used again, whatever that status is. */
	if (fail("close"))
		return DSSH_ERROR_INIT;
	return ret;
}

static int
app_create(thrd_t *thread, thrd_start_t entry, void *data)
{
	if (fail("worker"))
		return thrd_error;
	int ret = thrd_create(thread, entry, data);
	if (ret == thrd_success)
		workers++;
	return ret;
}

static int
app_join(thrd_t thread, int *result)
{
	int ret = thrd_join(thread, result);
	if (ret == thrd_success)
		joins++;
	if (fail("join")) {
		/* Join the real worker first; the example must abort on the reported
		 * ownership failure. No worker is stranded by this test adapter. */
		bool valid = ret == thrd_success && workers == 1 && joins == 1
		    && sessions == 1 && cleanups == 0 && channels == 1 && closes == 0
		    && builders == frees && !invalid && credential_closes == 1
		    && secret_allocations == 1 && secret_frees == 1 && secret == NULL;
		if (fprintf(stderr, "example_join_failure joined=%u owned=%u\n",
		    valid ? 1U : 0U, valid ? 1U : 0U) < 0 || fflush(stderr) != 0)
			_Exit(2);
		return thrd_error;
	}
	return ret;
}

static int
app_chan_poll(dssh_channel ch, int flags, int timeout)
{
	if ((flags & DSSH_POLL_READ) != 0 && fail("poll"))
		return DSSH_ERROR_INIT;
	return dssh_chan_poll(ch, flags, timeout);
}

int
main(int argc, char **argv)
{
	if (argc < 5)
		return 2;
	fault = argv[1];
	if (parse_password_descriptor(argv[4], &credential_fd) != 0)
		return 2;
	struct rlimit limit = {0, 0};
	if (setrlimit(RLIMIT_CORE, &limit) != 0)
		return 2;
	int status = client_example_main(argc - 1, &argv[1]);
	return report(status);
}
