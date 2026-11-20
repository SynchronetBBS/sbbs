/* Independent native peer for the POSIX client example. Uses libssh only;
 * fixed test credentials and deterministic data are confined to this fixture. */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>

#define STREAM_SIZE 262144U
static const uint8_t input_text[] = "example-input\n";
static const uint8_t digits[] = "0123456789abcdef";

struct server_state {
	ssh_session session;
	ssh_channel channel;
	struct ssh_server_callbacks_struct server_callbacks;
	struct ssh_channel_callbacks_struct channel_callbacks;
	struct timespec started;
	const char *auth;
	bool shell, idle, authenticated, terminal, eof, remote_closed, pty;
	bool challenge_sent, failed, log_error;
	size_t input_bytes;
	unsigned newkeys, password_calls, kbi_calls;
};
static struct server_state state;

static bool
expired(void)
{
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return true;
	double seconds = difftime(now.tv_sec, state.started.tv_sec);
	return seconds < 0 || seconds >= 60;
}

static void
log_message(int priority, const char *function, const char *message, void *data)
{
	(void)priority;
	(void)data;
	if (function == NULL || message == NULL) {
		state.log_error = true;
		return;
	}
	if (strstr(message, "Received SSH_MSG_NEWKEYS") != NULL) {
		if (state.newkeys == 1000)
			state.failed = true;
		else
			state.newkeys++;
	}
	if (fprintf(stderr, "%s: %s\n", function, message) < 0)
		state.log_error = true;
}

static int
password_auth(ssh_session session, const char *user, const char *password, void *data)
{
	(void)data;
	if (state.password_calls == 10)
		return SSH_AUTH_DENIED;
	state.password_calls++;
	if ((strcmp(state.auth, "password") != 0 && strcmp(state.auth, "partial") != 0)
	    || user == NULL || password == NULL
	    || strcmp(user, "audit") != 0 || strcmp(password, "example-password") != 0)
		return SSH_AUTH_DENIED;
	if (strcmp(state.auth, "partial") == 0) {
		ssh_set_auth_methods(session, SSH_AUTH_METHOD_INTERACTIVE);
		return SSH_AUTH_PARTIAL;
	}
	state.authenticated = true;
	return SSH_AUTH_SUCCESS;
}

static int
keyboard_auth(ssh_message message, ssh_session session, void *data)
{
	(void)data;
	if (strcmp(state.auth, "password") == 0 || state.kbi_calls == 4)
		return SSH_AUTH_DENIED;
	state.kbi_calls++;
	int response = ssh_message_auth_kbdint_is_response(message);
	if (response == 0 && !state.challenge_sent) {
		const char *user = ssh_message_auth_user(message);
		if (user == NULL || strcmp(user, "audit") != 0)
			return SSH_AUTH_DENIED;
		const char *prompts[] = {"Password:", "Confirm password:"};
		char echo[] = {0, 0};
		if (ssh_message_auth_interactive_request(message, "audit", "two answers", 2, prompts, echo) != SSH_OK)
			return SSH_AUTH_DENIED;
		state.challenge_sent = true;
		return SSH_AUTH_INFO;
	}
	if (response != 1 || !state.challenge_sent || ssh_userauth_kbdint_getnanswers(session) != 2)
		return SSH_AUTH_DENIED;
	for (unsigned i = 0; i < 2; i++) {
		const char *answer = ssh_userauth_kbdint_getanswer(session, i);
		if (answer == NULL || strcmp(answer, "example-password") != 0)
			return SSH_AUTH_DENIED;
	}
	state.authenticated = true;
	return SSH_AUTH_SUCCESS;
}

static int
receive_data(ssh_session session, ssh_channel channel, void *bytes, uint32_t length,
    int extended, void *data)
{
	(void)session;
	(void)channel;
	(void)data;
	size_t expected = state.shell && !state.idle ? sizeof(input_text) - 1 : 0;
	if (length > INT_MAX || state.input_bytes > expected || length > expected - state.input_bytes
	    || extended != 0 || (length != 0 && bytes == NULL)) {
		state.failed = true;
		return 0;
	}
	if (length != 0 && memcmp(bytes, &input_text[state.input_bytes], length) != 0) {
		state.failed = true;
		return 0;
	}
	state.input_bytes += length;
	int consumed = (int)length;
	return consumed;
}

static void
receive_eof(ssh_session session, ssh_channel channel, void *data)
{
	(void)session;
	(void)channel;
	(void)data;
	state.eof = true;
}

static void
receive_close(ssh_session session, ssh_channel channel, void *data)
{
	(void)session;
	(void)channel;
	(void)data;
	state.remote_closed = true;
}

static int
request_pty(ssh_session session, ssh_channel channel, const char *term,
    int width, int height, int pixels_x, int pixels_y, void *data)
{
	(void)session;
	(void)channel;
	(void)data;
	if (!state.shell || term == NULL || strcmp(term, "xterm-256color") != 0
	    || width != 80 || height != 24 || pixels_x != 0 || pixels_y != 0)
		return SSH_ERROR;
	state.pty = true;
	return SSH_OK;
}

static int
request_shell(ssh_session session, ssh_channel channel, void *data)
{
	(void)session;
	(void)channel;
	(void)data;
	if (!state.shell || !state.pty)
		return SSH_ERROR;
	state.terminal = true;
	return SSH_OK;
}

static int
request_exec(ssh_session session, ssh_channel channel, const char *command, void *data)
{
	(void)session;
	(void)channel;
	(void)data;
	if (state.shell || command == NULL || strcmp(command, "dssh-example") != 0)
		return SSH_ERROR;
	state.terminal = true;
	return SSH_OK;
}

static ssh_channel
open_channel(ssh_session session, void *data)
{
	(void)data;
	if (!state.authenticated || state.channel != NULL)
		return NULL;
	state.channel = ssh_channel_new(session);
	if (state.channel != NULL && ssh_set_channel_callbacks(state.channel, &state.channel_callbacks) != SSH_OK) {
		ssh_channel_free(state.channel);
		state.channel = NULL;
	}
	return state.channel;
}

static int
write_pattern(int stream)
{
	uint8_t block[1024];
	for (size_t i = 0; i < sizeof(block); i++)
		block[i] = stream == 0 ? digits[i % 16] : (uint8_t)(i % 256);
	for (unsigned part = 0; part < STREAM_SIZE / sizeof(block); part++) {
		size_t offset = 0;
		while (offset < sizeof(block)) {
			if (expired() || state.failed)
				return SSH_ERROR;
			size_t remaining = sizeof(block) - offset;
			uint32_t count = (uint32_t)remaining;
			int ret = stream == 0 ? ssh_channel_write(state.channel, &block[offset], count)
			    : ssh_channel_write_stderr(state.channel, &block[offset], count);
			if (ret <= 0 || ret > 1024)
				return SSH_ERROR;
			size_t written = (size_t)ret;
			if (written > remaining)
				return SSH_ERROR;
			offset += written;
		}
	}
	return SSH_OK;
}

static int
run_server(void)
{
	int ret = ssh_handle_key_exchange(state.session);
	if (ret != SSH_OK)
		return ret;
	const char *kex = ssh_get_kex_algo(state.session);
	const char *cipher_in = ssh_get_cipher_in(state.session);
	const char *cipher_out = ssh_get_cipher_out(state.session);
	const char *mac_in = ssh_get_hmac_in(state.session);
	const char *mac_out = ssh_get_hmac_out(state.session);
	if (kex == NULL || cipher_in == NULL || cipher_out == NULL || mac_in == NULL || mac_out == NULL
	    || strcmp(kex, "curve25519-sha256") != 0 || strcmp(cipher_in, "aes256-ctr") != 0
	    || strcmp(cipher_out, "aes256-ctr") != 0 || strcmp(mac_in, "hmac-sha2-256") != 0
	    || strcmp(mac_out, "hmac-sha2-256") != 0)
		return SSH_ERROR;
	ssh_event event = ssh_event_new();
	if (event == NULL)
		return SSH_ERROR;
	ret = ssh_event_add_session(event, state.session);
	bool added = ret == SSH_OK;
	while (ret == SSH_OK && !state.failed && !state.remote_closed && (!state.terminal || (!state.idle && !state.eof))) {
		if (expired()) {
			ret = SSH_ERROR;
			break;
		}
		ret = ssh_event_dopoll(event, 100);
		if (ret == SSH_AGAIN)
			ret = SSH_OK; /* Native poll timeout; the monotonic deadline still applies. */
	}
	if (ret == SSH_OK && (state.failed || state.remote_closed || !state.authenticated || state.channel == NULL
	    || state.input_bytes != (state.shell && !state.idle ? sizeof(input_text) - 1 : 0)))
		ret = SSH_ERROR;
	if (ret == SSH_OK)
		ret = write_pattern(0);
	if (ret == SSH_OK)
		ret = write_pattern(1);
	if (ret == SSH_OK)
		ret = ssh_channel_request_send_exit_status(state.channel, 0);
	if (ret == SSH_OK)
		ret = ssh_channel_send_eof(state.channel);
	if (ret == SSH_OK)
		ret = ssh_channel_close(state.channel);
	while (ret == SSH_OK && !state.remote_closed) {
		if (expired()) {
			ret = SSH_ERROR;
			break;
		}
		ret = ssh_event_dopoll(event, 100);
		if (ret == SSH_AGAIN)
			ret = SSH_OK; /* Native poll timeout; the monotonic deadline still applies. */
	}
	if (added && ssh_event_remove_session(event, state.session) != SSH_OK)
		ret = SSH_ERROR;
	ssh_event_free(event);
	return ret;
}

int
main(int argc, char **argv)
{
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || ssh_init() != SSH_OK)
		return 2;
	if (argc == 3 && strcmp(argv[1], "--public-key") == 0) {
		ssh_key key = NULL;
		char *blob = NULL;
		bool valid = ssh_pki_import_privkey_file(argv[2], "", NULL, NULL, &key) == SSH_OK
		    && ssh_pki_export_pubkey_base64(key, &blob) == SSH_OK && blob != NULL;
		if (valid)
			valid = printf("%s\n", blob) >= 0;
		if (blob != NULL)
			ssh_string_free_char(blob);
		if (key != NULL)
			ssh_key_free(key);
		if (ssh_finalize() != SSH_OK)
			valid = false;
		return valid ? 0 : 1;
	}
	if (argc != 7 || (strcmp(argv[3], "rsa-sha2-256") != 0 && strcmp(argv[3], "ssh-ed25519") != 0)
	    || (strcmp(argv[4], "password") != 0 && strcmp(argv[4], "kbi") != 0
	        && strcmp(argv[4], "fallback") != 0 && strcmp(argv[4], "partial") != 0)
	    || (strcmp(argv[5], "exec") != 0 && strcmp(argv[5], "shell") != 0 && strcmp(argv[5], "shell-idle") != 0)
	    || (strcmp(argv[6], "positive") != 0 && strcmp(argv[6], "negative") != 0)) {
		ssh_finalize();
		return 2;
	}
	char *end;
	errno = 0;
	long descriptor = strtol(argv[1], &end, 10);
	if (errno != 0 || *argv[1] == 0 || *end != 0 || descriptor < 0 || descriptor > INT_MAX) {
		ssh_finalize();
		return 2;
	}
	int fd = (int)descriptor;
	state.auth = argv[4];
	state.idle = strcmp(argv[5], "shell-idle") == 0;
	state.shell = state.idle || strcmp(argv[5], "shell") == 0;
	bool negative = strcmp(argv[6], "negative") == 0;
	ssh_bind bind = NULL;
	ssh_key key = NULL;
	bool handed_off = false, ready = false;
	int ret = SSH_ERROR;
	int process_config = 0, verbosity = SSH_LOG_DEBUG;
	long timeout = 10;
	uint64_t rekey_bytes = UINT64_C(65536);
	if (clock_gettime(CLOCK_MONOTONIC, &state.started) != 0)
		goto done;
	state.session = ssh_new();
	bind = ssh_bind_new();
	if (state.session == NULL || bind == NULL
	    || ssh_set_log_userdata(&state) != SSH_OK || ssh_set_log_callback(log_message) != SSH_OK
	    || ssh_set_log_level(SSH_LOG_DEBUG) != SSH_OK
	    || ssh_pki_import_privkey_file(argv[2], "", NULL, NULL, &key) != SSH_OK)
		goto done;
#define OPTION(which, value) do { \
	if (ssh_options_set(state.session, (which), (value)) != SSH_OK) \
		goto done; \
} while (0)
#define BIND(which, value) do { \
	if (ssh_bind_options_set(bind, (which), (value)) != SSH_OK) \
		goto done; \
} while (0)
	OPTION(SSH_OPTIONS_PROCESS_CONFIG, &process_config);
	OPTION(SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	OPTION(SSH_OPTIONS_TIMEOUT, &timeout);
	OPTION(SSH_OPTIONS_REKEY_DATA, &rekey_bytes);
	OPTION(SSH_OPTIONS_COMPRESSION_C_S, "none");
	OPTION(SSH_OPTIONS_COMPRESSION_S_C, "none");
	BIND(SSH_BIND_OPTIONS_PROCESS_CONFIG, &process_config);
	BIND(SSH_BIND_OPTIONS_LOG_VERBOSITY, &verbosity);
	BIND(SSH_BIND_OPTIONS_KEY_EXCHANGE, "curve25519-sha256");
	BIND(SSH_BIND_OPTIONS_HOSTKEY_ALGORITHMS, argv[3]);
	BIND(SSH_BIND_OPTIONS_CIPHERS_C_S, "aes256-ctr");
	BIND(SSH_BIND_OPTIONS_CIPHERS_S_C, "aes256-ctr");
	BIND(SSH_BIND_OPTIONS_HMAC_C_S, "hmac-sha2-256");
	BIND(SSH_BIND_OPTIONS_HMAC_S_C, "hmac-sha2-256");
	BIND(SSH_BIND_OPTIONS_IMPORT_KEY, key);
	key = NULL; /* bind owns the imported key after successful adoption */
#undef BIND
#undef OPTION
	state.server_callbacks = (struct ssh_server_callbacks_struct){ .userdata = &state,
	    .auth_password_function = password_auth, .auth_kbdint_function = keyboard_auth,
	    .channel_open_request_session_function = open_channel };
	state.channel_callbacks = (struct ssh_channel_callbacks_struct){ .userdata = &state,
	    .channel_data_function = receive_data, .channel_eof_function = receive_eof,
	    .channel_close_function = receive_close, .channel_pty_request_function = request_pty,
	    .channel_shell_request_function = request_shell, .channel_exec_request_function = request_exec };
	ssh_callbacks_init(&state.server_callbacks);
	ssh_callbacks_init(&state.channel_callbacks);
	if (ssh_set_server_callbacks(state.session, &state.server_callbacks) != SSH_OK)
		goto done;
	/* An accept error can precede or follow native fd adoption. The isolated
	 * child must exit on failure; do not close a possibly adopted fd twice. */
	handed_off = true;
	if (ssh_bind_accept_fd(bind, state.session, fd) != SSH_OK)
		goto done;
	unsigned methods = strcmp(state.auth, "password") == 0 || strcmp(state.auth, "partial") == 0
	    ? SSH_AUTH_METHOD_PASSWORD : SSH_AUTH_METHOD_INTERACTIVE;
	if (strcmp(state.auth, "fallback") == 0)
		methods |= SSH_AUTH_METHOD_PASSWORD;
	if (methods > INT_MAX)
		goto done;
	int methods_int = (int)methods;
	ssh_set_auth_methods(state.session, methods_int);
	ready = true;
	ret = run_server();
done:
	if (ret != SSH_OK && state.session != NULL) {
		const char *error = ssh_get_error(state.session);
		if (fprintf(stderr, "libssh example error: %s\n", error != NULL ? error : "unknown") < 0)
			state.log_error = true;
	}
	bool valid = ready && !state.failed && !state.log_error && (negative || (ret == SSH_OK && state.authenticated
	    && state.terminal && (state.idle || state.eof) && state.remote_closed && state.newkeys >= 2));
	unsigned newkeys = state.newkeys, password_calls = state.password_calls, kbi_calls = state.kbi_calls;
	bool authenticated = state.authenticated, terminal = state.terminal, eof = state.eof, closed = state.remote_closed;
	size_t input_bytes = state.input_bytes;
	if (state.channel != NULL) {
		ssh_channel_free(state.channel);
		state.channel = NULL;
	}
	if (state.session != NULL) {
		ssh_disconnect(state.session);
		ssh_free(state.session);
		state.session = NULL;
	}
	if (key != NULL)
		ssh_key_free(key);
	if (bind != NULL)
		ssh_bind_free(bind);
	if (!handed_off && close(fd) != 0)
		valid = false;
	if (ssh_finalize() != SSH_OK)
		valid = false;
	if (fprintf(stderr, "libssh_example newkeys=%u auth=%u terminal=%u eof=%u closed=%u input=%zu password=%u kbi=%u negative=%u valid=%u version=%s\n",
	    newkeys, authenticated ? 1U : 0U, terminal ? 1U : 0U, eof ? 1U : 0U, closed ? 1U : 0U,
	    input_bytes, password_calls, kbi_calls, negative ? 1U : 0U, valid ? 1U : 0U, ssh_version(0)) < 0)
		valid = false;
	return valid ? 0 : 1;
}
