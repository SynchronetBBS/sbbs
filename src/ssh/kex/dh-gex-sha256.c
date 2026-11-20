/* RFC 4419 shared protocol, retained storage and producer-owned K/H results. */
#include <stdlib.h>
#include <string.h>
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "workspace.h"
#include "dh-gex-sha256-ops.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

enum dhgex_slot { GEX_GROUP, GEX_PUBLIC, GEX_REPLY, GEX_HOSTKEY, GEX_SLOTS };
struct dhgex_state {
	struct dssh_kex_workspace *workspace;
	dssh_hash_ctx *hash;
	dssh_hash_ctx *fingerprint_hash;
	const struct dssh_dh_fixed_group *fixed;
	size_t hash_len;
	const struct dhgex_ops *ops;
	void *native;
	bool native_active;
	bool hash_dirty;
	bool fingerprint_dirty;
	bool client;
};

DSSH_PRIVATE void
dhgex_cleanup(void *opaque)
{
	struct dhgex_state *state = opaque;
	if (state == NULL)
		return;
	dssh_kex_workspace_destroy(state->workspace);
	dssh_hash_free(state->hash);
	dssh_hash_free(state->fingerprint_hash);
	if (state->native_active)
		state->ops->cleanup(state->native);
	dssh_cleanse(state, sizeof(*state));
	free(state);
}

static int
dh_init(bool client, void **opaque, const struct dhgex_ops *ops,
    const struct dssh_dh_fixed_group *fixed)
{
	if (opaque == NULL || ops == NULL || ops->init == NULL || ops->reset == NULL
	    || ops->cleanup == NULL || ops->keygen == NULL || ops->derive == NULL)
		return DSSH_ERROR_INVALID;
	struct dhgex_state *state = *opaque;
	bool fresh = state == NULL;
	int ret;
	if (fresh) {
		state = malloc(sizeof(*state));
		if (state == NULL)
			return DSSH_ERROR_ALLOC;
		*state = (struct dhgex_state){.ops = ops, .client = client, .fixed = fixed,
		    .hash_len = fixed != NULL ? fixed->hash_len : SHA256_DIGEST_LEN};
		ret = dssh_kex_workspace_create(GEX_SLOTS, &state->workspace);
		if (ret < 0)
			goto failed;
	}
	else {
		if (state->ops != ops || state->client != client || state->fixed != fixed)
			return DSSH_ERROR_INVALID;
		ret = dssh_kex_workspace_reset(state->workspace);
		if (ret < 0)
			return ret;
		ops->reset(state->native);
	}
	if (state->hash == NULL || state->hash_dirty) {
		dssh_hash_free(state->hash);
		state->hash = NULL;
		size_t length = 0;
		ret = dssh_hash_init(&state->hash, fixed != NULL ? fixed->hash_name : "SHA-256", &length);
		if (ret < 0)
			goto failed;
		if (length != state->hash_len) {
			state->hash_dirty = true;
			ret = DSSH_ERROR_INIT;
			goto failed;
		}
		state->hash_dirty = false;
	}
	/* Host fingerprints remain SHA-256 even when the exchange uses SHA-512.
	 * Retain a distinct context rather than allocating it for each rekey. */
	if (client && state->hash_len != SHA256_DIGEST_LEN
	    && (state->fingerprint_hash == NULL || state->fingerprint_dirty)) {
		dssh_hash_free(state->fingerprint_hash);
		state->fingerprint_hash = NULL;
		size_t length = 0;
		ret = dssh_hash_init(&state->fingerprint_hash, "SHA-256", &length);
		if (ret < 0)
			goto failed;
		if (length != SHA256_DIGEST_LEN) {
			state->fingerprint_dirty = true;
			ret = DSSH_ERROR_INIT;
			goto failed;
		}
		state->fingerprint_dirty = false;
	}
	if (fresh) {
		ret = ops->init(&state->native);
		state->native_active = ret == 0 || state->native != NULL;
		if (ret > 0)
			ret = DSSH_ERROR_INVALID;
		if (ret < 0)
			goto failed;
		*opaque = state;
	}
	return 0;
failed:
	if (fresh)
		dhgex_cleanup(state);
	return ret;
}

DSSH_PRIVATE int
dhgex_init_impl(bool client, void *config, void **opaque, const struct dhgex_ops *ops)
{
	(void)config;
	return dh_init(client, opaque, ops, NULL);
}

DSSH_PRIVATE int
dssh_dh_fixed_init(bool client, void **opaque, const struct dssh_dh_fixed_group *group)
{
	return dh_init(client, opaque, &dssh_dhgex_backend, group);
}

DSSH_PRIVATE int
dhgex_init(bool client, void *config, void **state)
{
	return dhgex_init_impl(client, config, state, &dssh_dhgex_backend);
}

DSSH_PRIVATE int64_t
dssh_dhgex_parse_mpint(const uint8_t *buf, size_t bufsz, const uint8_t **body, size_t *body_len)
{
	uint32_t wire_len;
	if (buf == NULL || body == NULL || body_len == NULL || bufsz < 4
	    || dssh_parse_uint32(buf, bufsz, &wire_len) < 4)
		return DSSH_ERROR_PARSE;
	if (wire_len > bufsz - 4)
		return DSSH_ERROR_PARSE;
	if (wire_len > DHGEX_RAW_MAX + 1)
		return DSSH_ERROR_INVALID;
	/* The 1025-byte bound fits even the smallest supported size_t. */
	size_t length = (size_t)wire_len;
	if (length > 0 && (buf[4] & DSSH_MPINT_SIGN_BIT) != 0)
		return DSSH_ERROR_PARSE;
	if (length > 0 && buf[4] == 0 && (length == 1 || (buf[5] & DSSH_MPINT_SIGN_BIT) == 0))
		return DSSH_ERROR_PARSE;
	*body = &buf[4];
	*body_len = length;
	int64_t consumed = (int64_t)(4 + length);
	return consumed;
}

static struct dssh_bytes
trim(struct dssh_bytes bytes)
{
	while (bytes.len != 0 && bytes.data[0] == 0) {
		bytes.data++;
		bytes.len--;
	}
	return bytes;
}

static size_t
raw_bits(struct dssh_bytes bytes)
{
	bytes = trim(bytes);
	if (bytes.len == 0)
		return 0;
	/* Every caller caps bytes at DHGEX_RAW_MAX + one sign byte. */
	size_t bits = (bytes.len - 1) * 8;
	for (uint8_t top = bytes.data[0]; top != 0; top >>= 1)
		bits++;
	return bits;
}

static int
parse_mpint(const uint8_t *payload, size_t length, size_t *pos,
    struct dssh_bytes *raw, struct dssh_bytes *wire)
{
	if (!dssh_buffer_has(length, *pos, 4))
		return DSSH_ERROR_PARSE;
	int64_t parsed = dssh_dhgex_parse_mpint(&payload[*pos], length - *pos, &raw->data, &raw->len);
	if (parsed < 0) {
		int ret = (int)parsed; /* Only defined negative error constants. */
		return ret;
	}
	size_t used = (size_t)parsed; /* Shared parser caps at 1029. */
	*wire = (struct dssh_bytes){&payload[*pos], used};
	*pos += used;
	*raw = trim(*raw);
	return 0;
}

static int
parse_string(const uint8_t *payload, size_t length, size_t *pos, struct dssh_bytes *bytes)
{
	uint32_t count;
	if (payload == NULL || !dssh_buffer_has(length, *pos, 4)
	    || dssh_parse_uint32(&payload[*pos], length - *pos, &count) < 4)
		return DSSH_ERROR_PARSE;
	*pos += 4;
#if SIZE_MAX < UINT32_MAX
	if (count > SIZE_MAX)
		return DSSH_ERROR_PARSE;
#endif
	size_t size = (size_t)count;
	if (!dssh_buffer_has(length, *pos, size))
		return DSSH_ERROR_PARSE;
	*bytes = (struct dssh_bytes){&payload[*pos], size};
	*pos += size;
	return 0;
}

static int
put_string(struct dssh_bytes bytes, uint8_t *out, size_t capacity, size_t *pos)
{
	if (bytes.len > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	uint32_t length = (uint32_t)bytes.len;
	int ret = dssh_serialize_uint32(length, out, capacity, pos);
	if (ret < 0)
		return ret;
	if (!dssh_buffer_has(capacity, *pos, bytes.len))
		return DSSH_ERROR_TOOLONG;
	memcpy(&out[*pos], bytes.data, bytes.len);
	*pos += bytes.len;
	return 0;
}

static int
put_mpint(struct dssh_bytes raw, uint8_t *out, size_t capacity, size_t *pos)
{
	raw = trim(raw);
	if (raw.len > DHGEX_RAW_MAX)
		return DSSH_ERROR_INVALID;
	bool pad = raw.len != 0 && (raw.data[0] & DSSH_MPINT_SIGN_BIT) != 0;
	/* raw is at most 1024 bytes; length+pad and packet arithmetic fit size_t. */
	size_t size = raw.len + (pad ? 1U : 0U);
	uint32_t length = (uint32_t)size;
	int ret = dssh_serialize_uint32(length, out, capacity, pos);
	if (ret < 0)
		return ret;
	if (!dssh_buffer_has(capacity, *pos, size))
		return DSSH_ERROR_TOOLONG;
	if (pad)
		out[(*pos)++] = 0;
	memcpy(&out[*pos], raw.data, raw.len);
	*pos += raw.len;
	return 0;
}

static int
hash_u32(dssh_hash_ctx *hash, uint32_t value)
{
	uint8_t bytes[4];
	size_t pos = 0;
	int ret = dssh_serialize_uint32(value, bytes, sizeof(bytes), &pos);
	if (ret < 0)
		return ret;
	return dssh_hash_update(hash, bytes, sizeof(bytes));
}

static int
hash_string(dssh_hash_ctx *hash, struct dssh_bytes bytes)
{
	if (bytes.len > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	uint32_t length = (uint32_t)bytes.len;
	int ret = hash_u32(hash, length);
	if (ret < 0)
		return ret;
	return dssh_hash_update(hash, bytes.data, bytes.len);
}

static int
exchange_hash(struct dhgex_state *state, const struct dssh_kex_context *ctx,
    struct dssh_bytes host, const uint32_t request[3], struct dssh_bytes group,
    struct dssh_bytes e, struct dssh_bytes f, struct dssh_bytes secret, uint8_t *out)
{
	const uint8_t *client = (const uint8_t *)ctx->v_c;
	const uint8_t *server = (const uint8_t *)ctx->v_s;
	const struct dssh_bytes strings[] = {{client, ctx->v_c_len}, {server, ctx->v_s_len},
		{ctx->i_c, ctx->i_c_len}, {ctx->i_s, ctx->i_s_len}, host};
	state->hash_dirty = true;
	int ret;
	for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
		ret = hash_string(state->hash, strings[i]);
		if (ret < 0)
			return ret;
	}
	if (state->fixed == NULL) {
		for (size_t i = 0; i < 3; i++) {
			ret = hash_u32(state->hash, request[i]);
			if (ret < 0)
				return ret;
		}
	}
	const struct dssh_bytes mpints[] = {group, e, f};
	for (size_t i = state->fixed != NULL ? 1U : 0U; i < sizeof(mpints) / sizeof(mpints[0]); i++) {
		ret = dssh_hash_update(state->hash, mpints[i].data, mpints[i].len);
		if (ret < 0)
			return ret;
	}
	/* K is encoded directly into the hash: no temporary secret mpint copy. */
	bool pad = (secret.data[0] & DSSH_MPINT_SIGN_BIT) != 0;
	size_t size = secret.len + (pad ? 1U : 0U); /* At most 1025. */
	uint32_t length = (uint32_t)size;
	ret = hash_u32(state->hash, length);
	if (ret < 0)
		return ret;
	if (pad) {
		const uint8_t zero = 0;
		ret = dssh_hash_update(state->hash, &zero, 1);
		if (ret < 0)
			return ret;
	}
	ret = dssh_hash_update(state->hash, secret.data, secret.len);
	if (ret < 0)
		return ret;
	ret = dssh_hash_final(state->hash, out, state->hash_len);
	if (ret == 0)
		state->hash_dirty = false;
	return ret;
}

/* Compare K against 1 and p-1 without allocating/copying a secret integer.
 * The loop covers the bounded public modulus width; all differences fit even
 * a 16-bit int. Only public modulus subtraction needs the conditional +256. */
static bool
secret_valid(struct dssh_bytes secret, struct dssh_bytes modulus)
{
	if (secret.len == 0 || secret.len > modulus.len)
		return false;
	int modulus_borrow = 1, key_borrow = 0;
	size_t padding = modulus.len - secret.len;
	for (size_t pos = modulus.len; pos != 0; pos--) {
		size_t index = pos - 1;
		int limit = modulus.data[index];
		limit -= modulus_borrow;
		modulus_borrow = limit < 0;
		if (limit < 0)
			limit += 256;
		int key = index < padding ? 0 : secret.data[index - padding];
		key_borrow = key - limit - key_borrow < 0;
	}
	return key_borrow != 0 && (secret.len > 1 || secret.data[0] > 1);
}

DSSH_PRIVATE int
dhgex_handler_impl(const struct dssh_kex_context *ctx, struct dssh_kex_result *out, const struct dhgex_ops *ops)
{
	if (ctx == NULL || ctx->state == NULL || out == NULL
	    || out->shared_secret.data != NULL || out->shared_secret.len != 0
	    || out->exchange_hash.data != NULL || out->exchange_hash.len != 0
	    || out->lease.release != NULL || out->lease.owner != NULL)
		return DSSH_ERROR_INVALID;
	struct dhgex_state *state = ctx->state;
	if (state->ops != ops || state->client != ctx->client || state->hash_dirty || state->fingerprint_dirty)
		return DSSH_ERROR_INVALID;
	dssh_key_algo key = ctx->key_algo;
	struct dssh_dh_gex_group_result provider = {0};
	struct dssh_bytes_result signature = {0};
	struct dssh_bytes p = {0}, g = {0}, host = {0}, peer = {0}, peer_wire = {0}, peer_signature = {0};
	struct dssh_bytes group = {0}, public_wire = {0}, secret = {0};
	uint8_t *group_packet = NULL, *public_packet = NULL, *reply = NULL;
	uint8_t *payload = NULL, *raw_public = NULL, *raw_secret = NULL, *hash = NULL;
	size_t payload_len = 0, pos = 1, public_len = 0, secret_len = 0;
	uint8_t type;
	uint32_t request[3] = {GEX_MIN, GEX_N, GEX_MAX};
	int ret, drop;
	if (!ctx->client) {
		if (key == NULL || key->pubkey == NULL || key->sign == NULL) {
			ret = DSSH_ERROR_INIT;
			goto done;
		}
		ret = key->pubkey(&host.data, &host.len, key->ctx);
		if (ret != 0)
			goto done;
		if (host.data == NULL || host.len == 0 || host.len > UINT32_MAX) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		/* Keep the hashed and emitted key independent of later producer
		 * callbacks. The workspace retains this capacity for rekey. */
		uint8_t *saved_hostkey = NULL;
		ret = dssh_kex_workspace_get(state->workspace, GEX_HOSTKEY, host.len, &saved_hostkey);
		if (ret < 0)
			goto done;
		memcpy(saved_hostkey, host.data, host.len);
		host.data = saved_hostkey;
	}
	if (state->fixed != NULL) {
		static const uint8_t generator = 2;
		p = (struct dssh_bytes){state->fixed->p, state->fixed->p_len};
		g = (struct dssh_bytes){&generator, 1};
	}
	else if (ctx->client) {
		uint8_t message[13] = {SSH_MSG_KEX_DH_GEX_REQUEST};
		for (size_t i = 0; i < 3; i++) {
			ret = dssh_serialize_uint32(request[i], message, sizeof(message), &pos);
			if (ret < 0)
				goto done;
		}
		ret = ctx->send(message, pos, ctx->io_ctx);
		if (ret != 0)
			goto done;
		ret = ctx->recv(&type, &payload, &payload_len, ctx->io_ctx);
		if (ret != 0)
			goto done;
		if (type != SSH_MSG_KEX_DH_GEX_GROUP || payload == NULL || payload_len < 1) {
			ret = DSSH_ERROR_PARSE;
			goto done;
		}
		struct dssh_bytes ignored;
		pos = 1;
		ret = parse_mpint(payload, payload_len, &pos, &p, &ignored);
		if (ret < 0)
			goto done;
		ret = parse_mpint(payload, payload_len, &pos, &g, &ignored);
		if (ret < 0)
			goto done;
		if (pos != payload_len) {
			ret = DSSH_ERROR_PARSE;
			goto done;
		}
		group = (struct dssh_bytes){&payload[1], payload_len - 1};
	}
	else {
		ret = ctx->recv(&type, &payload, &payload_len, ctx->io_ctx);
		if (ret != 0)
			goto done;
		if (type != SSH_MSG_KEX_DH_GEX_REQUEST || payload == NULL || payload_len != 13) {
			ret = DSSH_ERROR_PARSE;
			goto done;
		}
		for (size_t i = 0; i < 3; i++) {
			if (dssh_parse_uint32(&payload[1 + i * 4], payload_len - 1 - i * 4, &request[i]) < 4) {
				ret = DSSH_ERROR_PARSE;
				goto done;
			}
		}
		if (request[0] > request[1] || request[1] > request[2]
		    || request[0] > GEX_MAX || request[2] < GEX_MIN) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		const struct dssh_dh_gex_provider *source = ctx->config;
		if (source == NULL || source->select_group == NULL) {
			ret = DSSH_ERROR_INIT;
			goto done;
		}
		ret = source->select_group(request[0], request[1], request[2], &provider, source->cbdata);
		if (ret != 0)
			goto done;
		p = provider.p;
		g = provider.g;
		if (provider.lease.release == NULL || p.data == NULL || g.data == NULL || p.data == g.data
		    || p.len == 0 || g.len == 0 || p.len > DHGEX_RAW_MAX || g.len > DHGEX_RAW_MAX) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		p = trim(p);
		g = trim(g);
	}
	/* Inputs remain bounded independently of any subsequent native metadata. */
	size_t bits = raw_bits(p);
	if (bits < GEX_MIN || bits > GEX_MAX || bits < request[0] || bits > request[2]
	    || p.len == 0 || g.len == 0 || p.len > DHGEX_RAW_MAX || g.len > DHGEX_RAW_MAX) {
		ret = DSSH_ERROR_INVALID;
		goto done;
	}
	/* Save p,g across the next receive. On the client the original canonical
	 * wire form is copied once; on the server raw provider values encode once. */
	struct dssh_bytes modulus = p, ignored;
	if (state->fixed == NULL) {
		size_t group_capacity = ctx->client ? group.len + 1 : 11 + p.len + g.len;
		ret = dssh_kex_workspace_get(state->workspace, GEX_GROUP, group_capacity, &group_packet);
		if (ret < 0)
			goto done;
		group_packet[0] = SSH_MSG_KEX_DH_GEX_GROUP;
		if (ctx->client)
			memcpy(&group_packet[1], group.data, group.len);
		else {
			pos = 1;
			ret = put_mpint(p, group_packet, group_capacity, &pos);
			if (ret < 0)
				goto done;
			ret = put_mpint(g, group_packet, group_capacity, &pos);
			if (ret < 0)
				goto done;
			group.len = pos - 1;
		}
		group.data = &group_packet[1];
		/* Keep a canonical modulus borrow independent of producer/payload
		 * lifetime before the next receive or provider release. */
		pos = 0;
		ret = parse_mpint(group.data, group.len, &pos, &modulus, &ignored);
		if (ret < 0)
			goto done;
	}
	ret = dssh_kex_workspace_get(state->workspace, GEX_PUBLIC, 6 + p.len, &public_packet);
	if (ret < 0)
		goto done;
	raw_public = &public_packet[6];
	ret = ops->keygen(state->native, p.data, p.len, g.data, g.len, raw_public, p.len, &public_len);
	/* Native import has completed before releasing the borrowed provider values. */
	dssh_result_release(&provider.lease);
	if (ret != 0)
		goto done;
	if (public_len == 0 || public_len > p.len) {
		ret = DSSH_ERROR_INVALID;
		goto done;
	}
	struct dssh_bytes public_value = trim((struct dssh_bytes){raw_public, public_len});
	if (!secret_valid(public_value, modulus)) {
		ret = DSSH_ERROR_INVALID;
		goto done;
	}
	bool pad = (public_value.data[0] & DSSH_MPINT_SIGN_BIT) != 0;
	/* Six reserved bytes before raw output hold type, length and optional sign.
	 * A leading-zero subspan leaves even more room; no public-value copy. */
	size_t message_offset = 6 + (public_len - public_value.len) - (pad ? 6U : 5U);
	uint8_t *public_message = &public_packet[message_offset];
	public_message[0] = state->fixed != NULL ? UINT8_C(30) : SSH_MSG_KEX_DH_GEX_INIT;
	pos = 1;
	size_t encoded_length = public_value.len + (pad ? 1U : 0U);
	uint32_t wire_length = (uint32_t)encoded_length; /* At most 1025. */
	ret = dssh_serialize_uint32(wire_length, public_message, 6 + p.len - message_offset, &pos);
	if (ret < 0)
		goto done;
	if (pad)
		public_message[pos++] = 0;
	public_wire = (struct dssh_bytes){&public_message[1], 4 + encoded_length};
	/* p may have belonged to the released producer. Only its validated length
	 * is used after import; no released provider/payload byte is read again. */
	size_t secret_capacity = p.len;
	if (ctx->client) {
		ret = ctx->send(public_message, 1 + public_wire.len, ctx->io_ctx);
		if (ret != 0)
			goto done;
	}
	else if (state->fixed == NULL) {
		ret = ctx->send(group_packet, 1 + group.len, ctx->io_ctx);
		if (ret != 0)
			goto done;
	}
	ret = ctx->recv(&type, &payload, &payload_len, ctx->io_ctx);
	if (ret != 0)
		goto done;
	uint8_t expected = state->fixed != NULL ? (ctx->client ? UINT8_C(31) : UINT8_C(30))
	    : (ctx->client ? SSH_MSG_KEX_DH_GEX_REPLY : SSH_MSG_KEX_DH_GEX_INIT);
	if (payload == NULL || payload_len < 1 || type != expected) {
		ret = DSSH_ERROR_PARSE;
		goto done;
	}
	pos = 1;
	if (ctx->client) {
		ret = parse_string(payload, payload_len, &pos, &host);
		if (ret < 0)
			goto done;
	}
	ret = parse_mpint(payload, payload_len, &pos, &peer, &peer_wire);
	if (ret < 0)
		goto done;
	if (ctx->client) {
		ret = parse_string(payload, payload_len, &pos, &peer_signature);
		if (ret < 0)
			goto done;
	}
	if (pos != payload_len) {
		ret = DSSH_ERROR_PARSE;
		goto done;
	}
	if (peer.len > DHGEX_RAW_MAX || !secret_valid(peer, modulus)) {
		ret = DSSH_ERROR_INVALID;
		goto done;
	}
	ret = dssh_kex_workspace_result(state->workspace, secret_capacity, state->hash_len,
	    &out->lease, &raw_secret, &hash);
	if (ret < 0)
		goto done;
	ret = ops->derive(state->native, peer.data, peer.len, raw_secret, secret_capacity, &secret_len);
	if (ret != 0)
		goto done;
	if (secret_len == 0 || secret_len > secret_capacity) {
		ret = DSSH_ERROR_INVALID;
		goto done;
	}
	secret = trim((struct dssh_bytes){raw_secret, secret_len});
	if (!secret_valid(secret, modulus)) {
		ret = DSSH_ERROR_INVALID;
		goto done;
	}
	ret = exchange_hash(state, ctx, host, request, group,
	    ctx->client ? public_wire : peer_wire, ctx->client ? peer_wire : public_wire, secret, hash);
	if (ret < 0)
		goto done;
	if (ctx->client) {
		if (key == NULL || key->verify == NULL) {
			ret = DSSH_ERROR_INIT;
			goto done;
		}
		if (ctx->hostkey_verify != NULL) {
			uint8_t fingerprint[SHA256_DIGEST_LEN];
			dssh_hash_ctx *fingerprint_hash = state->hash_len == SHA256_DIGEST_LEN
			    ? state->hash : state->fingerprint_hash;
			bool *dirty = state->hash_len == SHA256_DIGEST_LEN ? &state->hash_dirty : &state->fingerprint_dirty;
			*dirty = true;
			ret = dssh_hash_update(fingerprint_hash, host.data, host.len);
			if (ret < 0)
				goto done;
			ret = dssh_hash_final(fingerprint_hash, fingerprint, sizeof(fingerprint));
			if (ret < 0)
				goto done;
			*dirty = false;
			unsigned int key_bits = key->key_bits != NULL ? key->key_bits(host.data, host.len) : 0;
			if (ctx->hostkey_verify(key->name, key_bits, fingerprint, host.data, host.len,
			        ctx->hostkey_verify_cbdata) != DSSH_HOSTKEY_ACCEPT) {
				ret = DSSH_ERROR_INVALID;
				goto done;
			}
		}
		ret = key->verify(host.data, host.len, peer_signature.data, peer_signature.len, hash, state->hash_len);
		if (ret != 0)
			goto done;
	}
	else {
		ret = key->sign(&signature, hash, state->hash_len, key->ctx);
		if (ret != 0)
			goto done;
		if (signature.lease.release == NULL || signature.bytes.data == NULL
		    || signature.bytes.len == 0 || signature.bytes.len > UINT32_MAX) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		/* Reserve the maximum canonical public mpint for this group, so the
		 * random sign bit/leading zeros do not trigger warmed reallocations. */
		size_t capacity = 14 + secret_capacity; /* At most 1038. */
		if (host.len > SIZE_MAX - capacity) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		capacity += host.len;
		if (signature.bytes.len > SIZE_MAX - capacity) {
			ret = DSSH_ERROR_INVALID;
			goto done;
		}
		capacity += signature.bytes.len;
		ret = dssh_kex_workspace_get(state->workspace, GEX_REPLY, capacity, &reply);
		if (ret < 0)
			goto done;
		reply[0] = state->fixed != NULL ? UINT8_C(31) : SSH_MSG_KEX_DH_GEX_REPLY;
		pos = 1;
		ret = put_string(host, reply, capacity, &pos);
		if (ret < 0)
			goto done;
		memcpy(&reply[pos], public_wire.data, public_wire.len);
		pos += public_wire.len;
		ret = put_string(signature.bytes, reply, capacity, &pos);
		if (ret < 0)
			goto done;
		dssh_result_release(&signature.lease);
		ret = ctx->send(reply, pos, ctx->io_ctx);
		if (ret != 0)
			goto done;
	}
	ret = 0;
done:
	ops->reset(state->native);
	dssh_result_release(&provider.lease);
	dssh_result_release(&signature.lease);
	for (size_t i = 0; i < GEX_SLOTS; i++) {
		drop = dssh_kex_workspace_drop(state->workspace, i);
		if (drop < 0 && ret == 0)
			ret = drop;
	}
	if (ret > 0)
		ret = DSSH_ERROR_INVALID;
	if (ret < 0)
		dssh_result_release(&out->lease);
	else {
		out->shared_secret = secret;
		out->exchange_hash = (struct dssh_bytes){hash, state->hash_len};
	}
	return ret;
}
