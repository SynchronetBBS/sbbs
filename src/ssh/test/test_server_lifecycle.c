/* Application-only fault schedules around each actual server's owner path.
 * Real sessions, callbacks, socket descriptors and cleanup remain in use.
 * Later protocol phases are substituted only to reach each application edge;
 * the separate server_io fixture performs actual encrypted exchanges. */
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "../examples/server-output.h"

enum fault {
	FD_GET, FD_SET, CLOCK_INIT, SESSION_INIT, SET_TERMINATE, SET_DEBUG,
	SET_UNIMPLEMENTED, SET_GLOBAL, DIAGNOSTIC, HANDSHAKE, REMOTE_NAME,
	KEX_NAME, HOST_NAME, ENC_C2S, ENC_S2C, MAC_C2S, MAC_S2C,
	COMP_C2S, COMP_S2C, BANNER, AUTH, START, ACCEPT, CLOSE,
	SHUTDOWN_ERROR, SHUTDOWN_NOT_CONNECTED, FAULT_COUNT,
};
static enum fault selected;
static unsigned releases, closes;
static bool fault_seen, continued;
static FILE *diagnostics;
static bool fail(enum fault which)
{
	if (fault_seen)
		continued = true;
	if (selected != which)
		return false;
	fault_seen = true;
	return true;
}
static int observed_fcntl(int, int, ...);
static int observed_clock(clockid_t, struct timespec *);
static dssh_session observed_init(bool, size_t);
static int observed_terminate_cb(dssh_session, dssh_terminate_cb, void *);
static int observed_debug_cb(dssh_session, dssh_debug_cb, void *);
static int observed_unimplemented_cb(dssh_session, dssh_unimplemented_cb, void *);
static int observed_global_cb(dssh_session, dssh_global_request_cb, void *);
static int observed_diagnostic(FILE *, const char *, va_list) DSSH_EXAMPLE_PRINTF_LIKE(2, 0);
static int observed_handshake(dssh_session);
static const char *observed_remote(dssh_session);
static const char *observed_kex(dssh_session);
static const char *observed_host(dssh_session);
static const char *observed_enc(dssh_session, enum dssh_direction);
static const char *observed_mac(dssh_session, enum dssh_direction);
static const char *observed_comp(dssh_session, enum dssh_direction);
static int observed_banner(dssh_session, const char *, const char *);
static int observed_auth(dssh_session, const struct dssh_auth_server_cbs *, uint8_t *, size_t *);
static int observed_start(dssh_session);
static dssh_channel observed_accept(dssh_session, const struct dssh_chan_accept_cbs *, int);
static void observed_cleanup(dssh_session);
static int observed_close(int);
static int observed_shutdown(int, int);

#define main example_server_main
#undef stderr
#define stderr diagnostics
#define fcntl observed_fcntl
#define clock_gettime observed_clock
#define dssh_session_init observed_init
#define dssh_session_set_terminate_cb observed_terminate_cb
#define dssh_session_set_debug_cb observed_debug_cb
#define dssh_session_set_unimplemented_cb observed_unimplemented_cb
#define dssh_session_set_global_request_cb observed_global_cb
#define vfprintf observed_diagnostic
#define dssh_transport_handshake observed_handshake
#define dssh_transport_get_remote_version observed_remote
#define dssh_transport_get_kex_name observed_kex
#define dssh_transport_get_hostkey_name observed_host
#define dssh_transport_get_enc_name observed_enc
#define dssh_transport_get_mac_name observed_mac
#define dssh_transport_get_comp_name observed_comp
#define dssh_auth_set_banner observed_banner
#define dssh_auth_server observed_auth
#define dssh_session_start observed_start
#define dssh_chan_accept observed_accept
#define dssh_session_cleanup observed_cleanup
#define close observed_close
#define shutdown observed_shutdown
#include "../examples/server-io.h"
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
#undef main
#undef stderr
#undef fcntl
#undef clock_gettime
#undef dssh_session_init
#undef dssh_session_set_terminate_cb
#undef dssh_session_set_debug_cb
#undef dssh_session_set_unimplemented_cb
#undef dssh_session_set_global_request_cb
#undef vfprintf
#undef dssh_transport_handshake
#undef dssh_transport_get_remote_version
#undef dssh_transport_get_kex_name
#undef dssh_transport_get_hostkey_name
#undef dssh_transport_get_enc_name
#undef dssh_transport_get_mac_name
#undef dssh_transport_get_comp_name
#undef dssh_auth_set_banner
#undef dssh_auth_server
#undef dssh_session_start
#undef dssh_chan_accept
#undef dssh_session_cleanup
#undef close
#undef shutdown

static int
observed_fcntl(int descriptor, int operation, ...)
{
	if (operation == F_GETFL) {
		if (fail(FD_GET)) {
			errno = EIO;
			return -1;
		}
		return fcntl(descriptor, operation);
	}
	va_list arguments;
	va_start(arguments, operation);
	int flags = va_arg(arguments, int);
	va_end(arguments);
	if (operation != F_SETFL)
		return -1;
	if (fail(FD_SET)) {
		errno = EIO;
		return -1;
	}
	return fcntl(descriptor, operation, flags);
}
static int
observed_clock(clockid_t clock, struct timespec *output)
{
	if (selected == CLOCK_INIT && fail(CLOCK_INIT)) {
		errno = EIO;
		return -1;
	}
	return clock_gettime(clock, output);
}
static dssh_session
observed_init(bool client, size_t maximum)
{
	return fail(SESSION_INIT) ? NULL : dssh_session_init(client, maximum);
}
#define SETTER(name, fault_id, type, real) \
static int name(dssh_session session, type callback, void *opaque) \
{ \
	return fail(fault_id) ? DSSH_ERROR_INIT : real(session, callback, opaque); \
}
SETTER(observed_terminate_cb, SET_TERMINATE, dssh_terminate_cb, dssh_session_set_terminate_cb)
SETTER(observed_debug_cb, SET_DEBUG, dssh_debug_cb, dssh_session_set_debug_cb)
SETTER(observed_unimplemented_cb, SET_UNIMPLEMENTED, dssh_unimplemented_cb, dssh_session_set_unimplemented_cb)
SETTER(observed_global_cb, SET_GLOBAL, dssh_global_request_cb, dssh_session_set_global_request_cb)
#undef SETTER
static int
observed_diagnostic(FILE *output, const char *format, va_list arguments)
{
	/* Only the first diagnostic is the injection site. Cleanup logging is
	 * permitted after a failure; protocol/setup calls are not. */
	if (selected == DIAGNOSTIC && !fault_seen) {
		fault_seen = true;
		errno = EIO;
		return -1;
	}
	return vfprintf(output, format, arguments);
}
static int
observed_handshake(dssh_session session)
{
	if (fail(HANDSHAKE))
		return dssh_transport_handshake(session); /* native closed-peer failure */
	if (selected >= CLOSE)
		return DSSH_ERROR_INIT;
	return 0;
}
#define NAME(name, fault_id) \
static const char *name(dssh_session session) \
{ \
	(void)session; \
	return fail(fault_id) ? NULL : "fixture"; \
}
NAME(observed_remote, REMOTE_NAME)
NAME(observed_kex, KEX_NAME)
NAME(observed_host, HOST_NAME)
#undef NAME
#define DIRECTIONAL(name, first, second) \
static const char *name(dssh_session session, enum dssh_direction direction) \
{ \
	(void)session; \
	return fail(direction == DSSH_CLIENT_TO_SERVER ? first : second) ? NULL : "fixture"; \
}
DIRECTIONAL(observed_enc, ENC_C2S, ENC_S2C)
DIRECTIONAL(observed_mac, MAC_C2S, MAC_S2C)
DIRECTIONAL(observed_comp, COMP_C2S, COMP_S2C)
#undef DIRECTIONAL
static int
observed_banner(dssh_session session, const char *message, const char *language)
{
	return fail(BANNER) ? DSSH_ERROR_INIT : dssh_auth_set_banner(session, message, language);
}
static int
observed_auth(dssh_session session, const struct dssh_auth_server_cbs *callbacks,
    uint8_t *username, size_t *length)
{
	(void)session;
	(void)callbacks;
	(void)username;
	(void)length;
	return fail(AUTH) ? DSSH_ERROR_INIT : 0;
}
static int
observed_start(dssh_session session)
{
	(void)session;
	return fail(START) ? DSSH_ERROR_INIT : 0;
}
static dssh_channel
observed_accept(dssh_session session, const struct dssh_chan_accept_cbs *callbacks, int timeout)
{
	(void)session;
	(void)callbacks;
	(void)timeout;
	(void)fail(ACCEPT);
	return NULL;
}
static void
observed_cleanup(dssh_session session)
{
	releases++;
	dssh_session_cleanup(session);
}
static int
observed_close(int descriptor)
{
	closes++;
	int result = close(descriptor);
	if (selected == CLOSE) {
		fault_seen = true;
		errno = EIO;
		return -1;
	}
	return result;
}
static int
observed_shutdown(int descriptor, int how)
{
	int result = shutdown(descriptor, how);
	if (selected == SHUTDOWN_ERROR || selected == SHUTDOWN_NOT_CONNECTED) {
		fault_seen = true;
		errno = selected == SHUTDOWN_ERROR ? EIO : ENOTCONN;
		return -1;
	}
	return result;
}
static bool
one_case(enum fault which)
{
	int descriptors[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) != 0)
		return false;
	if (close(descriptors[1]) != 0)
		return false;
	diagnostics = tmpfile();
	if (diagnostics == NULL) {
		if (close(descriptors[0]) != 0)
			abort();
		return false;
	}
	selected = which;
	releases = closes = 0;
	fault_seen = continued = false;
	conn_fd = descriptors[0];
	sess = NULL;
	int result = handle_connection();
	unsigned expected = which <= SESSION_INIT ? 0 : 1;
	bool valid = result == 1 && fault_seen && !continued && releases == expected && closes == 1
	    && conn_fd == -1 && sess == NULL && connection_io.descriptor == -1;
	valid &= fcntl(descriptors[0], F_GETFD) == -1 && errno == EBADF;
	if (which == SHUTDOWN_ERROR || which == SHUTDOWN_NOT_CONNECTED)
		valid &= atomic_load(&diagnostic_failed) == (which == SHUTDOWN_ERROR);
	/* The baseline must remain reusable even when its owner path is incomplete. */
	if (sess != NULL) {
		dssh_session_cleanup(sess);
		sess = NULL;
	}
	if (conn_fd >= 0) {
		if (close(conn_fd) != 0)
			abort();
		conn_fd = -1;
	}
	valid &= fclose(diagnostics) == 0;
	diagnostics = NULL;
	printf("server_lifecycle example=%s case=%d result=%d releases=%u closes=%u valid=%d\n",
	    EXAMPLE_NAME, which, result, releases, closes, valid);
	return valid;
}
int
main(void)
{
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || dssh_transport_set_callbacks(tx, rx, rxline, NULL) != 0
	    || dssh_register_curve25519_sha256() != 0 || dssh_register_ssh_ed25519() != 0
	    || dssh_register_aes256_ctr() != 0 || dssh_register_hmac_sha2_256() != 0
	    || dssh_register_none_comp() != 0 || dssh_ed25519_generate_key() != 0)
		return 2;
	unsigned failures = 0;
	for (enum fault which = FD_GET; which < FAULT_COUNT; which++) {
		if (!one_case(which))
			failures++;
	}
	printf("server_lifecycle example=%s cases=%d failures=%u\n", EXAMPLE_NAME, FAULT_COUNT, failures);
	return failures == 0 ? 0 : 1;
}
