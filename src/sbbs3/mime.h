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

#ifndef _MIME_H_
#define _MIME_H_

/* mime.c */
char *  mimegetboundary(void);
void    mimeheaders(SOCKET socket, const char* prot, int sess, char * boundary);
void    mimeblurb(SOCKET socket, const char* prot, int sess, char * boundary);
void    mimetextpartheader(SOCKET socket, const char* prot, int sess, char * boundary, const char* text_subtype, const char* charset);
BOOL    mimeattach(SOCKET socket, const char* prot, int sess, char * boundary, char * pathfile);
void    endmime(SOCKET socket, const char* prot, int sess, char * boundary);

#endif	/* Don't add anything after this line */
