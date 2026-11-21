/* Synchronet message base (SMB) message text library routines */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* ANSI */
#include <stdlib.h> /* malloc/realloc/free */
#include <string.h> /* strlen */
#include <ctype.h>  /* tolower */

/* XPDev */
#include "xpendian.h"

/* SMB-specific */
#include "smblib.h"
#include "base64.h"
#include "lzh.h"

char* smb_getmsgtxt(smb_t* smb, smbmsg_t* msg, uint mode)
{
	char*    buf;
	char*    preamble;
	char*    lzhbuf;
	char*    p;
	char*    str;
	uint16_t xlat;
	uint     i;
	int      lzh;   /* bool */
	uint32_t lzhlen;
	uint32_t lzh_decoded;
	int      l = 0, length;

	if (smb->sdt_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return NULL;
	}
	if ((buf = (char*)malloc(sizeof(char))) == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s malloc failure of %" XP_PRIsize_t "u bytes for buffer"
		              , __FUNCTION__, sizeof(char));
		return NULL;
	}
	*buf = 0;

	if (!(mode & GETMSGTXT_NO_HFIELDS)) {
		for (i = 0; i < (uint)msg->total_hfields; i++) {           /* comment headers are part of text */
			if (msg->hfield[i].type != SMB_COMMENT && msg->hfield[i].type != SMTPSYSMSG)
				continue;
			str = (char*)msg->hfield_dat[i];
			length = strlen(str) + 2;   /* +2 for crlf */
			if ((p = (char*)realloc(buf, l + length + 1)) == NULL) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error)
				              , "%s realloc failure of %d bytes for comment buffer"
				              , __FUNCTION__, l + length + 1);
				free(buf);
				return NULL;
			}
			buf = p;
			l += sprintf(buf + l, "%s\r\n", str);
		}
		if (l) { /* Add a blank line after comments */
			if ((p = (char*)realloc(buf, l + 3)) == NULL) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error)
				              , "%s realloc failure of %d bytes for comment buffer"
				              , __FUNCTION__, l + 3);
				free(buf);
				return NULL;
			}
			buf = p;
			l += sprintf(buf + l, "\r\n");
		}
		unsigned answers = 0;
		for (i = 0; i < (uint)msg->total_hfields; i++) {           /* Poll Answers are part of text */
			if (msg->hfield[i].type != SMB_POLL_ANSWER)
				continue;
			char tmp[128];
			length = safe_snprintf(tmp, sizeof(tmp), "%2u: %s\r\n", ++answers, (char*)msg->hfield_dat[i]);
			if ((p = (char*)realloc(buf, l + length + 1)) == NULL) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error)
				              , "%s realloc failure of %d bytes for comment buffer"
				              , __FUNCTION__, l + length + 1);
				free(buf);
				return NULL;
			}
			buf = p;
			memcpy(buf + l, tmp, length);
			l += length;
			buf[l] = 0;
		}
	}
	preamble = strdup(buf);

	for (i = 0; i < (uint)msg->hdr.total_dfields; i++) {
		if (msg->dfield[i].length <= sizeof(xlat))
			continue;
		switch (msg->dfield[i].type) {
			case TEXT_BODY:
				if (mode & GETMSGTXT_NO_BODY)
					continue;
				break;
			case TEXT_TAIL:
				if (!(mode & GETMSGTXT_TAILS))
					continue;
				break;
			default:    /* ignore other data types */
				continue;
		}
		smb_fseek(smb->sdt_fp, (off_t)msg->hdr.offset + msg->dfield[i].offset
		          , SEEK_SET);
		if (fread(&xlat, 1, sizeof(xlat), smb->sdt_fp) != sizeof(xlat))
			continue;
		lzh = 0;
		if (xlat == XLAT_LZH) {
			lzh = 1;
			if (fread(&xlat, 1, sizeof(xlat), smb->sdt_fp) != sizeof(xlat))
				continue;
		}
		if (xlat != XLAT_NONE)     /* no other translations currently supported */
			continue;

		length = msg->dfield[i].length - sizeof(xlat);
		if (lzh) {
			length -= sizeof(xlat);
			if (length < 1)
				continue;
			if ((lzhbuf = (char*)malloc(length)) == NULL) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error)
				              , "%s malloc failure of %d bytes for LZH buffer"
				              , __FUNCTION__, length);
				free(buf);
				free(preamble);
				return NULL;
			}
			if (smb_fread(smb, lzhbuf, length, smb->sdt_fp) != length) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error)
				              , "%s read failure of %d bytes for LZH data"
				              , __FUNCTION__, length);
				free(lzhbuf);
				free(buf);
				free(preamble);
				return NULL;
			}
			memcpy(&lzhlen, lzhbuf, sizeof(lzhlen));
			lzhlen = LE_INT32(lzhlen);
			if ((p = (char*)realloc(buf, l + lzhlen + 3L)) == NULL) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error)
				              , "%s realloc failure of %d bytes for text buffer"
				              , __FUNCTION__, l + lzhlen + 3);
				free(lzhbuf);
				free(buf);
				free(preamble);
				return NULL;
			}
			buf = p;
			lzh_decoded = lzh_decode((uint8_t *)lzhbuf, length, (uint8_t *)buf + l, l + lzhlen + 3);
			if (lzh_decoded < lzhlen) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error)
				              , "%s lzh_decode failure got %" PRIu32 " of %" PRIu32 " bytes for text buffer"
				              , __FUNCTION__, lzh_decoded, lzhlen);
				free(lzhbuf);
				free(buf);
				free(preamble);
				return NULL;
			}
			free(lzhbuf);
			l += lzhlen;
		}
		else {
			if ((p = (char*)realloc(buf, l + length + 3L)) == NULL) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error)
				              , "%s realloc failure of %d bytes for text buffer"
				              , __FUNCTION__, l + length + 3);
				free(buf);
				free(preamble);
				return NULL;
			}
			buf = p;
			p = buf + l;
			l += fread(p, 1, length, smb->sdt_fp);
		}
		if (!l)
			continue;
		l--;
		while (l && buf[l] == 0) l--;
		l++;
		*(buf + l) = '\r';  /* CR */
		l++;
		*(buf + l) = '\n';  /* LF */
		l++;
		*(buf + l) = 0;
	}

	if (mode & GETMSGTXT_PLAIN) {
		char* plaintext = smb_getplaintext(msg, buf);
		if (plaintext != NULL) {
			buf = malloc(strlen(preamble) + strlen(plaintext) + 1);
			if (buf == NULL)
				buf = plaintext;
			else {
				strcpy(buf, preamble);
				strcat(buf, plaintext);
				free(plaintext);
			}
		}
	}
	free(preamble);
	return buf;
}

void smb_freemsgtxt(char* buf)
{
	if (buf != NULL)
		free(buf);
}

enum content_transfer_encoding {
	CONTENT_TRANFER_ENCODING_NONE,
	CONTENT_TRANFER_ENCODING_BASE64,
	CONTENT_TRANFER_ENCODING_QUOTED_PRINTABLE,
	CONTENT_TRANFER_ENCODING_OTHER
};

/* Decode quoted-printable content-transfer-encoded text */
/* Ignores (strips) unsupported ctrl chars and non-ASCII chars */
/* Does not enforce 76 char line length limit */
char* qp_decode(char* buf)
{
	uchar* p = (uchar*)buf;
	uchar* dest = p;

	for (; *p != 0; p++) {
		if (*p == ' ' || (*p >= '!' && *p <= '~' && *p != '=') || *p == '\t' || *p == '\r' || *p == '\n')
			*dest++ = *p;
		else if (*p == '=') {
			p++;
			if (*p == '\r')  /* soft link break */
				p++;
			if (*p == 0)
				break;
			if (*p == '\n')
				continue;
			if (IS_HEXDIGIT(*p) && IS_HEXDIGIT(*(p + 1))) {
				uchar ch = HEX_CHAR_TO_INT(*p) << 4;
				p++;
				ch |= HEX_CHAR_TO_INT(*p);
				if (ch == '\t' || ch >= ' ')
					*dest++ = ch;
			} else {    /* bad encoding */
				*dest++ = '=';
				*dest++ = *p;
			}
		}
	}
	*dest = 0;
	return buf;
}

static size_t strStartsWith_i(const char* buf, const char* match)
{
	size_t len = strlen(match);
	if (strnicmp(buf, match, len) == 0)
		return len;
	return 0;
}

static enum content_transfer_encoding mime_encoding(const char* str)
{
	const char* p = str;

	if (str ==  NULL)
		return CONTENT_TRANFER_ENCODING_NONE;
	SKIP_WHITESPACE(p);
	if (strnicmp(p, "base64", 6) == 0)
		return CONTENT_TRANFER_ENCODING_BASE64;
	if (strnicmp(p, "quoted-printable", 16) == 0)
		return CONTENT_TRANFER_ENCODING_QUOTED_PRINTABLE;
	if (strnicmp(p, "7bit", 4) == 0 || strnicmp(p, "8bit", 4) == 0 || strnicmp(p, "binary", 6) == 0)
		return CONTENT_TRANFER_ENCODING_NONE;
	return CONTENT_TRANFER_ENCODING_OTHER;
}

static enum content_transfer_encoding mime_getxferencoding(const char* beg, const char* end)
{
	const char* p = beg;

	while (p < end) {
		SKIP_WHITESPACE(p);
		size_t len = strStartsWith_i(p, "content-transfer-encoding:");
		if (len < 1) {
			FIND_CHAR(p, '\n');
			continue;
		}
		return mime_encoding(p + len);
	}

	return CONTENT_TRANFER_ENCODING_NONE;
}

/* ToDo: parse and return the "modification-date" value */
static bool mime_getattachment(const char* beg, const char* end, char* attachment, size_t attachment_len)
{
	char        fname[MAX_PATH + 1];
	const char* p = beg;

	while (p < end) {
		SKIP_WHITESPACE(p);
		size_t len = strStartsWith_i(p, "content-disposition:");
		if (len < 1) {
			FIND_CHAR(p, '\n');
			continue;
		}
		p += len;
		SKIP_WHITESPACE(p);
		/* An "inline" part with a filename is an attachment too (e.g. a photo sent from iOS Mail, #485) */
		const char* filename = strstr(p, "filename=");
		if (filename == NULL || filename >= end) {
			FIND_CHAR(p, '\n');
			continue;
		}
		filename += 9;
		const char* term;
		if (*filename == '"') {
			filename++;
			term = strchr(filename, '"');
		} else {
			const char* wsp = filename;
			FIND_WHITESPACE(wsp);
			term = strchr(filename, ';');
			if (term > wsp)
				term = wsp;
		}
		if (term == NULL) {
			term = filename;
			FIND_WHITESPACE(term);
		}
		if (term - filename >= sizeof(fname))
			term = filename + sizeof(fname) - 1;
		memcpy(fname, filename, term - filename);
		fname[term - filename] = 0;
		if (attachment != NULL && attachment_len > 0) {
			strncpy(attachment, getfname(fname), attachment_len);
			attachment[attachment_len - 1] = '\0';
		}
		return true;
	}
	return false;
}

// Parses a MIME text/* content-type header field
void smb_parse_content_type(const char* content_type, char** subtype, char** charset, uint32_t* auxattr)
{
	if (subtype != NULL) {
		FREE_AND_NULL(*subtype);
	}
	if (charset != NULL) {
		FREE_AND_NULL(*charset);
	}
	if (content_type == NULL)
		return;
	char   buf[512];
	SAFECOPY(buf, content_type);
	char*  p;
	if ((p = strstr(buf, "\r\n\r\n")) != NULL)   /* Don't parse past the end of header */
		*p = 0;
	size_t len = strStartsWith_i(buf, "text/");
	if (len > 0) {
		p = buf + len;
		if (subtype != NULL) {
			if ((*subtype = strdup(p)) != NULL) {
				char* tp = *subtype;
				FIND_WHITESPACE(tp);
				*tp = 0;
				tp = *subtype;
				FIND_CHAR(tp, ';');
				*tp = 0;
			}
		}
		char* format;
		if (auxattr != NULL &&
		    ((format = strcasestr(p, " format=")) != NULL
		     || (format = strcasestr(p, ";format=")) != NULL
		     || (format = strcasestr(p, "\tformat=")) != NULL)) {
			bool quoted = false;
			format += 8;
			if (*format == '"') {
				quoted = true;
				format++;
			}
			char* tp = format;
			FIND_WHITESPACE(tp);
			*tp = 0;
			tp = format;
			if (quoted) {
				FIND_CHAR(tp, '"');
			} else {
				FIND_CHAR(tp, ';');
			}
			*tp = 0;
			if (stricmp(format, "fixed") == 0)
				*auxattr |= MSG_FIXED_FORMAT;
		}

		char* parms = p;
		if (charset != NULL &&
		    ((p = strcasestr(parms, " charset=")) != NULL
		     || (p = strcasestr(parms, ";charset=")) != NULL
		     || (p = strcasestr(parms, "\tcharset=")) != NULL)) {
			bool quoted = false;
			p += 9;
			if (*p == '"') {
				quoted = true;
				p++;
			}
			char* tp = p;
			FIND_WHITESPACE(tp);
			*tp = 0;
			tp = p;
			if (quoted) {
				FIND_CHAR(tp, '"');
			} else {
				FIND_CHAR(tp, ';');
			}
			*tp = 0;
			*charset = strdup(p);
		}
	}
}

/* Find the specified content-type in a multi-part MIME-encoded message body, recursively */
/* 'found' counts the matching parts seen so far and is shared by the whole
   recursion, so that 'index' enumerates across nested containers rather than
   restarting at each level */
static const char* mime_getpart(const char* buf, const char* content_type, const char* content_match
                                , int depth, enum content_transfer_encoding* encoding, char** charset, uint32_t* auxattr
                                , char* attachment, size_t attachment_len, int index, int* found)
{
	const char* txt;
	char*       p;
	char        boundary[256];
	char        match1[128];
	char        match2[128];
	int         match_len = 0;
	bool        quoted_boundary = false;

	if (content_match != NULL) {
		match_len = sprintf(match1, "%s;", content_match);
		sprintf(match2, "%s\r", content_match);
	}

	if (depth > 2)
		return NULL;
	if (content_type == NULL)    /* Not MIME-encoded */
		return NULL;
	size_t len;
	if (((len = strStartsWith_i(content_type, "multipart/alternative;")) < 1)
	    && ((len = strStartsWith_i(content_type, "multipart/mixed;")) < 1)
	    && ((len = strStartsWith_i(content_type, "multipart/report;")) < 1)
	    && ((len = strStartsWith_i(content_type, "multipart/")) < 1))
		return NULL;
	content_type += len;
	/* A nested part's Content-Type is parsed out of the message body rather than
	 * from a header field of its own, so it is only terminated by the end of the
	 * (possibly folded) header line. */
	const char* ct_end = content_type;
	while ((ct_end = strstr(ct_end, "\r\n")) != NULL) {
		if (ct_end[2] != ' ' && ct_end[2] != '\t')  /* not a folded continuation */
			break;
		ct_end += 2;
	}
	if (ct_end == NULL)
		ct_end = content_type + strlen(content_type);
	p = strcasestr(content_type, "boundary=");
	if (p == NULL || p >= ct_end)
		return NULL;
	p += 9;
	while (*p == ' ' || *p == '\t')  /* RFC822 white space is allowed around the '=' */
		p++;
	if (*p == '"') {
		p++;
		quoted_boundary = true;
	}
	SAFEPRINTF(boundary, "--%s", p);
	p = boundary + 2;
	FIND_CHARSET(p, quoted_boundary ? "\"\r\n" : "; \t\r\n");
	*p = 0;
	truncsp(boundary); // RFC2046: "NOT ending with white space"
	if (boundary[2] == '\0')  /* An empty boundary would match every delimiter */
		return NULL;
	txt = buf;
	while ((p = (char*)strstr(txt, boundary)) != NULL) {
		txt = p + strlen(boundary);
		if (strncmp(txt, "--\r\n", 4) == 0)
			break;
		SKIP_WHITESPACE(txt);
		p = (char*)strstr(txt, "\r\n\r\n");    /* End of header */
		if (p == NULL)
			continue;
		for (content_type = txt; content_type < p; content_type++) {
			SKIP_WHITESPACE(content_type);
			if ((len = strStartsWith_i(content_type, "Content-Type:")) > 0) {
				content_type += len;
				SKIP_WHITESPACE(content_type);
				break;
			}
			FIND_CHAR(content_type, '\r');
		}
		if (content_type >= p)
			continue;
		const char* cp;
		if ((match_len && strnicmp(content_type, match1, match_len) && strnicmp(content_type, match2, match_len))
		    || (attachment != NULL && !mime_getattachment(txt, p, attachment, attachment_len))) {
			if ((cp = mime_getpart(p, content_type, content_match, depth + 1, encoding, charset, auxattr, attachment, attachment_len, index, found)) != NULL)
				return cp;
			continue;
		}
		if ((*found)++ != index) {
			if ((cp = mime_getpart(p, content_type, content_match, depth + 1, encoding, charset, auxattr, attachment, attachment_len, index, found)) != NULL)
				return cp;
			continue;
		}
		if (encoding != NULL)
			*encoding = mime_getxferencoding(txt, p);
		if (charset != NULL || auxattr != NULL)
			smb_parse_content_type(content_type, NULL, charset, auxattr);

		txt = p + 4;    // strlen("\r\n\r\n")
		SKIP_WHITESPACE(txt);
		if ((p = (char*)strstr(txt, boundary)) != NULL)
			*p = 0;
		return txt;
	}
	return NULL;
}

/* Get just the (first) plain-text or HTML portion of a MIME-encoded multi-part message body */
/* Returns NULL if there is no MIME-encoded plain-text/html portion of the message */
char* smb_getplaintext(smbmsg_t* msg, char* buf)
{
	size_t                         len;
	const char*                    txt;
	enum content_transfer_encoding xfer_encoding = CONTENT_TRANFER_ENCODING_NONE;

	if (msg->mime_version == NULL || msg->content_type == NULL)  /* not MIME */
		return NULL;
	if (strStartsWith_i(msg->content_type, "multipart/") > 0) {
		int found = 0;
		txt = mime_getpart(buf, msg->content_type, "text/plain", 0, &xfer_encoding, &msg->text_charset, &msg->hdr.auxattr
		                   , /* attachment: */ NULL, /* attachment_len: */ 0, /* index: */ 0, &found);
		if (txt == NULL) {
			found = 0;
			txt = mime_getpart(buf, msg->content_type, "text/html", 0, &xfer_encoding, &msg->text_charset, &msg->hdr.auxattr
			                   , /* attachment: */ NULL, /* attachment_len: */ 0, /* index: */ 0, &found);
			if (txt == NULL)
				return NULL;
			free(msg->text_subtype);
			msg->text_subtype = strdup("html");
		} else {
			free(msg->text_subtype);
			msg->text_subtype = strdup("plain");
		}

		len = strlen(txt);
		memmove(buf, txt, len + 1);
	} else {
		/* Single-part MIME */
		if (strStartsWith_i(msg->content_type, "text/") < 1) {
			*buf = '\0';
			return buf;
		}
		xfer_encoding = mime_encoding(msg->content_encoding);
		len = strlen(buf);
	}
	if (len == 0)    /* No decoding necessary */
		return buf;
	if (xfer_encoding == CONTENT_TRANFER_ENCODING_QUOTED_PRINTABLE)
		qp_decode(buf);
	else if (xfer_encoding == CONTENT_TRANFER_ENCODING_BASE64) {
		char* decoded = strdup(buf);
		if (decoded == NULL)
			return NULL;
		if (b64_decode(decoded, len, buf, len) > 0)
			strcpy(buf, decoded);
		free(decoded);
	}
	return buf;
}

/* Get an attachment (just one) from single-part or multi-part MIME-encoded message body */
/* This function can be destructive (over-write 'buf' with decoded attachment)! */
uint8_t* smb_getattachment(smbmsg_t* msg, char* buf, char* filename, size_t filename_len, uint32_t* filelen, int index)
{
	const char*                    txt;
	enum content_transfer_encoding xfer_encoding = CONTENT_TRANFER_ENCODING_NONE;

	if (msg->mime_version == NULL || msg->content_type == NULL)  /* not MIME */
		return NULL;
	if (strStartsWith_i(msg->content_type, "multipart/") > 0) {
		int found = 0;
		txt = mime_getpart(buf, msg->content_type, /* match-type: */ NULL, 0, &xfer_encoding, /* charset: */ NULL, /* auxattr: */ NULL
		                   , /* attachment: */ filename, filename_len, index, &found);
		if (txt != NULL && *txt && xfer_encoding == CONTENT_TRANFER_ENCODING_BASE64) {
			size_t len = strlen(txt);
			memmove(buf, txt, len + 1);
			int    result = b64_decode(buf, len, buf, len);
			if (result < 1)
				return NULL;
			if (filelen != NULL)
				*filelen = result;
			return (uint8_t*)buf;
		}
		return NULL;    /* No attachment */
	}
	/* Single-part MIME */
	if (index > 0)
		return NULL;
	if (strStartsWith_i(msg->content_type, "text/") > 0)
		return NULL;
	if (filename != NULL) {
		char* fname = strstr(msg->content_type, "name=");
		if (fname == NULL)
			return NULL;
		fname += 5;
		char* term = NULL;
		if (*fname == '"') {
			fname++;
			term = strchr(fname, '"');
		} else
			term = strchr(fname, ';');
		if (term != NULL)
			*term = '\0';
		strncpy(filename, fname, filename_len);
		filename[filename_len - 1] = '\0';
	}
	if (mime_encoding(msg->content_encoding) == CONTENT_TRANFER_ENCODING_BASE64) {
		size_t len = strlen(buf);
		int    result = b64_decode(buf, len, buf, len);
		if (result < 1)
			return NULL;
		if (filelen != NULL)
			*filelen = result;
	} else {
		if (filelen != NULL)
			*filelen = strlen(buf);
	}
	return (uint8_t*)buf;
}

/* Return number of file attachments contained in MIME-encoded message body */
/* 'body' may be NULL if the body text is not already read/available */
uint smb_countattachments(smb_t* smb, smbmsg_t* msg, const char* body)
{
	if (msg->mime_version == NULL || msg->content_type == NULL)  /* not MIME */
		return 0;

	uint  count = 0;
	char* buf;

	if (body == NULL)
		buf = smb_getmsgtxt(smb, msg, GETMSGTXT_ALL);
	else
		buf = strdup(body);

	if (buf == NULL)
		return 0;

	char* tmp;
	while ((tmp = strdup(buf)) != NULL) {
		char     filename[MAX_PATH + 1];
		uint8_t* attachment = smb_getattachment(msg, tmp, filename, sizeof(filename), NULL, count);
		free(tmp);
		if (attachment == NULL)
			break;
		count++;
	}

	free(buf);
	return count;
}

/* Enumeration of the leaf parts of a (possibly nested) MIME body, for a
 * client that lists and saves them one at a time.
 * Every scan is bounded by the end of the range given, rather than by a NUL. */

struct mime_walk {
	smb_mimepart_t* part;
	size_t count;
	size_t alloc;
	bool failed;            /* memory allocation failure */
};

static const char* mime_nextline(const char* p, const char* end)
{
	const char* eol = memchr(p, '\n', end - p);

	return (eol == NULL) ? end : eol + 1;
}

/* Return a malloc'd copy of the (unfolded, trimmed) value of header field
 * 'name' in the header block [hdr, end), or NULL if there is none */
static char* mime_hdrfield(const char* hdr, const char* end, const char* name)
{
	size_t      namelen = strlen(name);
	const char* p;

	for (p = hdr; p < end; p = mime_nextline(p, end)) {
		if ((size_t)(end - p) <= namelen || strnicmp(p, name, namelen) != 0 || p[namelen] != ':')
			continue;
		const char* val = p + namelen + 1;
		const char* fend = mime_nextline(p, end);
		while (fend < end && (*fend == ' ' || *fend == '\t'))  /* folded continuation line */
			fend = mime_nextline(fend, end);
		char*       buf = (char*)malloc(fend - val + 1);
		if (buf == NULL)
			return NULL;
		size_t      len = 0;
		while (val < fend && (*val == ' ' || *val == '\t'))
			val++;
		for (; val < fend; val++) {
			if (*val != '\r' && *val != '\n')
				buf[len++] = *val;
		}
		buf[len] = '\0';
		truncsp(buf);
		return buf;
	}
	return NULL;
}

/* Copy the value of a header field up to its parameters, trimmed and
 * lowercased (e.g. a media type or a disposition type) */
static void mime_token(const char* value, char* out, size_t outlen)
{
	size_t len = 0;

	while (*value == ' ' || *value == '\t')
		value++;
	for (; *value != '\0' && *value != ';' && len + 1 < outlen; value++)
		out[len++] = (char)tolower((uchar) * value);
	out[len] = '\0';
	truncsp(out);
}

/* Copy the value of parameter 'attr' of a header field value into 'out' */
static bool mime_param(const char* value, const char* attr, char* out, size_t outlen)
{
	size_t      attrlen = strlen(attr);
	const char* p = strchr(value, ';');

	*out = '\0';
	while (p != NULL && *p != '\0') {
		p++;    /* skip the ';' */
		while (*p == ' ' || *p == '\t')
			p++;
		const char* name = p;
		while (*p != '\0' && *p != '=' && *p != ';')
			p++;
		const char* name_end = p;
		while (name_end > name && (name_end[-1] == ' ' || name_end[-1] == '\t'))
			name_end--;
		if (*p != '=')
			continue;
		bool   match = (size_t)(name_end - name) == attrlen && strnicmp(name, attr, attrlen) == 0;
		size_t len = 0;
		p++;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '"') {
			for (p++; *p != '\0' && *p != '"'; p++) {
				if (*p == '\\' && p[1] != '\0')
					p++;
				if (match && len + 1 < outlen)
					out[len++] = *p;
			}
		} else {
			for (; *p != '\0' && *p != ';' && *p != ' ' && *p != '\t'; p++) {
				if (match && len + 1 < outlen)
					out[len++] = *p;
			}
		}
		if (match) {
			out[len] = '\0';
			return true;
		}
		p = strchr(p, ';');
	}
	return false;
}

static void mime_lowercase(char* str)
{
	for (; *str != '\0'; str++)
		*str = (char)tolower((uchar) * str);
}

static bool mime_addpart(struct mime_walk* w, const smb_mimepart_t* part)
{
	if (w->count >= w->alloc) {
		size_t          alloc = w->alloc ? w->alloc * 2 : 8;
		smb_mimepart_t* np = (smb_mimepart_t*)realloc(w->part, alloc * sizeof(*np));
		if (np == NULL) {
			w->failed = true;
			return false;
		}
		w->part = np;
		w->alloc = alloc;
	}
	w->part[w->count] = *part;
	w->part[w->count].decoded_length = smb_decodemimepart(part, NULL);
	w->count++;
	return true;
}

/* Find the next delimiter line ("--boundary") at or after 'p', which must be
 * at the start of a line: only a whole boundary is a delimiter */
static const char* mime_delimiter(const char* p, const char* end, const char* delim, size_t len)
{
	for (; p < end; p = mime_nextline(p, end)) {
		if ((size_t)(end - p) < len || memcmp(p, delim, len) != 0)
			continue;
		const char* after = p + len;
		if (after >= end || *after == '\r' || *after == '\n' || *after == ' ' || *after == '\t'
		    || (*after == '-' && after + 1 < end && after[1] == '-'))
			return p;
	}
	return NULL;
}

/* Split an entity [p, end) at the empty line that ends its header block;
 * without an empty line, it is all header (RFC2046) */
static void mime_split(const char* p, const char* end, const char** hdr_end, const char** body)
{
	*hdr_end = end;
	*body = end;
	for (; p < end; p = mime_nextline(p, end)) {
		if (*p == '\n') {
			*hdr_end = p;
			*body = p + 1;
			return;
		}
		if (*p == '\r' && p + 1 < end && p[1] == '\n') {
			*hdr_end = p;
			*body = p + 2;
			return;
		}
	}
}

static void mime_walk_entity(struct mime_walk*, const char* hdr, const char* hdr_end
                             , const char* body, const char* body_end, const char* id, int depth);

static void mime_walk_multipart(struct mime_walk* w, const char* body, const char* end
                                , const char* boundary, const char* id, int depth)
{
	char        delim[2 + 256];   /* "--" + boundary */
	char        child[sizeof(w->part->id)];
	size_t      dlen = safe_snprintf(delim, sizeof(delim), "--%s", boundary);
	const char* p = mime_delimiter(body, end, delim, dlen);
	size_t      idlen = strlen(id);
	int         n = 0;

	while (p != NULL && !w->failed && w->count < SMB_MIMEPART_MAX_PARTS) {
		const char* after = p + dlen;
		if (end - after >= 2 && after[0] == '-' && after[1] == '-')   /* close delimiter */
			break;
		const char* content = mime_nextline(after, end);
		const char* next = mime_delimiter(content, end, delim, dlen);
		const char* content_end = end;
		if (next != NULL) {   /* the line break before a delimiter is part of it */
			content_end = next;
			if (content_end > content && content_end[-1] == '\n')
				content_end--;
			if (content_end > content && content_end[-1] == '\r')
				content_end--;
		}
		const char* hdr_end;
		const char* child_body;
		mime_split(content, content_end, &hdr_end, &child_body);
		/* A truncated id could name two parts: stop where the next one would not fit */
		size_t      digits = 1;
		n++;
		for (int i = n; i >= 10; i /= 10)
			digits++;
		if (idlen + (idlen > 0) + digits + 1 > sizeof(child))   /* parent "." index NUL */
			break;
		if (idlen == 0)
			safe_snprintf(child, sizeof(child), "%d", n);
		else
			safe_snprintf(child, sizeof(child), "%s.%d", id, n);
		mime_walk_entity(w, content, hdr_end, child_body, content_end, child, depth + 1);
		p = next;
	}
}

/* Add the leaf parts of an entity: a multipart's parts, or the entity itself.
 * 'id' is empty for the message itself. */
static void mime_walk_entity(struct mime_walk* w, const char* hdr, const char* hdr_end
                             , const char* body, const char* body_end, const char* id, int depth)
{
	smb_mimepart_t part;
	char           boundary[257];   /* one more than the longest boundary used */
	char*          ctype;
	char*          field;

	memset(&part, 0, sizeof(part));
	ctype = mime_hdrfield(hdr, hdr_end, "Content-Type");
	if (ctype != NULL) {
		mime_token(ctype, part.content_type, sizeof(part.content_type));
		if (strchr(part.content_type, '/') == NULL)
			part.content_type[0] = '\0';
	}
	if (part.content_type[0] == '\0')
		SAFECOPY(part.content_type, "text/plain");
	if (strncmp(part.content_type, "multipart/", 10) == 0) {
		if (depth < SMB_MIMEPART_MAX_DEPTH && ctype != NULL
		    && mime_param(ctype, "boundary", boundary, sizeof(boundary))
		    && *truncsp(boundary) != '\0' && strlen(boundary) < sizeof(boundary) - 1) {
			size_t count = w->count;
			mime_walk_multipart(w, body, body_end, boundary, id, depth);
			/* The message itself falls back to text/plain (smb_getmimeparts) */
			if (w->count != count || w->failed || w->count >= SMB_MIMEPART_MAX_PARTS || *id == '\0') {
				free(ctype);
				return;
			}
		}
		/* too deep, or not parsable, or no parts found: one part, as is */
		SAFECOPY(part.content_type, "application/octet-stream");
	} else {
		if (ctype != NULL) {
			mime_param(ctype, "charset", part.charset, sizeof(part.charset));
			mime_lowercase(part.charset);
			mime_param(ctype, "name", part.name, sizeof(part.name));
		}
		if ((field = mime_hdrfield(hdr, hdr_end, "Content-Transfer-Encoding")) != NULL) {
			part.encoding = mime_encoding(field);
			free(field);
		}
		if ((field = mime_hdrfield(hdr, hdr_end, "Content-Disposition")) != NULL) {
			char name[sizeof(part.name)];
			mime_token(field, part.disposition, sizeof(part.disposition));
			if (strcmp(part.disposition, "inline") != 0 && strcmp(part.disposition, "attachment") != 0)
				part.disposition[0] = '\0';
			if (mime_param(field, "filename", name, sizeof(name)) && *name != '\0')
				SAFECOPY(part.name, name);
			free(field);
		}
		memmove(part.name, getfname(part.name), strlen(getfname(part.name)) + 1);
		if ((field = mime_hdrfield(hdr, hdr_end, "Content-ID")) != NULL) {
			char* cid = field;
			if (*cid == '<')
				cid++;
			SAFECOPY(part.content_id, cid);
			char* last = lastchar(part.content_id);
			if (*last == '>')
				*last = '\0';
			free(field);
		}
	}
	free(ctype);
	SAFECOPY(part.id, *id == '\0' ? "1" : id);
	part.data = body;
	part.length = body_end - body;
	mime_addpart(w, &part);
}

/* The character set of a message that is not MIME-encoded */
static void mime_msgcharset(const smbmsg_t* msg, char* out, size_t outlen)
{
	size_t len = 0;

	if (smb_msg_is_utf8(msg)) {
		strlcpy(out, "utf-8", outlen);
		return;
	}
	if (msg->ftn_charset != NULL) {   /* e.g. "CP437 2" */
		const char* p = msg->ftn_charset;
		SKIP_WHITESPACE(p);
		for (; *p != '\0' && !IS_WHITESPACE(*p) && len + 1 < outlen; p++)
			out[len++] = (char)tolower((uchar) * p);
		out[len] = '\0';
		if (len > 0)
			return;
	}
	switch (msg->from_net.type) {
		case NET_NONE:
		case NET_FIDO:
		case NET_QWK:
			strlcpy(out, "cp437", outlen);
			break;
		default:
			strlcpy(out, "windows-1252", outlen);
			break;
	}
}

/* List the leaf parts of a message body (as read by smb_getmsgtxt() with
 * GETMSGTXT_BODY_ONLY), in document order: a message that is not MIME-encoded
 * is one text/plain part. Returns a malloc'd array of *count parts (that
 * point into 'body') to be freed with smb_freemimeparts(), or NULL on memory
 * allocation failure. */
smb_mimepart_t* smb_getmimeparts(const smbmsg_t* msg, const char* body, size_t* count)
{
	struct mime_walk w;
	smb_mimepart_t   part;
	size_t           len = strlen(body);

	memset(&w, 0, sizeof(w));
	*count = 0;
	if (msg->mime_version != NULL && msg->content_type != NULL) {
		/* The message's own MIME header fields, as a header block */
		size_t hlen = 0;
		char*  hdr;
		for (int i = 0; i < msg->total_hfields; i++) {
			if (msg->hfield[i].type == RFC822HEADER)
				hlen += strlen((char*)msg->hfield_dat[i]) + 2;
		}
		if ((hdr = (char*)malloc(hlen + 1)) == NULL)
			return NULL;
		hlen = 0;
		for (int i = 0; i < msg->total_hfields; i++) {
			if (msg->hfield[i].type == RFC822HEADER)
				hlen += sprintf(hdr + hlen, "%s\r\n", (char*)msg->hfield_dat[i]);
		}
		mime_walk_entity(&w, hdr, hdr + hlen, body, body + len, /* id: */ "", /* depth: */ 0);
		free(hdr);
	}
	if (w.count == 0 && !w.failed) {  /* not MIME-encoded, or no parts found */
		memset(&part, 0, sizeof(part));
		SAFECOPY(part.id, "1");
		SAFECOPY(part.content_type, "text/plain");
		if (msg->mime_version == NULL || msg->content_type == NULL)
			mime_msgcharset(msg, part.charset, sizeof(part.charset));
		part.data = body;
		part.length = len;
		mime_addpart(&w, &part);
	}
	if (w.failed) {
		free(w.part);
		return NULL;
	}
	*count = w.count;
	return w.part;
}

static int mime_b64_value(uchar ch)
{
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A';
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 26;
	if (ch >= '0' && ch <= '9')
		return ch - '0' + 52;
	if (ch == '+')
		return 62;
	if (ch == '/')
		return 63;
	return -1;
}

/* Characters outside the base64 alphabet are ignored (RFC2045) */
static size_t mime_b64_decode(const char* p, size_t len, uint8_t* out)
{
	size_t   n = 0;
	uint32_t bits = 0;
	int      nbits = 0;

	for (size_t i = 0; i < len; i++) {
		int val = mime_b64_value((uchar)p[i]);
		if (val < 0) {
			if (p[i] == '=')  /* padding: the end of the data */
				break;
			continue;
		}
		bits = (bits << 6) | (uint32_t)val;
		nbits += 6;
		if (nbits >= 8) {
			nbits -= 8;
			if (out != NULL)
				out[n] = (uint8_t)(bits >> nbits);
			n++;
			bits &= (1U << nbits) - 1;
		}
	}
	return n;
}

static bool mime_eol(const char* p, const char* end)
{
	return p >= end || *p == '\n' || (*p == '\r' && p + 1 < end && p[1] == '\n');
}

/* Unlike qp_decode(), keeps every byte value: for attachments as well as text */
static size_t mime_qp_decode(const char* p, size_t len, uint8_t* out)
{
	const char* end = p + len;
	const char* q;
	size_t      n = 0;

	while (p < end) {
		if (*p == '=') {
			if (end - p >= 3 && IS_HEXDIGIT(p[1]) && IS_HEXDIGIT(p[2])) {
				if (out != NULL)
					out[n] = (uint8_t)((HEX_CHAR_TO_INT(p[1]) << 4) | HEX_CHAR_TO_INT(p[2]));
				n++;
				p += 3;
				continue;
			}
			for (q = p + 1; q < end && (*q == ' ' || *q == '\t'); q++)
				;
			if (mime_eol(q, end)) {  /* soft line break */
				p = (q >= end) ? end : mime_nextline(q, end);
				continue;
			}
		} else if (*p == ' ' || *p == '\t') {
			for (q = p; q < end && (*q == ' ' || *q == '\t'); q++)
				;
			if (mime_eol(q, end)) {  /* trailing white space is not data */
				p = q;
				continue;
			}
			for (; p < q; p++) {
				if (out != NULL)
					out[n] = (uint8_t)*p;
				n++;
			}
			continue;
		}
		if (out != NULL)
			out[n] = (uint8_t)*p;
		n++;
		p++;
	}
	return n;
}

void smb_freemimeparts(smb_mimepart_t* parts)
{
	free(parts);
}

/* Decode a part into 'out' (part->decoded_length bytes), or if 'out' is NULL,
 * just return the decoded length */
size_t smb_decodemimepart(const smb_mimepart_t* part, uint8_t* out)
{
	switch (part->encoding) {
		case CONTENT_TRANFER_ENCODING_BASE64:
			return mime_b64_decode(part->data, part->length, out);
		case CONTENT_TRANFER_ENCODING_QUOTED_PRINTABLE:
			return mime_qp_decode(part->data, part->length, out);
		default:
			if (out != NULL)
				memcpy(out, part->data, part->length);
			return part->length;
	}
}
