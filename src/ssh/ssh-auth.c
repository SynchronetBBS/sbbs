// RFC 4252: SSH Authentication Protocol

#include <stdlib.h>
#include <string.h>

#include "deucessh-auth.h"
#include "deucessh-crypto.h"
#include "ssh-internal.h"

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

static int
handle_banner(struct dssh_session_s *sess, uint8_t *payload, size_t payload_len)
{
	dssh_auth_banner_cb cb = sess->banner_cb;

	if (cb == NULL)
		return 0;

	size_t   rpos = 1;
	uint32_t msg_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	msg_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + msg_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *message = &payload[rpos];

	rpos += msg_len;

	uint32_t       lang_len = 0;
	const uint8_t *language = NULL;

	if (rpos + 4 <= payload_len) {
		lang_len = DSSH_GET_U32(&payload[rpos]);
		rpos += 4;
		if (rpos + lang_len <= payload_len)
			language = &payload[rpos];
		else
			lang_len = 0;
	}

	cb(message, msg_len, language, lang_len, sess->banner_cbdata);
	return 0;
}

static int flush_pending_banner(struct dssh_session_s *sess);

/* Handler return values for per-method auth dispatch */
#define AUTH_HANDLER_CONTINUE 0 /* failure/partial sent, keep looping */
#define AUTH_HANDLER_SUCCESS  1 /* auth succeeded, goto done */

/* negative values = DSSH_ERROR_* (caller returns directly) */

/* ================================================================
 * Server-side helpers
 * ================================================================ */
static int
send_auth_failure(struct dssh_session_s *sess, const char *methods, bool partial_success)
{
	int ret = flush_pending_banner(sess);

	if (ret < 0)
		return ret;

	size_t mlen = strlen(methods);

	if ((mlen > UINT32_MAX) || (mlen > SIZE_MAX - AUTH_FAILURE_FIXED))
		return DSSH_ERROR_INVALID;

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_USERAUTH_FAILURE, &max, &err);

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
send_passwd_changereq(struct dssh_session_s *sess, const uint8_t *prompt, size_t prompt_len)
{
	if ((prompt_len > UINT32_MAX) || (prompt_len > SIZE_MAX - PASSWD_CHANGEREQ_FIXED))
		return DSSH_ERROR_INVALID;

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_USERAUTH_PASSWD_CHANGEREQ, &max, &err);

	if (msg == NULL)
		return err;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	DSSH_PUT_U32((uint32_t)prompt_len, msg, &pos);
	memcpy(&msg[pos], prompt, prompt_len);
	pos += prompt_len;
	DSSH_PUT_U32(0, msg, &pos);

	return send_commit(sess, pos, NULL);
}

static int
send_pk_ok(struct dssh_session_s *sess, const char *algo_name, size_t algo_len, const uint8_t *pubkey_blob,
    size_t pubkey_blob_len)
{
	if ((algo_len > UINT32_MAX) || (pubkey_blob_len > UINT32_MAX) || (algo_len > SIZE_MAX - 9)
	    || (pubkey_blob_len > SIZE_MAX - 9 - algo_len))
		return DSSH_ERROR_INVALID;

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_USERAUTH_PK_OK, &max, &err);

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

/*
 * Free KBI prompt arrays allocated by the callback.
 */
static void
free_kbi_prompts(char *name, char *instruction, uint32_t num_prompts, char **prompts, bool *echo)
{
	free(name);
	free(instruction);
	if (prompts != NULL) {
		for (uint32_t i = 0; i < num_prompts; i++)
			free(prompts[i]);
		free(prompts);
	}
	free(echo);
}

/*
 * Build and send SSH_MSG_USERAUTH_INFO_REQUEST (RFC 4256 s3.2).
 */
static int
send_info_request(struct dssh_session_s *sess, const char *name, const char *instruction, uint32_t num_prompts,
    char **prompts, const bool *echo)
{
	size_t name_len = name != NULL ? strlen(name) : 0;
	size_t inst_len = instruction != NULL ? strlen(instruction) : 0;

	/* Calculate total size */
	size_t msg_len = 1 + 4 + name_len + 4 + inst_len + 4 + 4;

	for (uint32_t i = 0; i < num_prompts; i++) {
		size_t plen = prompts[i] != NULL ? strlen(prompts[i]) : 0;

		if (msg_len > SIZE_MAX - 5 - plen)
			return DSSH_ERROR_INVALID;
		msg_len += 4 + plen + 1;
	}

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_USERAUTH_INFO_REQUEST, &max, &err);

	if (msg == NULL)
		return err;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;

	DSSH_PUT_U32((uint32_t)name_len, msg, &pos);
	if (name_len > 0) {
		memcpy(&msg[pos], name, name_len);
		pos += name_len;
	}
	DSSH_PUT_U32((uint32_t)inst_len, msg, &pos);
	if (inst_len > 0) {
		memcpy(&msg[pos], instruction, inst_len);
		pos += inst_len;
	}
	DSSH_PUT_U32(0, msg, &pos); /* language tag -- always empty */
	DSSH_PUT_U32(num_prompts, msg, &pos);

	for (uint32_t i = 0; i < num_prompts; i++) {
		size_t plen = prompts[i] != NULL ? strlen(prompts[i]) : 0;

		DSSH_PUT_U32((uint32_t)plen, msg, &pos);
		if (plen > 0) {
			memcpy(&msg[pos], prompts[i], plen);
			pos += plen;
		}
		msg[pos++] = echo[i] ? 1 : 0;
	}

	return send_commit(sess, pos, NULL);
}

/*
 * Parse the common prefix of a USERAUTH_REQUEST:
 *   string user, string service, string method
 * Returns the offset past the method string, or negative on error.
 */
DSSH_TESTABLE int64_t
parse_userauth_prefix(const uint8_t *payload, size_t payload_len, const uint8_t **username, size_t *username_len,
    const uint8_t **method, size_t *method_len)
{
	size_t   rpos = 1; /* skip msg_type */
	uint32_t ulen;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	ulen = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + ulen > payload_len)
		return DSSH_ERROR_PARSE;
	*username     = &payload[rpos];
	*username_len = ulen;
	rpos += ulen;

	/* service name (skip) */
	uint32_t slen;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	slen = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + slen > payload_len)
		return DSSH_ERROR_PARSE;
	rpos += slen;

	/* method name */
	uint32_t mlen;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	mlen = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + mlen > payload_len)
		return DSSH_ERROR_PARSE;
	*method     = &payload[rpos];
	*method_len = mlen;
	rpos += mlen;

	return (int64_t)rpos;
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
	size_t   max;
	int      err;
	uint8_t *pkt = send_begin(sess, SSH_MSG_USERAUTH_BANNER, &max, &err);

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
	if (auth_res == DSSH_AUTH_DISCONNECT) {
		dssh_transport_disconnect(sess, SSH_DISCONNECT_BY_APPLICATION, "authentication rejected");
		return DSSH_ERROR_TERMINATED;
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
static int
password_dispatch(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, int auth_res,
    uint8_t *prompt, size_t prompt_len)
{
	int res;

	if (auth_res == DSSH_AUTH_DISCONNECT) {
		free(prompt);
		dssh_transport_disconnect(sess, SSH_DISCONNECT_BY_APPLICATION, "authentication rejected");
		return DSSH_ERROR_TERMINATED;
	}
	if (auth_res == DSSH_AUTH_SUCCESS) {
		free(prompt);
		res = send_auth_success(sess);
		if (res < 0)
			return res;
		return AUTH_HANDLER_SUCCESS;
	}
	if ((auth_res == DSSH_AUTH_CHANGE_PASSWORD) && (prompt != NULL)) {
		res = send_passwd_changereq(sess, prompt, prompt_len);
		free(prompt);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}
	free(prompt);
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
    size_t user_len, const uint8_t *payload, size_t payload_len, size_t rpos)
{
	int res;

	if (cbs->password_cb == NULL) {
		res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}

	/* Parse: boolean change, string password [, string new_password] */
	if (rpos + 1 > payload_len)
		return DSSH_ERROR_PARSE;

	bool change = (payload[rpos] != 0);

	rpos++;

	uint32_t pw_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	pw_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + pw_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *pw = &payload[rpos];

	rpos += pw_len;

	if (change) {
		/* Password change request */
		uint32_t new_pw_len;

		if (rpos + 4 > payload_len)
			return DSSH_ERROR_PARSE;
		new_pw_len = DSSH_GET_U32(&payload[rpos]);
		rpos += 4;
		if (rpos + new_pw_len > payload_len)
			return DSSH_ERROR_PARSE;

		const uint8_t *new_pw     = &payload[rpos];
		int            auth_res   = DSSH_AUTH_FAILURE;
		uint8_t       *prompt     = NULL;
		size_t         prompt_len = 0;

		if (cbs->passwd_change_cb != NULL) {
			auth_res = cbs->passwd_change_cb(user, user_len, pw, pw_len, new_pw, new_pw_len, &prompt,
			    &prompt_len, cbs->cbdata);
		}

		return password_dispatch(sess, cbs, auth_res, prompt, prompt_len);
	}

	/* Normal password auth */
	uint8_t *prompt     = NULL;
	size_t   prompt_len = 0;
	int      auth_res   = cbs->password_cb(user, user_len, pw, pw_len, &prompt, &prompt_len, cbs->cbdata);

	return password_dispatch(sess, cbs, auth_res, prompt, prompt_len);
}

/*
 * Handle "keyboard-interactive" authentication method.
 */
static int
handle_auth_kbi(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, const uint8_t *user,
    size_t user_len, const uint8_t *payload, size_t payload_len, size_t rpos)
{
	int res;

	if (cbs->keyboard_interactive_cb == NULL) {
		res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}

	/* Parse language + submethods (both ignored) */
	uint32_t skip_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	skip_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + skip_len > payload_len)
		return DSSH_ERROR_PARSE;
	rpos += skip_len; /* language */

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	skip_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + skip_len > payload_len)
		return DSSH_ERROR_PARSE;
	rpos += skip_len; /* submethods */

			  /* KBI prompt/response loop */
	uint32_t        num_responses = 0;
	const uint8_t **responses     = NULL;
	size_t         *response_lens = NULL;

	for (;;) {
		char    *kbi_name     = NULL;
		char    *kbi_inst     = NULL;
		uint32_t kbi_nprompts = 0;
		char   **kbi_prompts  = NULL;
		bool    *kbi_echo     = NULL;
		int      auth_res     = cbs->keyboard_interactive_cb(user, user_len, num_responses, responses,
			     response_lens, &kbi_name, &kbi_inst, &kbi_nprompts, &kbi_prompts, &kbi_echo, cbs->cbdata);

		free(responses);
		free(response_lens);
		responses     = NULL;
		response_lens = NULL;
		num_responses = 0;

		if (auth_res == DSSH_AUTH_DISCONNECT) {
			free_kbi_prompts(kbi_name, kbi_inst, kbi_nprompts, kbi_prompts, kbi_echo);
			dssh_transport_disconnect(sess, SSH_DISCONNECT_BY_APPLICATION, "authentication rejected");
			return DSSH_ERROR_TERMINATED;
		}
		if (auth_res == DSSH_AUTH_SUCCESS) {
			free_kbi_prompts(kbi_name, kbi_inst, kbi_nprompts, kbi_prompts, kbi_echo);
			res = send_auth_success(sess);
			if (res < 0)
				return res;
			return AUTH_HANDLER_SUCCESS;
		}
		if (auth_res != DSSH_AUTH_KBI_PROMPT) {
			free_kbi_prompts(kbi_name, kbi_inst, kbi_nprompts, kbi_prompts, kbi_echo);
			res = send_auth_failure(sess, cbs->methods_str, auth_res == DSSH_AUTH_PARTIAL);
			if (res < 0)
				return res;
			return AUTH_HANDLER_CONTINUE;
		}

		/* Flush pending banner before INFO_REQUEST */
		res = flush_pending_banner(sess);
		if (res < 0) {
			free_kbi_prompts(kbi_name, kbi_inst, kbi_nprompts, kbi_prompts, kbi_echo);
			return res;
		}

		/* Send INFO_REQUEST */
		res = send_info_request(sess, kbi_name, kbi_inst, kbi_nprompts, kbi_prompts, kbi_echo);
		free_kbi_prompts(kbi_name, kbi_inst, kbi_nprompts, kbi_prompts, kbi_echo);
		if (res < 0)
			return res;

		/* Receive INFO_RESPONSE */
		uint8_t  resp_type;
		uint8_t *resp_payload;
		size_t   resp_len;

		res = recv_packet(sess, &resp_type, &resp_payload, &resp_len);
		if (res < 0)
			return res;

		while (resp_type == SSH_MSG_USERAUTH_BANNER) {
			res = handle_banner(sess, resp_payload, resp_len);
			if (res < 0)
				return res;

			/* Re-recv -- expect INFO_RESPONSE */
			res = recv_packet(sess, &resp_type, &resp_payload, &resp_len);
			if (res < 0)
				return res;
		}

		if (resp_type != SSH_MSG_USERAUTH_INFO_RESPONSE) {
			res = send_auth_failure(sess, cbs->methods_str, false);
			if (res < 0)
				return res;
			return AUTH_HANDLER_CONTINUE;
		}

		/* Parse INFO_RESPONSE */
		size_t rp = 1; /* skip msg_type */

		if (rp + 4 > resp_len)
			return DSSH_ERROR_PARSE;
		num_responses = DSSH_GET_U32(&resp_payload[rp]);
		rp += 4;

		if (num_responses > 0) {
			responses     = calloc(num_responses, sizeof(*responses));
			response_lens = calloc(num_responses, sizeof(*response_lens));
			if ((responses == NULL) || (response_lens == NULL)) {
				free(responses);
				free(response_lens);
				responses     = NULL;
				response_lens = NULL;
				return DSSH_ERROR_ALLOC;
			}
		}

		for (uint32_t i = 0; i < num_responses; i++) {
			uint32_t rlen;

			if (rp + 4 > resp_len) {
				free(responses);
				free(response_lens);
				return DSSH_ERROR_PARSE;
			}
			rlen = DSSH_GET_U32(&resp_payload[rp]);
			rp += 4;
			if (rp + rlen > resp_len) {
				free(responses);
				free(response_lens);
				return DSSH_ERROR_PARSE;
			}
			responses[i]     = &resp_payload[rp];
			response_lens[i] = rlen;
			rp += rlen;
		}
	}
}

/*
 * Handle "publickey" authentication method (probe + verify).
 */
static int
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
	if (rpos + 1 > payload_len)
		return DSSH_ERROR_PARSE;

	bool has_sig = (payload[rpos] != 0);

	rpos++;

	uint32_t algo_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	algo_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + algo_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *algo = &payload[rpos];

	rpos += algo_len;

	uint32_t pk_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	pk_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + pk_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *pk_blob = &payload[rpos];

	rpos += pk_len;

	/* NUL-terminate algo name for lookup */
	char   algo_name[65];
	size_t an_len = algo_len < sizeof(algo_name) - 1 ? algo_len : sizeof(algo_name) - 1;

	memcpy(algo_name, algo, an_len);
	algo_name[an_len] = 0;

	if (!has_sig) {
		/* Key probe -- ask app if key is acceptable */
		int auth_res = cbs->publickey_cb(user, user_len, algo_name, pk_blob, pk_len, false, cbs->cbdata);

		if (auth_res == DSSH_AUTH_DISCONNECT) {
			dssh_transport_disconnect(sess, SSH_DISCONNECT_BY_APPLICATION, "authentication rejected");
			return DSSH_ERROR_TERMINATED;
		}
		if (auth_res == DSSH_AUTH_SUCCESS)
			res = send_pk_ok(sess, algo_name, algo_len, pk_blob, pk_len);
		else
			res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}

	/* Has signature -- verify it */
	uint32_t sig_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	sig_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + sig_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *sig_blob = &payload[rpos];

	/* Find the key algorithm to verify the signature */
	dssh_key_algo ka = find_key_algo(algo_name);

	if ((ka == NULL) || (ka->verify == NULL)) {
		res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}

	/* Build the signed data (same as client builds):
        * session_id_string + payload[0..before_sig]
        * where before_sig is everything before the
        * signature string (i.e., before the uint32 sig_len).
        * rpos currently points at the sig data, so
        * rpos - 4 is the offset of the sig_len field. */
	size_t before_sig = rpos - 4;

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

	free(sign_data);

	if (vres < 0) {
		res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
		return AUTH_HANDLER_CONTINUE;
	}

	/* Signature valid -- ask app if user is authorized */
	int auth_res = cbs->publickey_cb(user, user_len, algo_name, pk_blob, pk_len, true, cbs->cbdata);

	if (auth_res == DSSH_AUTH_DISCONNECT) {
		dssh_transport_disconnect(sess, SSH_DISCONNECT_BY_APPLICATION, "authentication rejected");
		return DSSH_ERROR_TERMINATED;
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
DSSH_TESTABLE int
auth_server_impl(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, uint8_t *username_out,
    size_t *username_out_len)
{
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;
	int      res;

	/* Receive SERVICE_REQUEST */
	res = recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_SERVICE_REQUEST) {
		send_unimplemented(sess, sess->trans.last_rx_seq);
		return DSSH_ERROR_PARSE;
	}

	/* Send SERVICE_ACCEPT (echo back the service name) */
	{
		uint32_t slen      = 0;
		bool     have_name = false;

		if (payload_len > 5) {
			slen = DSSH_GET_U32(&payload[1]);
			if (5 + slen <= payload_len) {
#if SIZE_MAX < UINT32_MAX + 5ULL
				if (slen > SIZE_MAX - 5)
					return DSSH_ERROR_INVALID;
#endif
				have_name = true;
			}
		}

		size_t   max;
		int      err;
		uint8_t *accept = send_begin(sess, SSH_MSG_SERVICE_ACCEPT, &max, &err);

		if (accept == NULL)
			return err;

		size_t pos = 0;

		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (have_name) {
			DSSH_PUT_U32(slen, accept, &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		res = send_commit(sess, pos, NULL);
		if (res < 0)
			return res;
	}

	/* Saved username for output */
	uint8_t saved_user[256];
	size_t  saved_user_len = 0;

	/* Auth loop */
	for (;;) {
		/* Send pending banner before next auth exchange */
		res = flush_pending_banner(sess);
		if (res < 0)
			return res;

		res = recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type != SSH_MSG_USERAUTH_REQUEST) {
			send_unimplemented(sess, sess->trans.last_rx_seq);
			continue;
		}

		const uint8_t *user, *method;
		size_t         user_len, method_len;
		int64_t        rpos_raw =
		    parse_userauth_prefix(payload, payload_len, &user, &user_len, &method, &method_len);

		if (rpos_raw < 0)
			return (int)rpos_raw;
#if SIZE_MAX < INT64_MAX
		if (rpos_raw > SIZE_MAX)
			return DSSH_ERROR_INVALID;
#endif
		size_t rpos = (size_t)rpos_raw;

		/* Save username */
		saved_user_len = user_len < sizeof(saved_user) ? user_len : sizeof(saved_user) - 1;
		memcpy(saved_user, user, saved_user_len);

		/* Dispatch by method */
		int r;

		if ((method_len == DSSH_STRLEN(method_none))
		    && (memcmp(method, method_none, DSSH_STRLEN(method_none)) == 0)) {
			r = handle_auth_none(sess, cbs, user, user_len);
		}
		else if ((method_len == DSSH_STRLEN(method_password))
			 && (memcmp(method, method_password, DSSH_STRLEN(method_password)) == 0)) {
			r = handle_auth_password(sess, cbs, user, user_len, payload, payload_len, rpos);
		}
		else if ((method_len == DSSH_STRLEN(method_keyboard_interactive))
			 && (memcmp(method, method_keyboard_interactive, DSSH_STRLEN(method_keyboard_interactive))
			     == 0)) {
			r = handle_auth_kbi(sess, cbs, user, user_len, payload, payload_len, rpos);
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
	}

done:
	if ((username_out != NULL) && (username_out_len != NULL)) {
		size_t copy_len = saved_user_len < *username_out_len ? saved_user_len : *username_out_len;

		memcpy(username_out, saved_user, copy_len);
		*username_out_len = copy_len;
	}
	return 0;
}

/*
 * Check whether a negative result from a protocol function should
 * be promoted to DSSH_ERROR_TERMINATED.  If the session's terminate
 * flag was set (by send_packet/recv_packet on a fatal I/O or crypto
 * error), the connection is dead and callers need to know.
 *
 * Auth rejection (server sent USERAUTH_FAILURE) does NOT set
 * terminate, so those errors pass through unchanged.
 */
static inline int
auth_check_terminated(struct dssh_session_s *sess, int res)
{
	if ((res < 0) && sess->terminate)
		return DSSH_ERROR_TERMINATED;
	return res;
}

DSSH_PUBLIC int
dssh_auth_server(struct dssh_session_s *sess, const struct dssh_auth_server_cbs *cbs, uint8_t *username_out,
    size_t *username_out_len)
{
	if ((cbs == NULL) || (sess == NULL))
		return DSSH_ERROR_INVALID;
	return auth_check_terminated(sess, auth_server_impl(sess, cbs, username_out, username_out_len));
}

DSSH_PUBLIC int
dssh_auth_set_banner(struct dssh_session_s *sess, const char *message, const char *language)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;

	free(sess->pending_banner);
	free(sess->pending_banner_lang);
	sess->pending_banner      = NULL;
	sess->pending_banner_lang = NULL;

	if (message == NULL)
		return 0;
	if (message[0] == '\0')
		return DSSH_ERROR_INVALID;

	sess->pending_banner = strdup(message);
	if (sess->pending_banner == NULL)
		return DSSH_ERROR_ALLOC;
	if (language != NULL) {
		sess->pending_banner_lang = strdup(language);
		if (sess->pending_banner_lang == NULL) {
			free(sess->pending_banner);
			sess->pending_banner = NULL;
			return DSSH_ERROR_ALLOC;
		}
	}
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

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_SERVICE_REQUEST, &max, &err);

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

	ret = recv_packet(sess, &msg_type, &payload, &payload_len);
	if (ret < 0)
		return ret;
	if (msg_type != SSH_MSG_SERVICE_ACCEPT) {
		send_unimplemented(sess, sess->trans.last_rx_seq);
		return DSSH_ERROR_PARSE;
	}

	return 0;
}

static int
ensure_auth_service(struct dssh_session_s *sess)
{
	if (sess->auth_service_requested)
		return 0;

	int res = dssh_auth_request_service(sess, "ssh-userauth");

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
get_methods_impl(struct dssh_session_s *sess, const char *username, char *methods, size_t methods_sz)
{
	int res = ensure_auth_service(sess);

	if (res < 0)
		return res;

	size_t ulen = strlen(username);

	if ((ulen > UINT32_MAX) || (ulen > SIZE_MAX - AUTH_NONE_FIXED))
		return DSSH_ERROR_INVALID;

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_USERAUTH_REQUEST, &max, &err);

	if (msg == NULL)
		return err;

	size_t pos = build_userauth_request(msg, username, ulen, method_none, DSSH_STRLEN(method_none));

	res = send_commit(sess, pos, NULL);
	if (res < 0)
		return res;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	res = recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;

	while (msg_type == SSH_MSG_USERAUTH_BANNER) {
		res = handle_banner(sess, payload, payload_len);
		if (res < 0)
			return res;
		res = recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
	}

	if (msg_type == SSH_MSG_USERAUTH_SUCCESS) {
		if ((methods != NULL) && (methods_sz > 0))
			methods[0] = 0;
		return DSSH_AUTH_NONE_ACCEPTED;
	}

	if (msg_type == SSH_MSG_USERAUTH_FAILURE) {
		if (payload_len < 1 + 4)
			return DSSH_ERROR_PARSE;

		uint32_t mlen = DSSH_GET_U32(&payload[1]);

		if (1 + 4 + mlen > payload_len)
			return DSSH_ERROR_PARSE;

		/* RFC 4251 s6: names MUST NOT contain control chars or DEL */
		for (uint32_t j = 0; j < mlen; j++) {
			uint8_t ch = payload[5 + j];

			if ((ch <= ' ') || (ch >= DSSH_ASCII_DEL))
				return DSSH_ERROR_PARSE;
		}

		if ((methods != NULL) && (methods_sz > 0)) {
			size_t copylen = mlen < methods_sz - 1 ? mlen : methods_sz - 1;

			memcpy(methods, &payload[5], copylen);
			methods[copylen] = 0;
		}
		return DSSH_AUTH_METHODS_AVAILABLE;
	}

	return DSSH_ERROR_PARSE;
}

DSSH_PUBLIC int
dssh_auth_get_methods(struct dssh_session_s *sess, const char *username, char *methods, size_t methods_sz)
{
	if ((methods == NULL) || (username == NULL) || (sess == NULL))
		return DSSH_ERROR_INIT;
	return auth_check_terminated(sess, get_methods_impl(sess, username, methods, methods_sz));
}

/*
 * Build and send a password auth request.
 * If change is false: normal auth (boolean FALSE, password).
 * If change is true: password change (boolean TRUE, old_password, new_password).
 */
static int
send_password_request(struct dssh_session_s *sess, const char *username, const char *old_password,
    const uint8_t *new_password, size_t new_password_len, bool change)
{
	size_t ulen  = strlen(username);
	size_t oplen = strlen(old_password);

	if ((ulen > UINT32_MAX) || (oplen > UINT32_MAX) || (new_password_len > UINT32_MAX))
		return DSSH_ERROR_INVALID;

	if ((ulen > SIZE_MAX - AUTH_PASSWORD_FIXED) || (oplen > SIZE_MAX - AUTH_PASSWORD_FIXED - ulen))
		return DSSH_ERROR_INVALID;

	size_t msg_len = AUTH_PASSWORD_FIXED + ulen + oplen;

	if (change) {
		if (new_password_len > SIZE_MAX - 4 - msg_len)
			return DSSH_ERROR_INVALID;
		msg_len += 4 + new_password_len;
	}

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_USERAUTH_REQUEST, &max, &err);

	if (msg == NULL)
		return err;

	size_t pos = build_userauth_request(msg, username, ulen, method_password, DSSH_STRLEN(method_password));

	msg[pos++] = change ? 1 : 0;
	DSSH_PUT_U32((uint32_t)oplen, msg, &pos);
	memcpy(&msg[pos], old_password, oplen);
	pos += oplen;
	if (change) {
		DSSH_PUT_U32((uint32_t)new_password_len, msg, &pos);
		memcpy(&msg[pos], new_password, new_password_len);
		pos += new_password_len;
	}

	int ret = send_commit(sess, pos, NULL);

	dssh_cleanse(msg, pos);
	return ret;
}

DSSH_TESTABLE int
auth_password_impl(struct dssh_session_s *sess, const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *passwd_change_cbdata)
{
	int res = ensure_auth_service(sess);

	if (res < 0)
		return res;
	res = send_password_request(sess, username, password, NULL, 0, false);
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
			if ((res < 0) && (res != DSSH_ERROR_PARSE))
				return res;
			continue;
		}

		if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
			return 0;

		if (msg_type == SSH_MSG_USERAUTH_FAILURE)
			return DSSH_ERROR_AUTH_REJECTED;

		if (msg_type == SSH_MSG_USERAUTH_PASSWD_CHANGEREQ) {
			if (passwd_change_cb == NULL)
				return DSSH_ERROR_INIT;

			/* Parse: prompt string, language string */
			size_t   rpos = 1;
			uint32_t prompt_len;

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			prompt_len = DSSH_GET_U32(&payload[rpos]);
			rpos += 4;
			if (rpos + prompt_len > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *prompt = &payload[rpos];

			rpos += prompt_len;

			uint32_t       lang_len = 0;
			const uint8_t *language = NULL;

			if (rpos + 4 <= payload_len) {
				lang_len = DSSH_GET_U32(&payload[rpos]);
				rpos += 4;
				if (rpos + lang_len <= payload_len)
					language = &payload[rpos];
				else
					lang_len = 0;
			}

			uint8_t *new_password     = NULL;
			size_t   new_password_len = 0;

			res = passwd_change_cb(prompt, prompt_len, language, lang_len, &new_password,
			    &new_password_len, passwd_change_cbdata);
			if (res < 0) {
				if (new_password != NULL)
					dssh_cleanse(new_password, new_password_len);
				free(new_password);
				return res;
			}

			res =
			    send_password_request(sess, username, password, new_password, new_password_len, true);
			if (new_password != NULL)
				dssh_cleanse(new_password, new_password_len);
			free(new_password);
			if (res < 0)
				return res;
			continue;
		}

		send_unimplemented(sess, sess->trans.last_rx_seq);
		return DSSH_ERROR_PARSE;
	}
}

DSSH_PUBLIC int
dssh_auth_password(struct dssh_session_s *sess, const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *passwd_change_cbdata)
{
	if ((password == NULL) || (username == NULL) || (sess == NULL))
		return DSSH_ERROR_INVALID;
	return auth_check_terminated(sess,
	    auth_password_impl(sess, username, password, passwd_change_cb, passwd_change_cbdata));
}

DSSH_TESTABLE int
auth_kbi_impl(struct dssh_session_s *sess, const char *username, dssh_auth_kbi_prompt_cb prompt_cb, void *cbdata)
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

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_USERAUTH_REQUEST, &max, &err);

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
			if ((res < 0) && (res != DSSH_ERROR_PARSE))
				return res;
			continue;
		}

		if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
			return 0;

		if (msg_type == SSH_MSG_USERAUTH_FAILURE)
			return DSSH_ERROR_AUTH_REJECTED;

		if (msg_type == SSH_MSG_USERAUTH_INFO_REQUEST) {
			size_t   rpos = 1;
			uint32_t slen;

			/* Parse name string */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			slen = DSSH_GET_U32(&payload[rpos]);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *name     = &payload[rpos];
			size_t         name_len = slen;

			rpos += slen;

			/* Parse instruction string */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			slen = DSSH_GET_U32(&payload[rpos]);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *instruction     = &payload[rpos];
			size_t         instruction_len = slen;

			rpos += slen;

			/* Parse language string (ignored) */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			slen = DSSH_GET_U32(&payload[rpos]);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;
			rpos += slen;

			/* num_prompts */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;

			uint32_t num_prompts = DSSH_GET_U32(&payload[rpos]);

			rpos += 4;

			/* Parse each prompt + echo flag */
			const uint8_t **prompts       = NULL;
			size_t         *prompt_lens   = NULL;
			bool           *echo          = NULL;
			uint8_t       **responses     = NULL;
			size_t         *response_lens = NULL;

			if (num_prompts > 0) {
				prompts       = calloc(num_prompts, sizeof(*prompts));
				prompt_lens   = calloc(num_prompts, sizeof(*prompt_lens));
				echo          = calloc(num_prompts, sizeof(*echo));
				responses     = calloc(num_prompts, sizeof(*responses));
				response_lens = calloc(num_prompts, sizeof(*response_lens));
				if (!prompts || !prompt_lens || !echo || !responses || !response_lens) {
					res = DSSH_ERROR_ALLOC;
					goto kbi_cleanup;
				}

				for (uint32_t i = 0; i < num_prompts; i++) {
					if (rpos + 4 > payload_len) {
						res = DSSH_ERROR_PARSE;
						goto kbi_cleanup;
					}
					slen = DSSH_GET_U32(&payload[rpos]);
					rpos += 4;
					if (rpos + slen + 1 > payload_len) {
						res = DSSH_ERROR_PARSE;
						goto kbi_cleanup;
					}
					prompts[i]     = &payload[rpos];
					prompt_lens[i] = slen;
					rpos += slen;
					echo[i] = (payload[rpos] != 0);
					rpos += 1;
				}
			}

			/* Call the application's prompt callback */
			res = prompt_cb(name, name_len, instruction, instruction_len, num_prompts, prompts,
			    prompt_lens, echo, responses, response_lens, cbdata);

			free(prompts);
			free(prompt_lens);
			free(echo);
			prompts     = NULL;
			prompt_lens = NULL;
			echo        = NULL;

			if (res < 0)
				goto kbi_cleanup;

			/* Build INFO_RESPONSE from callback's responses */
			size_t resp_sz = 1 + 4;

			for (uint32_t i = 0; i < num_prompts; i++) {
				if (response_lens[i] > UINT32_MAX) {
					res = DSSH_ERROR_INVALID;
					goto kbi_cleanup;
				}
				if (response_lens[i] > SIZE_MAX - 4 - resp_sz) {
					res = DSSH_ERROR_INVALID;
					goto kbi_cleanup;
				}
				resp_sz += 4 + response_lens[i];
			}

			size_t   resp_max;
			int      resp_err;
			uint8_t *resp = send_begin(sess, SSH_MSG_USERAUTH_INFO_RESPONSE, &resp_max, &resp_err);

			if (resp == NULL) {
				res = resp_err;
				goto kbi_cleanup;
			}

			size_t rp = 0;

			resp[rp++] = SSH_MSG_USERAUTH_INFO_RESPONSE;
			DSSH_PUT_U32(num_prompts, resp, &rp);
			for (uint32_t i = 0; i < num_prompts; i++) {
				DSSH_PUT_U32((uint32_t)response_lens[i], resp, &rp);
				if (response_lens[i] > 0)
					memcpy(&resp[rp], responses[i], response_lens[i]);
				rp += response_lens[i];
				free(responses[i]);
			}
			free(responses);
			free(response_lens);

			res = send_commit(sess, rp, NULL);
			if (res < 0)
				return res;
			continue;

kbi_cleanup:
			free(prompts);
			free(prompt_lens);
			free(echo);
			if (responses != NULL) {
				for (uint32_t i = 0; i < num_prompts; i++)
					free(responses[i]);
			}
			free(responses);
			free(response_lens);
			return res;
		}

		send_unimplemented(sess, sess->trans.last_rx_seq);
		return DSSH_ERROR_PARSE;
	}
}

DSSH_PUBLIC int
dssh_auth_keyboard_interactive(struct dssh_session_s *sess, const char *username,
    dssh_auth_kbi_prompt_cb prompt_cb, void *cbdata)
{
	if ((prompt_cb == NULL) || (username == NULL) || (sess == NULL))
		return DSSH_ERROR_INVALID;
	return auth_check_terminated(sess, auth_kbi_impl(sess, username, prompt_cb, cbdata));
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
	if (!ka->haskey || !ka->haskey(ka->ctx))
		return DSSH_ERROR_INIT;

	/* Get the public key blob */
	const uint8_t *pubkey_buf = NULL;
	size_t         pubkey_len;
	int            res = ka->pubkey(&pubkey_buf, &pubkey_len, ka->ctx);

	if (res < 0)
		return res;

	size_t ulen = strlen(username);
	size_t alen = strlen(algo_name);

	if ((ulen > UINT32_MAX) || (alen > UINT32_MAX) || (pubkey_len > UINT32_MAX)
	    || (sess->trans.session_id_sz > UINT32_MAX))
		return DSSH_ERROR_INVALID;

	size_t rt = sess->trans.session_id_sz;

	if ((ulen > SIZE_MAX - AUTH_PUBLICKEY_FIXED - rt) || (alen > SIZE_MAX - AUTH_PUBLICKEY_FIXED - rt - ulen)
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
	uint8_t *sig_buf = NULL;
	size_t   sig_len;

	res = ka->sign(&sig_buf, &sig_len, sign_data, sp, ka->ctx);
	free(sign_data);
	if (res < 0) {
		free(sig_buf);
		return res;
	}
	if ((sig_len > UINT32_MAX) || (sig_len > SIZE_MAX - AUTH_PUBLICKEY_FIXED - ulen - alen - pubkey_len)) {
		free(sig_buf);
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
	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_USERAUTH_REQUEST, &max, &err);

	if (msg == NULL) {
		free(sig_buf);
		return err;
	}

	size_t pos = build_userauth_request(msg, username, ulen, method_publickey, DSSH_STRLEN(method_publickey));

	msg[pos++] = 1; /* TRUE */
	DSSH_PUT_U32((uint32_t)alen, msg, &pos);
	memcpy(&msg[pos], algo_name, alen);
	pos += alen;
	DSSH_PUT_U32((uint32_t)pubkey_len, msg, &pos);
	memcpy(&msg[pos], pubkey_buf, pubkey_len);
	pos += pubkey_len;
	DSSH_PUT_U32((uint32_t)sig_len, msg, &pos);
	memcpy(&msg[pos], sig_buf, sig_len);
	pos += sig_len;

	res = send_commit(sess, pos, NULL);
	free(sig_buf);
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
			if ((res < 0) && (res != DSSH_ERROR_PARSE))
				return res;
			continue;
		}
		break;
	}

	if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
		return 0;

	return DSSH_ERROR_AUTH_REJECTED;
}

DSSH_PUBLIC int
dssh_auth_publickey(struct dssh_session_s *sess, const char *username, const char *algo_name)
{
	if ((algo_name == NULL) || (username == NULL) || (sess == NULL))
		return DSSH_ERROR_INVALID;
	return auth_check_terminated(sess, auth_publickey_impl(sess, username, algo_name));
}
