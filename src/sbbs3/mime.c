/* Synchronet MIME functions, originally written/submitted by Marc Lanctot */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* See RFCs 1521,2045,2046,2047,2048,2049,1590 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "sbbs.h"
#include "mime.h"
#include "mailsrvr.h"
#include "base64.h"

#define SIZEOF_MIMEBOUNDARY     36

char* mimegetboundary()
{
	int   i, num;
	char* boundaryString = (char*)malloc(SIZEOF_MIMEBOUNDARY + 1);

	srand((unsigned int)time(NULL));
	if (boundaryString == NULL)
		return NULL;
	for (i = 0; i < SIZEOF_MIMEBOUNDARY; i++) {
		num = (rand() % 62);
		if (num < 10)
			num += 48;
		else if (num >= 10 && num < 36)
			num += 55;
		else
			num += 61;
		boundaryString[i] = (char)num;
	}
	boundaryString[i] = '\0';
	return boundaryString;
}

void mimeheaders(SOCKET socket, const char* prot, int sess, char* boundary)
{
	sockprintf(socket, prot, sess, "MIME-Version: 1.0");
	sockprintf(socket, prot, sess, "Content-Type: multipart/mixed;");
	sockprintf(socket, prot, sess, " boundary=\"%s\"", boundary);
}

void mimeblurb(SOCKET socket, const char* prot, int sess, char* boundary)
{
	sockprintf(socket, prot, sess, "This is a multi-part message in MIME format.");
	sockprintf(socket, prot, sess, "");
}

void mimetextpartheader(SOCKET socket, const char* prot, int sess, char* boundary
                        , const char* text_subtype, const char* charset)
{
	if (text_subtype == NULL || *text_subtype == '\0')
		text_subtype = "plain";
	sockprintf(socket, prot, sess, "--%s", boundary);
	sockprintf(socket, prot, sess, "Content-Type: text/%s;", text_subtype);
	sockprintf(socket, prot, sess, " charset=\"%s\"", charset);
	sockprintf(socket, prot, sess, "Content-Transfer-Encoding: 8bit");
}

bool base64out(SOCKET socket, const char* prot, int sess, char* pathfile)
{
	FILE * fp;
	char   in[57];
	char   out[77];
	int    bytesread;

	if ((fp = fopen(pathfile, "rb")) == NULL)
		return false;
	while (1) {
		bytesread = fread(in, 1, sizeof(in), fp);
		if ((b64_encode(out, sizeof(out), in, bytesread) == -1)
		    || !sockprintf(socket, prot, sess, "%s", out))  {
			fclose(fp);
			return false;
		}
		if (bytesread != sizeof(in) || feof(fp))
			break;
	}
	fclose(fp);
	sockprintf(socket, prot, sess, "");
	return true;
}

bool mimeattach(SOCKET socket, const char* prot, int sess, char* boundary, char* pathfile)
{
	char* fname = getfname(pathfile);

	sockprintf(socket, prot, sess, "--%s", boundary);
	sockprintf(socket, prot, sess, "Content-Type: application/octet-stream;");
	sockprintf(socket, prot, sess, " name=\"%s\"", fname);
	sockprintf(socket, prot, sess, "Content-Transfer-Encoding: base64");
	sockprintf(socket, prot, sess, "Content-Disposition: attachment;");
	sockprintf(socket, prot, sess, " filename=\"%s\"", fname);
	sockprintf(socket, prot, sess, "");
	if (!base64out(socket, prot, sess, pathfile))
		return false;
	sockprintf(socket, prot, sess, "");
	return true;
}

void endmime(SOCKET socket, const char* prot, int sess, char* boundary)
{
	/* last boundary */
	sockprintf(socket, prot, sess, "--%s--", boundary);
	sockprintf(socket, prot, sess, "");
}
