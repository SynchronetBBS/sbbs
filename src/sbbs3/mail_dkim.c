/* mail_dkim.c: DKIM (RFC 6376) outbound signing for the Synchronet mail server.
 *
 * Relaxed/relaxed, rsa-sha256.  Functional only when built with DKIM_OPENSSL
 * (OpenSSL/libcrypto present); otherwise the public entry points are stubs.
 * See mail_dkim.h for the API and the rationale for the two-pass body hashing.
 */

#include "mail_dkim.h"

#include <stdlib.h>
#include <string.h>

bool dkim_available(void)
{
#ifdef DKIM_OPENSSL
	return true;
#else
	return false;
#endif
}

#ifdef DKIM_OPENSSL

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

/* Hashing and base64 use OpenSSL's EVP here (the module is already OpenSSL-only):
 * this avoids a SHA256_CTX type clash between Synchronet's sha256.h and
 * <openssl/sha.h> and keeps mail_dkim self-contained on libcrypto. */

/* ---------------------------------------------------------------- membuf */

typedef struct {
	char* buf;
	size_t len;
	size_t cap;
	bool err;
} membuf_t;

static void mb_init(membuf_t* m) { m->buf = NULL; m->len = 0; m->cap = 0; m->err = false; }
static void mb_free(membuf_t* m) { free(m->buf); m->buf = NULL; m->len = m->cap = 0; }

static bool mb_reserve(membuf_t* m, size_t extra)
{
	if (m->err)
		return false;
	if (m->len + extra + 1 > m->cap) {
		size_t nc = m->cap ? m->cap : 256;
		while (nc < m->len + extra + 1)
			nc *= 2;
		char*  nb = (char*)realloc(m->buf, nc);
		if (nb == NULL) { m->err = true; return false; }
		m->buf = nb;
		m->cap = nc;
	}
	return true;
}

static void mb_append(membuf_t* m, const void* d, size_t n)
{
	if (!mb_reserve(m, n))
		return;
	memcpy(m->buf + m->len, d, n);
	m->len += n;
	m->buf[m->len] = '\0';
}

static void mb_puts(membuf_t* m, const char* s) { mb_append(m, s, strlen(s)); }
static void mb_putc(membuf_t* m, char c)        { mb_append(m, &c, 1); }

static void mb_printf(membuf_t* m, const char* fmt, ...)
{
	char    tmp[512];
	va_list ap;
	va_start(ap, fmt);
	int     n = vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	if (n > 0)
		mb_append(m, tmp, (size_t)n);
}

/* -------------------------------------------- relaxed header canon (RFC 6376) */

/* Append the relaxed canonicalization of one header field ("Name: value",
 * possibly folded) to 'mb': lowercased name, no WSP around the colon, value
 * unfolded with WSP runs reduced to single SP and trailing WSP removed.
 * Appends a trailing CRLF when 'crlf' is true. */
static void append_canon_header(membuf_t* mb, const char* field, bool crlf)
{
	const char* colon = strchr(field, ':');
	const char* name_end = colon ? colon : field + strlen(field);

	/* name: strip trailing WSP (before colon), lowercase */
	while (name_end > field && (name_end[-1] == ' ' || name_end[-1] == '\t'))
		name_end--;
	for (const char* p = field; p < name_end; p++)
		mb_putc(mb, (char)tolower((unsigned char)*p));
	mb_putc(mb, ':');

	if (colon != NULL) {
		bool wsp = false;       /* WSP pending */
		bool started = false;   /* emitted any value char (guards leading WSP) */
		for (const char* p = colon + 1; *p != '\0'; p++) {
			char c = *p;
			if (c == '\r' || c == '\n')     /* unfold */
				continue;
			if (c == ' ' || c == '\t') {    /* collapse */
				wsp = true;
				continue;
			}
			if (wsp && started)             /* internal WSP -> single SP */
				mb_putc(mb, ' ');
			wsp = false;
			started = true;
			mb_putc(mb, c);
		}
	}
	if (crlf)
		mb_puts(mb, "\r\n");
}

/* --------------------------------------------------------------- bodyhash */

struct dkim_bodyhash {
	EVP_MD_CTX* md;
	bool wsp;           /* WSP pending in current line */
	size_t crlf;        /* deferred CRLFs (trailing-empty-line handling) */
	bool any;           /* any content hashed */
};

dkim_bodyhash_t* dkim_bodyhash_new(void)
{
	dkim_bodyhash_t* bh = (dkim_bodyhash_t*)calloc(1, sizeof(*bh));
	if (bh == NULL)
		return NULL;
	bh->md = EVP_MD_CTX_new();
	if (bh->md == NULL || EVP_DigestInit_ex(bh->md, EVP_sha256(), NULL) != 1) {
		EVP_MD_CTX_free(bh->md);
		free(bh);
		return NULL;
	}
	return bh;
}

void dkim_bodyhash_update(dkim_bodyhash_t* bh, const void* data, size_t len)
{
	const unsigned char* p = (const unsigned char*)data;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = p[i];
		if (c == '\r')                  /* normalize line endings: drop CR */
			continue;
		if (c == '\n') {                /* end of line: trailing WSP dropped */
			bh->wsp = false;
			bh->crlf++;                 /* defer (may be a trailing empty line) */
			continue;
		}
		if (c == ' ' || c == '\t') {    /* collapse WSP */
			bh->wsp = true;
			continue;
		}
		/* content char: flush deferred CRLFs (real lines follow), then WSP */
		while (bh->crlf > 0) {
			EVP_DigestUpdate(bh->md, "\r\n", 2);
			bh->crlf--;
		}
		if (bh->wsp) {
			EVP_DigestUpdate(bh->md, " ", 1);
			bh->wsp = false;
		}
		EVP_DigestUpdate(bh->md, &c, 1);
		bh->any = true;
	}
}

bool dkim_bodyhash_final(dkim_bodyhash_t* bh, char* out, size_t outsz)
{
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int  dlen = 0;

	/* a non-empty body ends with exactly one CRLF; trailing empty lines and a
	 * wholly-empty body contribute nothing (relaxed body canonicalization) */
	if (bh->any)
		EVP_DigestUpdate(bh->md, "\r\n", 2);
	if (EVP_DigestFinal_ex(bh->md, digest, &dlen) != 1)
		return false;
	if (outsz < (size_t)(4 * ((dlen + 2) / 3) + 1))
		return false;
	EVP_EncodeBlock((unsigned char*)out, digest, (int)dlen);
	return true;
}

void dkim_bodyhash_free(dkim_bodyhash_t* bh)
{
	if (bh != NULL) {
		EVP_MD_CTX_free(bh->md);
		free(bh);
	}
}

/* ---------------------------------------------------------------- headers */

struct dkim_headers {
	membuf_t canon;     /* concatenated relaxed-canon "name:value\r\n" */
	membuf_t hlist;     /* "name1:name2:..." (h= tag) */
};

dkim_headers_t* dkim_headers_new(void)
{
	dkim_headers_t* h = (dkim_headers_t*)calloc(1, sizeof(*h));
	if (h != NULL) {
		mb_init(&h->canon);
		mb_init(&h->hlist);
	}
	return h;
}

bool dkim_headers_add(dkim_headers_t* h, const char* field)
{
	const char* colon = strchr(field, ':');
	const char* name_end = colon ? colon : field + strlen(field);
	while (name_end > field && (name_end[-1] == ' ' || name_end[-1] == '\t'))
		name_end--;

	/* h= tag: original-case header name, colon-separated */
	if (h->hlist.len > 0)
		mb_putc(&h->hlist, ':');
	mb_append(&h->hlist, field, (size_t)(name_end - field));

	append_canon_header(&h->canon, field, /*crlf*/ true);

	return !(h->canon.err || h->hlist.err);
}

void dkim_headers_free(dkim_headers_t* h)
{
	if (h == NULL)
		return;
	mb_free(&h->canon);
	mb_free(&h->hlist);
	free(h);
}

/* ----------------------------------------------------------------- signer */

struct dkim_signer {
	EVP_PKEY* key;
	char* domain;
	char* selector;
};

dkim_signer_t* dkim_signer_open(const char* domain, const char* selector, const char* keyfile)
{
	if (domain == NULL || *domain == '\0' || selector == NULL || *selector == '\0'
	    || keyfile == NULL || *keyfile == '\0')
		return NULL;

	/* Read the PEM key with our own C runtime and parse it from an in-memory BIO
	 * rather than passing a FILE* into libcrypto.  On Windows the OpenSSL DLL has
	 * its own CRT, and handing it a FILE* from this module requires the OpenSSL
	 * "applink" shim (else it fatally aborts with "no OPENSSL_Applink").  Going
	 * through a memory BIO avoids the CRT boundary entirely and is portable. */
	FILE* fp = fopen(keyfile, "rb");
	if (fp == NULL)
		return NULL;
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}
	long  sz = ftell(fp);       /* long: stdio API return type */
	if (sz <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return NULL;
	}
	char* pem = (char*)malloc((size_t)sz);
	if (pem == NULL) {
		fclose(fp);
		return NULL;
	}
	size_t got = fread(pem, 1, (size_t)sz, fp);
	fclose(fp);
	if (got != (size_t)sz) {
		free(pem);
		return NULL;
	}
	BIO* bio = BIO_new_mem_buf(pem, (int)got);
	if (bio == NULL) {
		free(pem);
		return NULL;
	}
	EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
	BIO_free(bio);
	free(pem);
	if (key == NULL)
		return NULL;
	if (!EVP_PKEY_is_a(key, "RSA")) {
		EVP_PKEY_free(key);
		return NULL;
	}

	dkim_signer_t* s = (dkim_signer_t*)calloc(1, sizeof(*s));
	if (s == NULL) {
		EVP_PKEY_free(key);
		return NULL;
	}
	s->key = key;
	s->domain = strdup(domain);
	s->selector = strdup(selector);
	if (s->domain == NULL || s->selector == NULL) {
		dkim_signer_close(s);
		return NULL;
	}
	return s;
}

void dkim_signer_close(dkim_signer_t* s)
{
	if (s == NULL)
		return;
	if (s->key != NULL)
		EVP_PKEY_free(s->key);
	free(s->domain);
	free(s->selector);
	free(s);
}

/* RSASSA-PKCS1-v1_5 over SHA-256 of 'data' -> malloc'd raw signature */
static bool rsa_sha256_sign(EVP_PKEY* key, const void* data, size_t len,
                            unsigned char** sig, size_t* siglen)
{
	EVP_MD_CTX*   md = EVP_MD_CTX_new();
	EVP_PKEY_CTX* pctx = NULL;
	bool          ok = false;

	*sig = NULL;
	if (md == NULL)
		return false;
	if (EVP_DigestSignInit(md, &pctx, EVP_sha256(), NULL, key) != 1)
		goto done;
	if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) != 1)
		goto done;
	if (EVP_DigestSign(md, NULL, siglen, (const unsigned char*)data, len) != 1)
		goto done;
	*sig = (unsigned char*)malloc(*siglen);
	if (*sig == NULL)
		goto done;
	if (EVP_DigestSign(md, *sig, siglen, (const unsigned char*)data, len) != 1) {
		free(*sig);
		*sig = NULL;
		goto done;
	}
	ok = true;
done:
	EVP_MD_CTX_free(md);
	return ok;
}

bool dkim_sign(dkim_signer_t* s, const dkim_headers_t* h, const char* bh, time_t when,
               char* out, size_t outsz)
{
	if (s == NULL || h == NULL || bh == NULL || h->hlist.len == 0)
		return false;

	/* Build the header text up to and including "b=" exactly as it will be
	 * emitted; this same text (canonicalized, b= empty) is what we sign, so the
	 * verifier's reconstruction matches regardless of our folding. */
	membuf_t hdr;
	mb_init(&hdr);
	mb_puts(&hdr, "DKIM-Signature: v=1; a=rsa-sha256; c=relaxed/relaxed;\r\n\t");
	mb_printf(&hdr, "d=%s; s=%s;", s->domain, s->selector);
	if (when != 0)
		mb_printf(&hdr, " t=%lld;", (long long)when);
	mb_puts(&hdr, "\r\n\th=");
	mb_append(&hdr, h->hlist.buf, h->hlist.len);
	mb_puts(&hdr, ";\r\n\tbh=");
	mb_puts(&hdr, bh);
	mb_puts(&hdr, ";\r\n\tb=");

	/* signing input = canonicalized signed headers + canonicalized
	 * DKIM-Signature header (with empty b=), the latter without trailing CRLF */
	membuf_t signin;
	mb_init(&signin);
	mb_append(&signin, h->canon.buf, h->canon.len);
	append_canon_header(&signin, hdr.buf, /*crlf*/ false);

	bool           ok = false;
	unsigned char* sig = NULL;
	size_t         siglen = 0;
	char*          b64 = NULL;

	if (hdr.err || signin.err)
		goto done;
	if (!rsa_sha256_sign(s->key, signin.buf, signin.len, &sig, &siglen))
		goto done;

	size_t b64cap = 4 * ((siglen + 2) / 3) + 1;
	b64 = (char*)malloc(b64cap);
	if (b64 == NULL)
		goto done;
	EVP_EncodeBlock((unsigned char*)b64, sig, (int)siglen);

	/* emit: header prefix + folded base64 signature, NO trailing CRLF (the
	 * caller terminates the header line, e.g. via sockprintf) */
	membuf_t fin;
	mb_init(&fin);
	mb_append(&fin, hdr.buf, hdr.len);
	for (size_t i = 0, col = 0; b64[i] != '\0'; i++) {
		if (col == 64) {            /* fold long b= (verifiers ignore WSP in b=) */
			mb_puts(&fin, "\r\n\t");
			col = 0;
		}
		mb_putc(&fin, b64[i]);
		col++;
	}

	if (!fin.err && fin.len + 1 <= outsz) {
		memcpy(out, fin.buf, fin.len + 1);
		ok = true;
	}
	mb_free(&fin);

done:
	free(sig);
	free(b64);
	mb_free(&hdr);
	mb_free(&signin);
	return ok;
}

/* ------------------------------- signed-header selection (for the capture) */

/* default signed-header set, lowercase, in preferred order */
static const char* const default_signed[] = {
	"from", "to", "cc", "subject", "date", "message-id", "reply-to",
	"in-reply-to", "references", "mime-version", "content-type",
	"content-transfer-encoding", NULL
};

static bool name_matches(const char* field, const char* lcname)
{
	size_t n = strlen(lcname);
	for (size_t i = 0; i < n; i++) {
		if (field[i] == '\0' || tolower((unsigned char)field[i]) != lcname[i])
			return false;
	}
	/* next non-name char must be ':' or WSP-then-':' */
	const char* p = field + n;
	while (*p == ' ' || *p == '\t')
		p++;
	return *p == ':';
}

/* ------------------------------------------------ capture observer (2-pass) */

struct dkim_capture {
	int mode;
	dkim_headers_t* hdrs;       /* SIGN only */
	dkim_bodyhash_t* body;
	bool in_body;
	bool have_from;
	bool mismatch;              /* VERIFY only */
	char bh[DKIM_BODYHASH_STRLEN];             /* SIGN: computed; VERIFY: expected */
};

dkim_capture_t* dkim_capture_new(int mode)
{
	dkim_capture_t* c = (dkim_capture_t*)calloc(1, sizeof(*c));
	if (c == NULL)
		return NULL;
	c->mode = mode;
	c->body = dkim_bodyhash_new();
	if (mode == DKIM_CAP_SIGN)
		c->hdrs = dkim_headers_new();
	if (c->body == NULL || (mode == DKIM_CAP_SIGN && c->hdrs == NULL)) {
		dkim_capture_free(c);
		return NULL;
	}
	return c;
}

int dkim_capture_line(dkim_capture_t* c, const char* line, size_t len)
{
	const int divert = (c->mode == DKIM_CAP_SIGN) ? DKIM_LINE_DIVERT : DKIM_LINE_SEND;

	if (!c->in_body) {
		if (len == 0) {                 /* blank line => end of header block */
			c->in_body = true;
			return divert;
		}
		if (c->mode == DKIM_CAP_SIGN) {
			for (const char* const* d = default_signed; *d != NULL; d++) {
				if (name_matches(line, *d)) {
					dkim_headers_add(c->hdrs, line);
					if (strcmp(*d, "from") == 0)
						c->have_from = true;
					break;
				}
			}
		}
		return divert;
	}

	if (len == 1 && line[0] == '.') {   /* SMTP end-of-data terminator */
		if (c->mode == DKIM_CAP_VERIFY) {
			char bh[DKIM_BODYHASH_STRLEN];
			if (!dkim_bodyhash_final(c->body, bh, sizeof bh) || strcmp(bh, c->bh) != 0) {
				c->mismatch = true;
				return DKIM_LINE_ABORT;   /* suppress terminator -> receiver discards */
			}
			return DKIM_LINE_SEND;
		}
		return DKIM_LINE_DIVERT;          /* SIGN: terminator isn't body */
	}

	/* body content line: reverse SMTP dot-stuffing, then hash with CRLF */
	const char* p = line;
	size_t      n = len;
	if (n > 0 && p[0] == '.') {
		p++;
		n--;
	}
	dkim_bodyhash_update(c->body, p, n);
	dkim_bodyhash_update(c->body, "\n", 1);
	return divert;
}

bool dkim_capture_sign(dkim_capture_t* c, dkim_signer_t* s, time_t when, char* out, size_t outsz)
{
	if (c->mode != DKIM_CAP_SIGN || !c->have_from)
		return false;
	if (!dkim_bodyhash_final(c->body, c->bh, sizeof c->bh))
		return false;
	return dkim_sign(s, c->hdrs, c->bh, when, out, outsz);
}

const char* dkim_capture_bodyhash(const dkim_capture_t* c) { return c->bh; }

void dkim_capture_expect(dkim_capture_t* c, const char* bh)
{
	if (bh != NULL) {
		strncpy(c->bh, bh, sizeof c->bh - 1);
		c->bh[sizeof c->bh - 1] = '\0';
	}
}

bool dkim_capture_mismatch(const dkim_capture_t* c) { return c->mismatch; }
bool dkim_capture_diverts(const dkim_capture_t* c) { return c->mode == DKIM_CAP_SIGN; }

void dkim_capture_free(dkim_capture_t* c)
{
	if (c == NULL)
		return;
	dkim_headers_free(c->hdrs);
	dkim_bodyhash_free(c->body);
	free(c);
}

#else /* !DKIM_OPENSSL ----------------------------------------------- stubs */

dkim_signer_t*   dkim_signer_open(const char* d, const char* s, const char* k) { (void)d; (void)s; (void)k; return NULL; }
void             dkim_signer_close(dkim_signer_t* s) { (void)s; }
dkim_bodyhash_t* dkim_bodyhash_new(void) { return NULL; }
void             dkim_bodyhash_update(dkim_bodyhash_t* b, const void* d, size_t n) { (void)b; (void)d; (void)n; }
bool             dkim_bodyhash_final(dkim_bodyhash_t* b, char* o, size_t n) { (void)b; (void)o; (void)n; return false; }
void             dkim_bodyhash_free(dkim_bodyhash_t* b) { (void)b; }
dkim_headers_t*  dkim_headers_new(void) { return NULL; }
bool             dkim_headers_add(dkim_headers_t* h, const char* f) { (void)h; (void)f; return false; }
void             dkim_headers_free(dkim_headers_t* h) { (void)h; }
bool             dkim_sign(dkim_signer_t* s, const dkim_headers_t* h, const char* bh, time_t w, char* o, size_t n)
{ (void)s; (void)h; (void)bh; (void)w; (void)o; (void)n; return false; }
dkim_capture_t*  dkim_capture_new(int mode) { (void)mode; return NULL; }
int              dkim_capture_line(dkim_capture_t* c, const char* l, size_t n) { (void)c; (void)l; (void)n; return DKIM_LINE_SEND; }
bool             dkim_capture_sign(dkim_capture_t* c, dkim_signer_t* s, time_t w, char* o, size_t n) { (void)c; (void)s; (void)w; (void)o; (void)n; return false; }
const char*      dkim_capture_bodyhash(const dkim_capture_t* c) { (void)c; return ""; }
void             dkim_capture_expect(dkim_capture_t* c, const char* bh) { (void)c; (void)bh; }
bool             dkim_capture_mismatch(const dkim_capture_t* c) { (void)c; return false; }
bool             dkim_capture_diverts(const dkim_capture_t* c) { (void)c; return false; }
void             dkim_capture_free(dkim_capture_t* c) { (void)c; }

#endif /* DKIM_OPENSSL */
