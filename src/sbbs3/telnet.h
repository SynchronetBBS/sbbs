/* telnet.h */

/* Synchronet telnet-related constants and function prototypes */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/* commands */

#define TELNET_IAC		255	// Interpret as command
#define TELNET_DONT		254 // Don't do option
#define TELNET_DO   	253 // Do option
#define TELNET_WONT 	252 // Won't do option
#define TELNET_WILL 	251 // Will do option

#define TELNET_SB       250	// sub-negotiation
#define TELNET_GA		249	// Go ahead
#define TELNET_EL		248 // Erase line
#define TELNET_EC		247 // Erase char
#define TELNET_AYT		246 // Are you there?
#define TELNET_AO		245 // Abort output
#define TELNET_IP		244 // Interrupt process
#define TELNET_BRK		243 // Break
#define TELNET_SYNC		242 // Data mark
#define TELNET_NOP		241 // No operation

#define TELNET_SE       240 //  End of subnegotiation parameters.

/* options */

enum {
 	 TELNET_BINARY
	,TELNET_ECHO
	,TELNET_RECONN
	,TELNET_SUP_GA		// suppress go ahead
	,TELNET_APPROX_MSG_SIZE
	,TELNET_STATUS
	,TELNET_TIMING_MARK
	,TELNET_REMOTE_CTRL
	,TELNET_OUTPUT_LINE_WIDTH
	,TELNET_OUTPUT_PAGE_SIZE
	,TELNET_OUTPUT_CR_DISP
	,TELNET_OUTPUT_HTAB_STOPS
	,TELNET_OUTPUT_HTAB_DISP
	,TELNET_OUTPUT_FF_DISP
	,TELNET_OUTPUT_VTAB_STOPS
	,TELNET_OUTPUT_VTAB_DISP
	,TELNET_OUTPUT_LF_DISP
	,TELNET_EXASCII
	,TELNET_LOGOUT
	,TELNET_BYTE_MACRO
	,TELNET_DATA_ENTRY_TERM
	,TELNET_SUPDUP
	,TELNET_SUPDUP_OUTPUT
	,TELNET_SEND_LOCATION
	,TELNET_TERM_TYPE
	,TELNET_END_OF_RECORD
	,TELNET_TACACS_USERID
	,TELNET_OUTPUT_MARKING
	,TELNET_TERM_LOCATION_NUMBER
	,TELNET_3270
	,TELNET_X3_PAD
	,TELNET_NEGOTIATE_WINDOW_SIZE
	,TELNET_TERM_SPEED
	,TELNET_REMOTE_FLOW
	,TELNET_LINE_MODE
	,TELNET_X_DISPLAY_LOCATION
	,TELNET_ENV_OPTION
	,TELNET_AUTH_OPTION
	,TELNET_ENCRYPTION_OPTION
	,TELNET_NEW_ENV_OPTION
	,TELNET_3270E

	,TELNET_EXOPL=255	// Extended options list
};

/* bits for telnet_mode */   
   
#define TELNET_MODE_BIN_RX	(1<<0)   
#define TELNET_MODE_ECHO	(1<<1)   
#define TELNET_MODE_GATE	(1<<2)	// Pass-through telnet commands/responses
#define TELNET_MODE_OFF		(1<<3)	// This is not a Telnet connection

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef SBBS_EXPORTS
		#define DLLEXPORT __declspec(dllexport)
	#else
		#define DLLEXPORT __declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL __stdcall
	#else
		#define DLLCALL
	#endif
#else
	#define DLLEXPORT
	#define DLLCALL
#endif

#ifdef __cplusplus  
extern "C" {   
#endif   

DLLEXPORT const char* DLLCALL telnet_cmd_desc(uchar cmd);   
DLLEXPORT const char* DLLCALL telnet_opt_desc(uchar opt);

#ifdef __cplusplus
}
#endif
