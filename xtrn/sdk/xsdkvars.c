/* xsdkvars.c */

/* Synchronet XSDK global variables */

/* $Id: xsdkvars.c,v 1.5 2005/09/20 06:00:38 deuce Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout xtrn	*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <time.h>
#include <stdio.h>
#include "xsdkinet.h"	/* SOCKET */
#include "xsdkdefs.h"

#ifndef GLOBAL
#define GLOBAL
#endif

#ifdef __cplusplus
	extern "C" {
#endif

/*******************************/
/* Global Variable Definitions */
/*******************************/
GLOBAL char 	lncntr; 			/* Line counter */
GLOBAL char 	tos;				/* Top of screen */
GLOBAL uchar	curatr; 			/* Current attribute */
GLOBAL char 	mnehigh,			/* Colors for mnemonics */
				mnelow;
GLOBAL uchar	sys_nodes,			/* Number of nodes on system */
				node_num;			/* Current node number */
GLOBAL ulong	node_misc;			/* Misc. node toggles */
GLOBAL char 	sys_name[41],		/* System name */
				sys_id[9],			/* System QWK ID */
				sys_op[41], 		/* System operator name */
				sys_guru[41];		/* System guru name */
GLOBAL ushort	timeleft;			/* Time left in seconds */
GLOBAL char 	ctrl_dir[128],		/* SBBS dirs */
				data_dir[128],
				node_dir[128],
				exec_dir[128],
				text_dir[128],
				temp_dir[128];
GLOBAL uint 	user_number;		/* User's number */
GLOBAL char 	user_name[45],		/* User's name/alias */
				user_misc,			/* User's misc settings */
				user_rows,			/* User's rows per screen */
				user_level, 		/* User's main level */
				user_flags1[27],	/* User's flag set #1 */
				user_flags2[27],	/* User's flag set #2 */
				user_flags3[27],	/* User's flag set #3 */
				user_flags4[27],	/* User's flag set #4 */
				user_exempt[27],	/* User's exemptions */
				user_rest[27],		/* User's restrictions */
				user_sex,			/* User's sex */
				user_birth[9],		/* User's birthday */
				user_phone[13], 	/* User's phone number */
				user_address[31],	/* User's address */
				user_location[31],	/* User's location */
				user_zipcode[11],	/* User's zip/postal code */
				user_realname[26];	/* User's real name or company name */
GLOBAL ulong	user_cdt,			/* User's credits */
				user_dce;			/* User's DCE rate */
GLOBAL time_t	user_expire;		/* User's expiration date */

									/* COM port registers: */
GLOBAL uint 	com_base,			/* COM base address */
				com_irq;			/* irq line number	   */
GLOBAL ulong	com_rate;			/* DTE rate in bps	   */
GLOBAL char  	com_port;			/* Number of COM port  */

									/* Modem command strings */
GLOBAL char 	mdm_init[64],		/* Initialization */
				mdm_spec[64],		/* Special Initialization */
				mdm_term[64],		/* Terminal Initialization String */
				mdm_dial[64],		/* Dial */
				mdm_offh[64],		/* Off hook */
				mdm_answ[64],		/* Answer */
                mdm_misc;           /* Misc bits used for flags */

GLOBAL time_t	starttime;			/* Start time-stamp */
GLOBAL char 	wordwrap[81];		/* Wordwrap buffer */
GLOBAL uint 	sec_warn,			/* Seconds till inactivity warning */
				sec_timeout;		/* Seconds till disconnect */
GLOBAL char 	timeleft_warn;		/* Minutes left warning */

GLOBAL int		nodefile;			/* File descriptor for NODE.DAB */

GLOBAL char 	slbuf[SAVE_LINES][LINE_BUFSIZE+1]; /* Saved for redisplay */
GLOBAL char 	slatr[SAVE_LINES];	/* Starting attribute of each line */
GLOBAL char 	slcnt;			/* Number of lines currently saved */
GLOBAL char 	lbuf[LINE_BUFSIZE];/* Temp storage for each line output */
GLOBAL int		lbuflen;		/* Number of characters in line buffer */
GLOBAL char     latr;           /* Starting attribute of line buffer */
GLOBAL uint 	inDV;			/* DESQview version if running under DV */
GLOBAL int		keybuftop,keybufbot;	/* Keyboard input buffer pointers */
GLOBAL char     keybuf[KEY_BUFSIZE];    /* Keyboard input buffer */
GLOBAL uint *msr;			/* Last modem status register contents */
GLOBAL char 	**xtrn; 		/* List of external program names */
GLOBAL uint 	total_xtrns;	/* Total number of external programs */
GLOBAL uchar	lastnodemsg;	/* Last node to send a message to */
GLOBAL uchar	name_len;		/* Length of name field in NAME.DAT */
GLOBAL char 	aborted;		/* Aborted flag - if ctrl-c hit */
GLOBAL char 	sysop_level;	/* Sysop Level */
GLOBAL FILE 	*con_fp;		/* Console file pointer (stdout/stderr) */

#ifndef __16BIT__	/* Sockets */

GLOBAL SOCKET	client_socket;

#endif

#ifdef __cplusplus
	}
#endif
