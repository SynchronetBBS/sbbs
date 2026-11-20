/* Compile the actual application's outer owner path with bounded application-
 * side startup/native-result schedules. No public listener, actual fork, native
 * signal delivery, registry mutation or repeated key generation occurs here.
 * Native socketpair descriptors and close are real. Encrypted operation lives
 * in the separate server_io production-library fixture. */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "../examples/server-output.h"
#include "../examples/server-port.h"

enum startup_case {
	PORT_EMPTY, PORT_NEGATIVE, PORT_SIGN, PORT_SPACE, PORT_TRAILING,
	PORT_HUGE, PORT_RANGE, PORT_EXTRA, PORT_ZERO, PORT_MAX, PORT_DEFAULT,
	PORT_PADDED, PIPE_SIGNAL, VERSION, CALLBACKS,
	REG1, REG2, REG3, REG4, REG5, REG6, REG7, REG8, REG9, REG10, REG11,
	ED_KEY, RSA_KEY, SOCKET_CREATE, LISTEN_GET, LISTEN_SET, SOCKET_OPTION, SOCKET_BIND, SOCKET_LISTEN,
	CHILD_MASK, CHILD_ACTION, LISTEN_LOG, WAIT_ERROR, WAIT_INTERRUPT, WAIT_IDLE,
	WAIT_INVALID, WAIT_NOT_READY, WAIT_HUP, ACCEPT_ERROR, ACCEPT_INTERRUPT, ACCEPT_AGAIN,
	ACCEPT_LOG, FORK_FLUSH, FORK_ERROR, FORK_CLOSE, PARENT_CLOSE,
	CHILD_LISTEN_CLOSE, CHILD_HANDLE, LISTEN_CLOSE, REAP_INTERRUPT,
	REAP_EMPTY, REAP_NO_CHILD, REAP_ERROR, STARTUP_CASES,
};
static enum startup_case selected;
static unsigned registrations, operations, accepts, forks, inits, waits, polls;
static unsigned listener_closes, connection_closes;
static bool fault_seen, halted, continued, bad_argument;
static int listener_descriptor, connection_descriptor, exit_result;
static uint16_t expected_port;
static FILE *diagnostics;
static jmp_buf exit_scope;
static void progress(void)
{
	operations++;
	if (halted)
		continued = true;
}
static bool fail(enum startup_case which)
{
	progress();
	if (selected != which)
		return false;
	fault_seen = halted = true;
	return true;
}
static void (*observed_signal(int, void (*)(int)))(int);
static int observed_emptyset(sigset_t *);
static int observed_action(int, const struct sigaction *, struct sigaction *);
static int observed_version(const char *, const char *);
static int observed_callbacks(dssh_transport_io_cb, dssh_transport_io_cb, dssh_transport_rxline_cb, void *);
static int observed_registration(void);
static int observed_ed_key(void);
static int observed_rsa_key(unsigned);
static int observed_socket(int, int, int);
static int observed_fcntl(int, int, ...);
static int observed_poll(struct pollfd *, nfds_t, int);
static int observed_option(int, int, int, const void *, socklen_t);
static int observed_bind(int, const struct sockaddr *, socklen_t);
static int observed_listen(int, int);
static int observed_accept(int, struct sockaddr *, socklen_t *);
static pid_t observed_fork(void);
static int observed_close(int);
static int observed_flush(FILE *);
static int observed_diagnostic(FILE *, const char *, va_list) DSSH_EXAMPLE_PRINTF_LIKE(2, 0);
static dssh_session observed_init(bool, size_t);
static pid_t observed_waitpid(pid_t, int *, int);
static _Noreturn void observed_exit(int);

#define main example_server_main
#undef stderr
#define stderr diagnostics
#define signal observed_signal
#define sigemptyset observed_emptyset
#define sigaction(...) observed_action(__VA_ARGS__)
#define dssh_transport_set_version observed_version
#define dssh_transport_set_callbacks observed_callbacks
#define dssh_register_mlkem768x25519_sha256 observed_registration
#define dssh_register_sntrup761x25519_sha512 observed_registration
#define dssh_register_curve25519_sha256 observed_registration
#define dssh_register_dh_gex_sha256 observed_registration
#define dssh_register_dh_group16_sha512 observed_registration
#define dssh_register_dh_group14_sha256 observed_registration
#define dssh_register_ssh_ed25519 observed_registration
#define dssh_register_rsa_sha2_256 observed_registration
#define dssh_register_aes256_ctr observed_registration
#define dssh_register_hmac_sha2_256 observed_registration
#define dssh_register_none_comp observed_registration
#define dssh_ed25519_generate_key observed_ed_key
#define dssh_rsa_sha2_256_generate_key observed_rsa_key
#define socket observed_socket
#define fcntl observed_fcntl
#define poll observed_poll
#define setsockopt observed_option
#define bind observed_bind
#define listen observed_listen
#define accept observed_accept
#define fork observed_fork
#define close observed_close
#define fflush observed_flush
#define vfprintf observed_diagnostic
#define dssh_session_init observed_init
#define waitpid observed_waitpid
#define _exit observed_exit
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
#undef signal
#undef sigemptyset
#undef sigaction
#undef dssh_transport_set_version
#undef dssh_transport_set_callbacks
#undef dssh_register_mlkem768x25519_sha256
#undef dssh_register_sntrup761x25519_sha512
#undef dssh_register_curve25519_sha256
#undef dssh_register_dh_gex_sha256
#undef dssh_register_dh_group16_sha512
#undef dssh_register_dh_group14_sha256
#undef dssh_register_ssh_ed25519
#undef dssh_register_rsa_sha2_256
#undef dssh_register_aes256_ctr
#undef dssh_register_hmac_sha2_256
#undef dssh_register_none_comp
#undef dssh_ed25519_generate_key
#undef dssh_rsa_sha2_256_generate_key
#undef socket
#undef fcntl
#undef poll
#undef setsockopt
#undef bind
#undef listen
#undef accept
#undef fork
#undef close
#undef fflush
#undef vfprintf
#undef dssh_session_init
#undef waitpid
#undef _exit

static void (*observed_signal(int number, void (*handler)(int)))(int)
{
	progress();
	if (number == SIGPIPE) {
		bad_argument |= handler != SIG_IGN;
		if (selected == PIPE_SIGNAL) {
			fault_seen = halted = true;
			return SIG_ERR;
		}
	}
	return SIG_DFL;
}
static int
observed_emptyset(sigset_t *mask)
{
	if (fail(CHILD_MASK))
		return -1;
	return sigemptyset(mask);
}
static int
observed_action(int number, const struct sigaction *action, struct sigaction *old)
{
	bad_argument |= number != SIGCHLD || action == NULL || old != NULL;
	if (action != NULL)
		bad_argument |= action->sa_handler != sigchld_handler || action->sa_flags != 0;
	return fail(CHILD_ACTION) ? -1 : 0;
}
static int
observed_version(const char *software, const char *comment)
{
	bad_argument |= software == NULL || comment == NULL;
	return fail(VERSION) ? DSSH_ERROR_INIT : 0;
}
static int
observed_callbacks(dssh_transport_io_cb tx_cb, dssh_transport_io_cb rx_cb,
    dssh_transport_rxline_cb line_cb, void *opaque)
{
	bad_argument |= tx_cb == NULL || rx_cb == NULL || line_cb == NULL || opaque != NULL;
	return fail(CALLBACKS) ? DSSH_ERROR_INIT : 0;
}
static int
observed_registration(void)
{
	progress();
	registrations++;
	if (selected >= REG1 && selected <= REG11) {
		unsigned index = (unsigned)(selected - REG1) + 1;
		if (registrations == index) {
			fault_seen = halted = true;
			return DSSH_ERROR_INIT;
		}
	}
	return 0;
}
static int observed_ed_key(void)
{
	return fail(ED_KEY) ? DSSH_ERROR_INIT : 0;
}
static int observed_rsa_key(unsigned bits)
{
	bad_argument |= bits != 4096;
	return fail(RSA_KEY) ? DSSH_ERROR_INIT : 0;
}
static int native_descriptor(void)
{
	int descriptors[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) != 0)
		abort();
	if (close(descriptors[1]) != 0)
		abort();
	return descriptors[0];
}
static int observed_socket(int family, int type, int protocol)
{
	bad_argument |= family != PF_INET || type != SOCK_STREAM || protocol != 0;
	if (fail(SOCKET_CREATE))
		return -1;
	listener_descriptor = native_descriptor();
	return listener_descriptor;
}
static int observed_fcntl(int descriptor, int operation, ...)
{
	if (operation == F_GETFL) {
		if (descriptor == listener_descriptor && fail(LISTEN_GET))
			return -1;
		return fcntl(descriptor, operation);
	}
	va_list arguments;
	va_start(arguments, operation);
	int flags = va_arg(arguments, int);
	va_end(arguments);
	bad_argument |= operation != F_SETFL;
	if (descriptor == listener_descriptor && fail(LISTEN_SET))
		return -1;
	return fcntl(descriptor, operation, flags);
}
static int observed_poll(struct pollfd *items, nfds_t count, int timeout)
{
	progress();
	polls++;
	bad_argument |= count != 1 || items == NULL || timeout != 1000 || polls > 4;
	if (items == NULL)
		return -1;
	bad_argument |= items[0].fd != listener_descriptor || items[0].events != POLLIN;
	if (selected == WAIT_ERROR || (selected == WAIT_INTERRUPT && polls == 1)) {
		errno = selected == WAIT_ERROR ? EIO : EINTR;
		return -1;
	}
	if (selected == REAP_ERROR && polls == 1) {
		errno = EDOM;
		sigchld_handler(SIGCHLD);
		bad_argument |= errno != EDOM;
		return 0; /* Error recorded after the flag check but before waiting. */
	}
	if (selected == WAIT_IDLE && polls == 1)
		return 0;
	items[0].revents = selected == WAIT_INVALID ? POLLNVAL : selected == WAIT_HUP ? POLLHUP
	    : selected == WAIT_NOT_READY && polls == 1 ? 0 : POLLIN;
	return 1;
}
static int
observed_option(int descriptor, int level, int option, const void *value, socklen_t length)
{
	bad_argument |= descriptor != listener_descriptor || level != SOL_SOCKET
	    || option != SO_REUSEADDR || value == NULL || length != sizeof(int);
	if (value != NULL && length == sizeof(int)) {
		int actual;
		memcpy(&actual, value, sizeof(actual));
		bad_argument |= actual != 1;
	}
	return fail(SOCKET_OPTION) ? -1 : 0;
}
static int observed_bind(int descriptor, const struct sockaddr *address, socklen_t length)
{
	bad_argument |= descriptor != listener_descriptor || address == NULL || length != sizeof(struct sockaddr_in);
	if (address != NULL && length == sizeof(struct sockaddr_in)) {
		struct sockaddr_in actual;
		memcpy(&actual, address, sizeof(actual));
		bad_argument |= actual.sin_family != AF_INET || ntohs(actual.sin_port) != expected_port
		    || actual.sin_addr.s_addr != htonl(INADDR_ANY);
	}
	return fail(SOCKET_BIND) ? -1 : 0;
}
static int observed_listen(int descriptor, int backlog)
{
	bad_argument |= descriptor != listener_descriptor || backlog != 5;
	return fail(SOCKET_LISTEN) ? -1 : 0;
}
static int observed_accept(int descriptor, struct sockaddr *address, socklen_t *length)
{
	progress();
	accepts++;
	bad_argument |= descriptor != listener_descriptor || address != NULL || length != NULL || accepts > 4;
	if ((selected == ACCEPT_INTERRUPT || selected == ACCEPT_AGAIN) && accepts == 1) {
		errno = selected == ACCEPT_INTERRUPT ? EINTR : EAGAIN;
		return -1;
	}
	if (selected >= REAP_INTERRUPT && accepts == 1) {
		errno = EDOM;
		sigchld_handler(SIGCHLD);
		bad_argument |= errno != EDOM;
		errno = EINTR;
		return -1;
	}
	bool connection = selected >= ACCEPT_LOG && selected <= CHILD_HANDLE;
	if (connection && accepts == 1) {
		connection_descriptor = native_descriptor();
		return connection_descriptor;
	}
	errno = EIO;
	return -1;
}
static pid_t observed_fork(void)
{
	progress();
	forks++;
	if (selected == FORK_ERROR || selected == FORK_CLOSE) {
		errno = EAGAIN;
		return -1;
	}
	return selected == CHILD_LISTEN_CLOSE || selected == CHILD_HANDLE ? 0 : 4242;
}
static int observed_close(int descriptor)
{
	if (descriptor == listener_descriptor)
		listener_closes++;
	else if (descriptor == connection_descriptor)
		connection_closes++;
	else
		bad_argument = true;
	int result = close(descriptor);
	if ((descriptor == listener_descriptor && (selected == LISTEN_CLOSE || selected == CHILD_LISTEN_CLOSE))
	    || (descriptor == connection_descriptor && (selected == FORK_CLOSE || selected == PARENT_CLOSE))) {
		fault_seen = true;
		errno = EIO;
		return -1;
	}
	return result;
}
static int observed_flush(FILE *output)
{
	if (output == NULL && selected == FORK_FLUSH) {
		fault_seen = halted = true;
		errno = EIO;
		return -1;
	}
	return fflush(output);
}
static int observed_diagnostic(FILE *output, const char *format, va_list arguments)
{
	if ((selected == LISTEN_LOG && strstr(format, "Listening") != NULL)
	    || (selected == ACCEPT_LOG && strstr(format, "Connection accepted") != NULL)) {
		fault_seen = halted = true;
		errno = EIO;
		return -1;
	}
	return vfprintf(output, format, arguments);
}
static dssh_session observed_init(bool client, size_t maximum)
{
	(void)client;
	(void)maximum;
	inits++;
	return NULL;
}
static pid_t observed_waitpid(pid_t child, int *status, int options)
{
	waits++;
	bad_argument |= child != -1 || status != NULL || options != WNOHANG;
	if (waits == 1)
		return 4242;
	if (selected == REAP_INTERRUPT && waits == 2) {
		errno = EINTR;
		return -1;
	}
	if (selected == REAP_EMPTY || selected == REAP_INTERRUPT)
		return 0;
	errno = selected == REAP_NO_CHILD ? ECHILD : EIO;
	return -1;
}
static _Noreturn void observed_exit(int result)
{
	exit_result = result;
	longjmp(exit_scope, 1);
}
static bool one_case(enum startup_case which)
{
	selected = which;
	registrations = operations = accepts = forks = inits = waits = polls = 0;
	listener_closes = connection_closes = 0;
	fault_seen = halted = continued = bad_argument = false;
	listener_descriptor = connection_descriptor = -1;
	conn_fd = -1;
	sess = NULL;
	diagnostics = tmpfile();
	if (diagnostics == NULL)
		return false;
	char *ports[] = { "", "-1", "+1", " 1", "1x", "999999999999999999999999999999", "65536", "1" };
	char *arguments[] = { "server", "2222", "extra", NULL };
	int argc = 1;
	expected_port = 2222;
	if (which <= PORT_EXTRA) {
		argc = which == PORT_EXTRA ? 3 : 2;
		arguments[1] = ports[which];
	}
	else if (which == PORT_ZERO || which == PORT_MAX || which == PORT_PADDED) {
		argc = 2;
		arguments[1] = which == PORT_ZERO ? "0" : which == PORT_MAX ? "65535" : "0002222";
		expected_port = which == PORT_ZERO ? 0 : which == PORT_MAX ? UINT16_MAX : 2222;
	}
	int result;
	if (setjmp(exit_scope) == 0)
		result = example_server_main(argc, arguments);
	else
		result = exit_result;
	bool valid = result == 1 && !continued && !bad_argument && conn_fd == -1 && sess == NULL;
	if (which <= PORT_EXTRA)
		valid &= operations == 0 && listener_descriptor == -1;
	else if ((which >= PIPE_SIGNAL && which <= LISTEN_LOG) || which == ACCEPT_LOG || which == FORK_FLUSH)
		valid &= fault_seen;
	if (listener_descriptor >= 0)
		valid &= listener_closes == 1 && fcntl(listener_descriptor, F_GETFD) == -1 && errno == EBADF;
	if (connection_descriptor >= 0)
		valid &= connection_closes == 1 && fcntl(connection_descriptor, F_GETFD) == -1 && errno == EBADF;
	if (which == ACCEPT_INTERRUPT || which == ACCEPT_AGAIN)
		valid &= accepts == 2;
	if (which == WAIT_INTERRUPT || which == WAIT_IDLE || which == WAIT_NOT_READY)
		valid &= polls == 2 && accepts == 1;
	if (which == WAIT_ERROR || which == WAIT_INVALID || which == WAIT_HUP)
		valid &= polls == 1 && accepts == 0;
	if (which == FORK_ERROR || which == FORK_CLOSE || which == PARENT_CLOSE)
		valid &= forks == 1;
	if (which == CHILD_LISTEN_CLOSE)
		valid &= inits == 0;
	if (which == CHILD_HANDLE)
		valid &= inits == 1;
	if (which >= REAP_INTERRUPT)
		valid &= waits == (which == REAP_INTERRUPT ? 3u : 2u) && accepts == (which == REAP_ERROR ? 0u : 2u);
	/* Reclaim incomplete baseline owners so independent schedules remain valid. */
	if (listener_descriptor >= 0 && fcntl(listener_descriptor, F_GETFD) >= 0 && close(listener_descriptor) != 0)
		abort();
	if (connection_descriptor >= 0 && fcntl(connection_descriptor, F_GETFD) >= 0 && close(connection_descriptor) != 0)
		abort();
	conn_fd = -1;
	valid &= fclose(diagnostics) == 0;
	diagnostics = NULL;
	printf("server_startup example=%s case=%d result=%d ops=%u registrations=%u accept=%u fork=%u init=%u wait=%u poll=%u listener_close=%u connection_close=%u valid=%d\n",
	    EXAMPLE_NAME, which, result, operations, registrations, accepts, forks, inits, waits, polls,
	    listener_closes, connection_closes, valid);
	return valid;
}
static bool port_domain(void)
{
	bool valid = true;
	for (uint32_t value = 0; value <= UINT16_MAX; value++) {
		char text[6];
		int length = snprintf(text, sizeof(text), "%" PRIu32, value);
		uint16_t result = 0;
		valid &= length > 0 && length < 6 && example_parse_port(text, &result) && result == value;
	}
	uint16_t result = 17;
	valid &= !example_parse_port(NULL, &result) && result == 17;
	valid &= !example_parse_port("1", NULL);
	valid &= !example_parse_port("65536", &result) && result == 17;
	valid &= !example_parse_port("1x", &result) && result == 17;
	printf("server_port_domain values=65536 valid=%d\n", valid);
	return valid;
}
int main(int argc, char **argv)
{
	bool baseline = argc == 2 && strcmp(argv[1], "--baseline") == 0;
	if (argc > 1 && !baseline)
		return 2;
	(void)observed_emptyset;
	(void)observed_action;
	unsigned failures = port_domain() ? 0u : 1u;
	unsigned cases = 0;
	for (enum startup_case which = PORT_EMPTY; which < STARTUP_CASES; which++) {
		/* Native atoi overflow is outside the historical observation. */
		if (baseline && which == PORT_HUGE)
			continue;
		cases++;
		if (!one_case(which))
			failures++;
	}
	printf("server_startup example=%s cases=%u failures=%u\n", EXAMPLE_NAME, cases, failures);
	return failures == 0 ? 0 : 1;
}
