/*
 * Part of the single-TU build.  All system and third-party includes
 * live in sftp.c; this file is #include'd from sftp.c and cannot be
 * compiled on its own.
 *
 * Outcome carriers:
 *   - Server side keeps the flex-array struct sftps_outcome (caller
 *     stack-allocates via SFTPS_OUTCOME_DECL).
 *   - Client side carries err / result / estr directly on the pending;
 *     estr is heap-allocated and grown via realloc as records
 *     accumulate, freed by sftpc_pending_free.  Errors aren't a hot
 *     path so the realloc-grow pattern is fine.
 */

/* Server-side fixed-buffer record helper.  Same shape as before. */
static void
sftps_record_impl(size_t sz, uint32_t *err_out, char *estr,
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
sftps_outcome_record_(struct sftps_outcome *out, sftp_err_code_t err,
                      const char *file, int line, const char *fmt, ...)
{
	if (out == NULL)
		return;
	va_list ap;
	va_start(ap, fmt);
	sftps_record_impl(out->sz, &out->err, out->estr, err, file, line,
	    fmt, ap);
	va_end(ap);
}

/* Grow pending->estr to hold at least `need` bytes (including a NUL).
 * Returns true on success; on failure leaves estr untouched and the
 * record is silently dropped — diagnostic-text overflow shouldn't
 * fail the op. */
static bool
pending_estr_reserve(struct sftpc_pending *p, size_t need)
{
	if (p->estr_cap >= need)
		return true;
	size_t cap = p->estr_cap == 0 ? 128 : p->estr_cap;
	while (cap < need)
		cap *= 2;
	char *nb = realloc(p->estr, cap);
	if (nb == NULL)
		return false;
	if (p->estr == NULL)
		nb[0] = '\0';
	p->estr = nb;
	p->estr_cap = cap;
	return true;
}

/* Append a formatted record to pending->estr, prepending CRLF if
 * something is already there.  Always sets pending->err, even when
 * the text-grow fails. */
static void
pending_record(struct sftpc_pending *p, sftp_err_code_t err,
               const char *file, int line, const char *fmt, ...)
{
	if (p == NULL)
		return;
	p->err = err;

	va_list ap;
	va_start(ap, fmt);
	va_list ap2;
	va_copy(ap2, ap);
	int body_len = vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);
	int prefix_len = snprintf(NULL, 0, "%s:%d: ", file, line);
	if (body_len < 0 || prefix_len < 0) {
		va_end(ap);
		return;
	}

	size_t add = (size_t)prefix_len + (size_t)body_len;
	if (p->estr_len > 0)
		add += 2;       /* CRLF separator */
	size_t need = p->estr_len + add + 1;  /* + NUL */
	if (!pending_estr_reserve(p, need)) {
		va_end(ap);
		return;
	}
	if (p->estr_len > 0) {
		p->estr[p->estr_len++] = '\r';
		p->estr[p->estr_len++] = '\n';
	}
	int n = snprintf(p->estr + p->estr_len, p->estr_cap - p->estr_len,
	    "%s:%d: ", file, line);
	if (n > 0)
		p->estr_len += (size_t)n;
	n = vsnprintf(p->estr + p->estr_len, p->estr_cap - p->estr_len,
	    fmt, ap);
	if (n > 0)
		p->estr_len += (size_t)n;
	va_end(ap);
}

/* File/line capture wrapper used at every call site. */
#define PENDING_RECORD(p, err_code, ...)                                  \
	pending_record((p), (err_code), __FILE__, __LINE__, __VA_ARGS__)

/* Append the server-supplied SSH_FXP_STATUS message text to pending's
 * estr.  Doesn't change pending->err — STATUS replies are typically
 * "OK with a result code", not failures. */
static void
pending_record_reply(struct sftpc_pending *p,
                     const char *msg, uint32_t msg_len,
                     const char *lang, uint32_t lang_len)
{
	if (p == NULL || msg == NULL || msg_len == 0)
		return;
	int n;
	if (lang != NULL && lang_len > 0)
		n = snprintf(NULL, 0, "Reply: \"%.*s\" (%.*s)",
		    (int)msg_len, msg, (int)lang_len, lang);
	else
		n = snprintf(NULL, 0, "Reply: \"%.*s\"", (int)msg_len, msg);
	if (n < 0)
		return;
	size_t add = (size_t)n;
	if (p->estr_len > 0)
		add += 2;
	size_t need = p->estr_len + add + 1;
	if (!pending_estr_reserve(p, need))
		return;
	if (p->estr_len > 0) {
		p->estr[p->estr_len++] = '\r';
		p->estr[p->estr_len++] = '\n';
	}
	int written;
	if (lang != NULL && lang_len > 0)
		written = snprintf(p->estr + p->estr_len,
		    p->estr_cap - p->estr_len, "Reply: \"%.*s\" (%.*s)",
		    (int)msg_len, msg, (int)lang_len, lang);
	else
		written = snprintf(p->estr + p->estr_len,
		    p->estr_cap - p->estr_len, "Reply: \"%.*s\"",
		    (int)msg_len, msg);
	if (written > 0)
		p->estr_len += (size_t)written;
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
