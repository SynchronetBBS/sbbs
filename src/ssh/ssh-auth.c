// RFC 4252: SSH Authentication Protocol

#include <openssl/crypto.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh-auth.h"
#include "ssh-internal.h"

static const char method_none[]                  = "none";
static const char method_password[]              = "password";
static const char method_publickey[]             = "publickey";
static const char method_keyboard_interactive[]  = "keyboard-interactive";

static int
handle_banner(dssh_session sess, uint8_t *payload, size_t payload_len)
{
	dssh_auth_banner_cb cb = sess->banner_cb;

	if (cb == NULL)
		return 0;

	size_t   rpos = 1;
	int64_t  pv;
	uint32_t msg_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &msg_len);
	if (pv < 0)
		return (int)pv;
	rpos += 4;
	if (rpos + msg_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *message = &payload[rpos];

	rpos += msg_len;

	uint32_t       lang_len = 0;
	const uint8_t *language = NULL;

	if (rpos + 4 <= payload_len) {
		pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &lang_len);
		if (pv < 0)
			lang_len = 0;
		else
			rpos += 4;
		if (rpos + lang_len <= payload_len)
			language = &payload[rpos];
		else
			lang_len = 0;
	}

	cb(message, msg_len, language, lang_len, sess->banner_cbdata);
	return 0;
}

static int flush_pending_banner(dssh_session sess);

/* ================================================================
 * Server-side helpers
 * ================================================================ */
static int
send_auth_failure(dssh_session sess, const char *methods, bool partial_success)
{
	int ret = flush_pending_banner(sess);

	if (ret < 0)
		return ret;

	size_t mlen = strlen(methods);

	if (mlen > UINT32_MAX || mlen > SIZE_MAX - 6)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = 1 + 4 + mlen + 1;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
	ret = dssh_serialize_uint32((uint32_t)mlen, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	memcpy(&msg[pos], methods, mlen);
	pos += mlen;
	msg[pos++] = partial_success ? 1 : 0;
	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

done:
	free(msg);
	return ret;
}

static int
send_auth_success(dssh_session sess)
{
	int ret = flush_pending_banner(sess);

	if (ret < 0)
		return ret;

	uint8_t msg = SSH_MSG_USERAUTH_SUCCESS;

	return dssh_transport_send_packet(sess, &msg, 1, NULL);
}

static int
send_passwd_changereq(dssh_session sess,
    const uint8_t *prompt, size_t prompt_len)
{
	if (prompt_len > UINT32_MAX || prompt_len > SIZE_MAX - 9)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = 9 + prompt_len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	ret = dssh_serialize_uint32((uint32_t)prompt_len, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	memcpy(&msg[pos], prompt, prompt_len);
	pos += prompt_len;
	ret = dssh_serialize_uint32(0, msg, msg_len, &pos);
	if (ret < 0)
		goto done;

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

done:
	free(msg);
	return ret;
}

static int
send_pk_ok(dssh_session sess,
    const char *algo_name, size_t algo_len,
    const uint8_t *pubkey_blob, size_t pubkey_blob_len)
{
	if (algo_len > UINT32_MAX || pubkey_blob_len > UINT32_MAX
	    || algo_len > SIZE_MAX - 9 || pubkey_blob_len > SIZE_MAX - 9 - algo_len)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = 9 + algo_len + pubkey_blob_len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_USERAUTH_PK_OK;
	ret = dssh_serialize_uint32((uint32_t)algo_len, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	memcpy(&msg[pos], algo_name, algo_len);
	pos += algo_len;
	ret = dssh_serialize_uint32((uint32_t)pubkey_blob_len, msg, msg_len, &pos);
	if (ret < 0)
		goto done;
	memcpy(&msg[pos], pubkey_blob, pubkey_blob_len);
	pos += pubkey_blob_len;

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

done:
	free(msg);
	return ret;
}

/*
 * Free KBI prompt arrays allocated by the callback.
 */
static void
free_kbi_prompts(char *name, char *instruction,
    uint32_t num_prompts, char **prompts, bool *echo)
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
send_info_request(dssh_session sess, const char *name,
    const char *instruction, uint32_t num_prompts,
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

	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;

#define KBI_SER(v) do { ret = dssh_serialize_uint32((v), msg, msg_len, &pos); \
	if (ret < 0) { free(msg); return ret; } } while (0)

	KBI_SER((uint32_t)name_len);
	if (name_len > 0) {
		memcpy(&msg[pos], name, name_len);
		pos += name_len;
	}
	KBI_SER((uint32_t)inst_len);
	if (inst_len > 0) {
		memcpy(&msg[pos], instruction, inst_len);
		pos += inst_len;
	}
	KBI_SER(0); /* language tag — always empty */
	KBI_SER(num_prompts);

	for (uint32_t i = 0; i < num_prompts; i++) {
		size_t plen = prompts[i] != NULL ? strlen(prompts[i]) : 0;
		KBI_SER((uint32_t)plen);
		if (plen > 0) {
			memcpy(&msg[pos], prompts[i], plen);
			pos += plen;
		}
		msg[pos++] = echo[i] ? 1 : 0;
	}

#undef KBI_SER

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	return ret;
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
	int64_t  pv;
	uint32_t ulen;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &ulen);
	if (pv < 0)
		return pv;
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
	pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
	if (pv < 0)
		return pv;
	rpos += 4;
	if (rpos + slen > payload_len)
		return DSSH_ERROR_PARSE;
	rpos += slen;

        /* method name */
	uint32_t mlen;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &mlen);
	if (pv < 0)
		return pv;
	rpos += 4;
	if (rpos + mlen > payload_len)
		return DSSH_ERROR_PARSE;
	*method = &payload[rpos];
	*method_len = mlen;
	rpos += mlen;

	return (int64_t)rpos;
}

/*
 * Send and consume the pending banner, if any.
 */
static int
flush_pending_banner(dssh_session sess)
{
	if (sess->pending_banner == NULL)
		return 0;

	uint32_t msg_len = (uint32_t)strlen(sess->pending_banner);
	const char *lang = sess->pending_banner_lang != NULL
	    ? sess->pending_banner_lang : "";
	uint32_t lang_len = (uint32_t)strlen(lang);

	size_t   pkt_sz = 1 + 4 + msg_len + 4 + lang_len;
	uint8_t *pkt = malloc(pkt_sz);

	if (pkt == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    res;

	pkt[pos++] = SSH_MSG_USERAUTH_BANNER;
	res = dssh_serialize_uint32(msg_len, pkt, pkt_sz, &pos);
	if (res < 0) {
		free(pkt);
		return res;
	}
	memcpy(&pkt[pos], sess->pending_banner, msg_len);
	pos += msg_len;
	res = dssh_serialize_uint32(lang_len, pkt, pkt_sz, &pos);
	if (res < 0) {
		free(pkt);
		return res;
	}
	memcpy(&pkt[pos], lang, lang_len);
	pos += lang_len;

	res = dssh_transport_send_packet(sess, pkt, pos, NULL);
	free(pkt);

	free(sess->pending_banner);
	free(sess->pending_banner_lang);
	sess->pending_banner = NULL;
	sess->pending_banner_lang = NULL;

	return res;
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
	int64_t  pv;

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
		size_t   accept_len = 1;
		uint32_t slen = 0;
		bool     have_name = false;

		if (payload_len > 5) {
			pv = dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			if (pv >= 4 && 5 + slen <= payload_len) {
				#if SIZE_MAX < UINT32_MAX + 5ULL
				if (slen > SIZE_MAX - 5)
					return DSSH_ERROR_INVALID;
			#endif
				accept_len = 1 + 4 + slen;
				have_name = true;
			}
		}

		uint8_t *accept = malloc(accept_len);

		if (accept == NULL)
			return DSSH_ERROR_ALLOC;

		size_t pos = 0;

		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (have_name) {
			res = dssh_serialize_uint32(slen, accept, accept_len, &pos);
			if (res < 0) {
				free(accept);
				return res;
			}
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		res = dssh_transport_send_packet(sess, accept, pos, NULL);
		free(accept);
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
		if ((method_len == DSSH_STRLEN(method_none)) && (memcmp(method, method_none, DSSH_STRLEN(method_none)) == 0)) {
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

		if ((method_len == DSSH_STRLEN(method_password)) && (memcmp(method, method_password, DSSH_STRLEN(method_password)) == 0)) {
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
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &pw_len);
			if (pv < 0)
				return (int)pv;
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
				pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &new_pw_len);
				if (pv < 0)
					return (int)pv;
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

		if ((method_len == DSSH_STRLEN(method_keyboard_interactive)) && (memcmp(method, method_keyboard_interactive, DSSH_STRLEN(method_keyboard_interactive)) == 0)) {
			if (cbs->keyboard_interactive_cb == NULL) {
				res = send_auth_failure(sess, cbs->methods_str, false);
				if (res < 0)
					return res;
				continue;
			}

			/* Parse language + submethods (both ignored) */
			uint32_t skip_len;

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &skip_len);
			if (pv < 0)
				return (int)pv;
			rpos += 4;
			if (rpos + skip_len > payload_len)
				return DSSH_ERROR_PARSE;
			rpos += skip_len; /* language */

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &skip_len);
			if (pv < 0)
				return (int)pv;
			rpos += 4;
			if (rpos + skip_len > payload_len)
				return DSSH_ERROR_PARSE;
			rpos += skip_len; /* submethods */

			/* KBI prompt/response loop */
			uint32_t        num_responses = 0;
			const uint8_t **responses = NULL;
			size_t         *response_lens = NULL;

			for (;;) {
				char    *kbi_name = NULL;
				char    *kbi_inst = NULL;
				uint32_t kbi_nprompts = 0;
				char   **kbi_prompts = NULL;
				bool    *kbi_echo = NULL;

				int auth_res = cbs->keyboard_interactive_cb(
				    user, user_len,
				    num_responses, responses, response_lens,
				    &kbi_name, &kbi_inst,
				    &kbi_nprompts, &kbi_prompts, &kbi_echo,
				    cbs->cbdata);

				free(responses);
				free(response_lens);
				responses = NULL;
				response_lens = NULL;
				num_responses = 0;

				if (auth_res == DSSH_AUTH_SUCCESS) {
					free_kbi_prompts(kbi_name, kbi_inst,
					    kbi_nprompts, kbi_prompts, kbi_echo);
					res = send_auth_success(sess);
					if (res < 0)
						return res;
					goto done;
				}
				if (auth_res != DSSH_AUTH_KBI_PROMPT) {
					free_kbi_prompts(kbi_name, kbi_inst,
					    kbi_nprompts, kbi_prompts, kbi_echo);
					res = send_auth_failure(sess,
					    cbs->methods_str,
					    auth_res == DSSH_AUTH_PARTIAL);
					if (res < 0)
						return res;
					break;
				}

				/* Flush pending banner before INFO_REQUEST */
				res = flush_pending_banner(sess);
				if (res < 0) {
					free_kbi_prompts(kbi_name, kbi_inst,
					    kbi_nprompts, kbi_prompts, kbi_echo);
					return res;
				}

				/* Send INFO_REQUEST */
				res = send_info_request(sess, kbi_name,
				    kbi_inst, kbi_nprompts,
				    kbi_prompts, kbi_echo);
				free_kbi_prompts(kbi_name, kbi_inst,
				    kbi_nprompts, kbi_prompts, kbi_echo);
				if (res < 0)
					return res;

				/* Receive INFO_RESPONSE */
				uint8_t  resp_type;
				uint8_t *resp_payload;
				size_t   resp_len;

				res = dssh_transport_recv_packet(sess,
				    &resp_type, &resp_payload, &resp_len);
				if (res < 0)
					return res;

				if (resp_type == SSH_MSG_USERAUTH_BANNER) {
					res = handle_banner(sess, resp_payload, resp_len);
					if (res < 0)
						return res;
					/* Re-recv — expect INFO_RESPONSE */
					res = dssh_transport_recv_packet(sess,
					    &resp_type, &resp_payload, &resp_len);
					if (res < 0)
						return res;
				}

				if (resp_type != SSH_MSG_USERAUTH_INFO_RESPONSE) {
					res = send_auth_failure(sess,
					    cbs->methods_str, false);
					if (res < 0)
						return res;
					break;
				}

				/* Parse INFO_RESPONSE */
				size_t rp = 1; /* skip msg_type */

				if (rp + 4 > resp_len)
					return DSSH_ERROR_PARSE;
				pv = dssh_parse_uint32(&resp_payload[rp],
				    resp_len - rp, &num_responses);
				if (pv < 0)
					return (int)pv;
				rp += 4;

				if (num_responses > 0) {
					responses = calloc(num_responses,
					    sizeof(*responses));
					response_lens = calloc(num_responses,
					    sizeof(*response_lens));
					if (responses == NULL
					    || response_lens == NULL) {
						free(responses);
						free(response_lens);
						responses = NULL;
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
					pv = dssh_parse_uint32(
					    &resp_payload[rp],
					    resp_len - rp, &rlen);
					if (pv < 0) {
						free(responses);
						free(response_lens);
						return (int)pv;
					}
					rp += 4;
					if (rp + rlen > resp_len) {
						free(responses);
						free(response_lens);
						return DSSH_ERROR_PARSE;
					}
					responses[i] = &resp_payload[rp];
					response_lens[i] = rlen;
					rp += rlen;
				}
			}
			continue;
		}

		if ((method_len == DSSH_STRLEN(method_publickey)) && (memcmp(method, method_publickey, DSSH_STRLEN(method_publickey)) == 0)) {
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
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &algo_len);
			if (pv < 0)
				return (int)pv;
			rpos += 4;
			if (rpos + algo_len > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *algo = &payload[rpos];

			rpos += algo_len;

			uint32_t pk_len;

			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &pk_len);
			if (pv < 0)
				return (int)pv;
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
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &sig_len);
			if (pv < 0)
				return (int)pv;
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
			if (sess->trans.session_id_sz > SIZE_MAX - 4
			    || before_sig > SIZE_MAX - 4 - sess->trans.session_id_sz)
				return DSSH_ERROR_INVALID;

			size_t   sd_len = 4 + sess->trans.session_id_sz + before_sig;
			uint8_t *sign_data = malloc(sd_len);

			if (sign_data == NULL)
				return DSSH_ERROR_ALLOC;

			size_t sp = 0;

			res = dssh_serialize_uint32((uint32_t)sess->trans.session_id_sz,
			    sign_data, sd_len, &sp);
			if (res < 0) {
				free(sign_data);
				return res;
			}
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
	if (sess == NULL || cbs == NULL)
		return DSSH_ERROR_INIT;
	return auth_check_terminated(sess,
	           auth_server_impl(sess, cbs, username_out, username_out_len));
}

DSSH_PUBLIC int
dssh_auth_set_banner(dssh_session sess, const char *message,
    const char *language)
{
	if (sess == NULL)
		return DSSH_ERROR_INIT;

	free(sess->pending_banner);
	free(sess->pending_banner_lang);
	sess->pending_banner = NULL;
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
dssh_auth_request_service(dssh_session sess, const char *service)
{
	size_t   slen = strlen(service);

	if (slen > UINT32_MAX || slen > SIZE_MAX - 5)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = 5 + slen;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_SERVICE_REQUEST;
	ret = dssh_serialize_uint32((uint32_t)slen, msg, msg_len, &pos);
	if (ret < 0) {
		free(msg);
		return ret;
	}
	memcpy(&msg[pos], service, slen);
	pos += slen;

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

	free(msg);
	if (ret < 0)
		return ret;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	ret = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (ret < 0)
		return ret;
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
	int     res = ensure_auth_service(sess);
	int64_t pv;

	if (res < 0)
		return res;

	size_t            ulen = strlen(username);

	if (ulen > UINT32_MAX || ulen > SIZE_MAX - 31)
		return DSSH_ERROR_INVALID;

	static const char service[] = "ssh-connection";
	size_t            msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1) + 4 + DSSH_STRLEN(method_none);
	uint8_t          *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	res = dssh_serialize_uint32((uint32_t)ulen, msg, msg_len, &pos);
	if (res < 0)
		goto methods_done;
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	res = dssh_serialize_uint32(DSSH_STRLEN(service), msg, msg_len, &pos);
	if (res < 0)
		goto methods_done;
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	res = dssh_serialize_uint32(DSSH_STRLEN(method_none), msg, msg_len, &pos);
	if (res < 0)
		goto methods_done;
	memcpy(&msg[pos], method_none, DSSH_STRLEN(method_none));
	pos += DSSH_STRLEN(method_none);

	res = dssh_transport_send_packet(sess, msg, pos, NULL);
methods_done:
	free(msg);
	if (res < 0)
		return res;

	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;

	if (msg_type == SSH_MSG_USERAUTH_BANNER) {
		res = handle_banner(sess, payload, payload_len);
		if (res < 0)
			return res;
		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
	}

	if (msg_type == SSH_MSG_USERAUTH_SUCCESS) {
		if (methods != NULL && methods_sz > 0)
			methods[0] = 0;
		return DSSH_AUTH_NONE_ACCEPTED;
	}

	if (msg_type == SSH_MSG_USERAUTH_FAILURE) {
		if (payload_len < 1 + 4)
			return DSSH_ERROR_PARSE;

		uint32_t mlen;

		pv = dssh_parse_uint32(&payload[1], payload_len - 1, &mlen);
		if (pv < 0)
			return (int)pv;
		if (1 + 4 + mlen > payload_len)
			return DSSH_ERROR_PARSE;

                /* RFC 4251 s6: names MUST NOT contain control chars or DEL */
		for (uint32_t j = 0; j < mlen; j++) {
			uint8_t ch = payload[5 + j];

			if ((ch <= ' ') || (ch >= DSSH_ASCII_DEL))
				return DSSH_ERROR_PARSE;
		}

		if (methods != NULL && methods_sz > 0) {
			size_t copylen = mlen < methods_sz - 1
			    ? mlen : methods_sz - 1;
			memcpy(methods, &payload[5], copylen);
			methods[copylen] = 0;
		}
		return DSSH_AUTH_METHODS_AVAILABLE;
	}

	return DSSH_ERROR_INIT;
}

DSSH_PUBLIC int
dssh_auth_get_methods(dssh_session sess,
    const char *username, char *methods, size_t methods_sz)
{
	if (sess == NULL || username == NULL)
		return DSSH_ERROR_INIT;
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
	if (ulen > SIZE_MAX - 40 || oplen > SIZE_MAX - 40 - ulen)
		return DSSH_ERROR_INVALID;

	static const char service[] = "ssh-connection";
	size_t            msg_len = 40 + ulen + oplen;

	if (change) {
		if (new_password_len > SIZE_MAX - 4 - msg_len)
			return DSSH_ERROR_INVALID;
		msg_len += 4 + new_password_len;
	}

	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

#define SER(v) do { ret = dssh_serialize_uint32((v), msg, msg_len, &pos); \
	if (ret < 0) goto pw_done; } while (0)

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	SER((uint32_t)ulen);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	SER(DSSH_STRLEN(service));
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	SER(DSSH_STRLEN(method_password));
	memcpy(&msg[pos], method_password, DSSH_STRLEN(method_password));
	pos += DSSH_STRLEN(method_password);
	msg[pos++] = change ? 1 : 0;
	SER((uint32_t)oplen);
	memcpy(&msg[pos], old_password, oplen);
	pos += oplen;
	if (change) {
		SER((uint32_t)new_password_len);
		memcpy(&msg[pos], new_password, new_password_len);
		pos += new_password_len;
	}

#undef SER

	ret = dssh_transport_send_packet(sess, msg, pos, NULL);

pw_done:
	OPENSSL_cleanse(msg, msg_len);
	free(msg);
	return ret;
}

DSSH_TESTABLE int
auth_password_impl(dssh_session sess,
    const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *passwd_change_cbdata)
{
	int     res = ensure_auth_service(sess);
	int64_t pv;

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
			res = handle_banner(sess, payload, payload_len);
			if (res < 0 && res != DSSH_ERROR_PARSE)
				return res;
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
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &prompt_len);
			if (pv < 0)
				return (int)pv;
			rpos += 4;
			if (rpos + prompt_len > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *prompt = &payload[rpos];

			rpos += prompt_len;

			uint32_t       lang_len = 0;
			const uint8_t *language = NULL;

			if (rpos + 4 <= payload_len) {
				pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &lang_len);
				if (pv < 0)
					lang_len = 0;
				else
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
				if (new_password != NULL)
					OPENSSL_cleanse(new_password, new_password_len);
				free(new_password);
				return res;
			}

			res = send_password_request(sess, username, password,
			        new_password, new_password_len, true);
			if (new_password != NULL)
				OPENSSL_cleanse(new_password, new_password_len);
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
	if (sess == NULL || username == NULL || password == NULL)
		return DSSH_ERROR_INIT;
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

	if (ulen > UINT32_MAX || ulen > SIZE_MAX - 55)
		return DSSH_ERROR_INVALID;

	static const char service[] = "ssh-connection";
	size_t            msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1)
	    + 4 + DSSH_STRLEN(method_keyboard_interactive) + 4 + 4;
	uint8_t          *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t  pos = 0;
	int     res;
	int64_t pv;

#define SER(v) do { res = dssh_serialize_uint32((v), msg, msg_len, &pos); \
	if (res < 0) { free(msg); return res; } } while (0)

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	SER((uint32_t)ulen);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	SER(DSSH_STRLEN(service));
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	SER(DSSH_STRLEN(method_keyboard_interactive));
	memcpy(&msg[pos], method_keyboard_interactive, DSSH_STRLEN(method_keyboard_interactive));
	pos += DSSH_STRLEN(method_keyboard_interactive);
	SER(0); /* language */
	SER(0); /* submethods */

#undef SER

	res = dssh_transport_send_packet(sess, msg, pos, NULL);

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
			res = handle_banner(sess, payload, payload_len);
			if (res < 0 && res != DSSH_ERROR_PARSE)
				return res;
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
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			if (pv < 0)
				return (int)pv;
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *name = &payload[rpos];
			size_t         name_len = slen;

			rpos += slen;

                        /* Parse instruction string */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			if (pv < 0)
				return (int)pv;
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;

			const uint8_t *instruction = &payload[rpos];
			size_t         instruction_len = slen;

			rpos += slen;

                        /* Parse language string (ignored) */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;
			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			if (pv < 0)
				return (int)pv;
			rpos += 4;
			if (rpos + slen > payload_len)
				return DSSH_ERROR_PARSE;
			rpos += slen;

                        /* num_prompts */
			if (rpos + 4 > payload_len)
				return DSSH_ERROR_PARSE;

			uint32_t num_prompts;

			pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &num_prompts);
			if (pv < 0)
				return (int)pv;
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
					pv = dssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
					if (pv < 0) {
						free(prompts);
						free(prompt_lens);
						free(echo);
						free(responses);
						free(response_lens);
						return (int)pv;
					}
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
				if (response_lens[i] > SIZE_MAX - 4 - resp_sz) {
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
			res = dssh_serialize_uint32(num_prompts, resp, resp_sz, &rp);
			if (res < 0) {
				for (uint32_t j = 0; j < num_prompts; j++)
					free(responses[j]);
				free(responses);
				free(response_lens);
				free(resp);
				return res;
			}
			for (uint32_t i = 0; i < num_prompts; i++) {
				res = dssh_serialize_uint32((uint32_t)response_lens[i], resp, resp_sz, &rp);
				if (res < 0) {
					for (uint32_t j = i; j < num_prompts; j++)
						free(responses[j]);
					free(responses);
					free(response_lens);
					free(resp);
					return res;
				}
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
	if (sess == NULL || username == NULL || prompt_cb == NULL)
		return DSSH_ERROR_INIT;
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
	const uint8_t *pubkey_buf = NULL;
	size_t  pubkey_len;
	int     res = ka->pubkey(&pubkey_buf, &pubkey_len, ka->ctx);

	if (res < 0)
		return res;

	size_t            ulen = strlen(username);
	size_t            alen = strlen(algo_name);

	if (ulen > UINT32_MAX || alen > UINT32_MAX
	    || pubkey_len > UINT32_MAX || sess->trans.session_id_sz > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	/* Overflow check: 49 bytes fixed + 4 runtime size_t values */
	size_t rt = sess->trans.session_id_sz;

	if (ulen > SIZE_MAX - 49 - rt || alen > SIZE_MAX - 49 - rt - ulen
	    || pubkey_len > SIZE_MAX - 49 - rt - ulen - alen)
		return DSSH_ERROR_INVALID;

	static const char service[] = "ssh-connection";

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
	    + 4 + DSSH_STRLEN(method_publickey) + 1 + 4 + alen + 4 + pubkey_len;
	uint8_t *sign_data = malloc(sign_data_len);

	if (sign_data == NULL)
		return DSSH_ERROR_ALLOC;

	size_t sp = 0;

#define SD_SER(v) do { res = dssh_serialize_uint32((v), sign_data, sign_data_len, &sp); \
	if (res < 0) { free(sign_data); return res; } } while (0)

	SD_SER((uint32_t)sess->trans.session_id_sz);
	memcpy(&sign_data[sp], sess->trans.session_id, sess->trans.session_id_sz);
	sp += sess->trans.session_id_sz;
	sign_data[sp++] = SSH_MSG_USERAUTH_REQUEST;
	SD_SER((uint32_t)ulen);
	memcpy(&sign_data[sp], username, ulen);
	sp += ulen;
	SD_SER(DSSH_STRLEN(service));
	memcpy(&sign_data[sp], service, sizeof(service) - 1);
	sp += sizeof(service) - 1;
	SD_SER(DSSH_STRLEN(method_publickey));
	memcpy(&sign_data[sp], method_publickey, DSSH_STRLEN(method_publickey));
	sp += DSSH_STRLEN(method_publickey);
	sign_data[sp++] = 1; /* TRUE */
	SD_SER((uint32_t)alen);
	memcpy(&sign_data[sp], algo_name, alen);
	sp += alen;
	SD_SER((uint32_t)pubkey_len);
	memcpy(&sign_data[sp], pubkey_buf, pubkey_len);
	sp += pubkey_len;

#undef SD_SER

        /* Sign the data */
	uint8_t *sig_buf = NULL;
	size_t  sig_len;

	res = ka->sign(&sig_buf, &sig_len, sign_data, sp, ka->ctx);
	free(sign_data);
	if (res < 0) {
		free(sig_buf);
		return res;
	}
	if (sig_len > UINT32_MAX
	    || sig_len > SIZE_MAX - 49 - ulen - alen - pubkey_len) {
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
	size_t   msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1)
	    + 4 + DSSH_STRLEN(method_publickey) + 1 + 4 + alen
	    + 4 + pubkey_len + 4 + sig_len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL) {
		free(sig_buf);
		return DSSH_ERROR_ALLOC;
	}

	size_t pos = 0;

#define MSG_SER(v) do { res = dssh_serialize_uint32((v), msg, msg_len, &pos); \
	if (res < 0) { free(msg); free(sig_buf); return res; } } while (0)

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	MSG_SER((uint32_t)ulen);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	MSG_SER(DSSH_STRLEN(service));
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	MSG_SER(DSSH_STRLEN(method_publickey));
	memcpy(&msg[pos], method_publickey, DSSH_STRLEN(method_publickey));
	pos += DSSH_STRLEN(method_publickey);
	msg[pos++] = 1; /* TRUE */
	MSG_SER((uint32_t)alen);
	memcpy(&msg[pos], algo_name, alen);
	pos += alen;
	MSG_SER((uint32_t)pubkey_len);
	memcpy(&msg[pos], pubkey_buf, pubkey_len);
	pos += pubkey_len;
	MSG_SER((uint32_t)sig_len);
	memcpy(&msg[pos], sig_buf, sig_len);
	pos += sig_len;

#undef MSG_SER

	res = dssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	free(sig_buf);
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
			res = handle_banner(sess, payload, payload_len);
			if (res < 0 && res != DSSH_ERROR_PARSE)
				return res;
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
	if (sess == NULL || username == NULL || algo_name == NULL)
		return DSSH_ERROR_INIT;
	return auth_check_terminated(sess,
	           auth_publickey_impl(sess, username, algo_name));
}
