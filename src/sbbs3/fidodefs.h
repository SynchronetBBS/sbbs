/* fidodefs.h */

/* FidoNet constants, macros, and structure definitions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef _FIDODEFS_H_
#define _FIDODEFS_H_

#include "gen_defs.h"	/* uchar */

#define FIDO_NAME_LEN	36			/* Includes '\0' terminator				*/
#define FIDO_SUBJ_LEN	72			/* Includes '\0' terminator				*/
#define FIDO_TIME_LEN	20			/* Includes '\0' terminator				*/
#define FIDO_PASS_LEN	8			/* Does NOT include '\0' terminator		*/

									/* Attribute bits for fmsghdr_t.attr	*/
#define FIDO_PRIVATE	(1<<0)		/* Private message						*/
#define FIDO_CRASH		(1<<1)		/* Crash-mail (send immediately)		*/
#define FIDO_RECV		(1<<2)		/* Received successfully				*/
#define FIDO_SENT		(1<<3)		/* Sent successfully					*/
#define FIDO_FILE		(1<<4)		/* File attached						*/
#define FIDO_INTRANS	(1<<5)		/* In transit							*/
#define FIDO_ORPHAN 	(1<<6)		/* Orphan								*/
#define FIDO_KILLSENT	(1<<7)		/* Kill it after sending it				*/
#define FIDO_LOCAL		(1<<8)		/* Created locally - on this system		*/
#define FIDO_HOLD		(1<<9)		/* Hold - don't send it yet				*/
#define FIDO_FREQ		(1<<11) 	/* File request							*/
#define FIDO_RRREQ		(1<<12) 	/* Return receipt request				*/
#define FIDO_RR 		(1<<13) 	/* This is a return receipt				*/
#define FIDO_AUDIT		(1<<14) 	/* Audit request						*/
#define FIDO_FUPREQ 	(1<<15) 	/* File update request					*/

#if defined(_WIN32) || defined(__BORLANDC__)
	#define PRAGMA_PACK
#endif

#if defined(PRAGMA_PACK)
	#define _PACK
#else
	#define _PACK __attribute__ ((packed))
#endif

#if defined(PRAGMA_PACK)
#pragma pack(push)		/* Disk image structures must be packed */
#pragma pack(1)
#endif

typedef struct _PACK {				/* Fidonet Packet Header				*/
    short orignode,					/* Origination Node of Packet			*/
          destnode,					/* Destination Node of Packet			*/
          year,						/* Year of Packet Creation e.g. 1995	*/
          month,					/* Month of Packet Creation 0-11		*/
          day,						/* Day of Packet Creation 1-31			*/
          hour,						/* Hour of Packet Creation 0-23			*/
          min,						/* Minute of Packet Creation 0-59		*/
          sec,						/* Second of Packet Creation 0-59		*/
          baud,						/* Max Baud Rate of Orig & Dest			*/
          pkttype,					/* Packet Type (-1 is obsolete)			*/
          orignet,					/* Origination Net of Packet			*/
          destnet;					/* Destination Net of Packet			*/
    uchar prodcode,					/* Product Code (00h is Fido)			*/
          sernum,					/* Binary Serial Number or NULL			*/
          password[FIDO_PASS_LEN];	/* Session Password or NULL				*/
    short origzone,					/* Origination Zone of Packet or NULL	*/
          destzone;					/* Destination Zone of Packet or NULL	*/
	union {							
		char padding[20];			/* Fill Characters (Type 2.0)			*/
		struct {					/* OR Type 2+ Packet Header Info		*/
			short auxnet,			/* Orig Net if Origin is a Point		*/
				  cwcopy;			/* Must be Equal to cword				*/
			uchar prodcode, 		/* Product Code							*/
				  revision; 		/* Revision								*/
			short cword,			/* Compatibility Word					*/
				  origzone, 		/* Zone of Packet Sender or NULL		*/
				  destzone, 		/* Zone of Packet Receiver or NULL		*/
				  origpoint,		/* Origination Point of Packet			*/
				  destpoint;		/* Destination Point of Packet			*/
			char empty[4];			
		} two_plus;					
		struct {					/* OR Type 2.2 Packet Header Info		*/
			char origdomn[8];		/* Origination Domain					*/
			char destdomn[8];		/* Destination Domain					*/
			char empty[4]; 			/* Product Specific Data				*/
		} two_two;
	} fill;

} fpkthdr_t;

#define FIDO_PACKET_HDR_LEN 58

typedef struct _PACK {				/* FidoNet Packed Message Header 		*/
	short	type;					/* Message type: 2						*/
	short	orignode;
	short	destnode;
	short	orignet;
	short	destnet;
	short	attr;
	short	cost;
	char	time[FIDO_TIME_LEN];	/* Time in goof-ball ASCII format		*/
} fpkdmsg_t;

#define FIDO_PACKED_MSG_HDR_LEN 34	/* Fixed header fields only */

typedef struct _PACK {				/* FidoNet Stored Message Header *.msg	*/
	char	from[FIDO_NAME_LEN],	/* From user							*/
			to[FIDO_NAME_LEN], 		/* To user								*/
			subj[FIDO_SUBJ_LEN],	/* Message title						*/
			time[FIDO_TIME_LEN];	/* Time in goof-ball ASCII format		*/
	short	read,					/* Times read							*/
			destnode,				/* Destination node						*/
			orignode,				/* Origin node							*/
			cost,					/* Cost in pennies						*/
			orignet,				/* Origin net							*/
			destnet,				/* Destination net						*/
			destzone,				/* Destination zone						*/
			origzone,				/* Origin zone							*/
			destpoint,				/* Destination point					*/
			origpoint,				/* Origin point							*/
			re, 					/* Message number regarding				*/
			attr,					/* Attributes - see FIDO_*				*/
			next;					/* Next message number in stream		*/
} fmsghdr_t;

#define FIDO_STORED_MSG_HDR_LEN 190

#if defined(PRAGMA_PACK)
#pragma pack(pop)		/* original packing */
#endif

#endif	/* Don't add anything after this line */

