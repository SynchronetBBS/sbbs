/* telnet.h */

/* Synchronet telnet-related constants and function prototypes */

/* $Id: telnet.h,v 1.21 2019/08/24 19:37:11 rswindell Exp $ */

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

#ifndef _TELNET_H
#define _TELNET_H

#include "gen_defs.h"	/* uchar */

/* commands */

#define TELNET_IAC		255	/* 0xff - Interpret as command */
#define TELNET_DONT		254 /* 0xfe - Don't do option */
#define TELNET_DO   	253 /* 0xfd - Do option */
#define TELNET_WONT 	252 /* 0xfc - Won't do option */
#define TELNET_WILL 	251 /* 0xfb - Will do option */

#define TELNET_SB       250	/* 0xfa - sub-negotiation */
#define TELNET_GA		249	/* 0xf9 - Go ahead */
#define TELNET_EL		248 /* 0xf8 - Erase line */
#define TELNET_EC		247 /* 0xf7 - Erase char */
#define TELNET_AYT		246 /* 0xf6 - Are you there? */
#define TELNET_AO		245 /* 0xf5 - Abort output */
#define TELNET_IP		244 /* 0xf4 - Interrupt process */
#define TELNET_BRK		243 /* 0xf3 - Break */
#define TELNET_SYNC		242 /* 0xf2 - Data mark */
#define TELNET_NOP		241 /* 0xf1 - No operation */

#define TELNET_SE       240 /* 0xf0 - End of subnegotiation parameters. */

/* options */

enum {
 	 TELNET_BINARY_TX
	,TELNET_ECHO
	,TELNET_RECONN
	,TELNET_SUP_GA					/* suppress go ahead */
	,TELNET_APPROX_MSG_SIZE
	,TELNET_STATUS
	,TELNET_TIMING_MARK
	,TELNET_REMOTE_CTRL
	,TELNET_OUTPUT_LINE_WIDTH
	,TELNET_OUTPUT_PAGE_SIZE
	,TELNET_OUTPUT_CR_DISP			/* 10 */
	,TELNET_OUTPUT_HTAB_STOPS
	,TELNET_OUTPUT_HTAB_DISP
	,TELNET_OUTPUT_FF_DISP
	,TELNET_OUTPUT_VTAB_STOPS
	,TELNET_OUTPUT_VTAB_DISP
	,TELNET_OUTPUT_LF_DISP
	,TELNET_EXASCII
	,TELNET_LOGOUT
	,TELNET_BYTE_MACRO
	,TELNET_DATA_ENTRY_TERM			/* 20 */
	,TELNET_SUPDUP
	,TELNET_SUPDUP_OUTPUT
	,TELNET_SEND_LOCATION			/* [RFC779], ASCII string argument */
	,TELNET_TERM_TYPE
	,TELNET_END_OF_RECORD
	,TELNET_TACACS_USERID
	,TELNET_OUTPUT_MARKING
	,TELNET_TERM_LOCATION_NUMBER	/* 64-bit argument */
	,TELNET_3270
	,TELNET_X3_PAD					/* 30 */
	,TELNET_NEGOTIATE_WINDOW_SIZE
	,TELNET_TERM_SPEED
	,TELNET_REMOTE_FLOW
	,TELNET_LINE_MODE
	,TELNET_X_DISPLAY_LOCATION
	,TELNET_ENVIRON					/* Not used */
	,TELNET_AUTH_OPTION
	,TELNET_ENCRYPTION_OPTION
	,TELNET_NEW_ENVIRON				/* RFC 1572 */
	,TELNET_3270E					/* 40 */
	,TELNET_XAUTH					/* [Earhart] */
	,TELNET_CHARSET					/* [RFC2066] */
	,TELNET_RSP	                    /* [Barnes] */
	,TELNET_COMPORT_CTRL			/* [RFC2217] */
	,TLENET_SUP_LOCAL_ECHO			/* [Atmar] */
	,TELNET_START_TLS               /* [Boe] */
	,TELNET_KERMIT                  /* [RFC2840] */
	,TELNET_SEND_URL                /* [Croft] */
	,TELNET_FORWARD_X				/* [Altman] */


	,TELNET_EXOPL=255	/* Extended options list */
};

/* Terminal-type sub option codes, see RFC 1091 */
#define TELNET_TERM_IS		0
#define TELNET_TERM_SEND	1
#define TELNET_TERM_MAXLEN	40

/* New environment sub option codes, see RFC 1572 */
#define TELNET_ENVIRON_IS		0
#define TELNET_ENVIRON_SEND		1
#define TELNET_ENVIRON_INFO		2

#define TELNET_ENVIRON_VAR		0
#define TELNET_ENVIRON_VALUE	1
#define TELNET_ENVIRON_ESC		2
#define TELNET_ENVIRON_USERVAR	3

/* bits for telnet_mode */   
   
#define TELNET_MODE_GATE	(1<<2)	/* Pass-through telnet commands/responses */
#define TELNET_MODE_OFF		(1<<3)	/* This is not a Telnet connection */

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif

#if defined(_WIN32)
	#if !defined(TELNET_NO_DLL)
		#ifdef SBBS_EXPORTS
			#define DLLEXPORT __declspec(dllexport)
		#else
			#define DLLEXPORT __declspec(dllimport)
		#endif
	#else
		#define DLLEXPORT
	#endif
#else
	#define DLLEXPORT
#endif

#ifdef __cplusplus  
extern "C" {   
#endif   

DLLEXPORT const char* telnet_cmd_desc(uchar cmd);   
DLLEXPORT const char* telnet_opt_desc(uchar opt);
DLLEXPORT		uchar telnet_opt_ack(uchar cmd);
DLLEXPORT		uchar telnet_opt_nak(uchar cmd);
DLLEXPORT size_t telnet_expand(const uchar* inbuf, size_t inlen, uchar* outbuf, size_t outlen
								,BOOL expand_cr, uchar** result);

#ifdef __cplusplus
}
#endif

#endif /* don't add anything after this line */
