/* Shared hybrid protocol with retained, independently leased session storage. */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "workspace.h"
#include "hybrid-pq-kex-ops.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

enum hybrid_slot { HYBRID_PUBLIC, HYBRID_REPLY, HYBRID_PQ_SECRET,
	HYBRID_CLASSICAL_SECRET, HYBRID_PQ_KEY, HYBRID_HOSTKEY, HYBRID_SLOTS };

struct hybrid_hash {
	dssh_hash_ctx *context;
	bool dirty;
};

struct hybrid_state {
	struct dssh_kex_workspace *workspace;
	const struct hybrid_pq_params *params;
	const struct hybrid_pq_ops *ops;
	struct hybrid_hash hash;
	struct hybrid_hash fingerprint;
	void *generation;
	bool generation_active;
	bool generation_dirty;
	bool client;
};

static int
prepare_hash(struct hybrid_hash *hash, const char *name, size_t expected)
{
	if (hash->context != NULL && !hash->dirty)
		return 0;
	dssh_hash_free(hash->context);
	hash->context = NULL;
	size_t length;
	int ret = dssh_hash_init(&hash->context, name, &length);
	if (ret < 0)
		return ret;
	if (length != expected) {
		dssh_hash_free(hash->context);
		hash->context = NULL;
		return DSSH_ERROR_INIT;
	}
	hash->dirty = false;
	return 0;
}

DSSH_PRIVATE void
hybrid_pq_cleanup(void *opaque)
{
	struct hybrid_state *state = opaque;
	if (state == NULL)
		return;
	dssh_kex_workspace_destroy(state->workspace);
	dssh_hash_free(state->hash.context);
	dssh_hash_free(state->fingerprint.context);
	if (state->generation_active)
		state->ops->x25519->cleanup(state->generation);
	dssh_cleanse(state, sizeof(*state));
	free(state);
}

DSSH_PRIVATE int
hybrid_pq_init_impl(bool client, void *config, void **opaque,
    const struct hybrid_pq_params *p, const struct hybrid_pq_ops *ops)
{
	(void)config;
	if (opaque == NULL || p == NULL || ops == NULL || p->hash_name == NULL
	    || p->digest_len == 0 || p->pq_pk_len == 0 || p->pq_ct_len == 0 || p->pq_ss_len == 0
	    || ops->x25519 == NULL || ops->x25519->init == NULL || ops->x25519->cleanup == NULL
	    || ops->x25519->keygen == NULL || ops->x25519->derive == NULL || ops->x25519->free_priv == NULL
	    || ops->pq_keygen == NULL || ops->pq_encaps == NULL || ops->pq_decaps == NULL || ops->pq_free_sk == NULL)
		return DSSH_ERROR_INVALID;
	/* Method-specific constants bound storage independently of received lengths.
	 * Check sums before constructing public-value/packet capacities. */
	if (p->pq_pk_len > SIZE_MAX - DSSH_X25519_LEN || p->pq_ct_len > SIZE_MAX - DSSH_X25519_LEN)
		return DSSH_ERROR_INVALID;
	if (p->q_c_len != p->pq_pk_len + DSSH_X25519_LEN || p->q_s_len != p->pq_ct_len + DSSH_X25519_LEN
	    || p->q_c_len > UINT32_MAX || p->q_s_len > UINT32_MAX || p->q_c_len > SIZE_MAX - 5)
		return DSSH_ERROR_INVALID;
	struct hybrid_state *state = *opaque;
	bool fresh = state == NULL;
	int ret;
	if (fresh) {
		state = malloc(sizeof(*state));
		if (state == NULL)
			return DSSH_ERROR_ALLOC;
		*state = (struct hybrid_state){.params = p, .ops = ops, .client = client};
		ret = dssh_kex_workspace_create(HYBRID_SLOTS, &state->workspace);
		if (ret < 0)
			goto failed;
	}
	else {
		if (state->params != p || state->ops != ops || state->client != client)
			return DSSH_ERROR_INVALID;
		ret = dssh_kex_workspace_reset(state->workspace);
		if (ret < 0)
			return ret;
	}
	ret = prepare_hash(&state->hash, p->hash_name, p->digest_len);
	if (ret < 0)
		goto failed;
	/* SHA-256 KEX can reuse its reset hash for host fingerprints. SHA-512
	 * clients retain a separate SHA-256 context; H itself remains untouched. */
	if (client && strcmp(p->hash_name, "SHA-256") != 0) {
		ret = prepare_hash(&state->fingerprint, "SHA-256", 32);
		if (ret < 0)
			goto failed;
	}
	if (fresh || state->generation_dirty) {
		if (state->generation_active)
			ops->x25519->cleanup(state->generation);
		state->generation = NULL;
		state->generation_active = false;
		ret = ops->x25519->init(&state->generation);
		state->generation_active = ret == 0 || state->generation != NULL;
		if (ret > 0)
			ret = DSSH_ERROR_INVALID;
		if (ret < 0)
			goto failed;
		state->generation_dirty = false;
	}
	if (fresh)
		*opaque = state;
	return 0;
failed:
	if (fresh)
		hybrid_pq_cleanup(state);
	return ret;
}

/* Caller supplies a fresh/reset hash matching the immutable method parameters.
 * Output is producer-owned storage, not an allocating/ownership-transfer helper. */
DSSH_TESTABLE int
hybrid_pq_compute_shared_secret(dssh_hash_ctx *hash, const struct hybrid_pq_params *p,
    const uint8_t *pq, const uint8_t *classical, uint8_t *out, size_t capacity)
{
	if (hash == NULL || p == NULL || pq == NULL || classical == NULL || out == NULL || p->digest_len == 0)
		return DSSH_ERROR_INVALID;
	if (capacity < p->digest_len)
		return DSSH_ERROR_TOOLONG;
	uint8_t nonzero = 0;
	for (size_t i = 0; i < DSSH_X25519_LEN; i++)
		nonzero |= classical[i];
	if (nonzero == 0)
		return DSSH_ERROR_INVALID;
	int ret = dssh_hash_update(hash, pq, p->pq_ss_len);
	if (ret == 0)
		ret = dssh_hash_update(hash, classical, DSSH_X25519_LEN);
	if (ret == 0)
		ret = dssh_hash_final(hash, out, p->digest_len);
	return ret;
}

static int
hash_string(dssh_hash_ctx *hash, struct dssh_bytes bytes)
{
	if (bytes.len > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	uint32_t length = (uint32_t)bytes.len;
	uint8_t prefix[4];
	size_t pos = 0;
	int ret = dssh_serialize_uint32(length, prefix, sizeof(prefix), &pos);
	if (ret < 0)
		return ret;
	ret = dssh_hash_update(hash, prefix, sizeof(prefix));
	if (ret < 0)
		return ret;
	return dssh_hash_update(hash, bytes.data, bytes.len);
}

static int
exchange_hash(struct hybrid_state *state, const struct dssh_kex_context *ctx,
    struct dssh_bytes key, struct dssh_bytes client, struct dssh_bytes server,
    const uint8_t *secret, uint8_t *out)
{
	const uint8_t *v_c = (const uint8_t *)ctx->v_c;
	const uint8_t *v_s = (const uint8_t *)ctx->v_s;
	const struct dssh_bytes inputs[] = {
		{v_c, ctx->v_c_len}, {v_s, ctx->v_s_len}, {ctx->i_c, ctx->i_c_len},
		{ctx->i_s, ctx->i_s_len}, key, client, server, {secret, state->params->digest_len},
	};
	state->hash.dirty = true;
	int ret;
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		ret = hash_string(state->hash.context, inputs[i]);
		if (ret < 0)
			return ret;
	}
	ret = dssh_hash_final(state->hash.context, out, state->params->digest_len);
	if (ret == 0)
		state->hash.dirty = false;
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
	size_t size = (size_t)length_u32;
	if (!dssh_buffer_has(length, *pos, size))
		return DSSH_ERROR_PARSE;
	*out = (struct dssh_bytes){&payload[*pos], size};
	*pos += size;
	return 0;
}

static int
serialize_string(struct dssh_bytes bytes, uint8_t *payload, size_t capacity, size_t *pos)
{
	if (bytes.len > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	uint32_t length = (uint32_t)bytes.len;
	int ret = dssh_serialize_uint32(length, payload, capacity, pos);
	if (ret < 0)
		return ret;
	if (!dssh_buffer_has(capacity, *pos, bytes.len))
		return DSSH_ERROR_TOOLONG;
	memcpy(&payload[*pos], bytes.data, bytes.len);
	*pos += bytes.len;
	return 0;
}

DSSH_PRIVATE int
hybrid_pq_handler_impl(const struct dssh_kex_context *ctx, struct dssh_kex_result *out,
    const struct hybrid_pq_params *p, const struct hybrid_pq_ops *ops)
{
	if (ctx == NULL || ctx->state == NULL || out == NULL
	    || out->shared_secret.data != NULL || out->shared_secret.len != 0
	    || out->exchange_hash.data != NULL || out->exchange_hash.len != 0
	    || out->lease.release != NULL || out->lease.owner != NULL)
		return DSSH_ERROR_INVALID;
	struct hybrid_state *state = ctx->state;
	if (state->params != p || state->ops != ops || state->client != ctx->client
	    || state->hash.dirty || state->fingerprint.dirty || state->generation_dirty)
		return DSSH_ERROR_INVALID;
	dssh_key_algo ka = ctx->key_algo;
	struct dssh_bytes_result signature = {0};
	struct dssh_bytes hostkey = {0}, peer = {0}, peer_signature = {0};
	struct dssh_bytes our_public = {0};
	uint8_t *public = NULL, *reply = NULL, *raw_key = NULL, *pq_secret = NULL, *classical = NULL;
	uint8_t *secret = NULL, *hash = NULL, *payload = NULL;
	void *private_key = NULL, *pq_key = NULL;
	size_t length = 0, pos = 1;
	uint8_t type;
	int ret, drop;
	if (ctx->client) {
		ret = dssh_kex_workspace_get(state->workspace, HYBRID_PUBLIC, 5 + p->q_c_len, &public);
		if (ret < 0)
			goto done;
		our_public = (struct dssh_bytes){&public[5], p->q_c_len};
		if (p->pq_sk_len != 0) {
			ret = dssh_kex_workspace_get(state->workspace, HYBRID_PQ_KEY, p->pq_sk_len, &raw_key);
			if (ret < 0)
				goto done;
		}
		ret = ops->pq_keygen(&public[5], raw_key, p->pq_sk_len, &pq_key);
		if (ret != 0)
			goto done;
		ret = ops->x25519->keygen(state->generation, &public[5 + p->pq_pk_len], &private_key);
		state->generation_dirty = ret != 0;
		if (ret != 0)
			goto done;
		public[0] = SSH_MSG_KEX_ECDH_INIT;
		uint32_t public_length = (uint32_t)p->q_c_len; /* Checked during init. */
		ret = dssh_serialize_uint32(public_length, public, 5 + p->q_c_len, &pos);
		if (ret < 0)
			goto done;
		ret = ctx->send(public, 5 + p->q_c_len, ctx->io_ctx);
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
		if (peer.len != p->q_s_len || pos != length) {
			ret = DSSH_ERROR_PARSE;
			goto done;
		}
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
		/* End the producer borrow before callbacks can reuse its storage.
		 * The owned slot is cleared on every exit and retained for rekey. */
		uint8_t *saved_hostkey = NULL;
		ret = dssh_kex_workspace_get(state->workspace, HYBRID_HOSTKEY, hostkey.len, &saved_hostkey);
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
		ret = parse_string(payload, length, &pos, &peer);
		if (ret < 0)
			goto done;
		if (peer.len != p->q_c_len || pos != length) {
			ret = DSSH_ERROR_PARSE;
			goto done;
		}
		ret = dssh_kex_workspace_get(state->workspace, HYBRID_PUBLIC, p->q_s_len, &public);
		if (ret < 0)
			goto done;
		our_public = (struct dssh_bytes){public, p->q_s_len};
	}
	ret = dssh_kex_workspace_get(state->workspace, HYBRID_PQ_SECRET, p->pq_ss_len, &pq_secret);
	if (ret < 0)
		goto done;
	ret = dssh_kex_workspace_get(state->workspace, HYBRID_CLASSICAL_SECRET, DSSH_X25519_LEN, &classical);
	if (ret < 0)
		goto done;
	if (ctx->client) {
		ret = ops->pq_decaps(pq_secret, peer.data, p->pq_ct_len, pq_key);
		ops->pq_free_sk(pq_key);
		pq_key = NULL;
		if (ret != 0)
			goto done;
		ret = dssh_kex_workspace_drop(state->workspace, HYBRID_PQ_KEY);
		if (ret < 0)
			goto done;
	}
	else {
		ret = ops->pq_encaps(public, pq_secret, peer.data, p->pq_pk_len);
		if (ret != 0)
			goto done;
		ret = ops->x25519->keygen(state->generation, &public[p->pq_ct_len], &private_key);
		state->generation_dirty = ret != 0;
		if (ret != 0)
			goto done;
	}
	ret = ops->x25519->derive(private_key,
	    &peer.data[ctx->client ? p->pq_ct_len : p->pq_pk_len], DSSH_X25519_LEN, classical);
	ops->x25519->free_priv(private_key);
	private_key = NULL;
	if (ret != 0)
		goto done;
	ret = dssh_kex_workspace_result(state->workspace, p->digest_len, p->digest_len,
	    &out->lease, &secret, &hash);
	if (ret < 0)
		goto done;
	state->hash.dirty = true;
	ret = hybrid_pq_compute_shared_secret(state->hash.context, p, pq_secret, classical, secret, p->digest_len);
	if (ret < 0)
		goto done;
	state->hash.dirty = false;
	ret = dssh_kex_workspace_drop(state->workspace, HYBRID_PQ_SECRET);
	if (ret < 0)
		goto done;
	ret = dssh_kex_workspace_drop(state->workspace, HYBRID_CLASSICAL_SECRET);
	if (ret < 0)
		goto done;
	ret = exchange_hash(state, ctx, hostkey, ctx->client ? our_public : peer,
	    ctx->client ? peer : our_public, secret, hash);
	if (ret < 0)
		goto done;
	if (ctx->client) {
		ret = dssh_kex_workspace_drop(state->workspace, HYBRID_PUBLIC);
		if (ret < 0)
			goto done;
		if (ka == NULL || ka->verify == NULL) {
			ret = DSSH_ERROR_INIT;
			goto done;
		}
		if (ctx->hostkey_verify != NULL) {
			struct hybrid_hash *fp_hash = state->fingerprint.context != NULL ? &state->fingerprint : &state->hash;
			uint8_t fingerprint[32];
			fp_hash->dirty = true;
			ret = dssh_hash_update(fp_hash->context, hostkey.data, hostkey.len);
			if (ret < 0)
				goto done;
			ret = dssh_hash_final(fp_hash->context, fingerprint, sizeof(fingerprint));
			if (ret < 0)
				goto done;
			fp_hash->dirty = false;
			unsigned int bits = ka->key_bits != NULL ? ka->key_bits(hostkey.data, hostkey.len) : 0;
			if (ctx->hostkey_verify(ka->name, bits, fingerprint, hostkey.data, hostkey.len,
			        ctx->hostkey_verify_cbdata) != DSSH_HOSTKEY_ACCEPT) {
				ret = DSSH_ERROR_INVALID;
				goto done;
			}
		}
		ret = ka->verify(hostkey.data, hostkey.len, peer_signature.data, peer_signature.len, hash, p->digest_len);
		if (ret != 0)
			goto done;
	}
	else {
		ret = ka->sign(&signature, hash, p->digest_len, ka->ctx);
		if (ret != 0)
			goto done;
		if (signature.lease.release == NULL || signature.bytes.data == NULL || signature.bytes.len == 0
		    || signature.bytes.len > UINT32_MAX) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		size_t size = 1 + 4 + 4 + 4;
		if (hostkey.len > SIZE_MAX - size) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		size += hostkey.len;
		if (our_public.len > SIZE_MAX - size) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		size += our_public.len;
		if (signature.bytes.len > SIZE_MAX - size) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		size += signature.bytes.len;
		ret = dssh_kex_workspace_get(state->workspace, HYBRID_REPLY, size, &reply);
		if (ret < 0)
			goto done;
		reply[0] = SSH_MSG_KEX_ECDH_REPLY;
		pos = 1;
		ret = serialize_string(hostkey, reply, size, &pos);
		if (ret < 0)
			goto done;
		ret = serialize_string(our_public, reply, size, &pos);
		if (ret < 0)
			goto done;
		ret = serialize_string(signature.bytes, reply, size, &pos);
		if (ret < 0)
			goto done;
		dssh_result_release(&signature.lease);
		ret = dssh_kex_workspace_drop(state->workspace, HYBRID_PUBLIC);
		if (ret < 0)
			goto done;
		ret = ctx->send(reply, pos, ctx->io_ctx);
		if (ret != 0)
			goto done;
	}
	ret = 0;
done:
	ops->x25519->free_priv(private_key);
	ops->pq_free_sk(pq_key);
	dssh_result_release(&signature.lease);
	for (size_t slot = 0; slot < HYBRID_SLOTS; slot++) {
		drop = dssh_kex_workspace_drop(state->workspace, slot);
		if (drop < 0 && ret == 0)
			ret = drop;
	}
	if (ret > 0)
		ret = DSSH_ERROR_INVALID;
	if (ret < 0)
		dssh_result_release(&out->lease);
	else {
		out->shared_secret = (struct dssh_bytes){secret, p->digest_len};
		out->exchange_hash = (struct dssh_bytes){hash, p->digest_len};
	}
	return ret;
}
