// RFC 8731: curve25519-sha256 -- shared protocol and retained session storage.
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "curve25519-sha256-ops.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "workspace.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

enum curve_slot { CURVE_MPINT, CURVE_REPLY, CURVE_HOSTKEY, CURVE_SLOTS };
#define CURVE_MPINT_CAPACITY (4 + 1 + DSSH_X25519_LEN)

struct curve_state {
	struct dssh_kex_workspace *workspace;
	dssh_hash_ctx *hash;
	const struct dssh_x25519_ops *ops;
	void *backend;
	bool backend_active;
	bool backend_dirty;
	bool hash_dirty;
	bool client;
};

DSSH_PRIVATE void
curve25519_cleanup(void *opaque)
{
	struct curve_state *state = opaque;
	if (state == NULL)
		return;
	dssh_kex_workspace_destroy(state->workspace);
	dssh_hash_free(state->hash);
	if (state->backend_active)
		state->ops->cleanup(state->backend);
	dssh_cleanse(state, sizeof(*state));
	free(state);
}

DSSH_PRIVATE int
curve25519_init_impl(bool client, void *config, void **opaque, const struct dssh_x25519_ops *ops)
{
	(void)config;
	if (opaque == NULL || ops == NULL || ops->init == NULL || ops->cleanup == NULL
	    || ops->keygen == NULL || ops->derive == NULL || ops->free_priv == NULL)
		return DSSH_ERROR_INVALID;
	struct curve_state *state = *opaque;
	bool fresh = state == NULL;
	int ret;
	if (fresh) {
		state = malloc(sizeof(*state));
		if (state == NULL)
			return DSSH_ERROR_ALLOC;
		*state = (struct curve_state){.ops = ops, .client = client};
		ret = dssh_kex_workspace_create(CURVE_SLOTS, &state->workspace);
		if (ret < 0)
			goto failed;
	}
	else {
		if (state->ops != ops || state->client != client)
			return DSSH_ERROR_INVALID;
		ret = dssh_kex_workspace_reset(state->workspace);
		if (ret < 0)
			return ret;
	}
	/* A successful final resets the hash for reuse. Rebuild only after an
	 * interrupted/failed hash; never mix the next transcript with its prefix. */
	if (state->hash == NULL || state->hash_dirty) {
		dssh_hash_free(state->hash);
		state->hash = NULL;
		size_t digest_len;
		ret = dssh_hash_init(&state->hash, "SHA-256", &digest_len);
		if (ret < 0)
			goto failed;
		if (digest_len != SHA256_DIGEST_LEN) {
			ret = DSSH_ERROR_INIT;
			goto failed;
		}
		state->hash_dirty = false;
	}
	if (fresh || state->backend_dirty) {
		if (state->backend_active)
			ops->cleanup(state->backend);
		state->backend = NULL;
		state->backend_active = false;
		ret = ops->init(&state->backend);
		state->backend_active = ret == 0 || state->backend != NULL;
		if (ret > 0)
			ret = DSSH_ERROR_INVALID;
		if (ret < 0)
			goto failed;
		state->backend_dirty = false;
	}
	if (fresh)
		*opaque = state;
	return 0;
failed:
	if (fresh)
		curve25519_cleanup(state);
	return ret;
}

/* Borrow the significant raw bytes and encode their SSH mpint into independent
 * caller-owned storage. X25519 fixes the raw bound at 32 and encoded bound at 37.
 * No shared-secret allocation or copy is needed, including leading-zero cases. */
DSSH_TESTABLE int
encode_shared_secret(const uint8_t *raw, size_t raw_len, struct dssh_bytes *secret,
    uint8_t *mpint, size_t capacity, size_t *mpint_len)
{
	if (secret == NULL || mpint_len == NULL)
		return DSSH_ERROR_INVALID;
	*secret = (struct dssh_bytes){0};
	*mpint_len = 0;
	if (raw == NULL || raw_len == 0 || raw_len > DSSH_X25519_LEN || mpint == NULL)
		return DSSH_ERROR_INVALID;
	uint8_t nonzero = 0;
	for (size_t i = 0; i < raw_len; i++)
		nonzero |= raw[i];
	if (nonzero == 0)
		return DSSH_ERROR_INVALID;
	while (raw_len > 1 && raw[0] == 0) {
		raw++;
		raw_len--;
	}
	bool pad = (raw[0] & DSSH_MPINT_SIGN_BIT) != 0;
	size_t data_len = raw_len + (pad ? 1U : 0U);
	size_t size = 4 + data_len; /* Checked fixed bound: at most 37. */
	if (capacity < size)
		return DSSH_ERROR_TOOLONG;
	uint32_t wire_len = (uint32_t)data_len;
	size_t pos = 0;
	int ret = dssh_serialize_uint32(wire_len, mpint, capacity, &pos);
	if (ret < 0)
		return ret;
	if (pad)
		mpint[pos++] = 0;
	memcpy(&mpint[pos], raw, raw_len);
	*secret = (struct dssh_bytes){raw, raw_len};
	*mpint_len = size;
	return 0;
}

static int
hash_string(dssh_hash_ctx *hash, struct dssh_bytes bytes)
{
	if (bytes.len > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	uint32_t wire_len = (uint32_t)bytes.len;
	uint8_t prefix[4];
	size_t pos = 0;
	int ret = dssh_serialize_uint32(wire_len, prefix, sizeof(prefix), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_hash_update(hash, prefix, sizeof(prefix));
	if (ret < 0)
		return ret;
	return dssh_hash_update(hash, bytes.data, bytes.len);
}

static int
compute_exchange_hash(struct curve_state *state, const struct dssh_kex_context *ctx,
    struct dssh_bytes hostkey, const uint8_t *q_c, const uint8_t *q_s,
    const uint8_t *mpint, size_t mpint_len, uint8_t *out)
{
	const uint8_t *v_c = (const uint8_t *)ctx->v_c;
	const uint8_t *v_s = (const uint8_t *)ctx->v_s;
	const struct dssh_bytes inputs[] = {
		{v_c, ctx->v_c_len}, {v_s, ctx->v_s_len},
		{ctx->i_c, ctx->i_c_len}, {ctx->i_s, ctx->i_s_len}, hostkey,
		{q_c, DSSH_X25519_LEN}, {q_s, DSSH_X25519_LEN},
	};
	state->hash_dirty = true;
	int ret;
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		ret = hash_string(state->hash, inputs[i]);
		if (ret < 0)
			return ret;
	}
	ret = dssh_hash_update(state->hash, mpint, mpint_len);
	if (ret < 0)
		return ret;
	ret = dssh_hash_final(state->hash, out, SHA256_DIGEST_LEN);
	if (ret == 0)
		state->hash_dirty = false;
	return ret;
}

static int
parse_string(const uint8_t *payload, size_t length, size_t *pos, struct dssh_bytes *out)
{
	uint32_t length_u32;
	if (payload == NULL || !dssh_buffer_has(length, *pos, 4)
	    || dssh_parse_uint32(&payload[*pos], length - *pos, &length_u32) < 4)
		return DSSH_ERROR_PARSE;
	*pos += 4;
#if SIZE_MAX < UINT32_MAX
	if (length_u32 > SIZE_MAX)
		return DSSH_ERROR_PARSE;
#endif
	size_t string_len = (size_t)length_u32;
	if (!dssh_buffer_has(length, *pos, string_len))
		return DSSH_ERROR_PARSE;
	*out = (struct dssh_bytes){&payload[*pos], string_len};
	*pos += string_len;
	return 0;
}

static int
serialize_string(struct dssh_bytes bytes, uint8_t *payload, size_t capacity, size_t *pos)
{
	if (bytes.len > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	uint32_t wire_len = (uint32_t)bytes.len;
	int ret = dssh_serialize_uint32(wire_len, payload, capacity, pos);
	if (ret < 0)
		return ret;
	if (!dssh_buffer_has(capacity, *pos, bytes.len))
		return DSSH_ERROR_TOOLONG;
	memcpy(&payload[*pos], bytes.data, bytes.len);
	*pos += bytes.len;
	return 0;
}

DSSH_PRIVATE int
curve25519_handler_impl(const struct dssh_kex_context *ctx, struct dssh_kex_result *out,
    const struct dssh_x25519_ops *ops)
{
	if (ctx == NULL || ctx->state == NULL || out == NULL
	    || out->shared_secret.data != NULL || out->shared_secret.len != 0
	    || out->exchange_hash.data != NULL || out->exchange_hash.len != 0
	    || out->lease.release != NULL || out->lease.owner != NULL)
		return DSSH_ERROR_INVALID;
	struct curve_state *state = ctx->state;
	if (state->ops != ops || state->client != ctx->client || state->hash_dirty || state->backend_dirty)
		return DSSH_ERROR_INVALID;
	dssh_key_algo ka = ctx->key_algo;
	void *priv = NULL;
	struct dssh_bytes_result signature = {0};
	struct dssh_bytes hostkey = {0}, peer = {0}, peer_signature = {0}, secret = {0};
	uint8_t our_public[DSSH_X25519_LEN];
	uint8_t *raw = NULL, *hash = NULL, *mpint = NULL, *reply = NULL;
	uint8_t *payload = NULL;
	size_t length = 0, pos = 1, mpint_len = 0;
	uint8_t type;
	int ret, drop;
	if (ctx->client) {
		ret = ops->keygen(state->backend, our_public, &priv);
		state->backend_dirty = ret != 0;
		if (ret != 0)
			goto done;
		uint8_t init[1 + 4 + DSSH_X25519_LEN];
		init[0] = SSH_MSG_KEX_ECDH_INIT;
		struct dssh_bytes public_key = {our_public, sizeof(our_public)};
		ret = serialize_string(public_key, init, sizeof(init), &pos);
		if (ret < 0)
			goto done;
		ret = ctx->send(init, pos, ctx->io_ctx);
		if (ret != 0)
			goto done;
		ret = ctx->recv(&type, &payload, &length, ctx->io_ctx);
		if (ret != 0)
			goto done;
		if (type != SSH_MSG_KEX_ECDH_REPLY) {
			ret = DSSH_ERROR_PARSE;
			goto done;
		}
		pos = 1;
		ret = parse_string(payload, length, &pos, &hostkey);
		if (ret < 0)
			goto done;
		ret = parse_string(payload, length, &pos, &peer);
		if (ret < 0)
			goto done;
		ret = parse_string(payload, length, &pos, &peer_signature);
		if (ret < 0)
			goto done;
	}
	else {
		if (ka == NULL || ka->pubkey == NULL || ka->sign == NULL) {
			ret = DSSH_ERROR_INIT;
			goto done;
		}
		ret = ka->pubkey(&hostkey.data, &hostkey.len, ka->ctx);
		if (ret != 0)
			goto done;
		if (hostkey.data == NULL || hostkey.len == 0 || hostkey.len > UINT32_MAX) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		/* Capture a non-leased producer view before any later callback.
		 * Reuse the session's retained slot on subsequent exchanges. */
		uint8_t *saved_hostkey = NULL;
		ret = dssh_kex_workspace_get(state->workspace, CURVE_HOSTKEY, hostkey.len, &saved_hostkey);
		if (ret < 0)
			goto done;
		memcpy(saved_hostkey, hostkey.data, hostkey.len);
		hostkey.data = saved_hostkey;
		ret = ctx->recv(&type, &payload, &length, ctx->io_ctx);
		if (ret != 0)
			goto done;
		if (type != SSH_MSG_KEX_ECDH_INIT) {
			ret = DSSH_ERROR_PARSE;
			goto done;
		}
		pos = 1;
		ret = parse_string(payload, length, &pos, &peer);
		if (ret < 0)
			goto done;
	}
	if (peer.len != DSSH_X25519_LEN || pos != length) {
		ret = DSSH_ERROR_PARSE;
		goto done;
	}
	if (!ctx->client) {
		ret = ops->keygen(state->backend, our_public, &priv);
		state->backend_dirty = ret != 0;
		if (ret != 0)
			goto done;
	}
	ret = dssh_kex_workspace_result(state->workspace, DSSH_X25519_LEN, SHA256_DIGEST_LEN,
	    &out->lease, &raw, &hash);
	if (ret < 0)
		goto done;
	ret = ops->derive(priv, peer.data, peer.len, raw);
	if (ret != 0)
		goto done;
	ops->free_priv(priv);
	priv = NULL;
	ret = dssh_kex_workspace_get(state->workspace, CURVE_MPINT, CURVE_MPINT_CAPACITY, &mpint);
	if (ret < 0)
		goto done;
	ret = encode_shared_secret(raw, DSSH_X25519_LEN, &secret, mpint, CURVE_MPINT_CAPACITY, &mpint_len);
	if (ret < 0)
		goto done;
	ret = compute_exchange_hash(state, ctx, hostkey,
	    ctx->client ? our_public : peer.data, ctx->client ? peer.data : our_public, mpint, mpint_len, hash);
	if (ret < 0)
		goto done;
	ret = dssh_kex_workspace_drop(state->workspace, CURVE_MPINT);
	if (ret < 0)
		goto done;
	if (ctx->client) {
		if (ka == NULL || ka->verify == NULL) {
			ret = DSSH_ERROR_INIT;
			goto done;
		}
		if (ctx->hostkey_verify != NULL) {
			uint8_t fingerprint[SHA256_DIGEST_LEN];
			state->hash_dirty = true;
			ret = dssh_hash_update(state->hash, hostkey.data, hostkey.len);
			if (ret < 0)
				goto done;
			ret = dssh_hash_final(state->hash, fingerprint, sizeof(fingerprint));
			if (ret < 0)
				goto done;
			state->hash_dirty = false;
			unsigned int bits = ka->key_bits != NULL ? ka->key_bits(hostkey.data, hostkey.len) : 0;
			if (ctx->hostkey_verify(ka->name, bits, fingerprint, hostkey.data, hostkey.len,
			        ctx->hostkey_verify_cbdata) != DSSH_HOSTKEY_ACCEPT) {
				ret = DSSH_ERROR_INVALID;
				goto done;
			}
		}
		ret = ka->verify(hostkey.data, hostkey.len, peer_signature.data, peer_signature.len, hash, SHA256_DIGEST_LEN);
		if (ret != 0)
			goto done;
	}
	else {
		ret = ka->sign(&signature, hash, SHA256_DIGEST_LEN, ka->ctx);
		if (ret != 0)
			goto done;
		if (signature.lease.release == NULL || signature.bytes.data == NULL || signature.bytes.len == 0
		    || signature.bytes.len > UINT32_MAX) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		size_t size = 1 + 4 + 4 + DSSH_X25519_LEN + 4;
		if (hostkey.len > SIZE_MAX - size) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		size += hostkey.len;
		if (signature.bytes.len > SIZE_MAX - size) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		size += signature.bytes.len;
		ret = dssh_kex_workspace_get(state->workspace, CURVE_REPLY, size, &reply);
		if (ret < 0)
			goto done;
		pos = 1;
		reply[0] = SSH_MSG_KEX_ECDH_REPLY;
		ret = serialize_string(hostkey, reply, size, &pos);
		if (ret < 0)
			goto done;
		struct dssh_bytes public_key = {our_public, sizeof(our_public)};
		ret = serialize_string(public_key, reply, size, &pos);
		if (ret < 0)
			goto done;
		ret = serialize_string(signature.bytes, reply, size, &pos);
		if (ret < 0)
			goto done;
		dssh_result_release(&signature.lease);
		ret = ctx->send(reply, pos, ctx->io_ctx);
		if (ret != 0)
			goto done;
	}
	ret = 0;
done:
	ops->free_priv(priv);
	dssh_result_release(&signature.lease);
	drop = dssh_kex_workspace_drop(state->workspace, CURVE_MPINT);
	if (drop < 0 && ret == 0)
		ret = drop;
	drop = dssh_kex_workspace_drop(state->workspace, CURVE_REPLY);
	if (drop < 0 && ret == 0)
		ret = drop;
	drop = dssh_kex_workspace_drop(state->workspace, CURVE_HOSTKEY);
	if (drop < 0 && ret == 0)
		ret = drop;
	if (ret > 0)
		ret = DSSH_ERROR_INVALID;
	if (ret < 0)
		dssh_result_release(&out->lease);
	else {
		out->shared_secret = secret;
		out->exchange_hash = (struct dssh_bytes){hash, SHA256_DIGEST_LEN};
	}
	return ret;
}
