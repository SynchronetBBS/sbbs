/* startup.h */

/* Synchronet main/telnet server thread startup structure */

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

#ifdef _WIN32
#include <windows.h>
#endif

#include "client.h"
#include "ringbuf.h"

typedef struct {

	DWORD	size;				// sizeof(bbs_struct_t)
    WORD	first_node;
    WORD	last_node;
	WORD	telnet_port;
	WORD	rlogin_port;
	WORD	reserved_word4;
	WORD	reserved_word3;
	WORD	reserved_word2;
	WORD	reserved_word1;
    DWORD   telnet_interface;
    DWORD	options;			// See BBS_OPT definitions
    DWORD	rlogin_interface;
    DWORD	reserved_dword7;
    DWORD	reserved_dword6;
    RingBuf** node_spybuf;		// Spy output buffer (each node)
    RingBuf** node_inbuf;		// User input buffer (each node)
    DWORD	reserved_dword3;
    DWORD	reserved_dword2;
    DWORD	reserved_dword1;
	int 	(*lputs)(char*);
	void	(*status)(char*);
    void	(*started)(void);
    void	(*terminated)(int code);
    void	(*clients)(int active);
    void	(*thread_up)(BOOL up);
	void	(*socket_open)(BOOL open);
    void	(*client_on)(BOOL on, int sock, client_t*);
    void	(*reserved_fptr4)(void);
    void	(*reserved_fptr3)(void);
    void	(*reserved_fptr2)(void);
    void	(*reserved_fptr1)(void);
    char    ctrl_dir[128];
    char	reserved_path8[128];
    char	reserved_path7[128];
    char	reserved_path6[128];
    char	reserved_path5[128];
    char	reserved_path4[128];
    char	reserved_path3[128];
	char	answer_sound[128];
	char	hangup_sound[128];
    char	reserved_path2[128];
    char	reserved_path1[128];

} bbs_startup_t;

#define BBS_OPT_KEEP_ALIVE			(1<<0)	/* Send keep-alives					*/
#define BBS_OPT_XTRN_MINIMIZED		(1<<1)	/* Run externals minimized			*/
#define BBS_OPT_AUTO_LOGON			(1<<2)	/* Auto-logon via IP				*/
#define BBS_OPT_DEBUG_TELNET		(1<<3)	/* Debug telnet commands			*/
#define BBS_OPT_SYSOP_AVAILABLE		(1<<4)	/* Available for chat				*/
#define BBS_OPT_ALLOW_RLOGIN		(1<<5)	/* Allow logins via BSD RLogin		*/
#define BBS_OPT_USE_2ND_RLOGIN		(1<<6)	/* Use 2nd username in BSD RLogin	*/
#define BBS_OPT_NO_QWK_EVENTS		(1<<7)	/* Don't run QWK-related events		*/
#define BBS_OPT_NO_HOST_LOOKUP		(1<<11)
#define BBS_OPT_LOCAL_TIMEZONE		(1<<30)	/* Don't force UCT/GMT				*/
#define BBS_OPT_MUTE				(1<<31)	/* Mute sounds						*/

