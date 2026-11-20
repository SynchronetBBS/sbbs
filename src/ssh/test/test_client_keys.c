/* Public C17 consumer: independent identities on simultaneous connections.
 * The custom signer is a deterministic fixture, not a cryptographic module. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "deucessh-auth.h"
#include "deucessh-algorithms.h"
#include "deucessh-key-algo.h"
#include "deucessh-crypto.h"
#include "mock_io.h"

#define CUSTOM "test-client-key"
#define CUSTOM_LEN (sizeof(CUSTOM) - 1)
#define KEY_TEXT_SIZE 4096

struct dssh_key_algo_ctx {
	dssh_session sess;
	uint8_t blob[4 + CUSTOM_LEN + 4 + 1];
	uint8_t signature[4 + CUSTOM_LEN + 4 + 33];
	unsigned releases, signatures;
	bool fail_sign, bad_pub, valid;
};

static void
context_release(void *opaque)
{
	struct dssh_key_algo_ctx *ctx = opaque;
	ctx->releases++;
	ctx->valid &= dssh_session_clear_client_key(ctx->sess, CUSTOM) == DSSH_ERROR_INVALID;
}

static void
signature_release(void *opaque)
{
	struct dssh_key_algo_ctx *ctx = opaque;
	ctx->signatures++;
	ctx->valid &= dssh_session_clear_client_key(ctx->sess, CUSTOM) == DSSH_ERROR_INVALID;
}

static int
custom_haskey(dssh_key_algo_ctx *ctx)
{
	return ctx != NULL;
}

static int
custom_pubkey(const uint8_t **out, size_t *length, dssh_key_algo_ctx *ctx)
{
	if (ctx == NULL)
		return DSSH_ERROR_INIT;
	*out = ctx->blob;
	*length = ctx->bad_pub ? 0 : sizeof(ctx->blob);
	return 0;
}

static int
custom_sign(struct dssh_bytes_result *out, const uint8_t *data, size_t length, dssh_key_algo_ctx *ctx)
{
	if (ctx == NULL)
		return DSSH_ERROR_INIT;
	ctx->valid &= dssh_session_clear_client_key(ctx->sess, CUSTOM) == DSSH_ERROR_INVALID;
	out->bytes.data = ctx->signature;
	out->bytes.len = sizeof(ctx->signature);
	out->lease = (struct dssh_result_lease){ signature_release, ctx };
	if (ctx->fail_sign)
		return DSSH_ERROR_INIT;
	size_t pos = 0;
	int ret = dssh_serialize_uint32(CUSTOM_LEN, ctx->signature, sizeof(ctx->signature), &pos);
	if (ret < 0)
		return ret;
	memcpy(ctx->signature + pos, CUSTOM, CUSTOM_LEN);
	pos += CUSTOM_LEN;
	ret = dssh_serialize_uint32(33, ctx->signature, sizeof(ctx->signature), &pos);
	if (ret < 0)
		return ret;
	ctx->signature[pos++] = ctx->blob[sizeof(ctx->blob) - 1];
	return dssh_hash_oneshot("SHA-256", data, length, ctx->signature + pos, 32);
}

static int
custom_verify(const uint8_t *key, size_t key_length, const uint8_t *sig, size_t sig_length,
    const uint8_t *data, size_t data_length)
{
	uint8_t digest[32];
	if (key_length != 4 + CUSTOM_LEN + 4 + 1 || sig_length != 4 + CUSTOM_LEN + 4 + 33)
		return DSSH_ERROR_INVALID;
	if (memcmp(key + 4, CUSTOM, CUSTOM_LEN) != 0 || memcmp(sig + 4, CUSTOM, CUSTOM_LEN) != 0
	    || key[key_length - 1] != sig[4 + CUSTOM_LEN + 4])
		return DSSH_ERROR_INVALID;
	int ret = dssh_hash_oneshot("SHA-256", data, data_length, digest, sizeof(digest));
	if (ret < 0)
		return ret;
	return memcmp(digest, sig + sig_length - 32, 32) == 0 ? 0 : DSSH_ERROR_INVALID;
}

static unsigned int
custom_bits(const uint8_t *key, size_t length)
{
	(void)key;
	return length == 4 + CUSTOM_LEN + 4 + 1 ? 256 : 0;
}

static bool
register_custom(void)
{
	dssh_key_algo module = malloc(sizeof(*module) + sizeof(CUSTOM));
	if (module == NULL)
		return false;
	*module = (struct dssh_key_algo_s){ .sign = custom_sign, .verify = custom_verify,
	    .pubkey = custom_pubkey, .haskey = custom_haskey, .key_bits = custom_bits,
	    .flags = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE };
	memcpy(module->name, CUSTOM, sizeof(CUSTOM));
	int ret = dssh_transport_register_key_algo(module);
	if (ret < 0)
		free(module);
	return ret == 0;
}

static bool
init_context(dssh_key_algo_ctx *ctx, dssh_session sess, uint8_t identity)
{
	*ctx = (struct dssh_key_algo_ctx){ .sess = sess, .valid = true };
	size_t pos = 0;
	int ret = dssh_serialize_uint32(CUSTOM_LEN, ctx->blob, sizeof(ctx->blob), &pos);
	if (ret < 0)
		return false;
	memcpy(ctx->blob + pos, CUSTOM, CUSTOM_LEN);
	pos += CUSTOM_LEN;
	ret = dssh_serialize_uint32(1, ctx->blob, sizeof(ctx->blob), &pos);
	if (ret < 0)
		return false;
	ctx->blob[pos] = identity;
	return true;
}

static bool
format_blob(char *text, size_t capacity, const uint8_t *blob, size_t length)
{
	uint32_t count;
	if (dssh_parse_uint32(blob, length, &count) < 0 || count == 0 || count > 64)
		return false;
	size_t size = (size_t)count;
	if (!dssh_buffer_has(length, 4, size) || capacity <= size + 1)
		return false;
	memcpy(text, blob + 4, size);
	text[size] = ' ';
	return dssh_base64_encode(blob, length, text + size + 1, capacity - size - 1) >= 0;
}

static bool
install_custom(dssh_session sess, dssh_key_algo_ctx *ctx)
{
	struct dssh_result_lease lease = { context_release, ctx };
	int ret = dssh_session_set_client_key_ctx(sess, CUSTOM, ctx, &lease);
	bool valid = ret == 0 && lease.release == NULL && lease.owner == NULL;
	if (lease.release != NULL)
		dssh_result_release(&lease);
	return valid;
}

struct side {
	struct mock_io_state *io;
	bool client;
};

struct pair {
	struct mock_io_state io;
	struct side sides[2];
	dssh_session client, server;
	const char *algorithm, *replacement_file;
	char expected[2][KEY_TEXT_SIZE];
	dssh_key_algo_ctx contexts[2];
	unsigned calls;
	bool custom, fallback, valid;
};

static int
tx(uint8_t *data, size_t length, dssh_session sess, void *opaque)
{
	struct side *side = opaque;
	return side->client ? mock_tx_client(data, length, sess, side->io)
	    : mock_tx_server(data, length, sess, side->io);
}

static int
rx(uint8_t *data, size_t length, dssh_session sess, void *opaque)
{
	struct side *side = opaque;
	return side->client ? mock_rx_client(data, length, sess, side->io)
	    : mock_rx_server(data, length, sess, side->io);
}

static int
line(uint8_t *data, size_t length, size_t *received, dssh_session sess, void *opaque)
{
	struct side *side = opaque;
	return side->client ? mock_rxline_client(data, length, received, sess, side->io)
	    : mock_rxline_server(data, length, received, sess, side->io);
}

static dssh_hostkey_decision
trust(const char *algorithm, unsigned bits, const uint8_t *hash,
    const uint8_t *key, size_t length, void *opaque)
{
	(void)algorithm;
	(void)bits;
	(void)hash;
	(void)key;
	(void)length;
	(void)opaque;
	return DSSH_HOSTKEY_ACCEPT;
}

static void
terminate_io(dssh_session sess, void *opaque)
{
	(void)sess;
	struct mock_io_state *io = opaque;
	mock_io_close_c2s(io);
	mock_io_close_s2c(io);
}

static int
accept_key(const uint8_t *username, size_t user_length, const char *algorithm,
    const uint8_t *blob, size_t length, bool signed_request, void *opaque)
{
	struct pair *pair = opaque;
	char actual[KEY_TEXT_SIZE];
	unsigned attempt = pair->calls++;
	if (attempt > 1 || !signed_request || user_length != 4 || memcmp(username, "user", 4) != 0
	    || strcmp(algorithm, pair->algorithm) != 0 || !format_blob(actual, sizeof(actual), blob, length)
	    || strcmp(actual, pair->expected[attempt]) != 0) {
		pair->valid = false;
		return DSSH_AUTH_DISCONNECT;
	}
	return attempt == 0 ? DSSH_AUTH_FAILURE : DSSH_AUTH_SUCCESS;
}

static int
server_worker(void *opaque)
{
	struct pair *pair = opaque;
	int ret = dssh_transport_handshake(pair->server);
	struct dssh_auth_server_cbs cbs = { .methods_str = "publickey", .publickey_cb = accept_key, .cbdata = pair };
	if (ret == 0)
		ret = dssh_auth_server(pair->server, &cbs, NULL, NULL);
	if (ret != 0)
		terminate_io(pair->server, &pair->io);
	return ret;
}

static int
client_worker(void *opaque)
{
	struct pair *pair = opaque;
	dssh_session sess = pair->client;
	const char *algorithm = pair->algorithm;
	int ret = dssh_transport_handshake(sess);
	if (ret == 0 && pair->custom) {
		pair->contexts[0].fail_sign = true;
		ret = dssh_auth_publickey(sess, "user", algorithm) == DSSH_ERROR_INIT ? 0 : -1;
		pair->contexts[0].fail_sign = false;
	}
	if (ret == 0)
		ret = dssh_auth_publickey(sess, "user", algorithm) == DSSH_AUTH_CONTINUE ? 0 : -1;
	if (ret == 0 && pair->custom) {
		struct dssh_result_lease lease = { context_release, &pair->contexts[1] };
		pair->contexts[1].bad_pub = true;
		ret = dssh_session_set_client_key_ctx(sess, CUSTOM, &pair->contexts[1], &lease) == DSSH_ERROR_INVALID
		    && lease.release == context_release && lease.owner == &pair->contexts[1] ? 0 : -1;
		pair->contexts[1].bad_pub = false;
		if (ret == 0)
			ret = dssh_session_set_client_key_ctx(sess, CUSTOM, &pair->contexts[1], &lease);
		if (lease.release != NULL)
			dssh_result_release(&lease);
	}
	else if (ret == 0) {
		char unchanged[KEY_TEXT_SIZE];
		ret = dssh_session_load_client_key_file(sess, algorithm, "", NULL, NULL) == DSSH_ERROR_INVALID
		    && dssh_session_get_client_key_pub_str(sess, algorithm, unchanged, sizeof(unchanged)) > 0
		    && strcmp(unchanged, pair->expected[0]) == 0 ? 0 : -1;
		if (ret == 0 && strcmp(algorithm, "rsa-sha2-256") != 0) {
			ret = dssh_session_clear_client_key(sess, algorithm);
			if (ret == 0)
				ret = dssh_auth_publickey(sess, "user", algorithm) == DSSH_ERROR_INIT ? 0 : -1;
		}
		if (ret == 0)
			ret = dssh_session_load_client_key_file(sess, algorithm, pair->replacement_file, NULL, NULL);
		if (ret == 0)
			ret = dssh_session_get_client_key_pub_str(sess, algorithm, unchanged, sizeof(unchanged)) > 0
			    && strcmp(unchanged, pair->fallback ? pair->expected[0] : pair->expected[1]) == 0 ? 0 : -1;
		if (ret == 0 && pair->fallback)
			ret = dssh_session_clear_client_key(sess, algorithm);
	}
	if (ret == 0)
		ret = dssh_auth_publickey(sess, "user", algorithm);
	if (ret == 0)
		ret = dssh_session_clear_client_key(sess, algorithm) == DSSH_ERROR_TOOLATE
		    && dssh_session_generate_client_key(sess, algorithm, 256) == DSSH_ERROR_TOOLATE ? 0 : -1;
	if (ret != 0)
		terminate_io(sess, &pair->io);
	return ret;
}

static bool
init_pair(struct pair *pair, const char *algorithm, bool custom)
{
	*pair = (struct pair){ .algorithm = algorithm, .custom = custom, .valid = true };
	if (mock_io_init(&pair->io, 0) != 0)
		return false;
	pair->client = dssh_session_init(true, 0);
	pair->server = dssh_session_init(false, 0);
	if (pair->client == NULL || pair->server == NULL)
		return false;
	pair->sides[0] = (struct side){ &pair->io, true };
	pair->sides[1] = (struct side){ &pair->io, false };
	return dssh_session_set_cbdata(pair->client, &pair->sides[0], &pair->sides[0], &pair->sides[0], NULL) == 0
	    && dssh_session_set_cbdata(pair->server, &pair->sides[1], &pair->sides[1], &pair->sides[1], NULL) == 0
	    && dssh_session_set_timeout(pair->client, 5000) == 0
	    && dssh_session_set_timeout(pair->server, 5000) == 0
	    && dssh_session_set_terminate_cb(pair->client, terminate_io, &pair->io) == 0
	    && dssh_session_set_terminate_cb(pair->server, terminate_io, &pair->io) == 0
	    && dssh_session_set_hostkey_verify_cb(pair->client, trust, NULL) == 0;
}

static bool
client_only(void)
{
	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL)
		return false;
	char text[KEY_TEXT_SIZE];
	bool valid = dssh_ed25519_get_pub_str(text, sizeof(text)) == DSSH_ERROR_INIT
	    && dssh_rsa_sha2_256_get_pub_str(text, sizeof(text)) == DSSH_ERROR_INIT
	    && dssh_session_generate_client_key(sess, "ssh-ed25519", 256) == 0
	    && dssh_session_get_client_key_pub_str(sess, "ssh-ed25519", text, sizeof(text)) > 0
	    && dssh_session_generate_client_key(sess, "ssh-ed25519", 255) == DSSH_ERROR_INVALID
	    && dssh_ed25519_generate_key() == DSSH_ERROR_TOOLATE
	    && dssh_session_clear_client_key(sess, "ssh-ed25519") == 0
	    && dssh_session_get_client_key_pub_str(sess, "ssh-ed25519", text, sizeof(text)) == DSSH_ERROR_INIT;
	dssh_session_cleanup(sess);
	return valid;
}

int
main(int argc, char **argv)
{
	if (argc != 3)
		return 2;
	const char *algorithm = argv[1];
	bool custom = strcmp(algorithm, CUSTOM) == 0;
	bool only = strcmp(algorithm, "client-only") == 0;
	if (dssh_register_curve25519_sha256() != 0 || dssh_register_ssh_ed25519() != 0
	    || dssh_register_rsa_sha2_256() != 0 || dssh_register_rsa_sha2_512() != 0
	    || dssh_register_aes256_ctr() != 0 || dssh_register_hmac_sha2_256() != 0
	    || dssh_register_none_comp() != 0 || !register_custom()
	    || dssh_transport_set_callbacks(NULL, NULL, NULL, NULL) != DSSH_ERROR_INVALID)
		return 2;
	if (only) {
		bool valid = client_only();
		printf("client_keys algorithm=client-only no_global=1 valid=%d\n", valid);
		return valid ? 0 : 1;
	}
	if (dssh_rsa_sha2_256_generate_key(2048) != 0
	    || dssh_transport_set_callbacks(tx, rx, line, NULL) != 0)
		return 2;
	/* Dispatch by session role while preserving independent socket owners. */
	struct pair pairs[2] = {0};
	bool ready[2] = {false};
	bool valid = true;
	char files[2][1024], public_file[1024];
	bool saved[2] = {false}, public_saved = false;
	for (unsigned i = 0; i < 2; i++) {
		int count = snprintf(files[i], sizeof(files[i]), "%s-%u.pem", argv[2], i);
		valid &= count >= 0 && (size_t)count < sizeof(files[i]);
	}
	int count = snprintf(public_file, sizeof(public_file), "%s.pub", argv[2]);
	valid &= count >= 0 && (size_t)count < sizeof(public_file);
	for (unsigned i = 0; i < 2 && valid; i++) {
		ready[i] = init_pair(&pairs[i], algorithm, custom);
		valid = ready[i];
		if (!valid)
			break;
		if (custom) {
			valid = init_context(&pairs[i].contexts[0], pairs[i].client, i == 0 ? 1 : 2)
			    && init_context(&pairs[i].contexts[1], pairs[i].client, i == 0 ? 3 : 4)
			    && install_custom(pairs[i].client, &pairs[i].contexts[0])
			    && format_blob(pairs[i].expected[1], sizeof(pairs[i].expected[1]),
			        pairs[i].contexts[1].blob, sizeof(pairs[i].contexts[1].blob));
		}
		else {
			unsigned bits = strcmp(algorithm, "ssh-ed25519") == 0 ? 256 : 2048;
			valid = dssh_session_generate_client_key(pairs[i].client, algorithm, bits) == 0;
			if (valid) {
				saved[i] = dssh_session_save_client_key_file(pairs[i].client, algorithm, files[i], NULL, NULL) == 0;
				valid = saved[i];
			}
		}
		if (valid)
			valid = dssh_session_get_client_key_pub_str(pairs[i].client, algorithm,
			    pairs[i].expected[0], sizeof(pairs[i].expected[0])) > 0;
	}
	if (valid && !custom) {
		pairs[0].fallback = strcmp(algorithm, "rsa-sha2-256") == 0;
		pairs[0].replacement_file = files[pairs[0].fallback ? 0 : 1];
		pairs[1].replacement_file = files[0];
		memcpy(pairs[1].expected[1], pairs[0].expected[0], sizeof(pairs[1].expected[1]));
		if (pairs[0].fallback)
			valid = dssh_rsa_sha2_256_get_pub_str(pairs[0].expected[1], sizeof(pairs[0].expected[1])) > 0;
		else
			memcpy(pairs[0].expected[1], pairs[1].expected[0], sizeof(pairs[0].expected[1]));
		public_saved = dssh_session_save_client_pub_file(pairs[0].client, algorithm, public_file) == 0;
		valid &= public_saved;
		if (valid) {
			char unchanged[KEY_TEXT_SIZE];
			valid = dssh_session_load_client_key_file(pairs[0].client, algorithm, public_file, NULL, NULL) < 0
			    && dssh_session_get_client_key_pub_str(pairs[0].client, algorithm, unchanged, sizeof(unchanged)) > 0
			    && strcmp(unchanged, pairs[0].expected[0]) == 0;
		}
	}
	if (valid)
		valid = strcmp(pairs[0].expected[0], pairs[1].expected[0]) != 0;
	thrd_t workers[4];
	bool started[4] = {false};
	for (unsigned i = 0; i < 4 && valid; i++) {
		started[i] = thrd_create(&workers[i], i % 2 == 0 ? client_worker : server_worker, &pairs[i / 2]) == thrd_success;
		valid = started[i];
	}
	if (!valid) {
		for (unsigned i = 0; i < 2; i++) {
			if (ready[i])
				terminate_io(pairs[i].client, &pairs[i].io);
		}
	}
	for (unsigned i = 0; i < 4; i++) {
		if (!started[i])
			continue;
		int result;
		if (thrd_join(workers[i], &result) != thrd_success)
			abort();
		valid &= result == 0;
	}
	for (unsigned i = 0; i < 2; i++) {
		if (ready[i]) {
			valid &= pairs[i].valid && pairs[i].calls == 2;
			dssh_session_cleanup(pairs[i].client);
			dssh_session_cleanup(pairs[i].server);
			mock_io_free(&pairs[i].io);
			if (custom)
				valid &= pairs[i].contexts[0].releases == 1 && pairs[i].contexts[1].releases == 1
				    && pairs[i].contexts[0].signatures == 2 && pairs[i].contexts[1].signatures == 1
				    && pairs[i].contexts[0].valid && pairs[i].contexts[1].valid;
		}
		if (saved[i])
			valid &= remove(files[i]) == 0;
	}
	if (public_saved)
		valid &= remove(public_file) == 0;
	printf("client_keys algorithm=%s connections=2 attempts=%u/%u local=1 replacement=1 valid=%d\n",
	    algorithm, pairs[0].calls, pairs[1].calls, valid);
	return valid ? 0 : 1;
}
