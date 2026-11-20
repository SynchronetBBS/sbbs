// RFC 4252: SSH Authentication Protocol

#include <stdlib.h>
#include <string.h>

#include "deucessh-auth.h"
#include "deucessh-crypto.h"
#include "ssh-internal.h"

static const char service_userauth[]            = "ssh-userauth";
static const char service_connection[]          = "ssh-connection";
static const char method_none[]                 = "none";
static const char method_password[]             = "password";
static const char method_publickey[]            = "publickey";
static const char method_keyboard_interactive[] = "keyboard-interactive";

/* Fixed overhead for each auth message type (excludes variable-length fields).
 * Used in both overflow checks and msg_len computations. */

/* USERAUTH_FAILURE: msg_type + methods_len + partial */
#define AUTH_FAILURE_FIXED (1 + 4 + 1)

/* PASSWD_CHANGEREQ: msg_type + prompt_len + language_len */
#define PASSWD_CHANGEREQ_FIXED (1 + 4 + 4)

/* USERAUTH_REQUEST (none): msg_type + user_len + svc_len + svc
 * + method_len + method */
#define AUTH_NONE_FIXED (1 + 4 + 4 + DSSH_STRLEN(service_connection) + 4 + DSSH_STRLEN(method_none))

/* USERAUTH_REQUEST (password): msg_type + user_len + svc_len + svc
 * + method_len + method + change_flag + old_pw_len */
#define AUTH_PASSWORD_FIXED \
	(1 + 4 + 4 + DSSH_STRLEN(service_connection) + 4 + DSSH_STRLEN(method_password) + 1 + 4)

/* USERAUTH_REQUEST (keyboard-interactive): msg_type + user_len + svc_len
 * + svc + method_len + method + language_len + submethods_len */
#define AUTH_KBI_FIXED \
	(1 + 4 + 4 + DSSH_STRLEN(service_connection) + 4 + DSSH_STRLEN(method_keyboard_interactive) + 4 + 4)

/* USERAUTH_REQUEST (publickey) sign data and message share the same
 * fixed overhead: 4 (session_id_len in sign data, sig_len in message)
 * + msg_type + user_len + svc_len + svc + method_len + method + TRUE
 * + algo_len + pubkey_len */
#define AUTH_PUBLICKEY_FIXED \
	(4 + 1 + 4 + 4 + DSSH_STRLEN(service_connection) + 4 + DSSH_STRLEN(method_publickey) + 1 + 4 + 4)

/* Validate the complete message size before any serialization into the
 * reserved packet buffer. Commit-time checks cannot protect earlier writes. */
static uint8_t *
auth_send_begin(struct dssh_session_s *sess, uint8_t msg_type, size_t needed, int *err_out)
{
	size_t max;
	uint8_t *msg = send_begin(sess, msg_type, &max, err_out);

	if (msg == NULL)
		return NULL;
	if (needed > max) {
		send_cancel(sess);
		*err_out = DSSH_ERROR_TOOLONG;
		return NULL;
	}
	return msg;
}

/* Parse into views only after checking the wire length in its original
 * type. Failure leaves the caller's offset and outputs unchanged. */
static bool
auth_read_string(const uint8_t *payload, size_t payload_len, size_t *pos,
    const uint8_t **value, size_t *length)
{
	size_t at = *pos;
	if (!dssh_buffer_has(payload_len, at, 4))
		return false;
	uint32_t wire_length = DSSH_GET_U32(&payload[at]);
	at += 4;
#if SIZE_MAX < UINT32_MAX
	if (wire_length > SIZE_MAX)
		return false;
#endif
	size_t size = (size_t)wire_length;
	if (!dssh_buffer_has(payload_len, at, size))
		return false;
	*value = &payload[at];
	*length = size;
	*pos = at + size;
	return true;
}

static bool
auth_name_valid(const uint8_t *name, size_t length)
{
	if (length == 0 || length > DSSH_ALGO_NAME_MAX)
		return false;
	for (size_t i = 0; i < length; i++) {
		if (name[i] <= ' ' || name[i] >= DSSH_ASCII_DEL || name[i] == ',')
			return false;
	}
	return true;
}

static bool
auth_methods_valid(const uint8_t *methods, size_t length)
{
	size_t pos = 0;
	while (pos < length) {
		size_t first = pos;
		while (pos < length && methods[pos] != ',')
			pos++;
		size_t size = pos - first;
		if (!auth_name_valid(&methods[first], size)
		    || (size == DSSH_STRLEN(method_none) && memcmp(&methods[first], method_none, size) == 0))
			return false;
		if (pos < length) {
			pos++;
			if (pos == length)
				return false;
		}
	}
	return true;
}

static int
parse_auth_failure(const uint8_t *payload, size_t payload_len,
    const uint8_t **methods, size_t *methods_len, bool *partial)
{
	size_t pos = 1;
	const uint8_t *names;
	size_t length;
	if (!auth_read_string(payload, payload_len, &pos, &names, &length)
	    || payload_len - pos != 1 || !auth_methods_valid(names, length))
		return DSSH_ERROR_PARSE;
	/* Every nonzero partial-success byte represents TRUE (RFC 4251 s5). */
	*methods = names;
	*methods_len = length;
	*partial = payload[pos] != 0;
	return 0;
}

/* Publish only complete replies; allocation failure preserves the prior
 * snapshot. Empty lists need no allocation, including initial success. */
static int
auth_publish_methods(struct dssh_session_s *sess, const uint8_t *methods, size_t length)
{
	if (length > sess->trans.packet_buf_sz || length == SIZE_MAX)
		return DSSH_ERROR_TOOLONG;
	if (length > 0 && length >= sess->auth_methods_capacity) {
		size_t needed = length + 1;
		char *storage = realloc(sess->auth_methods, needed);
		if (storage == NULL)
			return DSSH_ERROR_ALLOC;
		sess->auth_methods = storage;
		sess->auth_methods_capacity = needed;
	}
	if (length > 0)
		memcpy(sess->auth_methods, methods, length);
	if (sess->auth_methods != NULL)
		sess->auth_methods[length] = 0;
	sess->auth_methods_len = length;
	sess->auth_methods_available = true;
	return 0;
}

static int
auth_success_result(struct dssh_session_s *sess, size_t payload_len)
{
	if (payload_len != 1)
		return DSSH_ERROR_PARSE;
	return auth_publish_methods(sess, NULL, 0);
}

static int
auth_failure_result(struct dssh_session_s *sess, const uint8_t *payload, size_t payload_len)
{
	const uint8_t *methods;
	size_t methods_len;
	bool partial;
	int ret = parse_auth_failure(payload, payload_len, &methods, &methods_len, &partial);
	if (ret < 0)
		return ret;
	ret = auth_publish_methods(sess, methods, methods_len);
	if (ret < 0) {
		/* The server has completed an action, potentially accepting a factor
		 * or changing a password. Do not continue after losing its result. */
		session_set_terminate(sess);
		return ret;
	}
	return partial ? DSSH_AUTH_PARTIAL : DSSH_AUTH_CONTINUE;
}

static int
handle_banner(struct dssh_session_s *sess, uint8_t *payload, size_t payload_len)
{
	size_t pos = 1;
	const uint8_t *message, *language;
	size_t message_len, language_len;
	if (!auth_read_string(payload, payload_len, &pos, &message, &message_len)
	    || !auth_read_string(payload, payload_len, &pos, &language, &language_len)
	    || pos != payload_len)
		return DSSH_ERROR_PARSE;
	dssh_auth_banner_cb cb = sess->banner_cb;
	if (cb != NULL)
		cb(message, message_len, language, language_len, sess->banner_cbdata);
	return 0;
}

/* The synchronous server operation owns this receive allocation. Drop all
 * of its bytes, including tails left by earlier longer packets. */
static void
clear_auth_receive_buffer(struct dssh_session_s *sess)
{
	dssh_cleanse(sess->trans.rx_packet, 4);
	dssh_cleanse(&sess->trans.rx_packet[4], sess->trans.packet_buf_sz);
}

static bool
auth_result_valid(int result, int method_result)
{
	return result == DSSH_AUTH_SUCCESS || result == DSSH_AUTH_FAILURE
	    || result == DSSH_AUTH_DISCONNECT || result == DSSH_AUTH_PARTIAL || result == method_result;
}

/* Callback inputs are library-owned views. Keep their allocations for the
 * entire authentication exchange and replace them only when count grows. */
struct auth_kbi_views {
	const uint8_t **text;
	size_t *lengths;
	bool *echo;
	size_t capacity;
};

static void
auth_kbi_views_free(struct auth_kbi_views *views)
{
	free(views->text);
	free(views->lengths);
	free(views->echo);
}

static int
auth_kbi_views_reserve(struct auth_kbi_views *views, size_t count, bool with_echo)
{
	if (count <= views->capacity)
		return 0;
	if (count > SIZE_MAX / sizeof(*views->text)
	    || count > SIZE_MAX / sizeof(*views->lengths)
	    || (with_echo && count > SIZE_MAX / sizeof(*views->echo)))
		return DSSH_ERROR_INVALID;
	const uint8_t **text = calloc(count, sizeof(*text));
	size_t *lengths = calloc(count, sizeof(*lengths));
	bool *echo = with_echo ? calloc(count, sizeof(*echo)) : NULL;
	if (text == NULL || lengths == NULL || (with_echo && echo == NULL)) {
		free(text);
		free(lengths);
		free(echo);
		return DSSH_ERROR_ALLOC;
	}
	auth_kbi_views_free(views);
	views->text = text;
	views->lengths = lengths;
	views->echo = echo;
	views->capacity = count;
	return 0;
}

static int flush_pending_banner(struct dssh_session_s *sess);

/* Handler return values for per-method auth dispatch */
#define AUTH_HANDLER_CONTINUE 0 /* failure/partial sent, keep looping */
#define AUTH_HANDLER_SUCCESS  1 /* auth succeeded, goto done */
#define AUTH_HANDLER_RESTART  2 /* process the already received replacement request */

/* negative values = DSSH_ERROR_* (caller returns directly) */

/* ================================================================
 * Server-side helpers
 * ================================================================ */
static int
send_auth_failure(struct dssh_session_s *sess, const char *methods, bool partial_success)
{
	if (methods == NULL)
		return DSSH_ERROR_INVALID;
	size_t mlen = strlen(methods);
	const uint8_t *names = (const uint8_t *)methods;
	if (!auth_methods_valid(names, mlen))
		return DSSH_ERROR_INVALID;

	if ((mlen > UINT32_MAX) || (mlen > SIZE_MAX - AUTH_FAILURE_FIXED))
		return DSSH_ERROR_INVALID;
	int ret = flush_pending_banner(sess);
	if (ret < 0)
		return ret;

	int      err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_USERAUTH_FAILURE, AUTH_FAILURE_FIXED + mlen, &err);

	if (msg == NULL)
		return err;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
	DSSH_PUT_U32((uint32_t)mlen, msg, &pos);
	memcpy(&msg[pos], methods, mlen);
	pos += mlen;
	msg[pos++] = partial_success ? 1 : 0;
	return send_commit(sess, pos, NULL);
}

static int
send_auth_success(struct dssh_session_s *sess)
{
	int ret = flush_pending_banner(sess);

	if (ret < 0)
		return ret;

	uint8_t msg = SSH_MSG_USERAUTH_SUCCESS;

	return send_packet(sess, &msg, 1, NULL);
}

static int
send_passwd_changereq(struct dssh_session_s *sess, struct dssh_bytes_result *result)
{
	const uint8_t *prompt = result->bytes.data;
	size_t prompt_len = result->bytes.len;
	if ((prompt_len > UINT32_MAX) || (prompt_len > SIZE_MAX - PASSWD_CHANGEREQ_FIXED))
		return DSSH_ERROR_INVALID;

	int ret = flush_pending_banner(sess);

	if (ret < 0)
		return ret;

	int      err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_USERAUTH_PASSWD_CHANGEREQ,
	    PASSWD_CHANGEREQ_FIXED + prompt_len, &err);

	if (msg == NULL)
		return err;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	DSSH_PUT_U32((uint32_t)prompt_len, msg, &pos);
	memcpy(&msg[pos], prompt, prompt_len);
	pos += prompt_len;
	DSSH_PUT_U32(0, msg, &pos);

	dssh_result_release(&result->lease);
	return send_commit(sess, pos, NULL);
}

static int
send_pk_ok(struct dssh_session_s *sess, const char *algo_name, size_t algo_len, const uint8_t *pubkey_blob,
    size_t pubkey_blob_len)
{
	if ((algo_len > UINT32_MAX) || (pubkey_blob_len > UINT32_MAX) || (algo_len > SIZE_MAX - 9)
	    || (pubkey_blob_len > SIZE_MAX - 9 - algo_len))
		return DSSH_ERROR_INVALID;

	int ret = flush_pending_banner(sess);

	if (ret < 0)
		return ret;

	int      err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_USERAUTH_PK_OK, 9 + algo_len + pubkey_blob_len, &err);

	if (msg == NULL)
		return err;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_PK_OK;
	DSSH_PUT_U32((uint32_t)algo_len, msg, &pos);
	memcpy(&msg[pos], algo_name, algo_len);
	pos += algo_len;
	DSSH_PUT_U32((uint32_t)pubkey_blob_len, msg, &pos);
	memcpy(&msg[pos], pubkey_blob, pubkey_blob_len);
	pos += pubkey_blob_len;

	return send_commit(sess, pos, NULL);
}

/* Only inspect top-level pointer/lease metadata on unused or error results. */
static bool
kbi_challenge_lease_valid(const struct dssh_auth_challenge *result)
{
	return result->lease.release != NULL || (result->lease.owner == NULL
	    && result->name.data == NULL && result->instruction.data == NULL && result->prompts == NULL);
}

static int
send_info_request(struct dssh_session_s *sess, struct dssh_auth_challenge *result)
{
	size_t capacity = tx_payload_capacity(sess);
	if (!kbi_challenge_lease_valid(result)
	    || (result->name.len > 0 && result->name.data == NULL)
	    || (result->instruction.len > 0 && result->instruction.data == NULL)
	    || (result->count > 0 && result->prompts == NULL))
		return DSSH_ERROR_INVALID;
	if (capacity < 17 || result->count > (capacity - 17) / 6)
		return DSSH_ERROR_TOOLONG;
#if SIZE_MAX < UINT32_MAX
	if (result->count > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t count = (size_t)result->count;
	if (count > SIZE_MAX / sizeof(*result->prompts))
		return DSSH_ERROR_INVALID;
	size_t name_len = result->name.len;
	size_t inst_len = result->instruction.len;
	if (name_len > UINT32_MAX || inst_len > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
	if (name_len > SIZE_MAX - 17 || inst_len > SIZE_MAX - 17 - name_len)
		return DSSH_ERROR_INVALID;
	size_t msg_len = 17 + name_len + inst_len;
	if (msg_len > capacity)
		return DSSH_ERROR_TOOLONG;
	/* Bound the count before reading the producer's prompt array. */
	for (size_t i = 0; i < count; i++) {
		size_t length = result->prompts[i].text.len;
		if (length == 0 || result->prompts[i].text.data == NULL)
			return DSSH_ERROR_INVALID;
		if (length > UINT32_MAX)
			return DSSH_ERROR_TOOLONG;
		if (length > SIZE_MAX - 5 || msg_len > SIZE_MAX - 5 - length)
			return DSSH_ERROR_INVALID;
		msg_len += 5 + length;
		if (msg_len > capacity)
			return DSSH_ERROR_TOOLONG;
	}
	int err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_USERAUTH_INFO_REQUEST, msg_len, &err);
	if (msg == NULL)
		return err;
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	DSSH_PUT_U32((uint32_t)name_len, msg, &pos);
	if (name_len > 0) {
		memcpy(&msg[pos], result->name.data, name_len);
		pos += name_len;
	}
	DSSH_PUT_U32((uint32_t)inst_len, msg, &pos);
	if (inst_len > 0) {
		memcpy(&msg[pos], result->instruction.data, inst_len);
		pos += inst_len;
	}
	DSSH_PUT_U32(0, msg, &pos);
	DSSH_PUT_U32(result->count, msg, &pos);
	for (size_t i = 0; i < count; i++) {
		size_t length = result->prompts[i].text.len;
		DSSH_PUT_U32((uint32_t)length, msg, &pos);
		memcpy(&msg[pos], result->prompts[i].text.data, length);
		pos += length;
		msg[pos++] = result->prompts[i].echo ? 1 : 0;
	}
	dssh_result_release(&result->lease);
	return send_commit(sess, pos, NULL);
}

/*
 * Parse the common prefix of a USERAUTH_REQUEST:
 *   string user, string service, string method
 * Returns the offset past the method string, or negative on error.
 * REJECTED distinguishes a complete prefix for an unavailable service.
 */
DSSH_TESTABLE int64_t
parse_userauth_prefix(const uint8_t *payload, size_t payload_len, const uint8_t **username, size_t *username_len,
    const uint8_t **method, size_t *method_len)
{
	size_t pos = 1;
	const uint8_t *service;
	size_t service_len;
	if (!auth_read_string(payload, payload_len, &pos, username, username_len)
	    || !auth_read_string(payload, payload_len, &pos, &service, &service_len)
	    || !auth_read_string(payload, payload_len, &pos, method, method_len)
	    || !auth_name_valid(*method, *method_len))
		return DSSH_ERROR_PARSE;
	/* Distinguish an unavailable service from malformed framing. The server
	 * cannot carry authentication factors into a different service. */
	if (service_len != DSSH_STRLEN(service_connection)
	    || memcmp(service, service_connection, service_len) != 0)
		return DSSH_ERROR_REJECTED;
#if SIZE_MAX > INT64_MAX
	if (pos > INT64_MAX)
		return DSSH_ERROR_INVALID;
#endif
	int64_t result = (int64_t)pos;
	return result;
}

/*
 * Send and consume the pending banner, if any.
 */
static int
flush_pending_banner(struct dssh_session_s *sess)
{
	if (sess->pending_banner == NULL)
		return 0;

	size_t msg_sz = strlen(sess->pending_banner);

#if SIZE_MAX > UINT32_MAX
	if (msg_sz > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
#endif
	uint32_t    msg_len = (uint32_t)msg_sz;
	const char *lang    = sess->pending_banner_lang != NULL ? sess->pending_banner_lang : "";
	size_t      lang_sz = strlen(lang);

#if SIZE_MAX > UINT32_MAX
	if (lang_sz > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
#endif
	uint32_t lang_len = (uint32_t)lang_sz;
	if (msg_sz > SIZE_MAX - 9 || lang_sz > SIZE_MAX - 9 - msg_sz)
		return DSSH_ERROR_TOOLONG;
	int      err;
	uint8_t *pkt = auth_send_begin(sess, SSH_MSG_USERAUTH_BANNER, 9 + msg_sz + lang_sz, &err);

	if (pkt == NULL)
		return err;

	size_t pos = 0;

	pkt[pos++] = SSH_MSG_USERAUTH_BANNER;
	DSSH_PUT_U32(msg_len, pkt, &pos);
	memcpy(&pkt[pos], sess->pending_banner, msg_len);
	pos += msg_len;
	DSSH_PUT_U32(lang_len, pkt, &pos);
	memcpy(&pkt[pos], lang, lang_len);
	pos += lang_len;

	int res = send_commit(sess, pos, NULL);

	free(sess->pending_banner);
	free(sess->pending_banner_lang);
	sess->pending_banner      = NULL;
	sess->pending_banner_lang = NULL;

	return res;
}

/* ================================================================
 * Per-method server auth handlers
 *
 * Each returns AUTH_HANDLER_SUCCESS, AUTH_HANDLER_CONTINUE, or a
 * negative DSSH_ERROR_* code.
 * ================================================================ */

/*
 * Handle "none" authentication method.
 */
static int
handle_auth_none(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, const uint8_t *user,
    size_t user_len)
{
	int auth_res = DSSH_AUTH_FAILURE;
	int res;

	if (cbs->none_cb != NULL)
		auth_res = cbs->none_cb(user, user_len, cbs->cbdata);
	if (!auth_result_valid(auth_res, DSSH_AUTH_SUCCESS))
		return DSSH_ERROR_INVALID;
	if (auth_res == DSSH_AUTH_DISCONNECT) {
		return transport_disconnect_result(sess, SSH_DISCONNECT_BY_APPLICATION,
		    "authentication rejected", DSSH_ERROR_TERMINATED);
	}
	if (auth_res == DSSH_AUTH_SUCCESS) {
		res = send_auth_success(sess);
		if (res < 0)
			return res;
		return AUTH_HANDLER_SUCCESS;
	}
	res = send_auth_failure(sess, cbs->methods_str, auth_res == DSSH_AUTH_PARTIAL);
	if (res < 0)
		return res;
	return AUTH_HANDLER_CONTINUE;
}

/*
 * Dispatch auth result for password (normal and change) paths.
 * Handles SUCCESS, DISCONNECT, CHANGE_PASSWORD, and FAILURE/PARTIAL.
 */
static bool
password_result_lease_valid(const struct dssh_bytes_result *result)
{
	if (result->bytes.len > 0 && result->bytes.data == NULL)
		return false;
	return result->lease.release != NULL
	    || (result->lease.owner == NULL && result->bytes.data == NULL);
}

static int
password_dispatch(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, int auth_res,
    struct dssh_bytes_result *prompt)
{
	int res;

	if (!auth_result_valid(auth_res, DSSH_AUTH_CHANGE_PASSWORD)
	    || !password_result_lease_valid(prompt)) {
		dssh_result_release(&prompt->lease);
		return DSSH_ERROR_INVALID;
	}
	if (auth_res == DSSH_AUTH_CHANGE_PASSWORD) {
		if (prompt->bytes.data == NULL)
			res = DSSH_ERROR_INVALID;
		else
			res = send_passwd_changereq(sess, prompt);
		dssh_result_release(&prompt->lease);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}
	/* Unused prompt results remain producer-owned on every auth outcome. */
	dssh_result_release(&prompt->lease);
	if (auth_res == DSSH_AUTH_DISCONNECT)
		return transport_disconnect_result(sess, SSH_DISCONNECT_BY_APPLICATION,
		    "authentication rejected", DSSH_ERROR_TERMINATED);
	if (auth_res == DSSH_AUTH_SUCCESS) {
		res = send_auth_success(sess);
		if (res < 0)
			return res;
		return AUTH_HANDLER_SUCCESS;
	}
	res = send_auth_failure(sess, cbs->methods_str, auth_res == DSSH_AUTH_PARTIAL);
	if (res < 0)
		return res;
	return AUTH_HANDLER_CONTINUE;
}

/*
 * Handle "password" authentication method (normal + change).
 */
static int
handle_auth_password(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, const uint8_t *user,
    size_t user_len, uint8_t *payload, size_t payload_len, size_t rpos)
{
	size_t secret_start = rpos;
	int res = 0;
	int auth_res = DSSH_AUTH_FAILURE;
	struct dssh_bytes_result prompt = {0};
	if (cbs->password_cb == NULL && cbs->passwd_change_cb == NULL)
		goto done;
	if (!dssh_buffer_has(payload_len, rpos, 1)) {
		res = DSSH_ERROR_PARSE;
		goto done;
	}
	bool change = payload[rpos] != 0;
	rpos++;
	size_t pw_len;
	const uint8_t *pw;
	if (!auth_read_string(payload, payload_len, &rpos, &pw, &pw_len)) {
		res = DSSH_ERROR_PARSE;
		goto done;
	}
	if (change) {
		const uint8_t *new_pw;
		size_t new_pw_len;
		if (!auth_read_string(payload, payload_len, &rpos, &new_pw, &new_pw_len)
		    || rpos != payload_len) {
			res = DSSH_ERROR_PARSE;
			goto done;
		}
		if (cbs->passwd_change_cb != NULL)
			auth_res = cbs->passwd_change_cb(user, user_len, pw, pw_len, new_pw, new_pw_len,
			    &prompt, cbs->cbdata);
	}
	else {
		if (rpos != payload_len) {
			res = DSSH_ERROR_PARSE;
			goto done;
		}
		if (cbs->password_cb != NULL)
			auth_res = cbs->password_cb(user, user_len, pw, pw_len, &prompt, cbs->cbdata);
	}

done:
	if (secret_start < payload_len)
		dssh_cleanse(&payload[secret_start], payload_len - secret_start);
	if (res < 0) {
		dssh_result_release(&prompt.lease);
		return res;
	}
	return password_dispatch(sess, cbs, auth_res, &prompt);
}

/*
 * Handle "keyboard-interactive" authentication method.
 */
static int
handle_auth_kbi_run(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, const uint8_t *user,
    size_t user_len, const uint8_t *payload, size_t payload_len, size_t rpos,
    uint8_t **next_payload, size_t *next_payload_len, struct auth_kbi_views *views)
{
	int res;

	if (cbs->keyboard_interactive_cb == NULL) {
		res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}

	/* Language and submethods are not selected by this implementation. */
	const uint8_t *ignored;
	size_t ignored_len;
	if (!auth_read_string(payload, payload_len, &rpos, &ignored, &ignored_len)
	    || !auth_read_string(payload, payload_len, &rpos, &ignored, &ignored_len)
	    || rpos != payload_len)
		return DSSH_ERROR_PARSE;

	/* KBI prompt/response loop. */
	uint32_t        num_responses  = 0;
	uint32_t        last_nprompts  = 0;

	for (;;) {
		struct dssh_auth_challenge challenge = {0};
		int auth_res = cbs->keyboard_interactive_cb(user, user_len, num_responses,
		    num_responses > 0 ? views->text : NULL,
		    num_responses > 0 ? views->lengths : NULL, &challenge, cbs->cbdata);
		clear_auth_receive_buffer(sess);
		num_responses = 0;

		if (!auth_result_valid(auth_res, DSSH_AUTH_KBI_PROMPT) || !kbi_challenge_lease_valid(&challenge)) {
			dssh_result_release(&challenge.lease);
			return DSSH_ERROR_INVALID;
		}
		if (auth_res == DSSH_AUTH_DISCONNECT) {
			dssh_result_release(&challenge.lease);
			return transport_disconnect_result(sess, SSH_DISCONNECT_BY_APPLICATION,
			    "authentication rejected", DSSH_ERROR_TERMINATED);
		}
		if (auth_res == DSSH_AUTH_SUCCESS) {
			dssh_result_release(&challenge.lease);
			res = send_auth_success(sess);
			if (res < 0)
				return res;
			return AUTH_HANDLER_SUCCESS;
		}
		if (auth_res != DSSH_AUTH_KBI_PROMPT) {
			dssh_result_release(&challenge.lease);
			res = send_auth_failure(sess, cbs->methods_str, auth_res == DSSH_AUTH_PARTIAL);
			if (res < 0)
				return res;
			return AUTH_HANDLER_CONTINUE;
		}

		/* Flush pending banner before INFO_REQUEST */
		res = flush_pending_banner(sess);
		if (res < 0) {
			dssh_result_release(&challenge.lease);
			return res;
		}

		/* Send INFO_REQUEST */
		last_nprompts = challenge.count;
		res = send_info_request(sess, &challenge);
		dssh_result_release(&challenge.lease);
		if (res < 0)
			return res;

		/* Wait for the response or a replacement authentication request.
		 * Unrecognized packets do not complete or abort the method. */
		uint8_t  resp_type;
		uint8_t *resp_payload;
		size_t   resp_len;

		for (;;) {
			res = recv_packet(sess, &resp_type, &resp_payload, &resp_len);
			if (res < 0)
				return res;
			if (resp_type == SSH_MSG_USERAUTH_REQUEST) {
				*next_payload = resp_payload;
				*next_payload_len = resp_len;
				return AUTH_HANDLER_RESTART;
			}
			if (resp_type == SSH_MSG_USERAUTH_INFO_RESPONSE)
				break;
			res = send_unimplemented(sess, sess->trans.last_rx_seq);
			if (res < 0)
				return res;
		}

		/* Parse INFO_RESPONSE */
		size_t rp = 1; /* skip msg_type */

		if (!dssh_buffer_has(resp_len, rp, 4))
			return DSSH_ERROR_PARSE;
		num_responses = DSSH_GET_U32(&resp_payload[rp]);
		rp += 4;

		if (num_responses != last_nprompts) {
			clear_auth_receive_buffer(sess);
			res = send_auth_failure(sess, cbs->methods_str, false);
			if (res < 0)
				return res;
			return AUTH_HANDLER_CONTINUE;
		}

		if (num_responses > (resp_len - rp) / 4)
			return DSSH_ERROR_PARSE;
#if SIZE_MAX < UINT32_MAX
		if (num_responses > SIZE_MAX)
			return DSSH_ERROR_INVALID;
#endif
		size_t response_count = (size_t)num_responses;
		res = auth_kbi_views_reserve(views, response_count, false);
		if (res < 0)
			return res;

		for (uint32_t i = 0; i < num_responses; i++) {
			if (!auth_read_string(resp_payload, resp_len, &rp, &views->text[i], &views->lengths[i])) {
				return DSSH_ERROR_PARSE;
			}
		}
		if (rp != resp_len) {
			return DSSH_ERROR_PARSE;
		}
	}
}

static int
handle_auth_kbi(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs,
    const uint8_t *user, size_t user_len, const uint8_t *payload, size_t payload_len, size_t rpos,
    uint8_t **next_payload, size_t *next_payload_len)
{
	struct auth_kbi_views views = {0};
	int ret = handle_auth_kbi_run(sess, cbs, user, user_len, payload, payload_len, rpos,
	    next_payload, next_payload_len, &views);
	auth_kbi_views_free(&views);
	return ret;
}

/*
 * Handle "publickey" authentication method (probe + verify).
 */
DSSH_TESTABLE int
handle_auth_publickey(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, const uint8_t *user,
    size_t user_len, const uint8_t *payload, size_t payload_len, size_t rpos)
{
	int res;

	if (cbs->publickey_cb == NULL) {
		res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}

	/* Parse: boolean has_sig, string algo, string pubkey [, string sig] */
	if (!dssh_buffer_has(payload_len, rpos, 1))
		return DSSH_ERROR_PARSE;

	bool has_sig = (payload[rpos] != 0);

	rpos++;

	const uint8_t *algo, *pk_blob;
	size_t algo_len, pk_len;
	if (!auth_read_string(payload, payload_len, &rpos, &algo, &algo_len)
	    || !auth_read_string(payload, payload_len, &rpos, &pk_blob, &pk_len))
		return DSSH_ERROR_PARSE;
	size_t before_sig = rpos;
	const uint8_t *sig_blob = NULL;
	size_t sig_len = 0;
	if (has_sig && !auth_read_string(payload, payload_len, &rpos, &sig_blob, &sig_len))
		return DSSH_ERROR_PARSE;
	if (rpos != payload_len)
		return DSSH_ERROR_PARSE;
	if (!auth_name_valid(algo, algo_len))
		return send_auth_failure(sess, cbs->methods_str, false);
	char algo_name[DSSH_ALGO_NAME_MAX + 1];
	memcpy(algo_name, algo, algo_len);
	algo_name[algo_len] = 0;
	dssh_key_algo ka = find_key_algo(algo_name);
	if (ka == NULL || ka->verify == NULL)
		return send_auth_failure(sess, cbs->methods_str, false);

	if (!has_sig) {
		int auth_res = cbs->publickey_cb(user, user_len, algo_name, pk_blob, pk_len, false, cbs->cbdata);
		if (!auth_result_valid(auth_res, DSSH_AUTH_SUCCESS))
			return DSSH_ERROR_INVALID;
		if (auth_res == DSSH_AUTH_DISCONNECT)
			return transport_disconnect_result(sess, SSH_DISCONNECT_BY_APPLICATION,
			    "authentication rejected", DSSH_ERROR_TERMINATED);
		if (auth_res == DSSH_AUTH_SUCCESS)
			res = send_pk_ok(sess, algo_name, algo_len, pk_blob, pk_len);
		else
			res = send_auth_failure(sess, cbs->methods_str, false);
		return res < 0 ? res : AUTH_HANDLER_CONTINUE;
	}

	/* Bind the initial session ID and the exact request bytes preceding
	 * the signature string length field. */
	if (sess->trans.session_id_sz > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	if ((sess->trans.session_id_sz > SIZE_MAX - 4) || (before_sig > SIZE_MAX - 4 - sess->trans.session_id_sz))
		return DSSH_ERROR_INVALID;

	size_t   sd_len    = 4 + sess->trans.session_id_sz + before_sig;
	uint8_t *sign_data = malloc(sd_len);

	if (sign_data == NULL)
		return DSSH_ERROR_ALLOC;

	size_t sp = 0;

	DSSH_PUT_U32((uint32_t)sess->trans.session_id_sz, sign_data, &sp);
	memcpy(&sign_data[sp], sess->trans.session_id, sess->trans.session_id_sz);
	sp += sess->trans.session_id_sz;
	memcpy(&sign_data[sp], payload, before_sig);
	sp += before_sig;

	int vres = ka->verify(pk_blob, pk_len, sig_blob, sig_len, sign_data, sp);
	if (vres > 0)
		vres = DSSH_ERROR_INVALID;

	free(sign_data);

	if (vres < 0) {
		res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}

	/* Signature valid -- ask app if user is authorized */
	int auth_res = cbs->publickey_cb(user, user_len, algo_name, pk_blob, pk_len, true, cbs->cbdata);
	if (!auth_result_valid(auth_res, DSSH_AUTH_SUCCESS))
		return DSSH_ERROR_INVALID;

	if (auth_res == DSSH_AUTH_DISCONNECT) {
		return transport_disconnect_result(sess, SSH_DISCONNECT_BY_APPLICATION,
		    "authentication rejected", DSSH_ERROR_TERMINATED);
	}
	if (auth_res == DSSH_AUTH_SUCCESS) {
		res = send_auth_success(sess);
		if (res < 0)
			return res;
		return AUTH_HANDLER_SUCCESS;
	}
	res = send_auth_failure(sess, cbs->methods_str, auth_res == DSSH_AUTH_PARTIAL);
	if (res < 0)
		return res;
	return AUTH_HANDLER_CONTINUE;
}

/* ================================================================
 * Server-side authentication loop
 * ================================================================ */
static int
auth_server_accept_service(struct dssh_session_s *sess)
{
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;
	int      res;

	/* Receive SERVICE_REQUEST, preserving the exchange across unknown
	 * messages as required by RFC 4253 s11.4. */
	for (;;) {
		res = recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_SERVICE_REQUEST)
			break;
		res = send_unimplemented(sess, sess->trans.last_rx_seq);
		if (res < 0)
			return res;
	}

	/* Accept only a complete request for the authentication service. */
	size_t service_pos = 1;
	const uint8_t *service;
	size_t service_len;
	if (!auth_read_string(payload, payload_len, &service_pos, &service, &service_len)
	    || service_pos != payload_len)
		return DSSH_ERROR_PARSE;
	if (service_len != DSSH_STRLEN(service_userauth)
	    || memcmp(service, service_userauth, service_len) != 0)
		return transport_disconnect_result(sess, SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
		    "service unavailable", DSSH_ERROR_TERMINATED);

	/* Send SERVICE_ACCEPT. */
	{
		int      err;
		size_t accept_len = 5 + DSSH_STRLEN(service_userauth);
		uint8_t *accept = auth_send_begin(sess, SSH_MSG_SERVICE_ACCEPT, accept_len, &err);

		if (accept == NULL)
			return err;

		size_t pos = 0;

		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		DSSH_PUT_U32(DSSH_STRLEN(service_userauth), accept, &pos);
		memcpy(&accept[pos], service_userauth, DSSH_STRLEN(service_userauth));
		pos += DSSH_STRLEN(service_userauth);
		res = send_commit(sess, pos, NULL);
		if (res < 0)
			return res;
	}

	sess->server_auth_service_accepted = true;
	return 0;
}

/* Identity storage is session-owned, bounded by the receive packet capacity,
 * and retained across method attempts and recoverable operation returns. */
static int
auth_server_request_begin(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs,
    const uint8_t *user, size_t user_len, const uint8_t *method, size_t method_len)
{
	bool new_identity = !sess->server_auth_user_bound || user_len != sess->server_auth_user_len;
	if (!new_identity && user_len > 0)
		new_identity = memcmp(user, sess->server_auth_user, user_len) != 0;
	if (sess->server_auth_user_bound && new_identity && cbs->request_cb == NULL)
		return transport_disconnect_result(sess, SSH_DISCONNECT_BY_APPLICATION,
		    "authentication identity changed", DSSH_ERROR_TERMINATED);
	if (user_len > sess->trans.packet_buf_sz) {
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	size_t needed = user_len > 0 ? user_len : 1;
	if (needed > sess->server_auth_user_capacity) {
		uint8_t *storage = realloc(sess->server_auth_user, needed);
		if (storage == NULL) {
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
		sess->server_auth_user = storage;
		sess->server_auth_user_capacity = needed;
	}
	if (new_identity) {
		dssh_cleanse(sess->server_auth_user, sess->server_auth_user_capacity);
		if (user_len > 0)
			memcpy(sess->server_auth_user, user, user_len);
		sess->server_auth_user_len = user_len;
		sess->server_auth_user_bound = true;
	}
	if (cbs->request_cb != NULL) {
		int ret = cbs->request_cb(sess->server_auth_user, user_len, method, method_len,
		    new_identity, cbs->cbdata);
		if (sess->terminate)
			return DSSH_ERROR_TERMINATED;
		if (ret != 0)
			return transport_disconnect_result(sess, SSH_DISCONNECT_BY_APPLICATION,
			    "authentication request rejected", DSSH_ERROR_TERMINATED);
	}
	if (cbs->methods_str == NULL) {
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	const uint8_t *methods = (const uint8_t *)cbs->methods_str;
	if (!auth_methods_valid(methods, strlen(cbs->methods_str))) {
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	return 0;
}

static int
auth_server_run(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, uint8_t *username_out,
    size_t *username_out_len)
{
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res;
	if (!sess->server_auth_service_accepted) {
		res = auth_server_accept_service(sess);
		if (res < 0)
			return res;
	}
	bool have_request = false;

	/* Auth loop */
	for (;;) {
		/* Send pending banner before next auth exchange */
		res = flush_pending_banner(sess);
		if (res < 0)
			return res;

		if (!have_request) {
			res = recv_packet(sess, &msg_type, &payload, &payload_len);
			if (res < 0)
				return res;
		}
		have_request = false;
		if (msg_type != SSH_MSG_USERAUTH_REQUEST) {
			res = send_unimplemented(sess, sess->trans.last_rx_seq);
			if (res < 0)
				return res;
			continue;
		}

		const uint8_t *user, *method;
		size_t         user_len, method_len;
		int64_t        rpos_raw =
		    parse_userauth_prefix(payload, payload_len, &user, &user_len, &method, &method_len);

		if (rpos_raw == DSSH_ERROR_REJECTED)
			return transport_disconnect_result(sess, SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
			    "service unavailable", DSSH_ERROR_TERMINATED);
		if (rpos_raw < 0)
			return (int)rpos_raw;
#if SIZE_MAX < INT64_MAX
		if (rpos_raw > SIZE_MAX)
			return DSSH_ERROR_INVALID;
#endif
		size_t rpos = (size_t)rpos_raw;

		res = auth_server_request_begin(sess, cbs, user, user_len, method, method_len);
		if (res < 0)
			return res;
		/* INFO_RESPONSE reuses the receive packet; the identity stays live. */
		user = sess->server_auth_user;

		/* Dispatch by method */
		int r;

		if ((method_len == DSSH_STRLEN(method_none))
		    && (memcmp(method, method_none, DSSH_STRLEN(method_none)) == 0)) {
			if (rpos != payload_len)
				return DSSH_ERROR_PARSE;
			r = handle_auth_none(sess, cbs, user, user_len);
		}
		else if ((method_len == DSSH_STRLEN(method_password))
			 && (memcmp(method, method_password, DSSH_STRLEN(method_password)) == 0)) {
			r = handle_auth_password(sess, cbs, user, user_len, payload, payload_len, rpos);
		}
		else if ((method_len == DSSH_STRLEN(method_keyboard_interactive))
			 && (memcmp(method, method_keyboard_interactive, DSSH_STRLEN(method_keyboard_interactive))
			     == 0)) {
			r = handle_auth_kbi(sess, cbs, user, user_len, payload, payload_len, rpos,
			    &payload, &payload_len);
		}
		else if ((method_len == DSSH_STRLEN(method_publickey))
			 && (memcmp(method, method_publickey, DSSH_STRLEN(method_publickey)) == 0)) {
			r = handle_auth_publickey(sess, cbs, user, user_len, payload, payload_len, rpos);
		}
		else {
			/* Unknown method */
			r = send_auth_failure(sess, cbs->methods_str, false);
			if (r < 0)
				return r;
			continue;
		}

		if (r < 0)
			return r;
		if (r == AUTH_HANDLER_SUCCESS)
			goto done;
		if (r == AUTH_HANDLER_RESTART) {
			msg_type = SSH_MSG_USERAUTH_REQUEST;
			have_request = true;
		}
	}

done:
	if ((username_out != NULL) && (username_out_len != NULL)) {
		size_t saved_user_len = sess->server_auth_user_len < 255 ? sess->server_auth_user_len : 255;
		size_t copy_len = saved_user_len < *username_out_len ? saved_user_len : *username_out_len;

		if (copy_len > 0)
			memcpy(username_out, sess->server_auth_user, copy_len);
		*username_out_len = copy_len;
	}
	return 0;
}

/* The receive filter applies only while this server operation owns the
 * authentication exchange. Restore it on every return, including errors. */
DSSH_TESTABLE int
auth_server_impl(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, uint8_t *username_out,
    size_t *username_out_len)
{
	size_t methods_len = strlen(cbs->methods_str);
	const uint8_t *methods = (const uint8_t *)cbs->methods_str;
	if (!auth_methods_valid(methods, methods_len))
		return DSSH_ERROR_INVALID;
	bool previous = sess->server_auth_active;
	sess->server_auth_active = true;
	int ret = auth_server_run(sess, cbs, username_out, username_out_len);

	clear_auth_receive_buffer(sess);
	sess->server_auth_active = previous;
	return ret;
}

/*
 * Check whether a negative result from a protocol function should
 * be promoted to DSSH_ERROR_TERMINATED.  If the session's terminate
 * flag was set (by send_packet/recv_packet on a fatal I/O or crypto
 * error), the connection is dead and callers need to know.
 *
 * Complete USERAUTH_FAILURE replies produce positive PARTIAL/CONTINUE
 * outcomes and do not terminate the session.
 */
static inline int
auth_check_terminated(struct dssh_session_s *sess, int res)
{
	if ((res < 0) && sess->terminate) {
		DSSH_LOGF(sess, DSSH_LOG_ERROR, res, "Authentication operation terminated: %s",
		    dssh_strerror(res));
		dssh_log_termination(sess, res);
		return DSSH_ERROR_TERMINATED;
	}
	if (res == DSSH_AUTH_PARTIAL)
		DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE, "Authentication factor accepted; more authentication required");
	else if (res == DSSH_AUTH_CONTINUE)
		DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE, "Authentication requires another method");
	else if (res < 0)
		DSSH_LOGF(sess, DSSH_LOG_ERROR, res, "Authentication operation failed: %s", dssh_strerror(res));
	else if (res == 0)
		DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE, "Authentication operation completed successfully");
	return res;
}

/* The public receive operation owns the complete authentication exchange.
 * A method-list response or partial success must leave authentication open.
 * Private component entry points retain their independent wire-test scope. */
static int
auth_entry_check(struct dssh_session_s *sess, bool client)
{
	if (session_receive_api_conflict(sess))
		return DSSH_ERROR_INVALID;
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;
	if (sess->trans.client != client || !sess->handshake_complete || sess->auth_complete)
		return DSSH_ERROR_INVALID;
	return 0;
}

DSSH_PUBLIC int
dssh_auth_server(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, uint8_t *username_out,
    size_t *username_out_len)
{
	if (sess != NULL && (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess)))
		return DSSH_ERROR_INVALID;
	if ((cbs == NULL) || (sess == NULL) || (cbs->methods_str == NULL)
	    || ((username_out == NULL) != (username_out_len == NULL)))
		return DSSH_ERROR_INVALID;
	int ret = auth_entry_check(sess, false);
	if (ret < 0)
		return ret;
	struct dssh_session_scope scope;

	session_scope_enter(&scope, sess);
	ret = auth_check_terminated(sess, auth_server_impl(sess, cbs, username_out, username_out_len));

	if (ret == 0)
		sess->auth_complete = true;
	session_scope_leave(&scope);
	return ret;
}

DSSH_PUBLIC int
dssh_auth_set_banner(struct dssh_session_s *sess, const char *message, const char *language)
{
	if (sess != NULL && (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess)))
		return DSSH_ERROR_INVALID;
	if (sess == NULL)
		return DSSH_ERROR_INVALID;

	if (message == NULL) {
		free(sess->pending_banner);
		free(sess->pending_banner_lang);
		sess->pending_banner = NULL;
		sess->pending_banner_lang = NULL;
		return 0;
	}
	size_t message_len = strlen(message);
	size_t language_len = language != NULL ? strlen(language) : 0;
	if (message_len == 0)
		return DSSH_ERROR_INVALID;
#if SIZE_MAX > UINT32_MAX
	if (message_len > UINT32_MAX || language_len > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
#endif
	if (message_len > SIZE_MAX - 9 || language_len > SIZE_MAX - 9 - message_len)
		return DSSH_ERROR_TOOLONG;
	char *new_message = malloc(message_len + 1);
	if (new_message == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(new_message, message, message_len + 1);
	char *new_language = NULL;
	if (language != NULL) {
		new_language = malloc(language_len + 1);
		if (new_language == NULL) {
			free(new_message);
			return DSSH_ERROR_ALLOC;
		}
		memcpy(new_language, language, language_len + 1);
	}
	free(sess->pending_banner);
	free(sess->pending_banner_lang);
	sess->pending_banner = new_message;
	sess->pending_banner_lang = new_language;
	return 0;
}

/* ================================================================
 * Client-side authentication
 * ================================================================ */
static int
dssh_auth_request_service(struct dssh_session_s *sess, const char *service)
{
	size_t slen = strlen(service);

	if ((slen > UINT32_MAX) || (slen > SIZE_MAX - 5))
		return DSSH_ERROR_INVALID;

	int      err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_SERVICE_REQUEST, 5 + slen, &err);

	if (msg == NULL)
		return err;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_SERVICE_REQUEST;
	DSSH_PUT_U32((uint32_t)slen, msg, &pos);
	memcpy(&msg[pos], service, slen);
	pos += slen;

	ret = send_commit(sess, pos, NULL);
	if (ret < 0)
		return ret;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	for (;;) {
		ret = recv_packet(sess, &msg_type, &payload, &payload_len);
		if (ret < 0)
			return ret;
		if (msg_type == SSH_MSG_SERVICE_ACCEPT)
			break;
		ret = send_unimplemented(sess, sess->trans.last_rx_seq);
		if (ret < 0)
			return ret;
	}
	if (payload_len != 5 + slen || DSSH_GET_U32(&payload[1]) != slen
	    || memcmp(&payload[5], service, slen) != 0)
		return DSSH_ERROR_PARSE;

	return 0;
}

static int
ensure_auth_service(struct dssh_session_s *sess)
{
	if (sess->auth_service_requested)
		return 0;

	int res = dssh_auth_request_service(sess, service_userauth);

	if (res == 0)
		sess->auth_service_requested = true;
	return res;
}

/*
 * Serialize the common USERAUTH_REQUEST prefix into buf:
 *   byte      SSH_MSG_USERAUTH_REQUEST
 *   string    username
 *   string    "ssh-connection"
 *   string    method
 * Returns the number of bytes written.  Caller must ensure buf is large
 * enough (1 + 4 + ulen + 4 + sizeof(service_connection)-1 + 4 + method_len).
 */
static size_t
build_userauth_request(uint8_t *buf, const char *username, size_t ulen, const char *method, size_t method_len)
{
	size_t pos = 0;

	buf[pos++] = SSH_MSG_USERAUTH_REQUEST;
	DSSH_PUT_U32((uint32_t)ulen, buf, &pos);
	memcpy(&buf[pos], username, ulen);
	pos += ulen;
	DSSH_PUT_U32(DSSH_STRLEN(service_connection), buf, &pos);
	memcpy(&buf[pos], service_connection, DSSH_STRLEN(service_connection));
	pos += DSSH_STRLEN(service_connection);
	DSSH_PUT_U32((uint32_t)method_len, buf, &pos);
	memcpy(&buf[pos], method, method_len);
	pos += method_len;
	return pos;
}

DSSH_TESTABLE int
auth_begin_impl(struct dssh_session_s *sess, const char *username)
{
	int res = ensure_auth_service(sess);

	if (res < 0)
		return res;

	size_t ulen = strlen(username);

	if ((ulen > UINT32_MAX) || (ulen > SIZE_MAX - AUTH_NONE_FIXED))
		return DSSH_ERROR_INVALID;

	int      err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_USERAUTH_REQUEST, AUTH_NONE_FIXED + ulen, &err);

	if (msg == NULL)
		return err;

	size_t pos = build_userauth_request(msg, username, ulen, method_none, DSSH_STRLEN(method_none));

	res = send_commit(sess, pos, NULL);
	if (res < 0)
		return res;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	for (;;) {
		res = recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_USERAUTH_BANNER) {
			res = handle_banner(sess, payload, payload_len);
			if (res < 0)
				return res;
			continue;
		}
		if (msg_type == SSH_MSG_USERAUTH_SUCCESS || msg_type == SSH_MSG_USERAUTH_FAILURE)
			break;
		res = send_unimplemented(sess, sess->trans.last_rx_seq);
		if (res < 0)
			return res;
	}

	if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
		return auth_success_result(sess, payload_len);
	if (msg_type == SSH_MSG_USERAUTH_FAILURE)
		return auth_failure_result(sess, payload, payload_len);

	return DSSH_ERROR_PARSE;
}

DSSH_PUBLIC int
dssh_auth_begin(struct dssh_session_s *sess, const char *username)
{
	if (sess != NULL && (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess)))
		return DSSH_ERROR_INVALID;
	if (username == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	int ret = auth_entry_check(sess, true);
	if (ret < 0)
		return ret;
	struct dssh_session_scope scope;

	session_scope_enter(&scope, sess);
	ret = auth_check_terminated(sess, auth_begin_impl(sess, username));

	if (ret == DSSH_AUTH_SUCCESS)
		sess->auth_complete = true;
	session_scope_leave(&scope);
	return ret;
}

DSSH_TESTABLE int64_t
auth_get_methods_inner(struct dssh_session_s *sess, char *methods, size_t methods_sz)
{
	if (!sess->auth_methods_available)
		return DSSH_ERROR_INIT;
	/* Publication bounds the snapshot by the session packet limit and
	 * reserves its terminating byte before changing any visible state. */
	size_t needed = sess->auth_methods_len + 1;
#if SIZE_MAX > INT64_MAX
	if (needed > INT64_MAX)
		return DSSH_ERROR_INVALID;
#endif
	int64_t result = (int64_t)needed;
	if (methods == NULL || methods_sz == 0)
		return result;
	if (methods_sz < needed)
		return DSSH_ERROR_TOOLONG;
	if (sess->auth_methods_len > 0)
		memcpy(methods, sess->auth_methods, sess->auth_methods_len);
	methods[sess->auth_methods_len] = 0;
	return result;
}

DSSH_PUBLIC int64_t
dssh_auth_get_methods(struct dssh_session_s *sess, char *methods, size_t methods_sz)
{
	if (sess == NULL || dssh_io_callback_active(sess) || !sess->trans.client)
		return DSSH_ERROR_INVALID;
	return auth_get_methods_inner(sess, methods, methods_sz);
}

/*
 * Build and send a password auth request.
 * A nonnull replacement selects password change, including an empty password.
 * Its lease releases after copying, before the completed packet enters I/O.
 */
static int
send_password_request(struct dssh_session_s *sess, const char *username, const char *old_password,
    struct dssh_bytes_result *replacement)
{
	bool change = replacement != NULL;
	const uint8_t *new_password = change ? replacement->bytes.data : NULL;
	size_t new_password_len = change ? replacement->bytes.len : 0;
	if (change && new_password_len > 0 && new_password == NULL)
		return DSSH_ERROR_INVALID;
	size_t ulen  = strlen(username);
	size_t oplen = strlen(old_password);

	if ((ulen > UINT32_MAX) || (oplen > UINT32_MAX) || (new_password_len > UINT32_MAX))
		return DSSH_ERROR_INVALID;

	if ((ulen > SIZE_MAX - AUTH_PASSWORD_FIXED) || (oplen > SIZE_MAX - AUTH_PASSWORD_FIXED - ulen))
		return DSSH_ERROR_INVALID;

	size_t msg_len = AUTH_PASSWORD_FIXED + ulen + oplen;

	if (change) {
		if (msg_len > SIZE_MAX - 4 || new_password_len > SIZE_MAX - 4 - msg_len)
			return DSSH_ERROR_INVALID;
		msg_len += 4 + new_password_len;
	}

	int      err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_USERAUTH_REQUEST, msg_len, &err);

	if (msg == NULL)
		return err;

	size_t pos = build_userauth_request(msg, username, ulen, method_password, DSSH_STRLEN(method_password));

	msg[pos++] = change ? 1 : 0;
	DSSH_PUT_U32((uint32_t)oplen, msg, &pos);
	memcpy(&msg[pos], old_password, oplen);
	pos += oplen;
	if (change) {
		DSSH_PUT_U32((uint32_t)new_password_len, msg, &pos);
		if (new_password_len > 0)
			memcpy(&msg[pos], new_password, new_password_len);
		pos += new_password_len;
	}

	if (replacement != NULL)
		dssh_result_release(&replacement->lease);
	return send_commit_sensitive(sess, pos, NULL);
}

DSSH_TESTABLE int
auth_password_impl(struct dssh_session_s *sess, const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *passwd_change_cbdata)
{
	int res = ensure_auth_service(sess);

	if (res < 0)
		return res;
	res = send_password_request(sess, username, password, NULL);
	if (res < 0)
		return res;

	for (;;) {
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;

		res = recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;

		if (msg_type == SSH_MSG_USERAUTH_BANNER) {
			res = handle_banner(sess, payload, payload_len);
			if (res < 0)
				return res;
			continue;
		}

		if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
			return auth_success_result(sess, payload_len);

		if (msg_type == SSH_MSG_USERAUTH_FAILURE)
			return auth_failure_result(sess, payload, payload_len);

		if (msg_type == SSH_MSG_USERAUTH_PASSWD_CHANGEREQ) {
			size_t pos = 1;
			const uint8_t *prompt, *language;
			size_t prompt_len, lang_len;
			if (!auth_read_string(payload, payload_len, &pos, &prompt, &prompt_len)
			    || !auth_read_string(payload, payload_len, &pos, &language, &lang_len)
			    || pos != payload_len)
				return DSSH_ERROR_PARSE;
			if (passwd_change_cb == NULL)
				return DSSH_ERROR_INIT;

			struct dssh_bytes_result new_password = {0};

			res = passwd_change_cb(prompt, prompt_len, language, lang_len, &new_password,
			    passwd_change_cbdata);
			if (res > 0)
				res = DSSH_ERROR_INVALID;
			if (res == 0 && !password_result_lease_valid(&new_password))
				res = DSSH_ERROR_INVALID;
			if (res == 0)
				res = send_password_request(sess, username, password, &new_password);
			dssh_result_release(&new_password.lease);
			if (res < 0)
				return res;
			continue;
		}

		res = send_unimplemented(sess, sess->trans.last_rx_seq);
		if (res < 0)
			return res;
	}
}

DSSH_PUBLIC int
dssh_auth_password(struct dssh_session_s *sess, const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *passwd_change_cbdata)
{
	if (sess != NULL && (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess)))
		return DSSH_ERROR_INVALID;
	if ((password == NULL) || (username == NULL) || (sess == NULL))
		return DSSH_ERROR_INVALID;
	int ret = auth_entry_check(sess, true);
	if (ret < 0)
		return ret;
	struct dssh_session_scope scope;

	session_scope_enter(&scope, sess);
	ret = auth_check_terminated(sess, auth_password_impl(sess, username, password, passwd_change_cb, passwd_change_cbdata));

	if (ret == 0)
		sess->auth_complete = true;
	session_scope_leave(&scope);
	return ret;
}

static int
auth_kbi_run(struct dssh_session_s *sess, const char *username, dssh_auth_kbi_prompt_cb prompt_cb,
    void *cbdata, struct auth_kbi_views *views)
{
	if (prompt_cb == NULL)
		return DSSH_ERROR_INVALID;
	{
		int svc = ensure_auth_service(sess);

		if (svc < 0)
			return svc;
	}

	size_t ulen = strlen(username);

	if ((ulen > UINT32_MAX) || (ulen > SIZE_MAX - AUTH_KBI_FIXED))
		return DSSH_ERROR_INVALID;

	int      err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_USERAUTH_REQUEST, AUTH_KBI_FIXED + ulen, &err);

	if (msg == NULL)
		return err;

	int    res;
	size_t pos = build_userauth_request(msg, username, ulen, method_keyboard_interactive,
	    DSSH_STRLEN(method_keyboard_interactive));

	DSSH_PUT_U32(0, msg, &pos); /* language */
	DSSH_PUT_U32(0, msg, &pos); /* submethods */

	res = send_commit(sess, pos, NULL);
	if (res < 0)
		return res;

	for (;;) {
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;

		res = recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;

		if (msg_type == SSH_MSG_USERAUTH_BANNER) {
			res = handle_banner(sess, payload, payload_len);
			if (res < 0)
				return res;
			continue;
		}

		if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
			return auth_success_result(sess, payload_len);

		if (msg_type == SSH_MSG_USERAUTH_FAILURE)
			return auth_failure_result(sess, payload, payload_len);

		if (msg_type == SSH_MSG_USERAUTH_INFO_REQUEST) {
			size_t rpos = 1;
			const uint8_t *name, *instruction, *language;
			size_t name_len, instruction_len, language_len;
			if (!auth_read_string(payload, payload_len, &rpos, &name, &name_len)
			    || !auth_read_string(payload, payload_len, &rpos, &instruction, &instruction_len)
			    || !auth_read_string(payload, payload_len, &rpos, &language, &language_len))
				return DSSH_ERROR_PARSE;

			/* num_prompts */
			if (!dssh_buffer_has(payload_len, rpos, 4))
				return DSSH_ERROR_PARSE;

			uint32_t num_prompts = DSSH_GET_U32(&payload[rpos]);

			rpos += 4;

			if (num_prompts > (payload_len - rpos) / 6)
				return DSSH_ERROR_PARSE;

			struct dssh_auth_responses responses = {0};
			/* The complete minimum prompt encoding bounds the count by
			 * SIZE_MAX / 6 before narrowing or allocating callback views. */
			size_t prompt_count = (size_t)num_prompts;
			res = auth_kbi_views_reserve(views, prompt_count, true);
			if (res < 0)
				return res;
			for (uint32_t i = 0; i < num_prompts; i++) {
				if (!auth_read_string(payload, payload_len, &rpos, &views->text[i], &views->lengths[i])
				    || views->lengths[i] == 0 || !dssh_buffer_has(payload_len, rpos, 1)) {
					res = DSSH_ERROR_PARSE;
					goto kbi_cleanup;
				}
				views->echo[i] = payload[rpos] != 0;
				rpos++;
			}

			if (rpos != payload_len) {
				res = DSSH_ERROR_PARSE;
				goto kbi_cleanup;
			}
			/* Call the application's prompt callback */
			res = prompt_cb(name, name_len, instruction, instruction_len, num_prompts,
			    num_prompts > 0 ? views->text : NULL, num_prompts > 0 ? views->lengths : NULL,
			    num_prompts > 0 ? views->echo : NULL, &responses, cbdata);
			if (res > 0)
				res = DSSH_ERROR_INVALID;

			if (res < 0)
				goto kbi_cleanup;

			if (responses.count != num_prompts
			    || (responses.count > 0 && responses.items == NULL)
			    || (responses.lease.release == NULL
			    && (responses.lease.owner != NULL || responses.items != NULL))) {
				res = DSSH_ERROR_INVALID;
				goto kbi_cleanup;
			}
			if (prompt_count > SIZE_MAX / sizeof(*responses.items)) {
				res = DSSH_ERROR_INVALID;
				goto kbi_cleanup;
			}
			/* Build INFO_RESPONSE from callback's responses */
			size_t resp_sz = 1 + 4;

			for (uint32_t i = 0; i < num_prompts; i++) {
				if (responses.items[i].len > 0 && responses.items[i].data == NULL) {
					res = DSSH_ERROR_INVALID;
					goto kbi_cleanup;
				}
				if (responses.items[i].len > UINT32_MAX) {
					res = DSSH_ERROR_INVALID;
					goto kbi_cleanup;
				}
				if (resp_sz > SIZE_MAX - 4 || responses.items[i].len > SIZE_MAX - 4 - resp_sz) {
					res = DSSH_ERROR_INVALID;
					goto kbi_cleanup;
				}
				resp_sz += 4 + responses.items[i].len;
			}

			int      resp_err;
			uint8_t *resp = auth_send_begin(sess, SSH_MSG_USERAUTH_INFO_RESPONSE, resp_sz, &resp_err);

			if (resp == NULL) {
				res = resp_err;
				goto kbi_cleanup;
			}

			size_t rp = 0;

			resp[rp++] = SSH_MSG_USERAUTH_INFO_RESPONSE;
			DSSH_PUT_U32(num_prompts, resp, &rp);
			for (uint32_t i = 0; i < num_prompts; i++) {
				DSSH_PUT_U32((uint32_t)responses.items[i].len, resp, &rp);
				if (responses.items[i].len > 0)
					memcpy(&resp[rp], responses.items[i].data, responses.items[i].len);
				rp += responses.items[i].len;
			}
			dssh_result_release(&responses.lease);

			res = send_commit_sensitive(sess, rp, NULL);
			if (res < 0)
				return res;
			continue;

kbi_cleanup:
			dssh_result_release(&responses.lease);
			return res;
		}

		res = send_unimplemented(sess, sess->trans.last_rx_seq);
		if (res < 0)
			return res;
	}
}

DSSH_TESTABLE int
auth_kbi_impl(struct dssh_session_s *sess, const char *username, dssh_auth_kbi_prompt_cb prompt_cb, void *cbdata)
{
	struct auth_kbi_views views = {0};
	int ret = auth_kbi_run(sess, username, prompt_cb, cbdata, &views);
	auth_kbi_views_free(&views);
	return ret;
}

DSSH_PUBLIC int
dssh_auth_keyboard_interactive(struct dssh_session_s *sess, const char *username,
    dssh_auth_kbi_prompt_cb prompt_cb, void *cbdata)
{
	if (sess != NULL && (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess)))
		return DSSH_ERROR_INVALID;
	if ((prompt_cb == NULL) || (username == NULL) || (sess == NULL))
		return DSSH_ERROR_INVALID;
	int ret = auth_entry_check(sess, true);
	if (ret < 0)
		return ret;
	struct dssh_session_scope scope;

	session_scope_enter(&scope, sess);
	ret = auth_check_terminated(sess, auth_kbi_impl(sess, username, prompt_cb, cbdata));

	if (ret == 0)
		sess->auth_complete = true;
	session_scope_leave(&scope);
	return ret;
}

DSSH_TESTABLE int
auth_publickey_impl(struct dssh_session_s *sess, const char *username, const char *algo_name)
{
	{
		int svc = ensure_auth_service(sess);

		if (svc < 0)
			return svc;
	}
	dssh_key_algo ka = find_key_algo(algo_name);

	if ((ka == NULL) || (ka->sign == NULL) || (ka->pubkey == NULL))
		return DSSH_ERROR_INIT;
	if (ka->haskey == NULL)
		return DSSH_ERROR_INIT;
	dssh_key_algo_ctx *key_ctx = dssh_client_key_context(sess, ka);
	int res = ka->haskey(key_ctx);
	if (res < 0)
		return res;
	if (res == 0)
		return DSSH_ERROR_INIT;

	/* Get the public key blob */
	const uint8_t *pubkey_buf = NULL;
	size_t         pubkey_len = 0;
	res = ka->pubkey(&pubkey_buf, &pubkey_len, key_ctx);
	if (res > 0)
		res = DSSH_ERROR_INVALID;

	if (res < 0)
		return res;
	if (pubkey_buf == NULL || pubkey_len == 0)
		return DSSH_ERROR_INVALID;

	size_t ulen = strlen(username);
	size_t alen = strlen(algo_name);

	if ((ulen > UINT32_MAX) || (alen > UINT32_MAX) || (pubkey_len > UINT32_MAX)
	    || (sess->trans.session_id_sz > UINT32_MAX))
		return DSSH_ERROR_INVALID;

	size_t rt = sess->trans.session_id_sz;

	if (rt > SIZE_MAX - AUTH_PUBLICKEY_FIXED
	    || (ulen > SIZE_MAX - AUTH_PUBLICKEY_FIXED - rt) || (alen > SIZE_MAX - AUTH_PUBLICKEY_FIXED - rt - ulen)
	    || (pubkey_len > SIZE_MAX - AUTH_PUBLICKEY_FIXED - rt - ulen - alen))
		return DSSH_ERROR_INVALID;

	/*
         * Build the data to sign (RFC 4252 s7):
         *   string    session identifier
         *   byte      SSH_MSG_USERAUTH_REQUEST
         *   string    user name
         *   string    service name
         *   string    "publickey"
         *   boolean   TRUE
         *   string    public key algorithm name
         *   string    public key blob
         */
	size_t   sign_data_len = AUTH_PUBLICKEY_FIXED + rt + ulen + alen + pubkey_len;
	uint8_t *sign_data     = malloc(sign_data_len);

	if (sign_data == NULL)
		return DSSH_ERROR_ALLOC;

	size_t sp = 0;

	DSSH_PUT_U32((uint32_t)sess->trans.session_id_sz, sign_data, &sp);
	memcpy(&sign_data[sp], sess->trans.session_id, sess->trans.session_id_sz);
	sp += sess->trans.session_id_sz;
	size_t request_start = sp;
	sp += build_userauth_request(&sign_data[sp], username, ulen, method_publickey,
	    DSSH_STRLEN(method_publickey));
	sign_data[sp++] = 1; /* TRUE */
	DSSH_PUT_U32((uint32_t)alen, sign_data, &sp);
	memcpy(&sign_data[sp], algo_name, alen);
	sp += alen;
	DSSH_PUT_U32((uint32_t)pubkey_len, sign_data, &sp);
	memcpy(&sign_data[sp], pubkey_buf, pubkey_len);
	sp += pubkey_len;

	/* Sign the data */
	struct dssh_bytes_result signature = {0};
	const uint8_t *sig_buf = NULL;
	size_t   sig_len = 0;

	res = ka->sign(&signature, sign_data, sp, key_ctx);
	if (res > 0)
		res = DSSH_ERROR_INVALID;
	if (res < 0) {
		free(sign_data);
		dssh_result_release(&signature.lease);
		return res;
	}
	sig_buf = signature.bytes.data;
	sig_len = signature.bytes.len;
	if (signature.lease.release == NULL || sig_buf == NULL || sig_len == 0 || (sig_len > UINT32_MAX)
	    || (sig_len > SIZE_MAX - AUTH_PUBLICKEY_FIXED - ulen - alen - pubkey_len)) {
		free(sign_data);
		dssh_result_release(&signature.lease);
		return DSSH_ERROR_INVALID;
	}

	/*
         * Build the auth request message:
         *   byte      SSH_MSG_USERAUTH_REQUEST
         *   string    user name
         *   string    service name
         *   string    "publickey"
         *   boolean   TRUE
         *   string    public key algorithm name
         *   string    public key blob
         *   string    signature
         */
	int      err;
	uint8_t *msg = auth_send_begin(sess, SSH_MSG_USERAUTH_REQUEST,
	    AUTH_PUBLICKEY_FIXED + ulen + alen + pubkey_len + sig_len, &err);

	if (msg == NULL) {
		free(sign_data);
		dssh_result_release(&signature.lease);
		return err;
	}

	/* Emit the exact request prefix that was signed. No producer export
	 * borrow survives a later callback, and no second serialization is needed. */
	size_t pos = sp - request_start;
	memcpy(msg, &sign_data[request_start], pos);
	free(sign_data);
	DSSH_PUT_U32((uint32_t)sig_len, msg, &pos);
	memcpy(&msg[pos], sig_buf, sig_len);
	pos += sig_len;

	dssh_result_release(&signature.lease);
	res = send_commit(sess, pos, NULL);
	if (res < 0)
		return res;

	/* Wait for response */
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	for (;;) {
		res = recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_USERAUTH_BANNER) {
			res = handle_banner(sess, payload, payload_len);
			if (res < 0)
				return res;
			continue;
		}
		if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
			return auth_success_result(sess, payload_len);
		if (msg_type == SSH_MSG_USERAUTH_FAILURE)
			return auth_failure_result(sess, payload, payload_len);
		res = send_unimplemented(sess, sess->trans.last_rx_seq);
		if (res < 0)
			return res;
	}
}

DSSH_PUBLIC int
dssh_auth_publickey(struct dssh_session_s *sess, const char *username, const char *algo_name)
{
	if (sess != NULL && (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess)))
		return DSSH_ERROR_INVALID;
	if ((algo_name == NULL) || (username == NULL) || (sess == NULL))
		return DSSH_ERROR_INVALID;
	int ret = auth_entry_check(sess, true);
	if (ret < 0)
		return ret;
	struct dssh_session_scope scope;

	session_scope_enter(&scope, sess);
	ret = auth_check_terminated(sess, auth_publickey_impl(sess, username, algo_name));

	if (ret == 0)
		sess->auth_complete = true;
	session_scope_leave(&scope);
	return ret;
}
