/* Base64 encoding/decoding routines */

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

#include <stdlib.h>
#include <string.h>
#include "base64.h"
#include "gen_defs.h"

static const char * base64alphabet =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

ssize_t b64_decode(char *target, size_t tlen, const char *source, size_t slen)
{
	const char *inp;
	char *      outp;
	char *      outend;
	const char *inend;
	int         bits = 0;
	int         working = 0;
	const char *i;

	if (slen == 0)
		slen = strlen(source);
	outp = target;
	inp = source;
	outend = target + tlen;
	inend = source + slen;
	for (; outp < outend && inp < inend; inp++) {
		if (isspace(*inp))
			continue;
		working <<= 6;
		i = strchr(base64alphabet, (char)*inp);
		if (i == NULL) {
			return -1;
		}
		if (*i == '=')  { /* pad char */
			if ((working & 0xFF) != 0)
				return -1;
			break;
		}
		bits += 6;
		working |= (i - base64alphabet);
		if (bits >= 8) {
			*(outp++) = (char)((working & (0xFF << (bits - 8))) >> (bits - 8));
			bits -= 8;
		}
	}
	if (outp == outend)  {
		*(--outp) = 0;
		return -1;
	}
	*outp = 0;
	return outp - target;
}

static int add_char(char *pos, char ch, int done, char *end)
{
	if (pos >= end)  {
		return 1;
	}
	if (done)
		*pos = base64alphabet[64];
	else
		*pos = base64alphabet[(int)ch];
	return 0;
}

ssize_t b64_encode(char *target, size_t tlen, const char *source, size_t slen)  {
	const char *inp;
	char *      outp;
	char *      outend;
	const char *inend;
	char *      tmpbuf = NULL;
	int         done = 0;
	char        enc;
	int         buf;

	inp = source;
	if (source == target)  {
		tmpbuf = (char *)malloc(tlen);
		if (tmpbuf == NULL)
			return -1;
		outp = tmpbuf;
	}
	else
		outp = target;

	outend = outp + tlen;
	inend = inp + slen;
	for (; (inp < inend) && !done;)  {
		enc = *(inp++);
		buf = (enc & 0x03) << 4;
		enc = (enc & 0xFC) >> 2;
		if (add_char(outp++, enc, done, outend)) {
			FREE_AND_NULL(tmpbuf);
			return -1;
		}
		if (inp >= inend)
			enc = buf;
		else
			enc = buf | ((*inp & 0xF0) >> 4);
		if (add_char(outp++, enc, done, outend)) {
			FREE_AND_NULL(tmpbuf);
			return -1;
		}
		if (inp == inend)
			done = 1;
		if (!done) {
			buf = (*(inp++) << 2) & 0x3C;
			if (inp == inend)
				enc = buf;
			else
				enc = buf | ((*inp & 0xC0) >> 6);
		}
		if (add_char(outp++, enc, done, outend)) {
			FREE_AND_NULL(tmpbuf);
			return -1;
		}
		if (inp == inend)
			done = 1;
		if (!done)
			enc = ((int)*(inp++)) & 0x3F;
		if (add_char(outp++, enc, done, outend)) {
			FREE_AND_NULL(tmpbuf);
			return -1;
		}
		if (inp == inend)
			done = 1;
	}
	if (outp < outend)
		*outp = 0;
	int result;
	if (source == target) {
		memcpy(target, tmpbuf, tlen);
		result = outp - tmpbuf;
		free(tmpbuf);
	} else
		result = outp - target;

	return result;
}

#ifdef BASE64_TEST
int main(int argc, char**argv)
{
	int  i, j;
	char buf[512];

	for (i = 1; i < argc; i++) {
		j = b64_decode(buf, sizeof(buf), argv[i], 0);
		printf("%s (%d)\n", buf, j);
	}

	return 0;
}
#endif
