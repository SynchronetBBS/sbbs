/*
 * Part of the single-TU build.  All system and third-party includes
 * live in sftp.c; this file is #include'd from sftp.c and cannot be
 * compiled on its own.
 *
 * Outcome carrier for the public sftp client/server APIs — see
 * sftp.h for the contract.  Both sftpc_outcome and sftps_outcome
 * share the same (sz, err, estr) prefix, so the formatting and
 * \r\n-prepending logic lives in outcome_record_impl() and the two
 * public record helpers just delegate.
 */

static void
outcome_record_impl(size_t sz, uint32_t *err_out, char *estr,
                    sftp_err_code_t err,
                    const char *file, int line,
                    const char *fmt, va_list ap)
{
	*err_out = (uint32_t)err;
	if (sz == 0)
		return;
	size_t pos = strnlen(estr, sz);
	if (pos >= sz)
		return;
	if (pos > 0 && pos + 2 < sz) {
		estr[pos++] = '\r';
		estr[pos++] = '\n';
		estr[pos] = '\0';
	}
	int n = snprintf(estr + pos, sz - pos, "%s:%d: ", file, line);
	if (n < 0)
		return;
	if ((size_t)n >= sz - pos)
		return;
	pos += (size_t)n;
	vsnprintf(estr + pos, sz - pos, fmt, ap);
}

void
sftpc_outcome_record_(struct sftpc_outcome *out, sftp_err_code_t err,
                      const char *file, int line, const char *fmt, ...)
{
	if (out == NULL)
		return;
	va_list ap;
	va_start(ap, fmt);
	outcome_record_impl(out->sz, &out->err, out->estr, err, file, line,
	    fmt, ap);
	va_end(ap);
}

void
sftps_outcome_record_(struct sftps_outcome *out, sftp_err_code_t err,
                      const char *file, int line, const char *fmt, ...)
{
	if (out == NULL)
		return;
	va_list ap;
	va_start(ap, fmt);
	outcome_record_impl(out->sz, &out->err, out->estr, err, file, line,
	    fmt, ap);
	va_end(ap);
}

void
sftpc_outcome_reply(struct sftpc_outcome *out,
                    const char *msg, uint32_t msg_len,
                    const char *lang, uint32_t lang_len)
{
	if (out == NULL || out->sz == 0 || msg == NULL || msg_len == 0)
		return;
	size_t pos = strnlen(out->estr, out->sz);
	if (pos >= out->sz)
		return;
	if (pos > 0 && pos + 2 < out->sz) {
		out->estr[pos++] = '\r';
		out->estr[pos++] = '\n';
		out->estr[pos] = '\0';
	}
	int n;
	if (lang != NULL && lang_len > 0) {
		n = snprintf(out->estr + pos, out->sz - pos,
		    "Reply: \"%.*s\" (%.*s)",
		    (int)msg_len, msg, (int)lang_len, lang);
	}
	else {
		n = snprintf(out->estr + pos, out->sz - pos,
		    "Reply: \"%.*s\"", (int)msg_len, msg);
	}
	(void)n;
}

bool
sftp_err_is_transient(sftp_err_code_t err)
{
	switch (err) {
	case SFTP_ERR_SEND_FAILED:
	case SFTP_ERR_CHANNEL_CLOSED:
	case SFTP_ERR_ABORTED:
	case SFTP_ERR_REPLY_NULL:
		return true;
	default:
		return false;
	}
}
