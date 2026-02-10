/* FidoNet constants, macros, and structure definitions */

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

#ifndef _FIDODEFS_H_
#define _FIDODEFS_H_

#include "gen_defs.h"

#define FIDO_TLD        ".fidonet"  /* Fake top-level domain for gating netmail through SMTP  */
#define FIDO_ORIGIN_PREFIX  "\r * Origin: "
#define FIDO_FILELIST_SEP   " ,"        /* FTS-1 */
#define FIDO_PING_NAME      "PING"      /* 'To' username for PING netmail (FTS-5001) */
#define FIDO_AREAMGR_NAME   "AreaFix"   /* De-facto pseudo-standard */
#define FIDO_CONFMGR_NAME   "ConfMgr"   /* FSC-0057 */
#define FIDO_CHARSET_ASCII  "ASCII 1"   /* FTS-5003 */
#define FIDO_CHARSET_CP437  "CP437 2"   /* FTS-5003 */
#define FIDO_CHARSET_UTF8   "UTF-8 4"   /* FTS-5003 */

#define FIDO_NAME_LEN           36  /* Includes '\0' terminator				*/
#define FIDO_SUBJ_LEN           72  /* Includes '\0' terminator				*/
#define FIDO_TIME_LEN           20  /* Includes '\0' terminator				*/
#define FIDO_PASS_LEN           8   /* May NOT include '\0' terminator		*/
#define FIDO_DOMAIN_LEN         8   /* May NOT include '\0' terminator		*/
#define FIDO_PRODDATA_LEN       4   /* Product-specific Data */
#define FIDO_AREATAG_LEN        60  // Echo "area tag" (NOT including '\0') (see FSC-74)
#define FIDO_ECHO_TITLE_LEN     55  // Brief echo description, per echobase.hlp
#define FIDO_ORIGIN_PREFIX_LEN  12  /* Includes new-line character			*/

/* Attribute bits for fmsghdr_t.attr	*/
#define FIDO_PRIVATE    (1 << 0)      /* Private message						*/
#define FIDO_CRASH      (1 << 1)      /* Crash-mail (send immediately)		*/
#define FIDO_RECV       (1 << 2)      /* Received successfully				*/
#define FIDO_SENT       (1 << 3)      /* Sent successfully					*/
#define FIDO_FILE       (1 << 4)      /* File attached						*/
#define FIDO_INTRANS    (1 << 5)      /* In transit							*/
#define FIDO_ORPHAN     (1 << 6)      /* Orphan								*/
#define FIDO_KILLSENT   (1 << 7)      /* Kill it after sending it				*/
#define FIDO_LOCAL      (1 << 8)      /* Created locally - on this system		*/
#define FIDO_HOLD       (1 << 9)      /* Hold - don't send it yet				*/
#define FIDO_FREQ       (1 << 11)     /* File request							*/
#define FIDO_RRREQ      (1 << 12)     /* Return receipt request				*/
#define FIDO_RR         (1 << 13)     /* This is a return receipt				*/
#define FIDO_AUDIT      (1 << 14)     /* Audit request						*/
#define FIDO_FUPREQ     (1 << 15)     /* File update request					*/

#if defined(_WIN32) || defined(__BORLANDC__)
	#define PRAGMA_PACK
#endif

#if defined(PRAGMA_PACK) || defined(__WATCOMC__)
	#define _PACK
#else
	#define _PACK __attribute__ ((packed))
#endif

#if defined(PRAGMA_PACK)
	#pragma pack(push,1)            /* Disk image structures must be packed */
#endif

typedef struct _PACK {              /* Fidonet Packet Header (Type-2), FTS-1 */
	uint16_t orignode;              // Origination Node of Packet (all types)
	uint16_t destnode;              // Destination Node of Packet (all types)
	uint16_t year;                  // Year of Packet Creation e.g. 1995
	uint16_t month;                 // Month of Packet Creation 0-11
	uint16_t day;                   // Day of Packet Creation 1-31
	uint16_t hour;                  // Hour of Packet Creation 0-23
	uint16_t min;                   // Minute of Packet Creation 0-59
	uint16_t sec;                   // Second of Packet Creation 0-59
	uint16_t baud;                  // Max Baud Rate of Orig & Dest
	uint16_t pkttype;               // Packet Type (2)
	uint16_t orignet;               // Origination Net of Packet
	uint16_t destnet;               // Destination Net of Packet
	uint8_t prodcode;               // Product Code (00h is Fido)
	uint8_t sernum;                 // Binary Serial Number or NULL
	uint8_t password[FIDO_PASS_LEN];        // Session Password or NULL
	uint16_t origzone;              // Origination Zone of Packet or NULL (added in rev 12 of FTS-1)
	uint16_t destzone;              // Destination Zone of Packet or NULL (added in rev 12 of FTS-1)
	uint8_t fill[20];               // Unused (zeroed)
} fpkthdr2_t;

typedef struct _PACK {              /* Fidonet Packet Header (Type-2+), FSC-48 and FSC-39.4 (sans auxnet) */
	uint16_t orignode;              // Origination Node of Packet (all types)
	uint16_t destnode;              // Destination Node of Packet (all types)
	uint16_t year;                  // Year of Packet Creation e.g. 1995
	uint16_t month;                 // Month of Packet Creation 0-11
	uint16_t day;                   // Day of Packet Creation 1-31
	uint16_t hour;                  // Hour of Packet Creation 0-23
	uint16_t min;                   // Minute of Packet Creation 0-59
	uint16_t sec;                   // Second of Packet Creation 0-59
	uint16_t baud;                  // Max Baud Rate of Orig & Dest
	uint16_t pkttype;               // Packet Type (2)
	uint16_t orignet;               // Origination Net of Packet
	uint16_t destnet;               // Destination Net of Packet
	uint8_t prodcodeLo;             // Product Code (00h is Fido)
	uint8_t prodrevMajor;           // Revision (major)
	uint8_t password[FIDO_PASS_LEN];        // Session Password or NULL
	uint16_t oldOrigZone;           // Origination Zone in type 2 packet, unused in 2+
	uint16_t oldDestZone;           // Destination Zone in type 2 packet, unused in 2+
	/* 2 Fill data area: */
	uint16_t auxnet;                // Orig Net if Origin is a Point
	uint16_t cwcopy;                // Must be Equal to cword (byte-swapped), added in rev 4 of FSC-39
	uint8_t prodcodeHi;             // Product Code
	uint8_t prodrevMinor;           // Revision (minor)
	uint16_t cword;                 // Compatibility Word
	uint16_t origzone;              // Zone of Packet Sender or NULL
	uint16_t destzone;              // Zone of Packet Receiver or NULL
	uint16_t origpoint;             // Origination Point of Packet
	uint16_t destpoint;             // Destination Point of Packet
	uint8_t proddata[FIDO_PRODDATA_LEN];            // Product Specific Data
} fpkthdr2plus_t;

typedef struct _PACK {              /* Fidonet Packet Header (Type-2.2), FSC-45 */
	uint16_t orignode;              // Origination Node of Packet (all types)
	uint16_t destnode;              // Destination Node of Packet (all types)
	uint16_t origpoint;             // Origination Point of Packet
	uint16_t destpoint;             // Destination Point of Packet
	uint8_t reserved[8];            // Reserved, must be zero
	uint16_t subversion;            // Packet sub-version (2, indicates type 2.2 packet header)
	uint16_t pkttype;               // Packet Type (2)
	uint16_t orignet;               // Origination Net of Packet
	uint16_t destnet;               // Destination Net of Packet
	uint8_t prodcode;               // Product code (00h is Fido)
	uint8_t prodrev;                // Product revision level
	uint8_t password[FIDO_PASS_LEN];        // Session Password or NULL
	uint16_t origzone;              // Origination Zone of Packet or NULL (added in rev 12 of FTS-1)
	uint16_t destzone;              // Destination Zone of Packet or NULL (added in rev 12 of FTS-1)
	/* 2 Fill data area: */
	uint8_t origdomn[FIDO_DOMAIN_LEN];          // Origination Domain
	uint8_t destdomn[FIDO_DOMAIN_LEN];          // Destination Domain
	uint8_t proddata[FIDO_PRODDATA_LEN];            // Product Specific Data
} fpkthdr2_2_t;


typedef union _PACK {               /* Fidonet Packet Header (types 2, 2+, and 2.2) */
	fpkthdr2_t type2;
	fpkthdr2plus_t type2plus;
	fpkthdr2_2_t type2_2;
} fpkthdr_t;

#define FIDO_PACKET_HDR_LEN         58
#define FIDO_PACKET_TERMINATOR      0x0000  /* 16-bits */

typedef struct _PACK {              /* FidoNet Packed Message Header 		*/
	int16_t type;                   /* Message type: 2						*/
	int16_t orignode;
	int16_t destnode;
	int16_t orignet;
	int16_t destnet;
	int16_t attr;
	int16_t cost;
	char time[FIDO_TIME_LEN];       /* Time in goof-ball ASCII format		*/
} fpkdmsg_t;

#define FIDO_PACKED_MSG_HDR_LEN     34      /* Fixed header fields only */
#define FIDO_PACKED_MSG_TERMINATOR  '\0'    /* 8-bits */

typedef struct _PACK {              /* FidoNet Stored Message Header *.msg	*/
	char from[FIDO_NAME_LEN],       /* From user							*/
	     to[FIDO_NAME_LEN],         /* To user								*/
	     subj[FIDO_SUBJ_LEN],       /* Message title						*/
	     time[FIDO_TIME_LEN];       /* Time in goof-ball ASCII format		*/
	int16_t read,                   /* Times read							*/
	        destnode,               /* Destination node						*/
	        orignode,               /* Origin node							*/
	        cost,                   /* Cost in pennies						*/
	        orignet,                /* Origin net							*/
	        destnet,                /* Destination net						*/
	        destzone,               /* Destination zone						*/
	        origzone,               /* Origin zone							*/
	        destpoint,              /* Destination point					*/
	        origpoint,              /* Origin point							*/
	        re,                     /* Message number regarding				*/
	        attr,                   /* Attributes - see FIDO_*				*/
	        next;                   /* Next message number in stream		*/
} fmsghdr_t;

struct _PACK fidoaddr {     /* FidoNet 5D address (zone:net/node.point@domain) */
	uint16_t zone;
	uint16_t net;
	uint16_t node;
	uint16_t point;
	char domain[FIDO_DOMAIN_LEN + 1];
};

#define FIDO_STORED_MSG_HDR_LEN     190
#define FIDO_STORED_MSG_TERMINATOR  '\0'    /* 8-bits */

#define FIDO_SOFT_CR                0x8d

#if defined(PRAGMA_PACK)
#pragma pack(pop)       /* original packing */
#endif

#endif  /* Don't add anything after this line */

