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
		fseek(smb->sdt_fp, msg->hdr.offset + msg->dfield[i].offset
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
		if (strnicmp(p, "inline", 6) == 0) {
			FIND_CHAR(p, '\n');
			continue;
		}
		const char* filename = strstr(p, "filename=");
		if (filename == NULL) {
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
		     || (format = strcasestr(p, ";foramt=")) != NULL
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
static const char* mime_getpart(const char* buf, const char* content_type, const char* content_match
                                , int depth, enum content_transfer_encoding* encoding, char** charset, char* attachment, size_t attachment_len, int index)
{
	const char* txt;
	char*       p;
	char        boundary[256];
	char        match1[128];
	char        match2[128];
	int         match_len = 0;
	int         found = 0;
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
	p = strcasestr(content_type, "boundary=");
	if (p == NULL)
		return NULL;
	p += 9;
	if (*p == '"') {
		p++;
		quoted_boundary = true;
	}
	SAFEPRINTF(boundary, "--%s", p);
	if ((p = strchr(boundary, quoted_boundary ? '"' : ';')) != NULL)
		*p = 0;
	truncsp(boundary); // RC2046: "NOT ending with white space"
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
			if ((cp = mime_getpart(p, content_type, content_match, depth + 1, encoding, charset, attachment, attachment_len, index)) != NULL)
				return cp;
			continue;
		}
		if (found++ != index) {
			if ((cp = mime_getpart(p, content_type, content_match, depth + 1, encoding, charset, attachment, attachment_len, index)) != NULL)
				return cp;
			continue;
		}
		if (encoding != NULL)
			*encoding = mime_getxferencoding(txt, p);
		if (charset != NULL)
			smb_parse_content_type(content_type, NULL, charset, NULL);

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
		txt = mime_getpart(buf, msg->content_type, "text/plain", 0, &xfer_encoding, &msg->text_charset
		                   , /* attachment: */ NULL, /* attachment_len: */ 0, /* index: */ 0);
		if (txt == NULL) {
			txt = mime_getpart(buf, msg->content_type, "text/html", 0, &xfer_encoding, &msg->text_charset
			                   , /* attachment: */ NULL, /* attachment_len: */ 0, /* index: */ 0);
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
		txt = mime_getpart(buf, msg->content_type, /* match-type: */ NULL, 0, &xfer_encoding, /* charset: */ NULL
		                   , /* attachment: */ filename, filename_len, index);
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
