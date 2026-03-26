// RFC 4252: SSH Authentication Protocol

#include <stdlib.h>
#include <string.h>

#include "deucessh-auth.h"
#include "ssh-internal.h"

static void
handle_banner(dssh_session sess, uint8_t *payload, size_t payload_len)
{
	dssh_auth_banner_cb cb = sess->banner_cb;

	if (cb == NULL)
		return;

	size_t   rpos = 1;
	uint32_t msg_len;

	if (rpos + 4 > payload_len)
		return;
	dssh_parse_uint32(&payload[rpos], payload_len - rpos, &msg_len);
	rpos += 4;
	if (rpos + msg_len > payload_len)
		return;

	const uint8_t *message = &payload[rpos];

	rpos += msg_len;

	uint32_t       lang_len = 0;
	const uint8_t *language = NULL;

	if (rpos + 4 <= payload_len) {
		dssh_parse_uint32(&payload[rpos], payload_len - rpos, &lang_len);
		rpos += 4;
		if (rpos + lang_len <= payload_len)
			language = &payload[rpos];
		else
			lang_len = 0;
	}

	cb(message, msg_len, language, lang_len, sess->banner_cbdata);
}

/* ================================================================
 * Server-side helpers
 * ================================================================ */
static int
send_auth_failure(dssh_session sess, const char *methods, bool partial_success)
{
	size_t  mlen = strlen(methods);

	if (mlen > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	uint8_t msg[256];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
	dssh_serialize_uint32((uint32_t)mlen, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], methods, mlen);
	pos += mlen;
	msg[pos++] = partial_success ? 1 : 0;
	return dssh_transport_send_packet(sess, msg, pos, NULL);
}

static int
send_auth_success(dssh_session sess)
{
	uint8_t msg = SSH_MSG_USERAUTH_SUCCESS;

	return dssh_transport_send_packet(sess, &msg, 1, NULL);
}

static int
send_passwd_changereq(dssh_session sess,
    const uint8_t *prompt, size_t prompt_len)
{
	if (prompt_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = 1 + 4 + prompt_len + 4;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	dssh_serialize_uint32((uint32_t)prompt_len, msg, msg_len, &pos);
	memcpy(&msg[pos], prompt, prompt_len);
	pos += prompt_len;
	dssh_serialize_uint32(0, msg, msg_len, &pos); /* language tag */

	int res = dssh_transport_send_packet(sess, msg, pos, NULL);

	free(msg);
	return res;
}

static int
send_pk_ok(dssh_session sess,
    const char *algo_name, size_t algo_len,
    const uint8_t *pubkey_blob, size_t pubkey_blob_len)
{
	if (algo_len > UINT32_MAX || pubkey_blob_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = 1 + 4 + algo_len + 4 + pubkey_blob_len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_PK_OK;
	dssh_serialize_uint32((uint32_t)algo_len, msg, msg_len, &pos);
	memcpy(&msg[pos], algo_name, algo_len);
	pos += algo_len;
	dssh_serialize_uint32((uint32_t)pubkey_blob_len, msg, msg_len, &pos);
	memcpy(&msg[pos], pubkey_blob, pubkey_blob_len);
	pos += pubkey_blob_len;

	int res = dssh_transport_send_packet(sess, msg, pos, NULL);

	free(msg);
	return res;
}

/*
 * Parse the common prefix of a USERAUTH_REQUEST:
 *   string user, string service, string method
 * Returns the offset past the method string, or negative on error.
 */
DSSH_TESTABLE int64_t
parse_userauth_prefix(const uint8_t *payload, size_t payload_len,
    const uint8_t **username, size_t *username_len,
    const uint8_t **method, size_t *method_len)
{
	size_t   rpos = 1; /* skip msg_type */
	uint32_t ulen;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	dssh_parse_uint32(&payload[rpos], payload_len - rpos, &ulen);
	rpos += 4;
	if (rpos + ulen > payload_len)
		return DSSH_ERROR_PARSE;
	*username = &payload[rpos];
	*username_len = ulen;
	rpos += ulen;

        /* service name (skip) */
	uint32_t slen;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
	rpos += 4;
	if (rpos + slen > payload_len)
		return DSSH_ERROR_PARSE;
	rpos += slen;

        /* method name */
	uint32_t mlen;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	dssh_parse_uint32(&payload[rpos], payload_len - rpos, &mlen);
	rpos += 4;
	if (rpos + mlen > payload_len)
		return DSSH_ERROR_PARSE;
	*method = &payload[rpos];
	*method_len = mlen;
	rpos += mlen;

	return (int64_t)rpos;
}

/* ================================================================
 * Server-side authentication loop
 * ================================================================ */
DSSH_TESTABLE int
auth_server_impl(dssh_session sess,
    const struct dssh_auth_server_cbs *cbs,
    uint8_t *username_out, size_t *username_out_len)
{
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;
	int      res;

        /* Receive SERVICE_REQUEST */
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_SERVICE_REQUEST) {
		dssh_transport_send_unimplemented(sess, sess->trans.last_rx_seq);
		return DSSH_ERROR_PARSE;
	}

        /* Send SERVICE_ACCEPT (echo back the service name) */
	{
		uint8_t accept[64];
		size_t  pos = 0;

		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;

			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			if (5 + slen <= payload_len) {
				dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
				memcpy(&accept[pos], &payload[5], slen);
				pos += slen;
			}
		}
		res = dssh_transport_send_packet(sess, accept, pos, NULL);
		if (res < 0)
			return res;
	}

        /* Saved username for output */
	uint8_t saved_user[256];
	size_t  saved_user_len = 0;

        /* Auth loop */
	for (;;) {
		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type != SSH_MSG_USERAUTH_REQUEST) {
			dssh_transport_send_unimplemented(sess, sess->trans.last_rx_seq);
			continue;
		}

		const uint8_t *user, *method;
		size_t         user_len, method_len;
		int64_t        rpos_raw = parse_userauth_prefix(payload,
		        payload_len, &user, &user_len, &method, &method_len);

		if (rpos_raw < 0)
			return (int)rpos_raw;
#if SIZE_MAX < INT64_MAX
		if (rpos_raw > SIZE_MAX)
			return DSSH_ERROR_INVALID;
#endif
		size_t         rpos = (size_t)rpos_raw;

                /* Save username */
		saved_user_len = user_len < sizeof(saved_user) ? user_len : sizeof(saved_user) - 1;
		memcpy(saved_user, user, saved_user_len);

                /* Dispatch by method */
		if ((method_len == 4) && (memcmp(method, "none", 4) == 0)) {
			int auth_res = DSSH_AUTH_FAILURE;

			if (cbs->none_cb != NULL)
				auth_res = cbs->none_cb(user, user_len, cbs->cbdata);
			if (auth_res == DSSH_AUTH_SUCCESS) {
				res = send_auth_success(sess);
				if (res < 0)
					return res;
				goto done;
			}
			res = send_auth_failure(sess, cbs->methods_str,
			        auth_res == DSSH_AUTH_PARTIAL);
			if (res < 0)
				return res;
			continue;
		}

		if ((method_len == 8) && (memcmp(method, "password", 8) == 0)) {
			if (cbs->password_cb == NULL) {
				res = send_auth_failure(sess, cbs->methods_str, false);
				if (res < 0)
					return res;
				continue;
			}

                        /* Parse: boolean change, string password [, string new_password] */
			if (rpos + 1 > payload_len)
				return DSSH_ERROR_PARSE;

			bool change = (payload[rpos] != 0);

			rpos++;

			uint32_t pw_len;

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &pw_len);
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
				dssh_parse_uint32(&payload[rpos], payload_len - rpos, &new_pw_len);
				rpos += 4;
				if (rpos + new_pw_len > payload_len)
					return DSSH_ERROR_PARSE;

				const uint8_t *new_pw = &payload[rpos];
				int            auth_res = DSSH_AUTH_FAILURE;
				uint8_t       *prompt = NULL;
				size_t         prompt_len = 0;

				if (cbs->passwd_change_cb != NULL) {
					auth_res = cbs->passwd_change_cb(user, user_len,
					        pw, pw_len, new_pw, new_pw_len,
					        &prompt, &prompt_len, cbs->cbdata);
				}

				if (auth_res == DSSH_AUTH_SUCCESS) {
					free(prompt);
					res = send_auth_success(sess);
					if (res < 0)
						return res;
					goto done;
				}
				if ((auth_res == DSSH_AUTH_CHANGE_PASSWORD) && (prompt != NULL)) {
					res = send_passwd_changereq(sess, prompt, prompt_len);
					free(prompt);
					if (res < 0)
						return res;
					continue;
				}
				free(prompt);
				res = send_auth_failure(sess, cbs->methods_str,
				        auth_res == DSSH_AUTH_PARTIAL);
				if (res < 0)
					return res;
				continue;
			}

                        /* Normal password auth */
			uint8_t *prompt = NULL;
			size_t   prompt_len = 0;
			int      auth_res = cbs->password_cb(user, user_len,
			        pw, pw_len, &prompt, &prompt_len, cbs->cbdata);

			if (auth_res == DSSH_AUTH_SUCCESS) {
				free(prompt);
				res = send_auth_success(sess);
				if (res < 0)
					return res;
				goto done;
			}
			if ((auth_res == DSSH_AUTH_CHANGE_PASSWORD) && (prompt != NULL)) {
				res = send_passwd_changereq(sess, prompt, prompt_len);
				free(prompt);
				if (res < 0)
					return res;
				continue;
			}
			free(prompt);
			res = send_auth_failure(sess, cbs->methods_str,
			        auth_res == DSSH_AUTH_PARTIAL);
			if (res < 0)
				return res;
			continue;
		}

		if ((method_len == 9) && (memcmp(method, "publickey", 9) == 0)) {
			if (cbs->publickey_cb == NULL) {
				res = send_auth_failure(sess, cbs->methods_str, false);
				if (res < 0)
					return res;
				continue;
			}

                        /* Parse: boolean has_sig, string algo, string pubkey [, string sig] */
			if (rpos + 1 > payload_len)
				return DSSH_ERROR_PARSE;

			bool has_sig = (payload[rpos] != 0);

			rpos++;

			uint32_t algo_len;

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &algo_len);
			rpos += 4;
			if (rpos + algo_len > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *algo = &payload[rpos];

			rpos += algo_len;

			uint32_t pk_len;

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &pk_len);
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
                                /* Key probe — ask app if key is acceptable */
				int auth_res = cbs->publickey_cb(user, user_len,
				        algo_name, pk_blob, pk_len, false, cbs->cbdata);

				if (auth_res == DSSH_AUTH_SUCCESS) {
					res = send_pk_ok(sess, algo_name, algo_len,
					        pk_blob, pk_len);
				}
				else {
					res = send_auth_failure(sess, cbs->methods_str, false);
				}
				if (res < 0)
					return res;
				continue;
			}

                        /* Has signature — verify it */
			uint32_t sig_len;

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &sig_len);
			rpos += 4;
			if (rpos + sig_len > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *sig_blob = &payload[rpos];

                        /* Find the key algorithm to verify the signature */
			dssh_key_algo  ka = dssh_transport_find_key_algo(algo_name);

			if ((ka == NULL) || (ka->verify == NULL)) {
				res = send_auth_failure(sess, cbs->methods_str, false);
				if (res < 0)
					return res;
				continue;
			}

                        /* Build the signed data (same as client builds):
                        * session_id_string + payload[0..before_sig]
                        * where before_sig is everything before the
                        * signature string (i.e., before the uint32 sig_len).
                        * rpos currently points at the sig data, so
                        * rpos - 4 is the offset of the sig_len field. */
			size_t   before_sig = rpos - 4;

			if (sess->trans.session_id_sz > UINT32_MAX)
				return DSSH_ERROR_INVALID;

			size_t   sd_len = 4 + sess->trans.session_id_sz + before_sig;
			uint8_t *sign_data = malloc(sd_len);

			if (sign_data == NULL)
				return DSSH_ERROR_ALLOC;

			size_t sp = 0;

			dssh_serialize_uint32((uint32_t)sess->trans.session_id_sz,
			    sign_data, sd_len, &sp);
			memcpy(&sign_data[sp], sess->trans.session_id,
			    sess->trans.session_id_sz);
			sp += sess->trans.session_id_sz;
			memcpy(&sign_data[sp], payload, before_sig);
			sp += before_sig;

			int vres = ka->verify(pk_blob, pk_len, sig_blob, sig_len,
			        sign_data, sp);

			free(sign_data);

			if (vres < 0) {
				res = send_auth_failure(sess, cbs->methods_str, false);
				if (res < 0)
					return res;
				continue;
			}

                        /* Signature valid — ask app if user is authorized */
			int auth_res = cbs->publickey_cb(user, user_len,
			        algo_name, pk_blob, pk_len, true, cbs->cbdata);

			if (auth_res == DSSH_AUTH_SUCCESS) {
				res = send_auth_success(sess);
				if (res < 0)
					return res;
				goto done;
			}
			res = send_auth_failure(sess, cbs->methods_str,
			        auth_res == DSSH_AUTH_PARTIAL);
			if (res < 0)
				return res;
			continue;
		}

                /* Unknown method */
		res = send_auth_failure(sess, cbs->methods_str, false);
		if (res < 0)
			return res;
	}

done:
	if ((username_out != NULL) && (username_out_len != NULL)) {
		memcpy(username_out, saved_user, saved_user_len);
		*username_out_len = saved_user_len;
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
auth_check_terminated(dssh_session sess, int res)
{
	if ((res < 0) && sess->terminate)
		return DSSH_ERROR_TERMINATED;
	return res;
}

DSSH_PUBLIC int
dssh_auth_server(dssh_session sess,
    const struct dssh_auth_server_cbs *cbs,
    uint8_t *username_out, size_t *username_out_len)
{
	return auth_check_terminated(sess,
	           auth_server_impl(sess, cbs, username_out, username_out_len));
}

/* ================================================================
 * Client-side authentication
 * ================================================================ */
static int
dssh_auth_request_service(dssh_session sess, const char *service)
{
	size_t   slen = strlen(service);

	if (slen > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = 1 + 4 + slen;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_SERVICE_REQUEST;
	dssh_serialize_uint32((uint32_t)slen, msg, msg_len, &pos);
	memcpy(&msg[pos], service, slen);
	pos += slen;

	int res = dssh_transport_send_packet(sess, msg, pos, NULL);

	free(msg);
	if (res < 0)
		return res;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_SERVICE_ACCEPT) {
		dssh_transport_send_unimplemented(sess, sess->trans.last_rx_seq);
		return DSSH_ERROR_INIT;
	}

	return 0;
}

static int
ensure_auth_service(dssh_session sess)
{
	if (sess->auth_service_requested)
		return 0;

	int res = dssh_auth_request_service(sess, "ssh-userauth");

	if (res == 0)
		sess->auth_service_requested = true;
	return res;
}

DSSH_TESTABLE int
get_methods_impl(dssh_session sess,
    const char *username, char *methods, size_t methods_sz)
{
	int res = ensure_auth_service(sess);

	if (res < 0)
		return res;

	size_t            ulen = strlen(username);

	if (ulen > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	static const char service[] = "ssh-connection";
	static const char method[] = "none";
	size_t            msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1) + 4 + (sizeof(method) - 1);
	uint8_t          *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32((uint32_t)ulen, msg, msg_len, &pos);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	dssh_serialize_uint32(DSSH_STRLEN(service), msg, msg_len, &pos);
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	dssh_serialize_uint32(DSSH_STRLEN(method), msg, msg_len, &pos);
	memcpy(&msg[pos], method, sizeof(method) - 1);
	pos += sizeof(method) - 1;

	res = dssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	if (res < 0)
		return res;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;

	if (msg_type == SSH_MSG_USERAUTH_SUCCESS) {
		if (methods_sz > 0)
			methods[0] = 0;
		return 0;
	}

	if (msg_type == SSH_MSG_USERAUTH_FAILURE) {
		if (payload_len < 1 + 4)
			return DSSH_ERROR_PARSE;

		uint32_t mlen;

		dssh_parse_uint32(&payload[1], payload_len - 1, &mlen);
		if (1 + 4 + mlen > payload_len)
			return DSSH_ERROR_PARSE;

                /* RFC 4251 s6: names MUST NOT contain control chars or DEL */
		for (uint32_t j = 0; j < mlen; j++) {
			uint8_t ch = payload[5 + j];

			if ((ch <= ' ') || (ch >= 127))
				return DSSH_ERROR_PARSE;
		}

		size_t copylen = mlen < methods_sz - 1 ? mlen : methods_sz - 1;

		memcpy(methods, &payload[5], copylen);
		methods[copylen] = 0;
		return 1;
	}

	return DSSH_ERROR_INIT;
}

DSSH_PUBLIC int
dssh_auth_get_methods(dssh_session sess,
    const char *username, char *methods, size_t methods_sz)
{
	return auth_check_terminated(sess,
	           get_methods_impl(sess, username, methods, methods_sz));
}

/*
 * Build and send a password auth request.
 * If change is false: normal auth (boolean FALSE, password).
 * If change is true: password change (boolean TRUE, old_password, new_password).
 */
static int
send_password_request(dssh_session sess,
    const char *username, const char *old_password,
    const uint8_t *new_password, size_t new_password_len, bool change)
{
	size_t            ulen = strlen(username);
	size_t            oplen = strlen(old_password);

	if (ulen > UINT32_MAX || oplen > UINT32_MAX || new_password_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	static const char service[] = "ssh-connection";
	static const char method[] = "password";
	size_t            msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1)
	    + 4 + (sizeof(method) - 1) + 1 + 4 + oplen;

	if (change)
		msg_len += 4 + new_password_len;

	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32((uint32_t)ulen, msg, msg_len, &pos);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	dssh_serialize_uint32(DSSH_STRLEN(service), msg, msg_len, &pos);
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	dssh_serialize_uint32(DSSH_STRLEN(method), msg, msg_len, &pos);
	memcpy(&msg[pos], method, sizeof(method) - 1);
	pos += sizeof(method) - 1;
	msg[pos++] = change ? 1 : 0;
	dssh_serialize_uint32((uint32_t)oplen, msg, msg_len, &pos);
	memcpy(&msg[pos], old_password, oplen);
	pos += oplen;
	if (change) {
		dssh_serialize_uint32((uint32_t)new_password_len, msg, msg_len, &pos);
		memcpy(&msg[pos], new_password, new_password_len);
		pos += new_password_len;
	}

	int res = dssh_transport_send_packet(sess, msg, pos, NULL);

	free(msg);
	return res;
}

DSSH_TESTABLE int
auth_password_impl(dssh_session sess,
    const char *username, const char *password,
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

		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;

		if (msg_type == SSH_MSG_USERAUTH_BANNER) {
			handle_banner(sess, payload, payload_len);
			continue;
		}

		if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
			return 0;

		if (msg_type == SSH_MSG_USERAUTH_FAILURE)
			return DSSH_ERROR_INIT;

		if (msg_type == SSH_MSG_USERAUTH_PASSWD_CHANGEREQ) {
			if (passwd_change_cb == NULL)
				return DSSH_ERROR_INIT;

                        /* Parse: prompt string, language string */
			size_t   rpos = 1;
			uint32_t prompt_len;

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &prompt_len);
			rpos += 4;
			if (rpos + prompt_len > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *prompt = &payload[rpos];

			rpos += prompt_len;

			uint32_t       lang_len = 0;
			const uint8_t *language = NULL;

			if (rpos + 4 <= payload_len) {
				dssh_parse_uint32(&payload[rpos], payload_len - rpos, &lang_len);
				rpos += 4;
				if (rpos + lang_len <= payload_len)
					language = &payload[rpos];
				else
					lang_len = 0;
			}

			uint8_t *new_password = NULL;
			size_t   new_password_len = 0;

			res = passwd_change_cb(prompt, prompt_len,
			        language, lang_len,
			        &new_password, &new_password_len,
			        passwd_change_cbdata);
			if (res < 0) {
				free(new_password);
				return res;
			}

			res = send_password_request(sess, username, password,
			        new_password, new_password_len, true);
			free(new_password);
			if (res < 0)
				return res;
			continue;
		}

		dssh_transport_send_unimplemented(sess, sess->trans.last_rx_seq);
		return DSSH_ERROR_INIT;
	}
}

DSSH_PUBLIC int
dssh_auth_password(dssh_session sess,
    const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *passwd_change_cbdata)
{
	return auth_check_terminated(sess,
	           auth_password_impl(sess, username, password,
	           passwd_change_cb, passwd_change_cbdata));
}

DSSH_TESTABLE int
auth_kbi_impl(dssh_session sess,
    const char *username, dssh_auth_kbi_prompt_cb prompt_cb,
    void *cbdata)
{
	if (prompt_cb == NULL)
		return DSSH_ERROR_INIT;
	{
		int svc = ensure_auth_service(sess);

		if (svc < 0)
			return svc;
	}

	size_t            ulen = strlen(username);

	if (ulen > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	static const char service[] = "ssh-connection";
	static const char method[] = "keyboard-interactive";
	size_t            msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1)
	    + 4 + (sizeof(method) - 1) + 4 + 4;
	uint8_t          *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32((uint32_t)ulen, msg, msg_len, &pos);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	dssh_serialize_uint32(DSSH_STRLEN(service), msg, msg_len, &pos);
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	dssh_serialize_uint32(DSSH_STRLEN(method), msg, msg_len, &pos);
	memcpy(&msg[pos], method, sizeof(method) - 1);
	pos += sizeof(method) - 1;
	dssh_serialize_uint32(0, msg, msg_len, &pos); /* language */
	dssh_serialize_uint32(0, msg, msg_len, &pos); /* submethods */

	int res = dssh_transport_send_packet(sess, msg, pos, NULL);

	free(msg);
	if (res < 0)
		return res;

	for (;;) {
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;

		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;

		if (msg_type == SSH_MSG_USERAUTH_BANNER) {
			handle_banner(sess, payload, payload_len);
			continue;
		}

		if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
			return 0;

		if (msg_type == SSH_MSG_USERAUTH_FAILURE)
			return DSSH_ERROR_INIT;

		if (msg_type == SSH_MSG_USERAUTH_INFO_REQUEST) {
			size_t   rpos = 1;
			uint32_t slen;

                        /* Parse name string */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *name = &payload[rpos];
			size_t         name_len = slen;

			rpos += slen;

                        /* Parse instruction string */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *instruction = &payload[rpos];
			size_t         instruction_len = slen;

			rpos += slen;

                        /* Parse language string (ignored) */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;
			rpos += slen;

                        /* num_prompts */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;

			uint32_t num_prompts;

			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &num_prompts);
			rpos += 4;

                        /* Parse each prompt + echo flag */
			const uint8_t **prompts = NULL;
			size_t         *prompt_lens = NULL;
			bool           *echo = NULL;
			uint8_t       **responses = NULL;
			size_t         *response_lens = NULL;

			if (num_prompts > 0) {
				prompts = calloc(num_prompts, sizeof(*prompts));
				if (!prompts)
					return DSSH_ERROR_ALLOC;
				prompt_lens = calloc(num_prompts, sizeof(*prompt_lens));
				if (!prompt_lens) {
					free(prompts);
					return DSSH_ERROR_ALLOC;
				}
				echo = calloc(num_prompts, sizeof(*echo));
				if (!echo) {
					free(prompts);
					free(prompt_lens);
					return DSSH_ERROR_ALLOC;
				}
				responses = calloc(num_prompts, sizeof(*responses));
				if (!responses) {
					free(prompts);
					free(prompt_lens);
					free(echo);
					return DSSH_ERROR_ALLOC;
				}
				response_lens = calloc(num_prompts, sizeof(*response_lens));
				if (!response_lens) {
					free(prompts);
					free(prompt_lens);
					free(echo);
					free(responses);
					return DSSH_ERROR_ALLOC;
				}

				for (uint32_t i = 0; i < num_prompts; i++) {
					if (rpos + 4 > payload_len) {
						free(prompts);
						free(prompt_lens);
						free(echo);
						free(responses);
						free(response_lens);
						return DSSH_ERROR_PARSE;
					}
					dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
					rpos += 4;
					if (rpos + slen + 1 > payload_len) {
						free(prompts);
						free(prompt_lens);
						free(echo);
						free(responses);
						free(response_lens);
						return DSSH_ERROR_PARSE;
					}
					prompts[i] = &payload[rpos];
					prompt_lens[i] = slen;
					rpos += slen;
					echo[i] = (payload[rpos] != 0);
					rpos += 1;
				}
			}

                        /* Call the application's prompt callback */
			res = prompt_cb(name, name_len, instruction, instruction_len,
			        num_prompts, prompts, prompt_lens, echo,
			        responses, response_lens, cbdata);

			free(prompts);
			free(prompt_lens);
			free(echo);

			if (res < 0) {
				for (uint32_t i = 0; i < num_prompts; i++)
					free(responses[i]);
				free(responses);
				free(response_lens);
				return res;
			}

                        /* Build INFO_RESPONSE from callback's responses */
			size_t resp_sz = 1 + 4;

			for (uint32_t i = 0; i < num_prompts; i++) {
				if (response_lens[i] > UINT32_MAX) {
					for (uint32_t j = 0; j < num_prompts; j++)
						free(responses[j]);
					free(responses);
					free(response_lens);
					return DSSH_ERROR_INVALID;
				}
				resp_sz += 4 + response_lens[i];
			}

			uint8_t *resp = malloc(resp_sz);

			if (resp == NULL) {
				for (uint32_t i = 0; i < num_prompts; i++)
					free(responses[i]);
				free(responses);
				free(response_lens);
				return DSSH_ERROR_ALLOC;
			}

			size_t rp = 0;

			resp[rp++] = SSH_MSG_USERAUTH_INFO_RESPONSE;
			dssh_serialize_uint32(num_prompts, resp, resp_sz, &rp);
			for (uint32_t i = 0; i < num_prompts; i++) {
				dssh_serialize_uint32((uint32_t)response_lens[i], resp, resp_sz, &rp);
				if (response_lens[i] > 0)
					memcpy(&resp[rp], responses[i], response_lens[i]);
				rp += response_lens[i];
				free(responses[i]);
			}
			free(responses);
			free(response_lens);

			res = dssh_transport_send_packet(sess, resp, rp, NULL);
			free(resp);
			if (res < 0)
				return res;
			continue;
		}

		dssh_transport_send_unimplemented(sess, sess->trans.last_rx_seq);
		return DSSH_ERROR_PARSE;
	}
}

DSSH_PUBLIC int
dssh_auth_keyboard_interactive(dssh_session sess,
    const char *username, dssh_auth_kbi_prompt_cb prompt_cb,
    void *cbdata)
{
	return auth_check_terminated(sess,
	           auth_kbi_impl(sess, username, prompt_cb, cbdata));
}

DSSH_TESTABLE int
auth_publickey_impl(dssh_session sess,
    const char *username, const char *algo_name)
{
	{
		int svc = ensure_auth_service(sess);

		if (svc < 0)
			return svc;
	}
	dssh_key_algo ka = dssh_transport_find_key_algo(algo_name);

	if ((ka == NULL) || (ka->sign == NULL) || (ka->pubkey == NULL))
		return DSSH_ERROR_INIT;
	if (!ka->haskey || !ka->haskey(ka->ctx))
		return DSSH_ERROR_INIT;

        /* Get the public key blob */
	uint8_t pubkey_buf[1024];
	size_t  pubkey_len;
	int     res = ka->pubkey(pubkey_buf, sizeof(pubkey_buf), &pubkey_len, ka->ctx);

	if (res < 0)
		return res;

	size_t            ulen = strlen(username);
	size_t            alen = strlen(algo_name);

	if (ulen > UINT32_MAX || alen > UINT32_MAX
	    || pubkey_len > UINT32_MAX || sess->trans.session_id_sz > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	static const char service[] = "ssh-connection";
	static const char method[] = "publickey";

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
	size_t   sign_data_len = 4 + sess->trans.session_id_sz
	    + 1 + 4 + ulen + 4 + (sizeof(service) - 1)
	    + 4 + (sizeof(method) - 1) + 1 + 4 + alen + 4 + pubkey_len;
	uint8_t *sign_data = malloc(sign_data_len);

	if (sign_data == NULL)
		return DSSH_ERROR_ALLOC;

	size_t sp = 0;

	dssh_serialize_uint32((uint32_t)sess->trans.session_id_sz,
	    sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], sess->trans.session_id, sess->trans.session_id_sz);
	sp += sess->trans.session_id_sz;
	sign_data[sp++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32((uint32_t)ulen, sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], username, ulen);
	sp += ulen;
	dssh_serialize_uint32(DSSH_STRLEN(service),
	    sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], service, sizeof(service) - 1);
	sp += sizeof(service) - 1;
	dssh_serialize_uint32(DSSH_STRLEN(method),
	    sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], method, sizeof(method) - 1);
	sp += sizeof(method) - 1;
	sign_data[sp++] = 1; /* TRUE */
	dssh_serialize_uint32((uint32_t)alen, sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], algo_name, alen);
	sp += alen;
	dssh_serialize_uint32((uint32_t)pubkey_len, sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], pubkey_buf, pubkey_len);
	sp += pubkey_len;

        /* Sign the data */
	uint8_t sig_buf[1024];
	size_t  sig_len;

	res = ka->sign(sig_buf, sizeof(sig_buf), &sig_len, sign_data, sp, ka->ctx);
	free(sign_data);
	if (res < 0)
		return res;
	if (sig_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;

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
	size_t   msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1)
	    + 4 + (sizeof(method) - 1) + 1 + 4 + alen
	    + 4 + pubkey_len + 4 + sig_len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32((uint32_t)ulen, msg, msg_len, &pos);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	dssh_serialize_uint32(DSSH_STRLEN(service), msg, msg_len, &pos);
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	dssh_serialize_uint32(DSSH_STRLEN(method), msg, msg_len, &pos);
	memcpy(&msg[pos], method, sizeof(method) - 1);
	pos += sizeof(method) - 1;
	msg[pos++] = 1; /* TRUE */
	dssh_serialize_uint32((uint32_t)alen, msg, msg_len, &pos);
	memcpy(&msg[pos], algo_name, alen);
	pos += alen;
	dssh_serialize_uint32((uint32_t)pubkey_len, msg, msg_len, &pos);
	memcpy(&msg[pos], pubkey_buf, pubkey_len);
	pos += pubkey_len;
	dssh_serialize_uint32((uint32_t)sig_len, msg, msg_len, &pos);
	memcpy(&msg[pos], sig_buf, sig_len);
	pos += sig_len;

	res = dssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	if (res < 0)
		return res;

        /* Wait for response */
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	for (;;) {
		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_USERAUTH_BANNER) {
			handle_banner(sess, payload, payload_len);
			continue;
		}
		break;
	}

	if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
		return 0;

	return DSSH_ERROR_INIT;
}

DSSH_PUBLIC int
dssh_auth_publickey(dssh_session sess,
    const char *username, const char *algo_name)
{
	return auth_check_terminated(sess,
	           auth_publickey_impl(sess, username, algo_name));
}
