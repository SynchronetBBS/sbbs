/* Deterministic POSIX stdio peer for the optional independent-peer tests.
 * Only public headers and the production library are used. No listener,
 * account changes, demo authentication or internal test hooks are needed. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-lang.h"

#define OUTPUT_SIZE 262144
static const uint8_t digits[] = "0123456789abcdef";
static const uint8_t error_text[] = "interop-stderr\n";
static const uint8_t acknowledgement[] = "dssh-ack\n";
static struct timespec started;
static unsigned peer_seconds = 120;

struct break_observation {
	atomic_uint count;
	atomic_bool bad;
};

static bool
perform_break(dssh_channel channel, const struct dssh_chan_break_request *request, void *opaque)
{
	struct break_observation *observation = opaque;
	static const uint32_t durations[] = {0, 1, 499, 500, 3000, 3001, UINT32_MAX};
	unsigned index = atomic_load(&observation->count);
	if (index >= 14) {
		atomic_store(&observation->bad, true);
		return false;
	}
	if (request->length != durations[index / 2] || request->want_reply != (index % 2 == 1)
	    || request->stdout_pos != 0 || request->stderr_pos != 0
	    || dssh_chan_get_type(channel) != DSSH_CHAN_EXEC)
		atomic_store(&observation->bad, true);
	atomic_store(&observation->count, index + 1);
	/* An explicit fixture action outcome, never a claim of hardware I/O. */
	return request->length >= 500 && request->length <= 3000;
}

struct notification_observation {
	atomic_uint count;
	atomic_bool bad;
};

static bool
notification_bytes(struct dssh_bytes view, const char *text, size_t length)
{
	return view.len == length && (length == 0
	    || (view.data != NULL && memcmp(view.data, text, length) == 0));
}

static void
observe_notification(dssh_channel channel, const struct dssh_chan_event *event, void *opaque)
{
	(void)channel;
	if (event->type == DSSH_EVENT_EOF || event->type == DSSH_EVENT_CLOSE)
		return;
	struct notification_observation *observation = opaque;
	unsigned index = atomic_load(&observation->count);
	if (index >= 8) {
		atomic_store(&observation->bad, true);
		return;
	}
	bool valid = index == 0 && event->type == DSSH_EVENT_SIGNAL
	    && notification_bytes(event->signal.name, "TERM", 4);
	if (index == 1)
		valid = event->type == DSSH_EVENT_WINDOW_CHANGE && event->window_change.cols == 87
		    && event->window_change.rows == 29 && event->window_change.wpx == 696 && event->window_change.hpx == 232;
	else if (index == 2)
		valid = event->type == DSSH_EVENT_EXIT_STATUS && event->exit_status.exit_code == UINT32_C(0x10203040);
	else if (index == 3)
		valid = event->type == DSSH_EVENT_EXIT_SIGNAL && event->exit_signal.core_dumped
		    && notification_bytes(event->exit_signal.signal_name, "TERM", 4)
		    && notification_bytes(event->exit_signal.error_message, "notification text", 17)
		    && notification_bytes(event->exit_signal.language, "en", 2);
	if (!valid || event->stdout_pos != 0 || event->stderr_pos != 0)
		atomic_store(&observation->bad, true);
	atomic_store(&observation->count, index + 1);
}

struct authorization {
	uint8_t blob[8192];
	size_t length;
	const char *user;
	unsigned host_checks;
	bool signed_auth;
};

static int
remaining(void)
{
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return DSSH_ERROR_INIT;
	double seconds = difftime(now.tv_sec, started.tv_sec);
	double budget = (double)peer_seconds;
	if (seconds < 0)
		return DSSH_ERROR_INIT;
	if (seconds > budget || (seconds == budget && now.tv_nsec >= started.tv_nsec))
		return DSSH_ERROR_TIMEOUT;
	return 0;
}

static int
stdio_io(uint8_t *buf, size_t length, dssh_session session, bool sending)
{
	size_t offset = 0;
	int fd = sending ? STDOUT_FILENO : STDIN_FILENO;
	while (offset < length) {
		if (dssh_session_is_terminated(session))
			return DSSH_ERROR_TERMINATED;
		int ret = remaining();
		if (ret < 0)
			return ret;
		struct pollfd item = { .fd = fd, .events = sending ? POLLOUT : POLLIN };
		int ready = poll(&item, 1, 100);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			return DSSH_ERROR_INIT;
		}
		if (ready == 0)
			continue;
		if ((item.revents & POLLNVAL) != 0 || (item.revents & item.events) == 0)
			return DSSH_ERROR_INIT;
		size_t count = length - offset;
		if (count > 4096)
			count = 4096;
		ssize_t done = sending ? write(fd, &buf[offset], count) : read(fd, &buf[offset], count);
		if (done < 0) {
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			return DSSH_ERROR_INIT;
		}
		if (done == 0)
			return DSSH_ERROR_INIT;
		/* POSIX successful results are bounded by count <= 4096. */
		size_t advanced = (size_t)done;
		if (advanced > count)
			return DSSH_ERROR_INVALID;
		offset += advanced;
	}
	return 0;
}

static int
stdio_tx(uint8_t *buf, size_t length, dssh_session session, void *data)
{
	(void)data;
	return stdio_io(buf, length, session, true);
}

static int
stdio_rx(uint8_t *buf, size_t length, dssh_session session, void *data)
{
	(void)data;
	return stdio_io(buf, length, session, false);
}

static dssh_hostkey_decision
verify_host(const char *algorithm, unsigned bits, const uint8_t *hash,
    const uint8_t *blob, size_t length, void *data)
{
	(void)bits;
	(void)hash;
	struct authorization *auth = data;
	if (strcmp(algorithm, "rsa-sha2-256") != 0 || length != auth->length
	    || memcmp(blob, auth->blob, length) != 0)
		return DSSH_HOSTKEY_REJECT;
	/* Bound even an unexpectedly large number of peer-initiated exchanges. */
	if (auth->host_checks == 1000)
		return DSSH_HOSTKEY_REJECT;
	auth->host_checks++;
	return DSSH_HOSTKEY_ACCEPT;
}

static int
authorize(const uint8_t *user, size_t user_length, const char *algorithm,
    const uint8_t *blob, size_t length, bool signed_request, void *data)
{
	struct authorization *auth = data;
	if (strcmp(algorithm, "rsa-sha2-256") != 0 || user_length != strlen(auth->user)
	    || memcmp(user, auth->user, user_length) != 0 || length != auth->length
	    || memcmp(blob, auth->blob, length) != 0)
		return DSSH_AUTH_FAILURE;
	if (signed_request)
		auth->signed_auth = true;
	return DSSH_AUTH_SUCCESS;
}

static int
accept_command(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *data)
{
	(void)channel;
	(void)result;
	(void)data;
	return params->command != NULL && strcmp(params->command, "dssh-interop") == 0 ? 0 : -1;
}

struct terminal_mode_observation {
	const char *state;
	unsigned callbacks;
	bool valid;
};

static bool
check_terminal_modes(const struct dssh_chan_params *params, const char *state)
{
	if (params == NULL || params->term == NULL || strcmp(params->term, "dssh_iutf8") != 0
	    || params->cols != 87 || params->rows != 29 || params->wpx != 696 || params->hpx != 232)
		return false;
	bool absent = strcmp(state, "none") == 0;
	if (params->mode_count != (absent ? 1U : 2U) || params->modes == NULL)
		return false;
	bool echo = false, utf8 = false;
	for (size_t i = 0; i < params->mode_count; i++) {
		if (params->modes[i].opcode == 53 && params->modes[i].value == 0 && !echo)
			echo = true;
		else if (params->modes[i].opcode == 42 && !absent && !utf8
		    && params->modes[i].value == (strcmp(state, "1") == 0 ? 1U : 0U))
			utf8 = true;
		else
			return false;
	}
	return echo && (absent || utf8);
}

static int
observe_terminal_modes(dssh_channel channel, const struct dssh_chan_params *params, void *opaque)
{
	struct terminal_mode_observation *observation = opaque;
	observation->callbacks++;
	observation->valid = observation->valid && observation->callbacks == 1
	    && dssh_chan_get_pty(channel) == params && dssh_chan_has_pty(channel)
	    && check_terminal_modes(params, observation->state);
	return observation->valid ? 0 : DSSH_ERROR_REJECTED;
}

static int
write_all(dssh_channel channel, int stream, const uint8_t *data, size_t length)
{
	size_t offset = 0;
	while (offset < length) {
		int ret = remaining();
		if (ret < 0)
			return ret;
		ret = dssh_chan_poll(channel, DSSH_POLL_WRITE, 100);
		if (ret < 0)
			return ret;
		if ((ret & DSSH_POLL_WRITE) == 0)
			continue;
		int64_t done = dssh_chan_write(channel, stream, &data[offset], length - offset);
		if (done <= 0 || done > length - offset)
			return DSSH_ERROR_INIT;
		size_t advanced = (size_t)done;
		offset += advanced;
	}
	return 0;
}

static int
send_pattern(dssh_channel channel)
{
	uint8_t block[1024];
	for (size_t i = 0; i < sizeof(block); i++)
		block[i] = digits[i % 16];
	int ret = 0;
	for (unsigned i = 0; i < OUTPUT_SIZE / sizeof(block) && ret == 0; i++)
		ret = write_all(channel, 0, block, sizeof(block));
	return ret;
}

static int
receive_pattern(dssh_channel channel)
{
	uint32_t offset = 0;
	while (offset < OUTPUT_SIZE) {
		int ret = remaining();
		if (ret < 0)
			return ret;
		int ready = dssh_chan_poll(channel, DSSH_POLL_READ, 100);
		if (ready < 0)
			return ready;
		if ((ready & DSSH_POLL_READ) == 0)
			continue;
		uint8_t block[4096];
		uint32_t left = OUTPUT_SIZE - offset;
		size_t wanted = sizeof(block);
		if (left < wanted) {
			size_t available = (size_t)left;
			wanted = available;
		}
		int64_t amount = dssh_chan_read(channel, 0, block, wanted);
		if (amount <= 0 || amount > wanted)
			return DSSH_ERROR_INVALID;
		uint32_t advanced = (uint32_t)amount;
		for (uint32_t i = 0; i < advanced; i++) {
			if (block[i] != digits[(offset + i) % 16])
				return DSSH_ERROR_INVALID;
		}
		offset += advanced;
	}
	return 0;
}

static int
server_channel(dssh_session session, bool *acknowledged, bool bidirectional)
{
	struct terminal_mode_observation modes = {.state = getenv("DSSH_TEST_TERMINAL_MODE"), .valid = true};
	struct dssh_chan_accept_cbs callbacks = {.exec = accept_command};
	if (modes.state != NULL) {
		if (strcmp(modes.state, "none") != 0 && strcmp(modes.state, "0") != 0 && strcmp(modes.state, "1") != 0)
			return DSSH_ERROR_INVALID;
		callbacks.pty_req = observe_terminal_modes;
		callbacks.cbdata = &modes;
	}
	dssh_channel channel = dssh_chan_accept(session, &callbacks, 10000);
	if (channel == NULL)
		return DSSH_ERROR_INIT;
	int ret = 0;
	if (modes.state != NULL) {
		bool valid = modes.valid && modes.callbacks == 1 && check_terminal_modes(dssh_chan_get_pty(channel), modes.state);
		if (fprintf(stderr, "DeuceSSH terminal mode: state=%s callback=%u retained=%u valid=%u\n",
		    modes.state, modes.callbacks, valid ? 1U : 0U, valid ? 1U : 0U) < 0)
			ret = DSSH_ERROR_INIT;
		else if (!valid)
			ret = DSSH_ERROR_INVALID;
	}
	if (ret == 0 && bidirectional)
		ret = receive_pattern(channel);
	if (ret == 0)
		ret = send_pattern(channel);
	/* OpenSSH sends this application acknowledgement only after its reader has
	 * checked all stdout. During rekey it queues channel data until NEWKEYS,
	 * keeping this channel alive through the peer-initiated exchange. */
	uint8_t reply[sizeof(acknowledgement) - 1];
	size_t received = 0;
	while (ret == 0 && received < sizeof(reply)) {
		ret = remaining();
		if (ret < 0)
			break;
		int ready = dssh_chan_poll(channel, DSSH_POLL_READ, 100);
		if (ready < 0) {
			ret = ready;
			break;
		}
		if ((ready & DSSH_POLL_READ) == 0)
			continue;
		int64_t amount = dssh_chan_read(channel, 0, reply + received, sizeof(reply) - received);
		if (amount <= 0 || amount > sizeof(reply) - received) {
			ret = DSSH_ERROR_INIT;
			break;
		}
		size_t advanced = (size_t)amount; /* actual result <= nine bytes */
		received += advanced;
	}
	if (ret == 0) {
		if (memcmp(reply, acknowledgement, sizeof(reply)) != 0)
			ret = DSSH_ERROR_INVALID;
		else
			*acknowledged = true;
	}
	if (ret == 0)
		ret = write_all(channel, 1, error_text, sizeof(error_text) - 1);
	int closed = dssh_chan_close(channel, ret == 0 ? 23 : 1);
	if (ret == 0)
		ret = closed;
	/* Keep the transport alive through the independent peer's reciprocal
	 * CLOSE and disconnect. The demux remains its only reader. */
	while (ret == 0 && !dssh_session_is_terminated(session)) {
		ret = remaining();
		if (ret < 0)
			break;
		struct pollfd item = { .fd = STDIN_FILENO };
		int ready = poll(&item, 1, 100);
		if (ready < 0 && errno != EINTR)
			ret = DSSH_ERROR_INIT;
	}
	return ret;
}

static int
client_channel(dssh_session session, bool bidirectional, bool group_exchange)
{
	struct dssh_chan_params params;
	int ret = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
	if (ret < 0)
		return ret;
	ret = dssh_chan_params_set_command(&params, "dssh-interop");
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&params, false);
	/* Bound channel-data chunks independently of the external producer while
	 * retaining the complete transfer and the peer's byte-triggered rekeys. */
	if (ret == 0 && group_exchange)
		ret = dssh_chan_params_set_max_window(&params, 1024);
	dssh_channel channel = ret == 0 ? dssh_chan_open(session, &params) : NULL;
	dssh_chan_params_free(&params);
	if (channel == NULL)
		return ret < 0 ? ret : DSSH_ERROR_INIT;
	ret = bidirectional ? send_pattern(channel) : 0;
	if (ret == 0)
		ret = dssh_chan_shutwr(channel);
	size_t received[2] = { 0, 0 };
	bool eof[2] = { false, false };
	bool exit_seen = false;
	while (ret == 0 && (!eof[0] || !eof[1] || !exit_seen)) {
		ret = remaining();
		if (ret < 0)
			break;
		int events = DSSH_POLL_EVENT;
		if (!eof[0])
			events |= DSSH_POLL_READ;
		if (!eof[1])
			events |= DSSH_POLL_READEXT;
		int ready = dssh_chan_poll(channel, events, 100);
		if (ready < 0) {
			ret = ready;
			break;
		}
		if ((ready & DSSH_POLL_EVENT) != 0) {
			struct dssh_chan_event event;
			ret = dssh_chan_read_event(channel, &event);
			if (ret < 0)
				break;
			if (event.type == DSSH_EVENT_EXIT_STATUS) {
				if (exit_seen || event.exit_status.exit_code != 23)
					ret = DSSH_ERROR_INVALID;
				exit_seen = true;
			}
			if (event.type == DSSH_EVENT_EXIT_SIGNAL)
				ret = DSSH_ERROR_INVALID;
		}
		for (int stream = 0; stream < 2 && ret == 0; stream++) {
			int flag = stream == 0 ? DSSH_POLL_READ : DSSH_POLL_READEXT;
			if ((ready & flag) == 0 || eof[stream])
				continue;
			uint8_t buffer[4096];
			int64_t amount = dssh_chan_read(channel, stream, buffer, sizeof(buffer));
			if (amount < 0 || amount > sizeof(buffer)) {
				ret = DSSH_ERROR_INIT;
				break;
			}
			if (amount == 0) {
				eof[stream] = true;
				continue;
			}
			size_t length = (size_t)amount;
			size_t limit = stream == 0 ? OUTPUT_SIZE : sizeof(error_text) - 1;
			if (received[stream] > limit || length > limit - received[stream]) {
				ret = DSSH_ERROR_TOOLONG;
				break;
			}
			for (size_t i = 0; i < length; i++) {
				uint8_t expected = stream == 0 ? digits[(received[stream] + i) % 16] :
				    error_text[received[stream] + i];
				if (buffer[i] != expected)
					ret = DSSH_ERROR_INVALID;
			}
			received[stream] += length;
		}
	}
	if (ret == 0 && (received[0] != OUTPUT_SIZE || received[1] != sizeof(error_text) - 1 || !exit_seen))
		ret = DSSH_ERROR_INVALID;
	int closed = dssh_chan_close(channel, -1);
	return ret == 0 ? closed : ret;
}

static int
check_algorithm_fields(dssh_session sess, const char *cipher_c2s, const char *cipher_s2c,
    const char *c2s, const char *s2c, unsigned epoch)
{
	const enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	const char *expected[] = {c2s, s2c};
	const char *ciphers[] = {cipher_c2s, cipher_s2c};
	for (size_t i = 0; i < 2; i++) {
		const char *enc = dssh_transport_get_enc_name(sess, directions[i]);
		const char *mac = dssh_transport_get_mac_name(sess, directions[i]);
		const char *comp = dssh_transport_get_comp_name(sess, directions[i]);
		if (enc == NULL || mac == NULL || comp == NULL || strcmp(enc, ciphers[i]) != 0
		    || strcmp(mac, expected[i]) != 0 || strcmp(comp, "none") != 0)
			return DSSH_ERROR_INVALID;
		if (fprintf(stderr, "DeuceSSH algorithm epoch=%u direction=%s cipher=%s mac=%s comp=%s\n",
		    epoch, i == 0 ? "c2s" : "s2c", enc, mac, comp) < 0)
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static int
check_language_fields(dssh_session sess, bool updated)
{
	const enum dssh_direction directions[] = {
		DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT
	};
	const char *expected[] = {
		updated ? "de-DE" : "en-US,fr", updated ? "sv-SE" : "x-interop"
	};
	for (size_t i = 0; i < 2; i++) {
		char local[64], peer[64];
		/* Both fixed expected strings fit the buffer and int64_t. */
		int64_t needed = (int64_t)(strlen(expected[i]) + 1);
		if (dssh_session_get_languages(sess, directions[i], local, sizeof(local)) != needed
		    || strcmp(local, expected[i]) != 0
		    || dssh_session_get_peer_languages(sess, directions[i], peer, sizeof(peer)) != 1
		    || peer[0] != 0)
			return DSSH_ERROR_INVALID;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	if (argc != 6 || (strcmp(argv[1], "server") != 0 && strcmp(argv[1], "client") != 0))
		return 2;
	bool client = strcmp(argv[1], "client") == 0;
	const char *peer_budget = getenv("DSSH_OPENSSH_PEER_SECONDS");
	if (peer_budget != NULL && strcmp(peer_budget, "120") != 0) {
		if (!client || strcmp(argv[2], "diffie-hellman-group-exchange-sha256") != 0
		    || strcmp(peer_budget, "520") != 0)
			return 2;
		peer_seconds = 520;
	}
	bool notification_flags = getenv("DSSH_TEST_NOTIFICATION_FLAGS") != NULL;
	if (notification_flags && client)
		return 2;
	struct notification_observation notifications;
	atomic_init(&notifications.count, 0);
	atomic_init(&notifications.bad, false);
	bool break_actions = getenv("DSSH_TEST_BREAK_ACTION") != NULL;
	if (break_actions && client)
		return 2;
	struct break_observation break_observation;
	atomic_init(&break_observation.count, 0);
	atomic_init(&break_observation.bad, false);
	bool language_fields = getenv("DSSH_TEST_LANGUAGE_FIELDS") != NULL;
	const char *cipher = getenv("DSSH_TEST_ENC");
	if (cipher == NULL)
		cipher = "aes256-ctr";
	if (strcmp(cipher, "aes256-ctr") != 0 && strcmp(cipher, "aes128-cbc") != 0)
		return 2;
	const char *cipher_s2c = getenv("DSSH_TEST_ENC_S2C");
	if (cipher_s2c == NULL)
		cipher_s2c = cipher;
	if (strcmp(cipher_s2c, "aes256-ctr") != 0 && strcmp(cipher_s2c, "aes128-cbc") != 0)
		return 2;
	bool swap_rekey = getenv("DSSH_TEST_REKEY_SWAP") != NULL;
	const char *mac = getenv("DSSH_TEST_MAC");
	if (mac == NULL)
		mac = "hmac-sha2-256";
	if (strcmp(mac, "hmac-sha2-256") != 0 && strcmp(mac, "hmac-sha2-512") != 0)
		return 2;
	const char *mac_s2c = getenv("DSSH_TEST_MAC_S2C");
	if (mac_s2c == NULL)
		mac_s2c = mac;
	if (strcmp(mac_s2c, "hmac-sha2-256") != 0 && strcmp(mac_s2c, "hmac-sha2-512") != 0)
		return 2;
	if (swap_rekey && (strcmp(cipher, cipher_s2c) == 0 || strcmp(mac, mac_s2c) == 0))
		return 2;
	struct authorization auth = { .user = argv[5] };
	FILE *key = fopen(argv[4], "rb");
	if (key == NULL)
		return 2;
	auth.length = fread(auth.blob, 1, sizeof(auth.blob), key);
	bool key_error = ferror(key) != 0 || !feof(key) || auth.length == 0;
	if (fclose(key) != 0 || key_error)
		return 2;
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || clock_gettime(CLOCK_MONOTONIC, &started) != 0)
		return 2;
	for (int fd = STDIN_FILENO; fd <= STDOUT_FILENO; fd++) {
		int flags = fcntl(fd, F_GETFL);
		if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
			return 2;
	}
	int ret = DSSH_ERROR_INVALID;
	if (strcmp(argv[2], "curve25519-sha256") == 0)
		ret = dssh_register_curve25519_sha256();
	else if (strcmp(argv[2], "diffie-hellman-group-exchange-sha256") == 0)
		ret = dssh_register_dh_gex_sha256();
	else if (strcmp(argv[2], "diffie-hellman-group14-sha256") == 0)
		ret = dssh_register_dh_group14_sha256();
	else if (strcmp(argv[2], "diffie-hellman-group16-sha512") == 0)
		ret = dssh_register_dh_group16_sha512();
	else if (strcmp(argv[2], "sntrup761x25519-sha512") == 0)
		ret = dssh_register_sntrup761x25519_sha512();
	else if (strcmp(argv[2], "mlkem768x25519-sha256") == 0)
		ret = dssh_register_mlkem768x25519_sha256();
	if (ret == 0)
		ret = strcmp(cipher, "aes128-cbc") == 0
		    ? dssh_register_aes128_cbc() : dssh_register_aes256_ctr();
	if (ret == 0 && strcmp(cipher, cipher_s2c) != 0)
		ret = strcmp(cipher_s2c, "aes128-cbc") == 0
		    ? dssh_register_aes128_cbc() : dssh_register_aes256_ctr();
	if (ret == 0)
		ret = strcmp(mac, "hmac-sha2-512") == 0
		    ? dssh_register_hmac_sha2_512() : dssh_register_hmac_sha2_256();
	if (ret == 0 && strcmp(mac, mac_s2c) != 0)
		ret = strcmp(mac_s2c, "hmac-sha2-512") == 0
		    ? dssh_register_hmac_sha2_512() : dssh_register_hmac_sha2_256();
	if (ret != 0 || dssh_transport_set_callbacks(stdio_tx, stdio_rx, NULL, NULL) != 0
	    || dssh_register_rsa_sha2_256() != 0
	    || dssh_register_none_comp() != 0
	    || dssh_rsa_sha2_256_load_key_file(argv[3], NULL, NULL) != 0)
		return 2;
	dssh_session session = dssh_session_init(client, 0);
	if (session == NULL)
		return 2;
	const char *stage = "configuration";
	bool channel_ack = false;
	unsigned language_checks = 0;
	unsigned algorithm_checks = 0;
	ret = dssh_session_set_timeout(session, 75000);
	if (ret == 0 && break_actions)
		ret = dssh_session_set_break_cb(session, perform_break, &break_observation);
	if (ret == 0 && notification_flags)
		ret = dssh_session_set_event_cb(session, observe_notification, &notifications);
	if (ret == 0 && language_fields) {
		const char *first[] = {"en-US", "fr"};
		const char *second[] = {"x-interop"};
		ret = dssh_session_set_languages(session, DSSH_CLIENT_TO_SERVER, first, 2);
		if (ret == 0)
			ret = dssh_session_set_languages(session, DSSH_SERVER_TO_CLIENT, second, 1);
	}
	if (ret == 0 && client)
		ret = dssh_session_set_hostkey_verify_cb(session, verify_host, &auth);
	if (ret == 0) {
		stage = "handshake";
		ret = dssh_transport_handshake(session);
	}
	if (ret == 0 && language_fields) {
		ret = check_language_fields(session, false);
		if (ret == 0)
			language_checks++;
	}
	if (ret == 0 && (strcmp(dssh_transport_get_kex_name(session), argv[2]) != 0
	    || strcmp(dssh_transport_get_hostkey_name(session), "rsa-sha2-256") != 0))
		ret = DSSH_ERROR_INVALID;
	if (ret == 0) {
		ret = check_algorithm_fields(session, cipher, cipher_s2c, mac, mac_s2c, 0);
		if (ret == 0)
			algorithm_checks++;
	}
	if (ret == 0) {
		stage = "authentication";
		struct dssh_auth_server_cbs callbacks = {
		    .methods_str = "publickey", .publickey_cb = authorize, .cbdata = &auth };
		ret = client ? dssh_auth_publickey(session, auth.user, "rsa-sha2-256") :
		    dssh_auth_server(session, &callbacks, NULL, NULL);
		if (ret == 0 && !client && !auth.signed_auth)
			ret = DSSH_ERROR_INVALID;
	}
	if (ret == 0) {
		stage = "start";
		ret = dssh_session_start(session);
	}
	if (ret == 0 && language_fields) {
		const char *first[] = {"de-DE"};
		const char *second[] = {"sv-SE"};
		ret = dssh_session_set_languages(session, DSSH_CLIENT_TO_SERVER, first, 1);
		if (ret == 0)
			ret = dssh_session_set_languages(session, DSSH_SERVER_TO_CLIENT, second, 1);
	}
	if (ret == 0) {
		stage = "channel";
		bool bidirectional = strcmp(mac, mac_s2c) != 0 || strcmp(cipher, cipher_s2c) != 0;
		ret = client ? client_channel(session, bidirectional,
		    strcmp(argv[2], "diffie-hellman-group-exchange-sha256") == 0) : server_channel(session, &channel_ack, bidirectional);
	}
	if (ret == 0 && language_fields) {
		ret = check_language_fields(session, true);
		if (ret == 0)
			language_checks++;
	}
	if (ret == 0) {
		ret = check_algorithm_fields(session, swap_rekey ? cipher_s2c : cipher,
		    swap_rekey ? cipher : cipher_s2c, swap_rekey ? mac_s2c : mac,
		    swap_rekey ? mac : mac_s2c, 1);
		if (ret == 0)
			algorithm_checks++;
	}
	dssh_session_cleanup(session);
	if (notification_flags) {
		unsigned calls = atomic_load(&notifications.count);
		bool valid = calls == 4 && !atomic_load(&notifications.bad);
		if (ret == 0 && !valid)
			ret = DSSH_ERROR_INVALID;
		if (fprintf(stderr, "DeuceSSH notifications: calls=%u valid=%u\n", calls, valid ? 1U : 0U) < 0)
			ret = DSSH_ERROR_INIT;
	}
	if (break_actions) {
		unsigned calls = atomic_load(&break_observation.count);
		bool valid = calls == 14 && !atomic_load(&break_observation.bad);
		if (ret == 0 && !valid)
			ret = DSSH_ERROR_INVALID;
		if (fprintf(stderr, "DeuceSSH break actions: calls=%u valid=%u\n", calls, valid ? 1U : 0U) < 0)
			ret = DSSH_ERROR_INIT;
	}
	if (ret == 0 && client && auth.host_checks < 2)
		ret = DSSH_ERROR_INVALID;
	if (fprintf(stderr, "DeuceSSH interop: role=%s kex=%s cipher=%s mac=%s stage=%s result=%d host-checks=%u signed-auth=%d channel-ack=%u language-checks=%u mac-s2c=%s algorithm-checks=%u peer-seconds=%u\n",
	    argv[1], argv[2], cipher, mac, stage, ret, auth.host_checks, auth.signed_auth, channel_ack ? 1U : 0U, language_checks, mac_s2c, algorithm_checks, peer_seconds) < 0
	    || fflush(stderr) != 0)
		return 1;
	return ret == 0 ? 0 : 1;
}
