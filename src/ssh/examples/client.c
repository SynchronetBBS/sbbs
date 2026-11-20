/*
 * Example SSH client using the high-level DeuceSSH API.
 * Usage: client hostkey-sha256 username password-fd [host] [port] [command]
 * If command is given, runs exec mode (no pty).
 * If omitted, opens an interactive shell with pty.
 */

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>
#include <signal.h>
#include <stdatomic.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-crypto.h"
#include "deucessh.h"

static int          sock = -1;
static dssh_session sess;
static atomic_bool input_stop = ATOMIC_VAR_INIT(false);

/* One application-owned allocation, retained through synchronous authentication.
 * Two extra input bytes allow a maximum-length body followed by CRLF. */
#define PASSWORD_LIMIT 16384
struct password_input {
	char *text;
	size_t capacity;
};

static void
password_clear(struct password_input *input)
{
	if (input->text != NULL) {
		dssh_cleanse(input->text, input->capacity);
		free(input->text);
	}
	input->text = NULL;
	input->capacity = 0;
}

static int
parse_password_descriptor(const char *text, int *descriptor)
{
	if (text == NULL || *text == 0)
		return -1;
	uintmax_t value = 0;
	for (const char *p = text; *p != 0; p++) {
		if (*p < '0' || *p > '9')
			return -1;
		unsigned int digit = (unsigned int)(*p - '0');
		if (value > (INT_MAX - digit) / 10)
			return -1;
		value = value * 10 + digit;
	}
	if (value < 3)
		return -1;
	int result = (int)value;
	*descriptor = result;
	return 0;
}

/* Consume exactly one line (or EOF) and retire our descriptor before DNS.
 * Do not change descriptor flags: the parent may share its open file description. */
static int
password_read(int descriptor, struct password_input *input)
{
	int result = -1;
	size_t used = 0;
	input->capacity = PASSWORD_LIMIT + 3;
	input->text = malloc(input->capacity);
	if (input->text == NULL)
		goto done;
	for (;;) {
		size_t count = input->capacity - 1 - used;
		if (count > 4096)
			count = 4096;
		ssize_t n = read(descriptor, &input->text[used], count);
		if (n > 0) {
			if (n > 4096)
				goto done;
			size_t advanced = (size_t)n;
			if (advanced > count)
				goto done;
			size_t end = used + advanced;
			for (size_t i = used; i < end; i++) {
				if (input->text[i] == '\n') {
					size_t length = i;
					if (length > 0 && input->text[length - 1] == '\r')
						length--;
					if (length > PASSWORD_LIMIT)
						goto done;
					input->text[length] = 0;
					result = 0;
					goto done;
				}
				if (input->text[i] == 0)
					goto done;
			}
			used = end;
			if (used > PASSWORD_LIMIT
			    && !(used == PASSWORD_LIMIT + 1 && input->text[PASSWORD_LIMIT] == '\r'))
				goto done;
		}
		else if (n == 0) {
			if (used > PASSWORD_LIMIT)
				goto done;
			input->text[used] = 0;
			result = 0;
			goto done;
		}
		else if (errno == EAGAIN || errno == EWOULDBLOCK) {
			struct pollfd fd = { .fd = descriptor, .events = POLLIN };
			int ready;
			do {
				ready = poll(&fd, 1, -1);
			} while (ready < 0 && errno == EINTR);
			if (ready <= 0 || (fd.revents & (POLLERR | POLLNVAL)) != 0
			    || (fd.revents & (POLLIN | POLLHUP)) == 0)
				goto done;
		}
		else if (errno != EINTR)
			goto done;
	}
done:
	/* Do not retry close after EINTR: descriptor ownership is then ambiguous. */
	if (close(descriptor) != 0)
		result = -1;
	if (result != 0)
		password_clear(input);
	return result;
}

/* ================================================================
 * Keyboard-interactive callback: answer every prompt with password
 * ================================================================ */
struct kbi_state {
	const char *password;
	uint8_t *answer;
	size_t answer_capacity;
	struct dssh_bytes *responses;
	size_t response_capacity;
	bool leased;
};

static void
kbi_release(void *owner)
{
	struct kbi_state *state = owner;
	dssh_cleanse(state->answer, state->answer_capacity);
	if (state->responses != NULL)
		dssh_cleanse(state->responses, state->response_capacity * sizeof(*state->responses));
	state->leased = false;
}

static int
kbi_prompt(const uint8_t *name, size_t name_len, const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts, const uint8_t **prompts, const size_t *prompt_lens, const bool *echo,
    struct dssh_auth_responses *out, void *cbdata)
{
	(void)name;
	(void)name_len;
	(void)instruction;
	(void)instruction_len;
	(void)prompts;
	(void)prompt_lens;
	(void)echo;
	struct kbi_state *state = cbdata;
	if (state == NULL || state->password == NULL || state->leased || out == NULL)
		return DSSH_ERROR_INVALID;
	state->leased = true;
	out->lease.release = kbi_release;
	out->lease.owner = state;
#if SIZE_MAX < UINT32_MAX
	if (num_prompts > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t count = (size_t)num_prompts;
	if (count > SIZE_MAX / sizeof(*state->responses))
		return DSSH_ERROR_INVALID;
	if (count > state->response_capacity) {
		struct dssh_bytes *replacement = malloc(count * sizeof(*replacement));
		if (replacement == NULL)
			return DSSH_ERROR_ALLOC;
		free(state->responses);
		state->responses = replacement;
		state->response_capacity = count;
	}
	size_t length = strlen(state->password);
	if (count > 0 && length > state->answer_capacity) {
		uint8_t *replacement = malloc(length);
		if (replacement == NULL)
			return DSSH_ERROR_ALLOC;
		dssh_cleanse(state->answer, state->answer_capacity);
		free(state->answer);
		state->answer = replacement;
		state->answer_capacity = length;
	}
	if (count > 0 && length > 0)
		memcpy(state->answer, state->password, length);
	for (size_t i = 0; i < count; i++) {
		state->responses[i].data = state->answer;
		state->responses[i].len = length;
	}
	out->items = count > 0 ? state->responses : NULL;
	out->count = num_prompts;
	return 0;
}

/* The expected SHA-256 fingerprint has one canonical textual form. */
static bool
valid_fingerprint(const char *text)
{
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	static const char final_chars[] = "AEIMQUYcgkosw048";
	if (text == NULL || strlen(text) != 50 || memcmp(text, "SHA256:", 7) != 0)
		return false;
	for (size_t i = 7; i < 50; i++) {
		if (strchr(alphabet, text[i]) == NULL)
			return false;
	}
	return strchr(final_chars, text[49]) != NULL;
}

/* The pin comes from trusted application configuration and remains valid until
 * cleanup. Check it on every initial/rekey host-key callback. */
static dssh_hostkey_decision
verify_hostkey(const char *algo_name, unsigned int key_bits, const uint8_t *sha256_hash,
    const uint8_t *key_blob, size_t key_blob_len, void *cbdata)
{
	const char *expected = cbdata;
	if (!valid_fingerprint(expected) || algo_name == NULL || sha256_hash == NULL
	    || key_blob == NULL || key_blob_len == 0)
		return DSSH_HOSTKEY_REJECT;
	char encoded[45];
	int written = dssh_base64_encode(sha256_hash, 32, encoded, sizeof(encoded));
	if (written != 44 || encoded[43] != '=')
		return DSSH_HOSTKEY_REJECT;
	char fingerprint[51] = "SHA256:";
	memcpy(&fingerprint[7], encoded, 43);
	fingerprint[50] = 0;
	if (fprintf(stderr, "Host key: %s %u %s\n", algo_name, key_bits, fingerprint) < 0)
		return DSSH_HOSTKEY_REJECT;
	return strcmp(expected, fingerprint) == 0 ? DSSH_HOSTKEY_ACCEPT : DSSH_HOSTKEY_REJECT;
}

/* ================================================================
 * Terminate callback -- unblock I/O callbacks
 * ================================================================ */
static void
on_terminate(dssh_session s, void *cbdata)
{
	(void)s;
	(void)cbdata;
	if (sock != -1) {
		int ret;
		do {
			ret = shutdown(sock, SHUT_RDWR);
		} while (ret < 0 && errno == EINTR);
		if (ret < 0 && errno != ENOTCONN)
			perror("shutdown");
	}
}

/* ================================================================
 * I/O callbacks for the transport layer
 * ================================================================ */
/* Socket requests stay small enough for every POSIX ssize_t. The callbacks
 * consume the original spans directly, retaining offsets after interruptions. */
static int
tx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	(void)cbdata;
	if (s == NULL || (bufsz > 0 && buf == NULL))
		return DSSH_ERROR_INVALID;
	size_t offset = 0;
	while (offset < bufsz && !dssh_session_is_terminated(s)) {
		struct pollfd fd = { .fd = sock, .events = POLLOUT };
		int ready = poll(&fd, 1, 1000);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			return DSSH_ERROR_INIT;
		}
		if (ready == 0)
			continue;
		if ((fd.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0)
			return DSSH_ERROR_INIT;
		if ((fd.revents & POLLOUT) == 0)
			continue;
		size_t count = bufsz - offset;
		if (count > 4096)
			count = 4096;
		ssize_t n = send(sock, &buf[offset], count, 0);
		if (n > 0) {
			if (n > 4096)
				return DSSH_ERROR_INVALID;
			size_t advanced = (size_t)n;
			if (advanced > count)
				return DSSH_ERROR_INVALID;
			offset += advanced;
		}
		else if (n == 0)
			return DSSH_ERROR_INIT;
		else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			return DSSH_ERROR_INIT;
	}
	return offset == bufsz ? 0 : DSSH_ERROR_TERMINATED;
}

static int
rx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	(void)cbdata;
	if (s == NULL || (bufsz > 0 && buf == NULL))
		return DSSH_ERROR_INVALID;
	size_t offset = 0;
	while (offset < bufsz && !dssh_session_is_terminated(s)) {
		struct pollfd fd = { .fd = sock, .events = POLLIN };
		int ready = poll(&fd, 1, 1000);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			return DSSH_ERROR_INIT;
		}
		if (ready == 0)
			continue;
		if ((fd.revents & POLLNVAL) != 0)
			return DSSH_ERROR_INIT;
		if ((fd.revents & POLLIN) == 0) {
			if ((fd.revents & (POLLHUP | POLLERR)) != 0)
				return DSSH_ERROR_INIT;
			continue;
		}
		size_t count = bufsz - offset;
		if (count > 4096)
			count = 4096;
		ssize_t n = recv(sock, &buf[offset], count, 0);
		if (n > 0) {
			if (n > 4096)
				return DSSH_ERROR_INVALID;
			size_t advanced = (size_t)n;
			if (advanced > count)
				return DSSH_ERROR_INVALID;
			offset += advanced;
		}
		else if (n == 0)
			return DSSH_ERROR_INIT;
		else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			return DSSH_ERROR_INIT;
	}
	return offset == bufsz ? 0 : DSSH_ERROR_TERMINATED;
}

static int
rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received, dssh_session s, void *cbdata)
{
	if (buf == NULL || bufsz == 0 || bytes_received == NULL || s == NULL)
		return DSSH_ERROR_INVALID;
	*bytes_received = 0;
	size_t pos = 0;
	bool lastcr = false;
	while (pos < bufsz && !dssh_session_is_terminated(s)) {
		int ret = rx(&buf[pos], 1, s, cbdata);
		if (ret < 0)
			return ret;
		if (buf[pos] == '\n' && lastcr) {
			*bytes_received = pos + 1;
			return 0;
		}
		lastcr = buf[pos] == '\r';
		pos++;
	}
	return dssh_session_is_terminated(s) ? DSSH_ERROR_TERMINATED : DSSH_ERROR_TOOLONG;
}

static int
extra_line(uint8_t *buf, size_t bufsz, void *cbdata)
{
	(void)cbdata;
	if (bufsz > 0 && buf == NULL)
		return DSSH_ERROR_INVALID;
	if (fputs("Server: ", stderr) == EOF)
		return DSSH_ERROR_INIT;
	for (size_t i = 0; i < bufsz; i++) {
		unsigned int byte = buf[i];
		if (byte >= 0x20 && byte <= 0x7e) {
			if (fputc(buf[i], stderr) == EOF)
				return DSSH_ERROR_INIT;
		}
		else if (fprintf(stderr, "\\x%02x", byte) < 0)
			return DSSH_ERROR_INIT;
	}
	return fputc('\n', stderr) == EOF ? DSSH_ERROR_INIT : 0;
}

static int
write_output(int fd, const uint8_t *buf, size_t length)
{
	size_t offset = 0;
	while (offset < length) {
		size_t count = length - offset;
		if (count > 4096)
			count = 4096;
		ssize_t n = write(fd, &buf[offset], count);
		if (n > 0) {
			if (n > 4096)
				return -1;
			size_t advanced = (size_t)n;
			if (advanced > count)
				return -1;
			offset += advanced;
			continue;
		}
		if (n == 0)
			return -1;
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			return -1;
		struct pollfd item = { .fd = fd, .events = POLLOUT };
		int ready = poll(&item, 1, 1000);
		if (ready < 0 && errno != EINTR)
			return -1;
		if (ready > 0 && (item.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0)
			return -1;
	}
	return 0;
}

/* ================================================================
 * Stdin writer thread -- reads from stdin, writes to channel
 * ================================================================ */
static int
stdin_thread(void *arg)
{
	dssh_channel ch = arg;
	uint8_t buf[4096];
	while (!atomic_load_explicit(&input_stop, memory_order_acquire) && !dssh_session_is_terminated(sess)) {
		struct pollfd fd = { .fd = STDIN_FILENO, .events = POLLIN };
		int ready = poll(&fd, 1, 1000);
		if (atomic_load_explicit(&input_stop, memory_order_acquire) || dssh_session_is_terminated(sess))
			return 0;
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			goto failed;
		}
		if (ready == 0)
			continue;
		if ((fd.revents & (POLLERR | POLLNVAL)) != 0)
			goto failed;
		if ((fd.revents & (POLLIN | POLLHUP)) == 0)
			continue;
		ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
		if (n < 0) {
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			goto failed;
		}
		if (n == 0) {
			int ret = dssh_chan_shutwr(ch);
			if (ret == 0 || ret == DSSH_ERROR_TERMINATED)
				return 0;
			goto failed;
		}
		if (n > 4096)
			goto failed;
		size_t length = (size_t)n;
		size_t offset = 0;
		while (offset < length) {
			int events = dssh_chan_poll(ch, DSSH_POLL_WRITE, 1000);
			if (atomic_load_explicit(&input_stop, memory_order_acquire) || dssh_session_is_terminated(sess))
				return 0;
			if (events < 0)
				goto failed;
			if ((events & DSSH_POLL_WRITE) == 0)
				continue;
			int64_t written = dssh_chan_write(ch, 0, &buf[offset], length - offset);
			if (written == DSSH_ERROR_TERMINATED)
				return 0;
			if (written <= 0 || written > 4096)
				goto failed;
			size_t count = (size_t)written;
			if (count > length - offset)
				goto failed;
			offset += count;
		}
	}
	return 0;
failed:
	dssh_session_terminate(sess);
	return 1;
}

static int
parse_port(const char *text, uint16_t *port)
{
	if (text == NULL || *text == 0 || port == NULL)
		return -1;
	uint32_t value = 0;
	for (const char *p = text; *p != 0; p++) {
		if (*p < '0' || *p > '9')
			return -1;
		uint32_t digit = (uint32_t)(*p - '0');
		if (value > (UINT16_MAX - digit) / 10)
			return -1;
		value = value * 10 + digit;
	}
	if (value == 0)
		return -1;
	*port = (uint16_t)value;
	return 0;
}

/* Every candidate stays owned here until it is closed or returned. */
static int
connect_host(const char *host, const char *port)
{
	struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
	struct addrinfo *addresses = NULL;
	int ret = getaddrinfo(host, port, &hints, &addresses);
	if (ret != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		return -1;
	}
	int connected = -1;
	int last_error = EHOSTUNREACH;
	for (struct addrinfo *address = addresses; address != NULL; address = address->ai_next) {
		int candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (candidate < 0) {
			last_error = errno;
			continue;
		}
		if (connect(candidate, address->ai_addr, address->ai_addrlen) == 0) {
			int flags = fcntl(candidate, F_GETFL);
			int descriptor_flags = fcntl(candidate, F_GETFD);
			if (flags >= 0 && descriptor_flags >= 0
			    && fcntl(candidate, F_SETFL, flags | O_NONBLOCK) == 0
			    && fcntl(candidate, F_SETFD, descriptor_flags | FD_CLOEXEC) == 0) {
				connected = candidate;
				break;
			}
		}
		last_error = errno;
		if (close(candidate) != 0) {
			last_error = errno;
			break;
		}
	}
	freeaddrinfo(addresses);
	if (connected < 0) {
		errno = last_error;
		perror("connect");
	}
	return connected;
}

static int
register_algorithms(void)
{
	int (*const registrations[])(void) = {
		dssh_register_mlkem768x25519_sha256,
		dssh_register_sntrup761x25519_sha512,
		dssh_register_curve25519_sha256,
		dssh_register_dh_gex_sha256,
		dssh_register_dh_group16_sha512,
		dssh_register_dh_group14_sha256,
		dssh_register_rsa_sha2_256,
		dssh_register_ssh_ed25519,
		dssh_register_aes256_ctr,
		dssh_register_hmac_sha2_256,
		dssh_register_none_comp,
	};
	int ret = dssh_transport_set_callbacks(tx, rx, rxline, extra_line);
	for (size_t i = 0; ret == 0 && i < sizeof(registrations) / sizeof(registrations[0]); i++)
		ret = registrations[i]();
	return ret;
}

static bool
offered_method(const char *list, const char *wanted)
{
	size_t wanted_length = strlen(wanted);
	for (const char *name = list; *name != 0;) {
		size_t length = strcspn(name, ",");
		if (length == wanted_length && memcmp(name, wanted, length) == 0)
			return true;
		name += length;
		if (*name == ',')
			name++;
	}
	return false;
}

/* ================================================================
 * Main
 * ================================================================ */
int
main(int argc, char **argv)
{
	const char *program = argc > 0 && argv != NULL && argv[0] != NULL ? argv[0] : "client";
	if (argc < 4 || argc > 7 || argv == NULL) {
		fprintf(stderr, "Usage: %s hostkey-sha256 username password-fd [host] [port] [command]\n", program);
		return 1;
	}
	char *expected_hostkey = argv[1];
	const char *username = argv[2];
	int password_descriptor;
	const char *host = argc > 4 ? argv[4] : "localhost";
	const char *command = argc > 6 ? argv[6] : NULL;
	uint16_t port = 22;
	if (!valid_fingerprint(expected_hostkey) || username == NULL || *username == 0
	    || parse_password_descriptor(argv[3], &password_descriptor) != 0 || host == NULL || *host == 0
	    || (argc > 5 && parse_port(argv[5], &port) != 0)) {
		fputs("Invalid fingerprint, username, password descriptor, host or port.\n", stderr);
		return 1;
	}
	char port_str[16];
	unsigned int port_number = port;
	int printed = snprintf(port_str, sizeof(port_str), "%u", port_number);
	if (printed < 0 || printed >= 16)
		return 1;
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("signal");
		return 1;
	}

	int status = 1;
	struct password_input password = {0};
	char *methods = NULL;
	size_t methods_capacity = 0;
	atomic_store_explicit(&input_stop, false, memory_order_release);
	dssh_channel channel = NULL;
	struct dssh_chan_params params;
	bool have_params = false, have_writer = false;
	thrd_t writer;
	if (password_read(password_descriptor, &password) != 0) {
		fputs("Password input failed.\n", stderr);
		goto done;
	}
	sock = connect_host(host, port_str);
	if (sock < 0)
		goto done;
	if (fprintf(stderr, "Connected to %s:%u\n", host, port_number) < 0)
		goto done;
	int ret = register_algorithms();
	if (ret != 0) {
		fprintf(stderr, "algorithm setup failed: %d\n", ret);
		goto done;
	}
	sess = dssh_session_init(true, 0);
	if (sess == NULL) {
		fputs("session_init failed\n", stderr);
		goto done;
	}
	ret = dssh_session_set_terminate_cb(sess, on_terminate, NULL);
	if (ret == 0)
		ret = dssh_session_set_hostkey_verify_cb(sess, verify_hostkey, expected_hostkey);
	if (ret != 0) {
		fprintf(stderr, "callback setup failed: %d\n", ret);
		goto done;
	}
	if (fputs("Handshake...\n", stderr) == EOF)
		goto done;
	ret = dssh_transport_handshake(sess);
	if (ret != 0) {
		fprintf(stderr, "handshake failed: %d\n", ret);
		goto done;
	}
	const char *remote = dssh_transport_get_remote_version(sess);
	const char *kex = dssh_transport_get_kex_name(sess);
	const char *key = dssh_transport_get_hostkey_name(sess);
	if (remote == NULL || kex == NULL || key == NULL
	    || fprintf(stderr, "Remote: %s\nKEX: %s, Host key: %s\n", remote, kex, key) < 0)
		goto done;
	const enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	const char *labels[] = {"Client to server", "Server to client"};
	for (size_t i = 0; i < 2; i++) {
		const char *enc = dssh_transport_get_enc_name(sess, directions[i]);
		const char *mac = dssh_transport_get_mac_name(sess, directions[i]);
		const char *comp = dssh_transport_get_comp_name(sess, directions[i]);
		if (enc == NULL || mac == NULL || comp == NULL
		    || fprintf(stderr, "%s: Enc: %s, MAC: %s, Compression: %s\n", labels[i], enc, mac, comp) < 0)
			goto done;
	}

	ret = dssh_auth_begin(sess, username);
	bool tried_password = false, tried_kbi = false;
	while (ret == DSSH_AUTH_PARTIAL || ret == DSSH_AUTH_CONTINUE) {
		int64_t needed = dssh_auth_get_methods(sess, NULL, 0);
		if (needed < 1) {
			fprintf(stderr, "auth methods unavailable: %" PRId64 "\n", needed);
			goto done;
		}
#if SIZE_MAX < INT64_MAX
		if (needed > SIZE_MAX)
			goto done;
#endif
		size_t size = (size_t)needed;
		if (size > methods_capacity) {
			char *replacement = realloc(methods, size);
			if (replacement == NULL)
				goto done;
			methods = replacement;
			methods_capacity = size;
		}
		if (dssh_auth_get_methods(sess, methods, methods_capacity) != needed)
			goto done;
		if (fprintf(stderr, "Auth methods: %s\n", methods) < 0)
			goto done;
		if (!tried_password && offered_method(methods, "password")) {
			tried_password = true;
			ret = dssh_auth_password(sess, username, password.text, NULL, NULL);
		}
		else if (!tried_kbi && offered_method(methods, "keyboard-interactive")) {
			tried_kbi = true;
			struct kbi_state state = { .password = password.text };
			ret = dssh_auth_keyboard_interactive(sess, username, kbi_prompt, &state);
			kbi_release(&state);
			free(state.answer);
			free(state.responses);
		}
		else {
			fputs("No untried supported authentication method.\n", stderr);
			goto done;
		}
	}
	if (ret != DSSH_AUTH_SUCCESS) {
		fprintf(stderr, "authentication failed: %d\n", ret);
		goto done;
	}
	password_clear(&password);
	if (fputs("Authenticated.\n", stderr) == EOF)
		goto done;
	ret = dssh_session_start(sess);
	if (ret != 0) {
		fprintf(stderr, "session_start failed: %d\n", ret);
		goto done;
	}
	ret = dssh_chan_params_init(&params, command != NULL ? DSSH_CHAN_EXEC : DSSH_CHAN_SHELL);
	if (ret != 0)
		goto done;
	have_params = true;
	if (command != NULL) {
		ret = dssh_chan_params_set_command(&params, command);
		if (ret == 0)
			ret = dssh_chan_params_set_pty(&params, false);
	}
	else {
		ret = dssh_chan_params_set_term(&params, "xterm-256color");
		if (ret == 0)
			ret = dssh_chan_params_set_size(&params, 80, 24, 0, 0);
	}
	if (ret != 0) {
		fprintf(stderr, "channel parameters failed: %d\n", ret);
		goto done;
	}
	channel = dssh_chan_open(sess, &params);
	dssh_chan_params_free(&params);
	have_params = false;
	if (channel == NULL) {
		fputs("channel open failed\n", stderr);
		goto done;
	}
	if (command != NULL) {
		/* Exec mode has no stdin worker, so publish its empty input now. */
		if (dssh_chan_shutwr(channel) != 0)
			goto done;
	}
	else {
		if (thrd_create(&writer, stdin_thread, channel) != thrd_success) {
			fputs("stdin worker creation failed\n", stderr);
			goto done;
		}
		have_writer = true;
	}

	uint8_t buf[4096];
	int requested = DSSH_POLL_READ | DSSH_POLL_READEXT | DSSH_POLL_EVENT;
	bool closed = false;
	for (;;) {
		if ((requested & (DSSH_POLL_READ | DSSH_POLL_READEXT)) == 0) {
			if (closed) {
				status = 0;
				break;
			}
			if (dssh_session_is_terminated(sess))
				goto done;
		}
		int events = dssh_chan_poll(channel, requested, 1000);
		if (events < 0)
			goto done;
		if (events == 0) {
			if (dssh_session_is_terminated(sess))
				goto done;
			continue;
		}
		if ((events & DSSH_POLL_EVENT) != 0) {
			struct dssh_chan_event event;
			if (dssh_chan_read_event(channel, &event) != 0)
				goto done;
			if (event.type == DSSH_EVENT_CLOSE)
				closed = true;
			if (event.type == DSSH_EVENT_SIGNAL) {
				if (fputs("[signal bytes:", stderr) == EOF)
					goto done;
				for (size_t i = 0; i < event.signal.name.len; i++) {
					unsigned int byte = event.signal.name.data[i];
					if (fprintf(stderr, " %02x", byte) < 0)
						goto done;
				}
				if (fputs("]\n", stderr) == EOF)
					goto done;
			}
		}
		for (int stream = 0; stream < 2; stream++) {
			int flag = stream == 0 ? DSSH_POLL_READ : DSSH_POLL_READEXT;
			if ((events & flag) == 0)
				continue;
			int64_t n = dssh_chan_read(channel, stream, buf, sizeof(buf));
			if (n < 0 || n > 4096)
				goto done;
			if (n == 0) {
				requested &= ~flag;
				continue;
			}
			size_t count = (size_t)n;
			if (write_output(stream == 0 ? STDOUT_FILENO : STDERR_FILENO, buf, count) != 0)
				goto done;
		}
	}

done:
	free(methods);
	password_clear(&password);
	if (have_params)
		dssh_chan_params_free(&params);
	if (have_writer) {
		atomic_store_explicit(&input_stop, true, memory_order_release);
		if (status != 0)
			dssh_session_terminate(sess);
		int result;
		if (thrd_join(writer, &result) != thrd_success)
			abort();
		if (result != 0)
			status = 1;
	}
	if (channel != NULL) {
		int result = dssh_chan_close(channel, -1);
		channel = NULL;
		if (result != 0)
			status = 1;
	}
	if (sess != NULL) {
		dssh_session_cleanup(sess);
		sess = NULL;
	}
	if (sock != -1) {
		int descriptor = sock;
		sock = -1;
		if (close(descriptor) != 0) {
			perror("close");
			status = 1;
		}
	}
	return status;
}
