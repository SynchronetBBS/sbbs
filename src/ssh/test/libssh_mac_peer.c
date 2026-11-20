/* Independent libssh process for the production stdio peer. POSIX-only test
 * code; the supplied socketpair endpoint is scoped to this process lifetime. */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>

#define OUTPUT_SIZE 262144U
static const uint8_t digits[] = "0123456789abcdef";
static const uint8_t error_text[] = "interop-stderr\n";
static const uint8_t acknowledgement[] = "dssh-ack\n";

struct peer {
	ssh_session session;
	ssh_key key;
	ssh_channel channel;
	struct ssh_server_callbacks_struct server_callbacks;
	struct ssh_channel_callbacks_struct channel_callbacks;
	struct timespec started;
	const char *user;
	const char *c2s;
	const char *s2c;
	const char *cipher_c2s;
	const char *cipher_s2c;
	bool client;
	bool swap_rekey;
	bool rekey_configured;
	bool signed_auth;
	bool exec_seen;
	bool remote_closed;
	bool log_error;
	unsigned newkeys;
	unsigned checks;
};

static bool
expired(struct peer *peer)
{
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return true;
	double seconds = difftime(now.tv_sec, peer->started.tv_sec);
	return seconds < 0 || seconds >= 45;
}

static void
log_message(int priority, const char *function, const char *message, void *data)
{
	(void)priority;
	struct peer *peer = data;
	if (peer == NULL)
		return;
	if (strstr(function, "ssh_packet_newkeys") != NULL
	    && strstr(message, "Received SSH_MSG_NEWKEYS") != NULL) {
		if (peer->newkeys < 1000)
			peer->newkeys++;
		else
			peer->log_error = true;
	}
	if (fprintf(stderr, "%s: %s\n", function, message) < 0)
		peer->log_error = true;
}

static int
check_directions(struct peer *peer)
{
	if (peer->checks >= 2)
		return SSH_ERROR;
	bool swapped = peer->swap_rekey && peer->checks != 0;
	const char *c2s = swapped ? peer->s2c : peer->c2s;
	const char *s2c = swapped ? peer->c2s : peer->s2c;
	const char *enc_c2s = swapped ? peer->cipher_s2c : peer->cipher_c2s;
	const char *enc_s2c = swapped ? peer->cipher_c2s : peer->cipher_s2c;
	const char *incoming = ssh_get_hmac_in(peer->session);
	const char *outgoing = ssh_get_hmac_out(peer->session);
	const char *expected_in = peer->client ? s2c : c2s;
	const char *expected_out = peer->client ? c2s : s2c;
	const char *kex = ssh_get_kex_algo(peer->session);
	const char *cipher_in = ssh_get_cipher_in(peer->session);
	const char *cipher_out = ssh_get_cipher_out(peer->session);
	if (incoming == NULL || outgoing == NULL || kex == NULL || cipher_in == NULL || cipher_out == NULL
	    || strcmp(incoming, expected_in) != 0 || strcmp(outgoing, expected_out) != 0
	    || strcmp(incoming, outgoing) == 0 || strcmp(kex, "curve25519-sha256") != 0
	    || strcmp(cipher_in, peer->client ? enc_s2c : enc_c2s) != 0
	    || strcmp(cipher_out, peer->client ? enc_c2s : enc_s2c) != 0)
		return SSH_ERROR;
	if (fprintf(stderr, "libssh_algorithms epoch=%u cipher-c2s=%s cipher-s2c=%s mac-c2s=%s mac-s2c=%s\n",
	    peer->checks, peer->client ? cipher_out : cipher_in, peer->client ? cipher_in : cipher_out,
	    peer->client ? outgoing : incoming, peer->client ? incoming : outgoing) < 0)
		return SSH_ERROR;
	peer->checks++;
	return SSH_OK;
}

static int
prepare_rekey(struct peer *peer)
{
	if (!peer->swap_rekey)
		return SSH_OK;
	/* Change only later proposals, after the initial active selection was
	 * checked. Actual post-NEWKEYS getters must prove the new selections. */
	if (ssh_options_set(peer->session, SSH_OPTIONS_CIPHERS_C_S, peer->cipher_s2c) != SSH_OK
	    || ssh_options_set(peer->session, SSH_OPTIONS_CIPHERS_S_C, peer->cipher_c2s) != SSH_OK
	    || ssh_options_set(peer->session, SSH_OPTIONS_HMAC_C_S, peer->s2c) != SSH_OK
	    || ssh_options_set(peer->session, SSH_OPTIONS_HMAC_S_C, peer->c2s) != SSH_OK)
		return SSH_ERROR;
	peer->rekey_configured = true;
	return SSH_OK;
}

static int
write_all(struct peer *peer, const uint8_t *bytes, uint32_t length, bool extended)
{
	uint32_t offset = 0;
	while (offset < length) {
		if (expired(peer))
			return SSH_ERROR;
		int amount = extended ? ssh_channel_write_stderr(peer->channel, &bytes[offset], length - offset)
		    : ssh_channel_write(peer->channel, &bytes[offset], length - offset);
		if (amount <= 0 || amount > length - offset)
			return SSH_ERROR;
		uint32_t advanced = (uint32_t)amount;
		offset += advanced;
	}
	return SSH_OK;
}

static int
read_exact(struct peer *peer, const uint8_t *expected, uint32_t length, bool extended)
{
	uint8_t buffer[4096];
	uint32_t offset = 0;
	while (offset < length) {
		if (expired(peer))
			return SSH_ERROR;
		uint32_t wanted = length - offset;
		if (wanted > sizeof(buffer))
			wanted = sizeof(buffer);
		int amount = ssh_channel_read_timeout(peer->channel, buffer, wanted, extended ? 1 : 0, 10000);
		if (amount <= 0 || amount > wanted)
			return SSH_ERROR;
		uint32_t advanced = (uint32_t)amount;
		for (uint32_t i = 0; i < advanced; i++) {
			uint8_t byte = expected == NULL ? digits[(offset + i) % 16] : expected[offset + i];
			if (buffer[i] != byte)
				return SSH_ERROR;
		}
		offset += advanced;
	}
	return SSH_OK;
}

static int
write_pattern(struct peer *peer)
{
	uint8_t block[1024];
	for (size_t i = 0; i < sizeof(block); i++)
		block[i] = digits[i % 16];
	for (unsigned sent = 0; sent < OUTPUT_SIZE / sizeof(block); sent++) {
		int ret = write_all(peer, block, sizeof(block), false);
		if (ret != SSH_OK)
			return ret;
	}
	return SSH_OK;
}

static int
client_run(struct peer *peer)
{
	int ret = ssh_connect(peer->session);
	if (ret != SSH_OK || check_directions(peer) != SSH_OK || prepare_rekey(peer) != SSH_OK)
		return SSH_ERROR;
	ssh_key host = NULL;
	ret = ssh_get_server_publickey(peer->session, &host);
	bool trusted = ret == SSH_OK && host != NULL && ssh_key_cmp(host, peer->key, SSH_KEY_CMP_PUBLIC) == 0;
	ssh_key_free(host);
	if (!trusted || ssh_userauth_publickey(peer->session, peer->user, peer->key) != SSH_AUTH_SUCCESS)
		return SSH_ERROR;
	peer->signed_auth = true;
	peer->channel = ssh_channel_new(peer->session);
	if (peer->channel == NULL || ssh_channel_open_session(peer->channel) != SSH_OK
	    || ssh_channel_request_exec(peer->channel, "dssh-interop") != SSH_OK)
		return SSH_ERROR;
	peer->exec_seen = true;
	if (write_pattern(peer) != SSH_OK || read_exact(peer, NULL, OUTPUT_SIZE, false) != SSH_OK
	    || write_all(peer, acknowledgement, sizeof(acknowledgement) - 1, false) != SSH_OK
	    || read_exact(peer, error_text, sizeof(error_text) - 1, true) != SSH_OK)
		return SSH_ERROR;
	uint8_t byte;
	ret = ssh_channel_read_timeout(peer->channel, &byte, 1, 0, 10000);
	if (ret != 0 || ssh_channel_is_eof(peer->channel) != 1)
		return SSH_ERROR;
	uint32_t status = UINT32_MAX;
	char *exit_signal = NULL;
	int core = 0;
	ret = ssh_channel_get_exit_state(peer->channel, &status, &exit_signal, &core);
	bool exited = ret == SSH_OK && status == 23 && exit_signal == NULL && core == 0;
	free(exit_signal);
	if (!exited || check_directions(peer) != SSH_OK || ssh_channel_close(peer->channel) != SSH_OK)
		return SSH_ERROR;
	return SSH_OK;
}

static int
publickey_auth(ssh_session session, const char *user, ssh_key key, char state, void *data)
{
	(void)session;
	struct peer *peer = data;
	if (strcmp(user, peer->user) != 0 || ssh_key_cmp(key, peer->key, SSH_KEY_CMP_PUBLIC) != 0)
		return SSH_AUTH_DENIED;
	if (state == SSH_PUBLICKEY_STATE_VALID) {
		peer->signed_auth = true;
		return SSH_AUTH_SUCCESS;
	}
	return state == SSH_PUBLICKEY_STATE_NONE ? SSH_AUTH_SUCCESS : SSH_AUTH_DENIED;
}

static int
accept_exec(ssh_session session, ssh_channel channel, const char *command, void *data)
{
	(void)session;
	struct peer *peer = data;
	if (channel != peer->channel || strcmp(command, "dssh-interop") != 0 || peer->exec_seen)
		return 1;
	peer->exec_seen = true;
	return 0;
}

static ssh_channel
open_channel(ssh_session session, void *data)
{
	struct peer *peer = data;
	if (!peer->signed_auth || peer->channel != NULL)
		return NULL;
	ssh_channel channel = ssh_channel_new(session);
	if (channel == NULL)
		return NULL;
	if (ssh_set_channel_callbacks(channel, &peer->channel_callbacks) != SSH_OK) {
		ssh_channel_free(channel);
		return NULL;
	}
	peer->channel = channel;
	return channel;
}

static void
remote_close(ssh_session session, ssh_channel channel, void *data)
{
	(void)session;
	struct peer *peer = data;
	if (channel != peer->channel)
		peer->log_error = true;
	peer->remote_closed = true;
}

static int
server_run(struct peer *peer, ssh_bind bind, int fd)
{
	struct ssh_server_callbacks_struct callbacks = {
		.userdata = peer, .auth_pubkey_function = publickey_auth,
		.channel_open_request_session_function = open_channel
	};
	peer->server_callbacks = callbacks;
	ssh_callbacks_init(&peer->server_callbacks);
	peer->channel_callbacks.userdata = peer;
	peer->channel_callbacks.channel_exec_request_function = accept_exec;
	peer->channel_callbacks.channel_close_function = remote_close;
	ssh_callbacks_init(&peer->channel_callbacks);
	if (ssh_set_server_callbacks(peer->session, &peer->server_callbacks) != SSH_OK
	    || ssh_bind_accept_fd(bind, peer->session, fd) != SSH_OK)
		return SSH_ERROR;
	ssh_set_auth_methods(peer->session, SSH_AUTH_METHOD_PUBLICKEY);
	if (ssh_handle_key_exchange(peer->session) != SSH_OK || check_directions(peer) != SSH_OK
	    || prepare_rekey(peer) != SSH_OK)
		return SSH_ERROR;
	ssh_event event = ssh_event_new();
	if (event == NULL)
		return SSH_ERROR;
	int ret = ssh_event_add_session(event, peer->session);
	bool added = ret == SSH_OK;
	while (ret == SSH_OK && !peer->exec_seen) {
		if (expired(peer)) {
			ret = SSH_ERROR;
			break;
		}
		ret = ssh_event_dopoll(event, 100);
		if (ret == SSH_AGAIN)
			ret = SSH_OK;
	}
	if (ret == SSH_OK && (!peer->signed_auth || peer->channel == NULL))
		ret = SSH_ERROR;
	if (ret == SSH_OK)
		ret = read_exact(peer, NULL, OUTPUT_SIZE, false);
	if (ret == SSH_OK)
		ret = write_pattern(peer);
	if (ret == SSH_OK)
		ret = write_all(peer, error_text, sizeof(error_text) - 1, true);
	if (ret == SSH_OK)
		ret = check_directions(peer);
	if (ret == SSH_OK)
		ret = ssh_channel_request_send_exit_status(peer->channel, 23);
	if (ret == SSH_OK)
		ret = ssh_channel_send_eof(peer->channel);
	if (ret == SSH_OK)
		ret = ssh_channel_close(peer->channel);
	/* Keep callback storage alive while handling the reciprocal close. */
	while (ret == SSH_OK && !peer->remote_closed) {
		if (expired(peer)) {
			ret = SSH_ERROR;
			break;
		}
		ret = ssh_event_dopoll(event, 100);
		if (ret == SSH_AGAIN)
			ret = SSH_OK;
	}
	if (added && ssh_event_remove_session(event, peer->session) != SSH_OK)
		ret = SSH_ERROR;
	ssh_event_free(event);
	/* All borrowed callback definitions remain in peer through final cleanup. */
	return ret;
}

int
main(int argc, char **argv)
{
	if ((argc != 7 && argc != 10) || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0)
	    || (strcmp(argv[3], "hmac-sha2-256") != 0 && strcmp(argv[3], "hmac-sha2-512") != 0)
	    || (strcmp(argv[4], "hmac-sha2-256") != 0 && strcmp(argv[4], "hmac-sha2-512") != 0)
	    || strcmp(argv[3], argv[4]) == 0)
		return 2;
	if (argc == 10 && ((strcmp(argv[7], "aes256-ctr") != 0 && strcmp(argv[7], "aes128-cbc") != 0)
	    || (strcmp(argv[8], "aes256-ctr") != 0 && strcmp(argv[8], "aes128-cbc") != 0)
	    || strcmp(argv[7], argv[8]) == 0 || (strcmp(argv[9], "keep") != 0 && strcmp(argv[9], "swap") != 0)))
		return 2;
	char *end;
	errno = 0;
	long descriptor = strtol(argv[2], &end, 10);
	if (errno != 0 || end == argv[2] || *end != 0 || descriptor < 0 || descriptor > INT_MAX)
		return 2;
	int fd = (int)descriptor;
	struct peer peer = {.client = strcmp(argv[1], "client") == 0,
		.c2s = argv[3], .s2c = argv[4], .user = argv[6],
		.cipher_c2s = argc == 10 ? argv[7] : "aes256-ctr",
		.cipher_s2c = argc == 10 ? argv[8] : "aes256-ctr",
		.swap_rekey = argc == 10 && strcmp(argv[9], "swap") == 0};
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || clock_gettime(CLOCK_MONOTONIC, &peer.started) != 0
	    || ssh_init() != SSH_OK)
		return 2;
	peer.session = ssh_new();
	ssh_bind bind = NULL;
	bool bind_owns_key = false;
	int ret = SSH_ERROR;
	int process_config = 0;
	int verbosity = SSH_LOG_DEBUG;
	long timeout = 10;
	uint64_t rekey_bytes = UINT64_C(65536);
	if (peer.session == NULL || ssh_set_log_userdata(&peer) != SSH_OK
	    || ssh_set_log_callback(log_message) != SSH_OK
	    || ssh_set_log_level(SSH_LOG_DEBUG) != SSH_OK
	    || ssh_pki_import_privkey_file(argv[5], NULL, NULL, NULL, &peer.key) != SSH_OK)
		goto done;
#define OPTION(option, value) do { \
	if (ssh_options_set(peer.session, (option), (value)) != SSH_OK) \
		goto done; \
} while (0)
	OPTION(SSH_OPTIONS_PROCESS_CONFIG, &process_config);
	OPTION(SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	OPTION(SSH_OPTIONS_TIMEOUT, &timeout);
	OPTION(SSH_OPTIONS_REKEY_DATA, &rekey_bytes);
	if (peer.client) {
		OPTION(SSH_OPTIONS_HOST, "mac.audit");
		OPTION(SSH_OPTIONS_USER, peer.user);
		OPTION(SSH_OPTIONS_FD, &fd);
		OPTION(SSH_OPTIONS_KNOWNHOSTS, "/dev/null");
		OPTION(SSH_OPTIONS_GLOBAL_KNOWNHOSTS, "/dev/null");
		OPTION(SSH_OPTIONS_KEY_EXCHANGE, "curve25519-sha256");
		OPTION(SSH_OPTIONS_HOSTKEYS, "rsa-sha2-256");
		OPTION(SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES, "rsa-sha2-256");
		OPTION(SSH_OPTIONS_CIPHERS_C_S, peer.cipher_c2s);
		OPTION(SSH_OPTIONS_CIPHERS_S_C, peer.cipher_s2c);
		OPTION(SSH_OPTIONS_HMAC_C_S, peer.c2s);
		OPTION(SSH_OPTIONS_HMAC_S_C, peer.s2c);
		OPTION(SSH_OPTIONS_COMPRESSION_C_S, "none");
		OPTION(SSH_OPTIONS_COMPRESSION_S_C, "none");
		ret = client_run(&peer);
	}
	else {
		bind = ssh_bind_new();
		if (bind == NULL)
			goto done;
#define BIND(option, value) do { \
	if (ssh_bind_options_set(bind, (option), (value)) != SSH_OK) \
		goto done; \
} while (0)
		BIND(SSH_BIND_OPTIONS_PROCESS_CONFIG, &process_config);
		if (ssh_bind_options_set(bind, SSH_BIND_OPTIONS_IMPORT_KEY, peer.key) != SSH_OK)
			goto done;
		/* The bind owns the imported key; callbacks borrow it until cleanup. */
		bind_owns_key = true;
		BIND(SSH_BIND_OPTIONS_KEY_EXCHANGE, "curve25519-sha256");
		BIND(SSH_BIND_OPTIONS_HOSTKEY_ALGORITHMS, "rsa-sha2-256");
		BIND(SSH_BIND_OPTIONS_PUBKEY_ACCEPTED_KEY_TYPES, "rsa-sha2-256");
		BIND(SSH_BIND_OPTIONS_CIPHERS_C_S, peer.cipher_c2s);
		BIND(SSH_BIND_OPTIONS_CIPHERS_S_C, peer.cipher_s2c);
		BIND(SSH_BIND_OPTIONS_HMAC_C_S, peer.c2s);
		BIND(SSH_BIND_OPTIONS_HMAC_S_C, peer.s2c);
#undef BIND
		ret = server_run(&peer, bind, fd);
	}
#undef OPTION
done:
	if (ret != SSH_OK && peer.session != NULL)
		fprintf(stderr, "libssh error: %s\n", ssh_get_error(peer.session));
	bool valid = ret == SSH_OK && peer.newkeys >= 2 && peer.checks == 2
	    && peer.signed_auth && peer.exec_seen && !peer.log_error && peer.rekey_configured == peer.swap_rekey;
	if (peer.channel != NULL) {
		ssh_channel_free(peer.channel);
		peer.channel = NULL;
	}
	if (peer.session != NULL) {
		ssh_disconnect(peer.session);
		ssh_free(peer.session);
	}
	ssh_bind_free(bind);
	if (!bind_owns_key)
		ssh_key_free(peer.key);
	peer.key = NULL;
	if (fprintf(stderr, "libssh_mac role=%s c2s=%s s2c=%s bytes-each-way=%u newkeys=%u direction-checks=%u signed-auth=%u exec=%u valid=%u version=%s\n",
	    argv[1], peer.c2s, peer.s2c, OUTPUT_SIZE, peer.newkeys, peer.checks,
	    peer.signed_auth ? 1U : 0U, peer.exec_seen ? 1U : 0U, valid ? 1U : 0U, ssh_version(0)) < 0)
		valid = false;
	if (fprintf(stderr, "libssh_cipher initial-c2s=%s initial-s2c=%s final-c2s=%s final-s2c=%s swap=%u configured=%u checks=%u valid=%u\n",
	    peer.cipher_c2s, peer.cipher_s2c, peer.swap_rekey ? peer.cipher_s2c : peer.cipher_c2s,
	    peer.swap_rekey ? peer.cipher_c2s : peer.cipher_s2c, peer.swap_rekey ? 1U : 0U,
	    peer.rekey_configured ? 1U : 0U, peer.checks, valid ? 1U : 0U) < 0)
		valid = false;
	if (ssh_finalize() != SSH_OK)
		valid = false;
	if (peer.log_error)
		valid = false;
	return valid ? 0 : 1;
}
