/* Synchronet QWK-related structures, constants, and prototypes */

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

#define QWK_NEWLINE     '\xe3'  /* QWK line terminator (227) */
#define QWK_BLOCK_LEN   128
#define QWK_HFIELD_LEN  25      /* Header field (To/From/Subject) length */

/* QWK mode bits for qwktomsg() and msgtoqwk() */
#define QM_EXPCTLA  (1 << 0)  /* Expand Ctrl-A codes to ANSI */
#define QM_RETCTLA  (1 << 1)  /* Retain Ctrl-A codes */
#define QM_CTRL_A   (QM_EXPCTLA | QM_RETCTLA)
#define QM_STRIP    0       /* Strip Ctrl-A codes */
#define QM_TAGLINE  (1 << 5)  /* Place tagline at end of QWK message */
#define QM_TO_QNET  (1 << 6)  /* Sending to QWKnet hub */
#define QM_REP      (1 << 7)  /* It's a REP packet */
#define QM_VIA      (1 << 8)  /* Include @VIA kludge */
#define QM_TZ       (1 << 9)  /* Include @TZ kludge */
#define QM_MSGID    (1 << 10) /* Include @MSGID and @REPLY kludges */
#define QM_REPLYTO  (1 << 11) /* Include @REPLYTO kludge */
#define QM_EXT      (1 << 13) /* QWK Extended (QWKE) mode (same as QWK_EXT and QHUB_EXT) */
#define QM_UTF8     (1 << 18) /* Include UTF-8 characters in exported messages */
#define QM_WORDWRAP (1 << 19) /* Word-wrap exported message text */
#define QM_MIME     (1 << 20) /* MIME-decode exported message text */

float   ltomsbin(int32_t val);
bool    route_circ(char *via, char *id);

