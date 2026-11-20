#include <string.h>

#include "ssh-internal.h"
#include "client-key.h"
#include "key-text.h"

static struct dssh_client_key_slot *
find_slot(struct dssh_session_s *sess, dssh_key_algo module)
{
	for (struct dssh_client_key_slot *slot = sess->client_keys; slot != NULL; slot = slot->next) {
		if (slot->module == module)
			return slot;
	}
	return NULL;
}

DSSH_PRIVATE dssh_key_algo_ctx *
dssh_client_key_context(struct dssh_session_s *sess, dssh_key_algo module)
{
	struct dssh_client_key_slot *slot = find_slot(sess, module);
	return slot != NULL && slot->lease.release != NULL ? slot->ctx : module->ctx;
}

DSSH_PRIVATE void
dssh_client_keys_cleanup(struct dssh_session_s *sess)
{
	struct dssh_client_key_slot *slot = sess->client_keys;
	sess->client_keys = NULL;
	while (slot != NULL) {
		struct dssh_client_key_slot *next = slot->next;
		dssh_result_release(&slot->lease);
		free(slot);
		slot = next;
	}
}

static int
key_entry_check(struct dssh_session_s *sess, const char *name, bool mutate, dssh_key_algo *module)
{
	if (sess == NULL || name == NULL || name[0] == 0)
		return DSSH_ERROR_INVALID;
	if (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess)
	    || session_scope_active(sess) || session_on_demux_thread(sess))
		return DSSH_ERROR_INVALID;
	if (!sess->trans.client)
		return DSSH_ERROR_INVALID;
	if (mutate) {
		if (sess->terminate)
			return DSSH_ERROR_TERMINATED;
		if (sess->auth_complete || sess->conn_initialized)
			return DSSH_ERROR_TOOLATE;
	}
	dssh_key_algo selected = find_key_algo(name);
	if (selected == NULL)
		return DSSH_ERROR_INIT;
	if ((selected->flags & DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE) == 0
	    || selected->sign == NULL || selected->pubkey == NULL || selected->haskey == NULL)
		return DSSH_ERROR_INVALID;
	*module = selected;
	return 0;
}

static const struct dssh_client_key_ops *
builtin_ops(dssh_key_algo module)
{
	if (strcmp(module->name, "ssh-ed25519") == 0)
		return dssh_ed25519_client_ops(module);
	if (strcmp(module->name, "rsa-sha2-256") == 0)
		return dssh_rsa256_client_ops(module);
	if (strcmp(module->name, "rsa-sha2-512") == 0)
		return dssh_rsa512_client_ops(module);
	return NULL;
}

/* No publication until module callbacks and allocation have succeeded. The
 * caller owns its lease on failure; producer callbacks run within API scope. */
static int
install_context(struct dssh_session_s *sess, dssh_key_algo module,
    dssh_key_algo_ctx *ctx, struct dssh_result_lease *lease)
{
	int ret = module->haskey(ctx);
	if (ret <= 0)
		return ret < 0 ? ret : DSSH_ERROR_INIT;
	const uint8_t *blob = NULL;
	size_t length = 0;
	ret = module->pubkey(&blob, &length, ctx);
	if (ret != 0)
		return ret < 0 ? ret : DSSH_ERROR_INVALID;
	if (blob == NULL || length == 0 || length > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;
	struct dssh_client_key_slot *slot = find_slot(sess, module);
	if (slot == NULL) {
		slot = malloc(sizeof(*slot));
		if (slot == NULL)
			return DSSH_ERROR_ALLOC;
		*slot = (struct dssh_client_key_slot){ .module = module, .next = sess->client_keys };
		sess->client_keys = slot;
	}
	struct dssh_result_lease old = slot->lease;
	slot->ctx = ctx;
	slot->lease = *lease;
	*lease = (struct dssh_result_lease){0};
	dssh_result_release(&old);
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_client_key_ctx(dssh_session sess, const char *algo_name,
    void *ctx, struct dssh_result_lease *lease)
{
	if (lease == NULL || lease->release == NULL)
		return DSSH_ERROR_INVALID;
	dssh_key_algo module;
	int ret = key_entry_check(sess, algo_name, true, &module);
	if (ret < 0)
		return ret;
	dssh_key_algo_ctx *opaque = (dssh_key_algo_ctx *)ctx;
	struct dssh_session_scope scope;
	session_scope_enter(&scope, sess);
	ret = install_context(sess, module, opaque, lease);
	session_scope_leave(&scope);
	return ret;
}

static int
construct_key(struct dssh_session_s *sess, dssh_key_algo module,
    const struct dssh_client_key_ops *ops, const char *path,
    dssh_pem_password_cb cb, void *cbdata, unsigned int bits)
{
	void *ctx = NULL;
	struct dssh_session_scope scope;
	session_scope_enter(&scope, sess);
	int ret = path != NULL ? ops->load(&ctx, path, cb, cbdata) : ops->generate(&ctx, bits);
	struct dssh_result_lease lease = { ops->release, ctx };
	if (ret == 0) {
		if (ctx == NULL)
			ret = DSSH_ERROR_INIT;
		else {
			dssh_key_algo_ctx *opaque = (dssh_key_algo_ctx *)ctx;
			ret = install_context(sess, module, opaque, &lease);
		}
	}
	else if (ret > 0)
		ret = DSSH_ERROR_INVALID;
	/* Constructor failures keep no native object, but a partial result is
	 * still retired by its producer if a backend ever returns one. */
	if (lease.owner != NULL)
		dssh_result_release(&lease);
	session_scope_leave(&scope);
	return ret;
}

DSSH_PUBLIC int
dssh_session_load_client_key_file(dssh_session sess, const char *algo_name,
    const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INVALID;
	dssh_key_algo module;
	int ret = key_entry_check(sess, algo_name, true, &module);
	if (ret < 0)
		return ret;
	const struct dssh_client_key_ops *ops = builtin_ops(module);
	if (ops == NULL)
		return DSSH_ERROR_INVALID;
	return construct_key(sess, module, ops, path, cb, cbdata, 0);
}

DSSH_PUBLIC int
dssh_session_generate_client_key(dssh_session sess, const char *algo_name, unsigned int bits)
{
	dssh_key_algo module;
	int ret = key_entry_check(sess, algo_name, true, &module);
	if (ret < 0)
		return ret;
	const struct dssh_client_key_ops *ops = builtin_ops(module);
	if (ops == NULL)
		return DSSH_ERROR_INVALID;
	return construct_key(sess, module, ops, NULL, NULL, NULL, bits);
}

DSSH_PUBLIC int
dssh_session_clear_client_key(dssh_session sess, const char *algo_name)
{
	dssh_key_algo module;
	int ret = key_entry_check(sess, algo_name, true, &module);
	if (ret < 0)
		return ret;
	struct dssh_client_key_slot *slot = find_slot(sess, module);
	if (slot != NULL) {
		struct dssh_result_lease old = slot->lease;
		slot->ctx = NULL;
		slot->lease = (struct dssh_result_lease){0};
		dssh_result_release(&old);
	}
	return 0;
}

DSSH_PUBLIC int
dssh_session_save_client_key_file(dssh_session sess, const char *algo_name,
    const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INVALID;
	dssh_key_algo module;
	int ret = key_entry_check(sess, algo_name, false, &module);
	if (ret < 0)
		return ret;
	const struct dssh_client_key_ops *ops = builtin_ops(module);
	if (ops == NULL)
		return DSSH_ERROR_INVALID;
	void *ctx = dssh_client_key_context(sess, module);
	struct dssh_session_scope scope;
	session_scope_enter(&scope, sess);
	ret = ops->save(ctx, path, cb, cbdata);
	session_scope_leave(&scope);
	return ret;
}

/* Format the key's wire type, which for RSA is ssh-rsa independently of the
 * chosen SHA-2 signing algorithm. Views remain borrowed for this operation. */
static int
public_key_parts(dssh_key_algo module, dssh_key_algo_ctx *ctx,
    const uint8_t **blob, size_t *length, const char **name, size_t *name_length)
{
	int ret = module->pubkey(blob, length, ctx);
	if (ret != 0)
		return ret < 0 ? ret : DSSH_ERROR_INVALID;
	if (*blob == NULL || *length > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	uint32_t size;
	int64_t parsed = dssh_parse_uint32(*blob, *length, &size);
	if (parsed < 0)
		return DSSH_ERROR_INVALID;
	if (size == 0 || size > 64)
		return DSSH_ERROR_INVALID;
	size_t count = (size_t)size; /* size <= 64, representable on every target. */
	if (!dssh_buffer_has(*length, 4, count))
		return DSSH_ERROR_INVALID;
	for (size_t i = 0; i < count; i++) {
		if ((*blob)[4 + i] < 0x21 || (*blob)[4 + i] > 0x7e)
			return DSSH_ERROR_INVALID;
	}
	const char *text = (const char *)*blob;
	*name = text + 4;
	*name_length = count;
	return 0;
}

DSSH_PUBLIC int64_t
dssh_session_get_client_key_pub_str(dssh_session sess, const char *algo_name, char *buf, size_t bufsz)
{
	dssh_key_algo module;
	int ret = key_entry_check(sess, algo_name, false, &module);
	if (ret < 0)
		return ret;
	const uint8_t *blob = NULL;
	size_t length = 0, name_length = 0;
	const char *name = NULL;
	struct dssh_session_scope scope;
	session_scope_enter(&scope, sess);
	ret = public_key_parts(module, dssh_client_key_context(sess, module), &blob, &length, &name, &name_length);
	int64_t result = ret < 0 ? ret : dssh_key_text_format(blob, length, name, name_length, buf, bufsz);
	session_scope_leave(&scope);
	return result;
}

DSSH_PUBLIC int
dssh_session_save_client_pub_file(dssh_session sess, const char *algo_name, const char *path)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INVALID;
	dssh_key_algo module;
	int ret = key_entry_check(sess, algo_name, false, &module);
	if (ret < 0)
		return ret;
	const uint8_t *blob = NULL;
	size_t length = 0, name_length = 0;
	const char *name = NULL;
	struct dssh_session_scope scope;
	session_scope_enter(&scope, sess);
	ret = public_key_parts(module, dssh_client_key_context(sess, module), &blob, &length, &name, &name_length);
	if (ret == 0)
		ret = dssh_key_text_write(path, blob, length, name, name_length);
	session_scope_leave(&scope);
	return ret;
}
