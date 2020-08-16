/* sbldefs.h */

/* Synchronet BBS List Macros, constants, and type definitions */

/* $Id: sbldefs.h,v 1.11 2013/09/15 09:16:47 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2013 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "gen_defs.h"

#define MAX_SYSOPS  5
#define MAX_NUMBERS 20
#define MAX_NETS	10
#define MAX_TERMS	5
#define DESC_LINES	5
#define DEF_LIST_FMT "NSTP"

#ifndef IPPORT_TELNET
#define IPPORT_TELNET 23
#endif

#define Y2K_2DIGIT_WINDOW	70

/* Misc bits */

#define FROM_SMB	(1L<<0) 	/* BBS info imported from message base */

#ifdef _WIN32	/* necessary for compatibility with SBL */
#pragma pack(push)
#pragma pack(1)
#endif

#ifndef _PACK
	#ifdef __GNUC__
		#define _PACK __attribute__ ((packed))
	#else
		#define _PACK
	#endif
#endif

typedef union _PACK {

		struct {
			char	number[13]; 		/* Phone number */
			char	desc[16];			/* Modem description */
			char	location[31];		/* Location of phone number */
			uint16_t	min_rate;			/* Minimum connect rate */
			uint16_t	max_rate;			/* Maximum connect rate */
		} modem;

		struct {
			char	addr[29];			/* Telnet address */
			char	location[31];		/* Location  */
			uint16_t	unused;				/* 0xffff */
			uint16_t	port;				/* TCP port number */
		} telnet;

} number_t;

typedef struct _PACK {
	char	 name[26]					/* System name */
			,user[26]					/* User who created entry */
			,software[16]				/* BBS software */
			,total_sysops
			,sysop[MAX_SYSOPS][26]		/* Sysop names */
			,total_numbers
			,total_networks
			,network[MAX_NETS][16]		/* Network names */
			,address[MAX_NETS][26]		/* Network addresses */
			,total_terminals
			,terminal[MAX_TERMS][16]	/* Terminals supported */
			,desc[DESC_LINES][51]		/* 5 line description */
			;
	uint16_t	 nodes						/* Total nodes */
			,users						/* Total users */
			,subs						/* Total sub-boards */
			,dirs						/* Total file dirs */
			,xtrns						/* Total external programs */
			;
	uint32_t	 created					/* Time/date entry was created */
			,updated					/* Time/date last updated */
			,birth						/* Birthdate of BBS */
			;
	uint32_t	 megs						/* Storage space in megabytes */
			,msgs						/* Total messages */
			,files						/* Total files */
			,misc						/* Miscellaneous bits */
			;
	number_t number[MAX_NUMBERS];		/* Access numbers */

	char	userupdated[26];			/* User who last updated */
	uint32_t	verified;					/* Time/Date last vouched for */
	char	userverified[26];			/* User who last vouched */
	char	web_url[61];				/* Web-site address */
	char	sysop_email[61];			/* Sysop's e-mail address */
	uint32_t	exported;					/* Date last exported to SMB */
	uint32_t	verification_count;			/* Number of successful auto-verifications */
	uint32_t	verification_attempts;		/* Number of auto-verification attempts */
	char	unused[310];				/* Unused space */
} bbs_t;

#ifdef _WIN32
#pragma pack(pop)		/* original packing */
#endif



/* End of SBL.H */

