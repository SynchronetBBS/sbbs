/* $Id: wordwrap.c,v 1.51 2019/07/11 03:08:49 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <ctype.h>
#include <genwrap.h>
#include <stdlib.h>		/* realloc */
#include "wordwrap.h"
#include "utf8.h"

struct prefix {
	size_t cols;
	char *bytes;
};

enum prefix_pos {
	PREFIX_START,
	PREFIX_FIRST,
	PREFIX_SECOND,
	PREFIX_THIRD,
	PREFIX_END,
	PREFIX_PAD,
	PREFIX_FINISHED
};

/*
 * Parses a prefix from the passed text, returns a struct containing
 * an allocated bytes pointer and the number of columns the prefix
 * takes up in the output.
 */
static struct prefix parse_prefix(const char *text)
{
	enum prefix_pos expect = PREFIX_START;
	const char *pos = text;
	const char *end = text;
	struct prefix ret = {0, NULL};
	int cols = 0;

	// Quote may begin the line or have a space before it.
	if (text[0] != ' ')
		expect = PREFIX_FIRST;
	for (;*pos && expect != PREFIX_FINISHED; pos++) {
		// Skip CTRL-A Codes
		while(*pos == '\x01') {
			pos++;
			if (*pos != 0) {
				pos++;
				continue;
			}
		}
		/*
		 * If end of line or message, or obviously outside of prefixes, exit loop.
		 * However, if we're in the "pad" section, don't abort since we have a 
		 * legal prefix.
		 */
		if(expect != PREFIX_PAD) {
			if (*pos == 0 || *pos == '\n' || *pos == '\r' || *pos == '\t')
				break;
		}
		cols++;
		switch(expect) {
			// If there's no space before the quote mark, it's not a prefix.
			case PREFIX_START:
				if (*pos == ' ')
					expect = PREFIX_FIRST;
				else
					expect = PREFIX_FINISHED;
				break;
			// Next char should be alphanum or >
			case PREFIX_FIRST:
			case PREFIX_SECOND:
			case PREFIX_THIRD:
				if(*pos == ' ')
					expect = PREFIX_FINISHED;
				else
					if(*pos == '>')
						expect = PREFIX_PAD;
					else
						expect++;
				break;
			// Next char must be >
			case PREFIX_END:
				if (*pos == '>')
					expect = PREFIX_PAD;
				else
					expect = PREFIX_FINISHED;
				break;
			// Must be a space after the '>'
			case PREFIX_PAD:
				if (*pos == ' ' || *pos == '\r' || *pos == '\n' || *pos == '\t') {
					ret.cols = cols;
					if (*pos == ' ')
						end = pos+1;
					else
						end = pos;
					if (*(pos+1) == ' ')
						expect = PREFIX_START;
					else
						expect = PREFIX_FIRST;
				}
				else
					expect = PREFIX_FINISHED;
				break;
			default:
				expect = PREFIX_FINISHED;
				break;
		}
	}
	if (end > text) {
		ret.bytes = (char *)malloc((end-text)+2);
		memcpy(ret.bytes, text, (end-text));
		if (ret.bytes[end-text-1] != ' ') {
			ret.bytes[end-text] = ' ';
			ret.bytes[end-text+1] = 0;
		}
		else
			ret.bytes[end-text] = 0;
	}
	else {
		ret.bytes = (char *)malloc(1);
		ret.bytes[0] = 0;
	}
	return ret;
}

int cmp_prefix(struct prefix *p1, struct prefix *p2)
{
	if (p1->cols != p2->cols)
		return p1->cols-p2->cols;
	return strcmp(p1->bytes ? p1->bytes : "", p2->bytes ? p2->bytes : "");
}

/*
 * Appends to a malloc()ed buffer, realloc()ing if needed.
 */
static void outbuf_append(char **outbuf, char **outp, char *append, int len, int *outlen)
{
	char	*p;

	/* Terminate outbuf */
	**outp=0;
	/* Check if there's room */
	if(*outp - *outbuf + len < *outlen) {
		memcpy(*outp, append, len);
		*outp+=len;
		return;
	}
	/* Not enough room, double the size. */
	*outlen *= 2;
	if(*outp - *outbuf + len >= *outlen) 
		*outlen = *outp - *outbuf + len + 1;
	p=realloc(*outbuf, *outlen);
	if(p==NULL) {
		/* Can't do it. */
		*outlen/=2;
		return;
	}
	/* Set outp for new buffer */
	*outp=p+(*outp - *outbuf);
	*outbuf=p;
	memcpy(*outp, append, len);
	*outp+=len;
	return;
}

/*
 * Holds the length of a "section"... either a word or whitespace.
 * Length is in bytes and "len" (the number of columns)
 */
struct section_len {
	size_t bytes;
	size_t len;
};

/*
 * Gets the length of a run of whitespace starting at the beginning
 * of buf, which occurs in column col.
 * 
 * The column is needed for tab size calculations.
 */
static struct section_len get_ws_len(char *buf, int col)
{
	struct section_len ret = {0,0};

	for(ret.bytes=0; ; ret.bytes++) {
		if (!buf[ret.bytes])
			break;
		if (!isspace((unsigned char)buf[ret.bytes]))
			break;
		if(buf[ret.bytes] == '\t') {
			ret.len++;
			while((ret.len+col)%8)
				ret.len++;
			ret.len--;
		}
		ret.len++;
	}

	return ret;
}

/*
 * Gets the length of a word, optionally limiting the max number
 * of columns to consume to maxlen.
 * 
 * When maxlen < 0, returns the word length in cols and bytes.
 * When maxlen >= 0, returns the number of cols and bytes up to
 * maxlen cols (used to find the number of bytes to fill a specific
 * number of columns).
 */
static struct section_len get_word_len(char *buf, int maxlen, BOOL is_utf8)
{
	struct section_len ret = {0,0};

	int len;
	for(ret.bytes=0; ;ret.bytes += len) {
		len = 1;
		if (!buf[ret.bytes])
			break;
		else if (isspace((unsigned char)buf[ret.bytes]))
			break;
		else if (buf[ret.bytes]==DEL)
			continue;
		else if (buf[ret.bytes]=='\x01') {
			ret.bytes++;
			if (buf[ret.bytes] == '\\')
				break;
			continue;
		}
		else if (buf[ret.bytes]=='\b') {
			// This doesn't handle BS the same way... bit it's kinda BS anyway.
			ret.len--;
			continue;
		}
		if (maxlen > 0 && ret.len >= (size_t)maxlen)
			break;
		if(is_utf8 && (buf[ret.bytes]&0x80)) {
			len = utf8_getc(buf + ret.bytes, strlen(buf + ret.bytes), NULL);
			if(len < 1)
				len = 1;
		}
		ret.len++;
	}

	return ret;
}

/*
 * This structure holds a "paragraph" defined as everything from either
 * the beginning of the message, or the previous hard CR to the next
 * hard CR or end of message
 */
struct paragraph {
	struct prefix prefix;
	char *text;
	size_t alloc_size;
	size_t len;
};

/*
 * Free()s the allocations in an array of paragraphs.  If count is
 * provided, that many paragraphs are freed.  If count == -1, frees
 * up to the first paragraph with a NULL text member.
 */
static void free_paragraphs(struct paragraph *paragraph, int count)
{
	int i;

	for(i=0; count == -1 || i<count ;i++) {
		FREE_AND_NULL(paragraph[i].prefix.bytes);
		if (count == -1 && paragraph[i].text == NULL)
			break;
		FREE_AND_NULL(paragraph[i].text);
	}
}

/*
 * Appends bytes to a paragraph, realloc()ing space if needed.
 */
static BOOL paragraph_append(struct paragraph *paragraph, const char *bytes, size_t count)
{
	char *new_text;

	while (paragraph->len + count + 1 > paragraph->alloc_size) {
		new_text = realloc(paragraph->text, paragraph->alloc_size * 2);
		if (new_text == NULL)
			return FALSE;
		paragraph->text = new_text;
		paragraph->alloc_size *= 2;
	}
	memcpy(paragraph->text + paragraph->len, bytes, count);
	paragraph->text[paragraph->len+count] = 0;
	paragraph->len += count;
	return TRUE;
}

/*
 * This unwraps a message into infinite line length paragraphs.
 * Optionally, each with separate prefix.
 * 
 * The returned malloc()ed array will have the text member of the last
 * paragraph set to NULL.
 */
static struct paragraph *word_unwrap(char *inbuf, int oldlen, BOOL handle_quotes, BOOL *has_crs, BOOL is_utf8)
{
	unsigned inpos=0;
	struct prefix new_prefix;
	int incol;
	int paragraph = 0;
	struct paragraph *ret = NULL;
	struct paragraph *newret = NULL;
	BOOL paragraph_done;
	int next_word_len;
	size_t new_prefix_len;
	size_t alloc_len = oldlen+1;

	if (alloc_len > 4096)
		alloc_len = 4096;
	if(has_crs)
		*has_crs = FALSE;
	while(inbuf[inpos]) {
		incol = 0;
		/* Start of a new paragraph (ie: after a hard CR) */
		newret = realloc(ret, (paragraph+1) * sizeof(struct paragraph));
		if (newret == NULL) {
			free_paragraphs(ret, paragraph);
			return NULL;
		}
		ret = newret;
		ret[paragraph].text = (char *)malloc(alloc_len);
		ret[paragraph].len = 0;
		ret[paragraph].prefix.bytes = NULL;
		if (ret[paragraph].text == NULL) {
			free_paragraphs(ret, paragraph+1);
			return NULL;
		}
		ret[paragraph].alloc_size = alloc_len;
		ret[paragraph].text[0] = 0;
		if (handle_quotes) {
			ret[paragraph].prefix = parse_prefix(inbuf+inpos);
			inpos += strlen(ret[paragraph].prefix.bytes);
			incol = ret[paragraph].prefix.cols;
		}
		else
			memset(&ret[paragraph].prefix, 0, sizeof(ret[paragraph].prefix));
		paragraph_done = FALSE;
		while(!paragraph_done) {
			switch(inbuf[inpos]) {
				case '\r':		// Strip CRs and add them in later.
					if (has_crs)
						*has_crs = TRUE;
					// Fall-through to strip
				case '\b':		// Strip backspaces.
				case DEL:	// Strip delete chars.
					break;
				case '\x01':	// CTRL-A code.
					if (!paragraph_append(&ret[paragraph], inbuf+inpos, 2))
						goto fail_return;
					inpos++;
					break;
				case '\n':		// End of line... figure out if it's soft or hard...
					// First, check if we're at the end...
					if (inbuf[inpos+1] == 0)
						break;
					// If the original line was overly long, it's hard.
					if (incol > oldlen) {
						paragraph_done = TRUE;
						break;
					}
					// Now, if the prefix changes, it's hard.
					if (handle_quotes) {
						new_prefix = parse_prefix(&inbuf[inpos+1]);
						new_prefix_len = strlen(new_prefix.bytes);
					}
					else {
						memset(&new_prefix, 0, sizeof(new_prefix));
						new_prefix_len = 0;
					}
					if (cmp_prefix(&new_prefix, &ret[paragraph].prefix) != 0) {
						paragraph_done = TRUE;
						FREE_AND_NULL(new_prefix.bytes);
						break;
					}
					// If the next line start with whitespace, it's hard
					switch(inbuf[inpos+1+new_prefix_len]) {
						case 0:
						case ' ':
						case '\t':
						case '\r':
						case '\n':
							FREE_AND_NULL(new_prefix.bytes);
							paragraph_done = TRUE;
							break;
					}
					if (paragraph_done) {
						FREE_AND_NULL(new_prefix.bytes);
						paragraph_done = TRUE;
						break;
					}
					// If this paragraph was only whitespace, it's hard.
					if(strspn(ret[paragraph].text, " \t\r") == strlen(ret[paragraph].text)) {
						FREE_AND_NULL(new_prefix.bytes);
						paragraph_done = TRUE;
						break;
					}

					// If the first word on the next line would have fit here, it's hard
					next_word_len = get_word_len(inbuf+inpos+1+new_prefix_len, -1, is_utf8).len;
					if ((incol + next_word_len + 1 - 1) < oldlen) {
						FREE_AND_NULL(new_prefix.bytes);
						paragraph_done = TRUE;
						break;
					}
					// Skip the new prefix...
					inpos += new_prefix_len;
					incol = new_prefix.cols;
					FREE_AND_NULL(new_prefix.bytes);
					if (!paragraph_append(&ret[paragraph], " ", 1))
						goto fail_return;
					break;
				case '\t':		// Tab... bah.
					if (!paragraph_append(&ret[paragraph], inbuf+inpos, 1))
						goto fail_return;
					incol++;
					while(incol%8)
						incol++;
					break;
				default:
					if (!paragraph_append(&ret[paragraph], inbuf+inpos, 1))
						goto fail_return;
					incol++;
					break;
			}
			inpos++;
			if (inbuf[inpos] == 0)
				paragraph_done = TRUE;
		}
		paragraph++;
	}

	newret = realloc(ret, (paragraph+1) * sizeof(struct paragraph));
	if (newret == NULL) {
		free_paragraphs(ret, paragraph);
		return NULL;
	}
	ret = newret;
	memset(&ret[paragraph], 0, sizeof(ret[0]));

	return ret;

fail_return:
	free_paragraphs(ret, paragraph+1);
	return NULL;
}

/*
 * Wraps a set of infinite line length paragraphs to the specified length
 * optionally prepending the prefixes.
 * 
 * Returns a malloc()ed string.
 */
static char *wrap_paragraphs(struct paragraph *paragraph, size_t outlen, BOOL handle_quotes, BOOL has_crs, BOOL is_utf8)
{
	int outcol;
	char *outbuf = NULL;
	char *outp = NULL;
	int outbuf_size = outlen;
	char *prefix_copy;
	size_t prefix_cols;
	size_t prefix_bytes;
	char *inp;
	struct section_len ws_len;
	struct section_len word_len;

	outbuf = (char *)malloc(outbuf_size);
	outp = outbuf;
	while(paragraph->text) {
		if (handle_quotes) {
			if (paragraph->prefix.cols > (outlen / 2)) {
				// Massive prefix... chop it down...
				prefix_copy = paragraph->prefix.bytes + strlen(paragraph->prefix.bytes) - (outlen/2);
				while (*prefix_copy != ' ')
					prefix_copy--;
				word_len = get_word_len(prefix_copy, -1, is_utf8);
				prefix_cols = word_len.len;
				prefix_bytes = word_len.bytes;
			}
			else {
				prefix_copy = paragraph->prefix.bytes;
				prefix_cols = paragraph->prefix.cols;
				prefix_bytes = strlen(prefix_copy);
			}
		}
		else
			prefix_cols = 0;
		inp = paragraph->text;
		if (*inp == 0) {
			if (has_crs)
				outbuf_append(&outbuf, &outp, "\r\n", 2, &outbuf_size);
			else
				outbuf_append(&outbuf, &outp, "\n", 1, &outbuf_size);
		}
		while (*inp) {
			outcol = 0;
			// First, add the prefix...
			if (handle_quotes) {
				outbuf_append(&outbuf, &outp, prefix_copy, prefix_bytes, &outbuf_size);
				outcol = prefix_cols;
			}
			// Now add words until the line is full...
			while(1) {
				if (*inp == 0)
					break;
				ws_len = get_ws_len(inp, outcol);
				word_len = get_word_len(inp+ws_len.bytes, -1, is_utf8);
				// Do we need to chop a long word?
				if (word_len.len > (outlen - prefix_cols))
					word_len = get_word_len(inp + ws_len.bytes, outlen - ws_len.bytes - outcol, is_utf8);
				if (outcol + ws_len.len + word_len.len > outlen) {
					inp += ws_len.bytes;
					break;
				}
				outbuf_append(&outbuf, &outp, inp, ws_len.bytes, &outbuf_size);
				inp += ws_len.bytes;
				outcol += ws_len.len;
				outbuf_append(&outbuf, &outp, inp, word_len.bytes, &outbuf_size);
				inp += word_len.bytes;
				outcol += word_len.len;
			}
			if (has_crs)
				outbuf_append(&outbuf, &outp, "\r\n", 2, &outbuf_size);
			else
				outbuf_append(&outbuf, &outp, "\n", 1, &outbuf_size);
		}
		paragraph++;
	}
	outbuf_append(&outbuf, &outp, "", 1, &outbuf_size);
	return outbuf;
}

char* wordwrap(char* inbuf, int len, int oldlen, BOOL handle_quotes, BOOL is_utf8)
{
	char*		outbuf;
	struct paragraph *paragraphs;
	BOOL		has_crs;

	paragraphs = word_unwrap(inbuf, oldlen, handle_quotes, &has_crs, is_utf8);
	if (paragraphs == NULL)
		return NULL;
#if 0
	for(int i=0;paragraphs[i].text;i++)
		fprintf(stderr, "PREFIX: '%s'\nTEXT: '%s'\n\n", paragraphs[i].prefix.bytes, paragraphs[i].text);
#endif

	outbuf = wrap_paragraphs(paragraphs, len, handle_quotes, has_crs, is_utf8);
	free_paragraphs(paragraphs, -1);
	free(paragraphs);
	return outbuf;
}
