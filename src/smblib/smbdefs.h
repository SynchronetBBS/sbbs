/* Synchronet message base constant and structure definitions */

/* $Id: smbdefs.h,v 1.119 2019/07/30 10:20:20 rswindell Exp $ */
// vi: tabstop=4

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

#ifndef _SMBDEFS_H
#define _SMBDEFS_H

/* ANSI headers */
#include <stdio.h>
#include <time.h>

/* XPDEV headers */
#include "genwrap.h"	/* truncsp() and get_errno() */
#include "dirwrap.h"	/* MAX_PATH */
#include "filewrap.h"	/* SH_DENYRW */

/* SMBLIB Headers */
#include "md5.h"		/* MD5_DIGEST_SIZE */

/**********/
/* Macros */
/**********/

#define SMB_HEADER_ID	"SMB\x1a"		/* <S> <M> <B> <^Z> */
#define SHD_HEADER_ID	"SHD\x1a"		/* <S> <H> <D> <^Z> */
#define LEN_HEADER_ID	4

#ifndef uchar
	#if defined(TYPEDEF_UCHAR)
		typedef unsigned char uchar;
	#else
		#define uchar unsigned char
	#endif
#endif
#ifdef __GLIBC__
	#include <sys/types.h>
#else
	#ifndef ushort
	#define ushort				unsigned short
	#define ulong				unsigned long
	#define uint				unsigned int
	#endif
#endif

#define HUGE16
#define FAR16
#define REALLOC realloc
#define LMALLOC malloc
#define MALLOC malloc
#define LFREE free
#define FREE free


#define SDT_BLOCK_LEN		256 		/* Size of data blocks */
#define SHD_BLOCK_LEN		256 		/* Size of header blocks */

#define SMB_MAX_HDR_LEN		0xffffU		/* Message header length is 16-bit */

#define SMB_SELFPACK		0			/* Self-packing storage allocation */
#define SMB_FASTALLOC		1			/* Fast allocation */

										/* status.attr bit flags: */
#define SMB_EMAIL			1			/* User numbers stored in Indexes */
#define SMB_HYPERALLOC		2			/* No allocation (also storage value for smb_addmsghdr) */
#define SMB_NOHASH			4			/* Do not calculate or store hashes */

										/* Time zone macros for when_t.zone */
#define DAYLIGHT			0x8000		/* Daylight savings is active */
#define US_ZONE 			0x4000		/* U.S. time zone */
#define WESTERN_ZONE		0x2000		/* Non-standard zone west of UT */
#define EASTERN_ZONE		0x1000		/* Non-standard zone east of UT */
#define SMB_DST_OFFSET		60			/* Daylight Saving Time offset, in minutes */

										/* US Time Zones (standard) */
#define AST 				0x40F0		/* Atlantic 			(-04:00) */
#define EST 				0x412C		/* Eastern				(-05:00) */
#define CST 				0x4168		/* Central				(-06:00) */
#define MST 				0x41A4		/* Mountain 			(-07:00) */
#define PST 				0x41E0		/* Pacific				(-08:00) */
#define YST 				0x421C		/* Yukon				(-09:00) */
#define HST 				0x4258		/* Hawaii/Alaska		(-10:00) */
#define BST 				0x4294		/* Bering				(-11:00) */

										/* US Time Zones (daylight) */
#define ADT 				0xC0F0		/* Atlantic 			(-03:00) */
#define EDT 				0xC12C		/* Eastern				(-04:00) */
#define CDT 				0xC168		/* Central				(-05:00) */
#define MDT 				0xC1A4		/* Mountain 			(-06:00) */
#define PDT 				0xC1E0		/* Pacific				(-07:00) */
#define YDT 				0xC21C		/* Yukon				(-08:00) */
#define HDT 				0xC258		/* Hawaii/Alaska		(-09:00) */
#define BDT 				0xC294		/* Bering				(-10:00) */

										/* Non-US Time Zones */
#define MID 				0x2294		/* Midway				(-11:00) */
#define VAN 				0x21E0		/* Vancouver			(-08:00) */
#define EDM 				0x21A4		/* Edmonton 			(-07:00) */
#define WIN 				0x2168		/* Winnipeg 			(-06:00) */
#define BOG 				0x212C		/* Bogota				(-05:00) */
#define CAR 				0x20F0		/* Caracas				(-04:00) */
#define RIO 				0x20B4		/* Rio de Janeiro		(-03:00) */
#define FER 				0x2078		/* Fernando de Noronha	(-02:00) */
#define AZO 				0x203C		/* Azores				(-01:00) */
#define WET 				0x1000		/* Western European		(+00:00) */
#define WEST 				0x9000		/* WE Summer Time		(+01:00) AKA BST */
#define CET 				0x103C		/* Central European		(+01:00) */
#define CEST 				0x903C		/* CE Summer Time		(+02:00) */
#define EET 				0x1078		/* Eastern European		(+02:00) */
#define EEST				0x9078		/* EE Summer Time		(+03:00) */
#define MOS 				0x10B4		/* Moscow				(+03:00) */
#define DUB 				0x10F0		/* Dubai				(+04:00) */
#define KAB 				0x110E		/* Kabul				(+04:30) */
#define KAR 				0x112C		/* Karachi				(+05:00) */
#define BOM 				0x114A		/* Bombay				(+05:30) */
#define KAT 				0x1159		/* Kathmandu			(+05:45) */
#define DHA 				0x1168		/* Dhaka				(+06:00) */
#define BAN 				0x11A4		/* Bangkok				(+07:00) */
#define HON 				0x11E0		/* Hong Kong			(+08:00) */
#define TOK 				0x121C		/* Tokyo				(+09:00) */
#define ACST				0x123a		/* Australian Central	(+09:30) */
#define AEST 				0x1258		/* Australian Eastern	(+10:00) (Sydney) */
#define ACDT				0x923a		/* Australian Central D	(+10:30) */
#define AEDT 				0x9258		/* Australian Eastern D	(+11:00) (Sydney) */
#define NOU 				0x1294		/* Noumea				(+11:00) */
#define NZST 				0x12D0		/* New Zealand 			(+12:00) (Wellington) */
#define NZDT				0x92D0		/* New Zealand Daylight	(+13:00) (Wellington) */

#define OTHER_ZONE(zone) (zone<=1000 && zone>=-1000)

#define SMB_TZ_HAS_DST(zone)	((!OTHER_ZONE(zone)) && ((zone&(US_ZONE|DAYLIGHT)) \
								|| zone==WET || zone==CET || zone==EET || zone==NZST || zone==AEST || zone==ACST))

										/* Valid hfield_t.types */
#define SENDER				0x00
#define SENDERAGENT 		0x01
#define SENDERNETTYPE		0x02
#define SENDERNETADDR		0x03		/* Note: SENDERNETTYPE may be NET_NONE and this field present and contain a valid string */
#define SENDEREXT			0x04
#define SENDERPOS			0x05
#define SENDERORG			0x06
#define SENDERIPADDR		0x07		/* for tracing */
#define SENDERHOSTNAME		0x08		/* for tracing */
#define SENDERPROTOCOL		0x09		/* for tracing */
#define SENDERPORT_BIN		0x0a		/* deprecated */
#define SENDERPORT			0x0b		/* for tracing */

										/* Used for the SMTP Originator-Info header field: */
#define SENDERUSERID		0x0c		/* user-id */
#define SENDERTIME			0x0d		/* authentication/connection time */
#define SENDERSERVER		0x0e		/* server hostname that authenticated user */

#define REPLYTO 			0x20		/* Name only */
#define REPLYTOAGENT		0x21
#define REPLYTONETTYPE		0x22
#define REPLYTONETADDR		0x23
#define REPLYTOEXT			0x24
#define REPLYTOPOS			0x25
#define REPLYTOORG			0x26
#define REPLYTOLIST			0x27

#define RECIPIENT			0x30
#define RECIPIENTAGENT		0x31
#define RECIPIENTNETTYPE	0x32
#define RECIPIENTNETADDR	0x33	/* Note: RECIPIENTNETTYPE may be NET_NONE and this field present and contain a valid string */
#define RECIPIENTEXT		0x34
#define RECIPIENTPOS		0x35
#define RECIPIENTORG		0x36
#define RECIPIENTLIST		0x37

#define FORWARDED			0x48

#define SUBJECT 			0x60	/* or filename */
#define SMB_SUMMARY 		0x61	/* or file description */
#define SMB_COMMENT 		0x62
#define SMB_CARBONCOPY		0x63	/* Comma-separated list of secondary recipients, RFC822-style */
#define SMB_GROUP			0x64
#define SMB_EXPIRATION		0x65
#define SMB_PRIORITY		0x66	/* DEPRECATED */
#define SMB_COST			0x67
#define	SMB_EDITOR			0x68	/* Associated with FTN ^aNOTE: control line */
#define SMB_TAGS			0x69	/* List of tags (ala hash-tags) related to this message */
#define SMB_TAG_DELIMITER	" "
#define SMB_COLUMNS			0x6a	/* original text editor width in fixed-width columns */

#define FIDOCTRL			0xa0
#define FIDOAREA			0xa1
#define FIDOSEENBY			0xa2
#define FIDOPATH			0xa3
#define FIDOMSGID			0xa4
#define FIDOREPLYID 		0xa5
#define FIDOPID 			0xa6
#define FIDOFLAGS			0xa7
#define FIDOTID 			0xa8
#define FIDOCHARSET			0xa9	// CHRS or CHARSET control line

// RFC822* header field values are strings of US-ASCII chars, but potentially MIME-encoded (RFC2047)
// (i.e. base64 or Q-encoded UTF-8, ISO-8859-1, etc.)
#define RFC822HEADER		0xb0
#define RFC822MSGID 		0xb1
#define RFC822REPLYID		0xb2
#define RFC822TO			0xb3		// Comma-separated list of recipients, RFC822-style
#define RFC822FROM			0xb4		// Original, unparsed/modified RFC822 header "From" value
#define RFC822REPLYTO		0xb5		// Comma-separated list of recipients, RFC822-style
#define RFC822CC			0xb6
#define RFC822ORG			0xb7
#define RFC822SUBJECT		0xb8

#define USENETPATH			0xc0
#define USENETNEWSGROUPS	0xc1

#define SMTPCOMMAND			0xd0		/* Arbitrary SMTP command */
#define SMTPREVERSEPATH		0xd1		/* MAIL FROM: argument, "reverse path" */
#define SMTPFORWARDPATH		0xd2		/* RCPT TO: argument, "forward path" */
#define SMTPRECEIVED		0xd3		/* SMTP "Received" information */

#define SMTPSYSMSG			0xd8		/* for delivery failure notification (deprecated) */

#define SMB_POLL_ANSWER		0xe0		/* the subject is the question */

#define UNKNOWN 			0xf1
#define UNKNOWNASCII		0xf2
#define UNUSED				0xff

										/* Valid dfield_t.types */
#define TEXT_BODY			0x00
#define TEXT_TAIL			0x02

#define UNUSED				0xff


										/* Message attributes */
#define MSG_PRIVATE 		(1<<0)
#define MSG_READ			(1<<1)
#define MSG_PERMANENT		(1<<2)
#define MSG_LOCKED			(1<<3)		/* DEPRECATED (never used) */
#define MSG_DELETE			(1<<4)
#define MSG_ANONYMOUS		(1<<5)
#define MSG_KILLREAD		(1<<6)
#define MSG_MODERATED		(1<<7)
#define MSG_VALIDATED		(1<<8)
#define MSG_REPLIED			(1<<9)		/* User replied to this message */
#define MSG_NOREPLY			(1<<10)		/* No replies (or bounces) should be sent to the sender */
#define MSG_UPVOTE			(1<<11)		/* This message is an upvote */
#define MSG_DOWNVOTE		(1<<12)		/* This message is a downvote */
#define MSG_POLL			(1<<13)		/* This message is a poll */
#define MSG_SPAM			(1<<14)		/* This message has been flagged as SPAM */

#define MSG_VOTE			(MSG_UPVOTE|MSG_DOWNVOTE)	/* This message is a poll-vote */
#define MSG_POLL_CLOSURE	(MSG_POLL|MSG_VOTE)			/* This message is a poll-closure */
#define MSG_POLL_VOTE_MASK	MSG_POLL_CLOSURE

#define MSG_POLL_MAX_ANSWERS	16

										/* Auxiliary header attributes */
#define MSG_FILEREQUEST 	(1<<0)		/* File request */
#define MSG_FILEATTACH		(1<<1)		/* File(s) attached to Msg (file paths/names in subject) */
#define MSG_MIMEATTACH		(1<<2)		/* Message has one or more MIME-embedded attachments */
#define MSG_KILLFILE		(1<<3)		/* Delete file(s) when sent */
#define MSG_RECEIPTREQ		(1<<4)		/* Return receipt requested */
#define MSG_CONFIRMREQ		(1<<5)		/* Confirmation receipt requested */
#define MSG_NODISP			(1<<6)		/* Msg may not be displayed to user */
#define MSG_HFIELDS_UTF8	(1<<13)		/* Message header fields are UTF-8 encoded */
#define POLL_CLOSED			(1<<24)		/* Closed to voting */
#define POLL_RESULTS_MASK	(3U<<30)	/* 4 possible values: */
#define POLL_RESULTS_SECRET	(3U<<30)	/* No one but pollster can see results */
#define POLL_RESULTS_CLOSED	(2U<<30)	/* No one but pollster can see results until poll is closed */
#define POLL_RESULTS_OPEN	(1U<<30)	/* Results are visible to everyone always */
#define POLL_RESULTS_VOTERS	(0U<<30)	/* Voters can see results right away, everyone else when closed */
#define POLL_RESULTS_SHIFT	30

										/* Message network attributes */
#define MSG_LOCAL			(1<<0)		/* Msg created locally */
#define MSG_INTRANSIT		(1<<1)		/* Msg is in-transit */
#define MSG_SENT			(1<<2)		/* Sent to remote */
#define MSG_KILLSENT		(1<<3)		/* Kill when sent */
#define MSG_HOLD			(1<<5)		/* Hold for pick-up */
#define MSG_CRASH			(1<<6)		/* Crash */
#define MSG_IMMEDIATE		(1<<7)		/* Send Msg now, ignore restrictions */
#define MSG_DIRECT			(1<<8)		/* Send directly to destination */

enum smb_net_type {
     NET_NONE				/* Local message */
    ,NET_UNKNOWN			/* Unknown network type */
    ,NET_FIDO				/* FidoNet address, faddr_t format (4D) */
    ,NET_POSTLINK			/* Imported with UTI driver */
    ,NET_QWK				/* QWK networked messsage */
	,NET_INTERNET			/* Internet e-mail, netnews, etc. */
};

enum smb_agent_type {
     AGENT_PERSON
    ,AGENT_PROCESS			/* unknown process type */
	,AGENT_SMBUTIL			/* imported via Synchronet SMBUTIL */
	,AGENT_SMTPSYSMSG		/* Synchronet SMTP server system message */
};

enum smb_xlat_type {
     XLAT_NONE              /* No translation/End of translation list */
	,XLAT_LZH = 9			/* LHarc (LHA) Dynamic Huffman coding */
};

enum smb_priority {			/* msghdr_t.priority */
	SMB_PRIORITY_UNSPECIFIED	= 0,
	SMB_PRIORITY_HIGHEST		= 1,
	SMB_PRIORITY_HIGH			= 2,
	SMB_PRIORITY_NORMAL			= 3,
	SMB_PRIORITY_LOW			= 4,
	SMB_PRIORITY_LOWEST			= 5
};

/************/
/* Typedefs */
/************/

#if defined(_WIN32) || defined(__BORLANDC__) 
	#define PRAGMA_PACK
#endif

#if defined(PRAGMA_PACK) || defined(__WATCOMC__)
	#define _PACK
#else
	#define _PACK __attribute__ ((packed))
#endif

#if defined(PRAGMA_PACK)
	#pragma pack(push,1)	/* Disk image structures must be packed */
#endif

typedef struct _PACK {		/* Time with time-zone */

	uint32_t	time;			/* Local time (unix format) */
	int16_t		zone;			/* Time zone */

} when_t;

typedef struct _PACK {		/* Index record */

	union {
		struct _PACK {
			uint16_t	to; 		/* 16-bit CRC of recipient name (lower case) or user # */
			uint16_t	from;		/* 16-bit CRC of sender name (lower case) or user # */
			uint16_t	subj;		/* 16-bit CRC of subject (lower case, w/o RE:) */
		};
		struct _PACK {
			uint16_t	votes;		/* votes value */
			uint32_t	remsg;		/* number of message this vote is in response to */
		};
	};
	uint16_t	attr;			/* attributes (read, permanent, etc.) */
	uint32_t	offset; 		/* byte-offset of msghdr in header file */
	uint32_t	number; 		/* number of message (1 based) */
	uint32_t	time;			/* time/date message was imported/posted */

} idxrec_t;

										/* valid bits in hash_t.flags		*/
#define SMB_HASH_CRC16			(1<<0)	/* CRC-16 hash is valid				*/
#define SMB_HASH_CRC32			(1<<1)	/* CRC-32 hash is valid				*/
#define SMB_HASH_MD5			(1<<2)	/* MD5 digest is valid				*/
#define SMB_HASH_MASK			(SMB_HASH_CRC16|SMB_HASH_CRC32|SMB_HASH_MD5)
								
#define SMB_HASH_MARKED			(1<<4)	/* Used by smb_findhash()			*/

#define SMB_HASH_STRIP_CTRL_A	(1<<5)	/* Strip Ctrl-A codes first			*/
#define SMB_HASH_STRIP_WSP		(1<<6)	/* Strip white-space chars first	*/
#define SMB_HASH_LOWERCASE		(1<<7)	/* Convert A-Z to a-z first			*/
#define SMB_HASH_PROC_MASK		(SMB_HASH_STRIP_CTRL_A|SMB_HASH_STRIP_WSP|SMB_HASH_LOWERCASE)
#define SMB_HASH_PROC_COMP_MASK	(SMB_HASH_STRIP_WSP|SMB_HASH_LOWERCASE)

enum smb_hash_source_type {
	 SMB_HASH_SOURCE_BODY
	,SMB_HASH_SOURCE_MSG_ID
	,SMB_HASH_SOURCE_FTN_ID
	,SMB_HASH_SOURCE_SUBJECT

/* Add new ones here (max value of 31) */

    ,SMB_HASH_SOURCE_TYPES
};

#define SMB_HASH_SOURCE_MASK	0x1f
#define SMB_HASH_SOURCE_NONE	0
#define SMB_HASH_SOURCE_ALL		0xff
								/* These are the hash sources stored/compared for duplicate message detection: */
#define SMB_HASH_SOURCE_DUPE	((1<<SMB_HASH_SOURCE_BODY)|(1<<SMB_HASH_SOURCE_MSG_ID)|(1<<SMB_HASH_SOURCE_FTN_ID))
								/* These are the hash sources stored/compared for SPAM message detection: */
#define SMB_HASH_SOURCE_SPAM	((1<<SMB_HASH_SOURCE_BODY))

typedef struct _PACK {

	uint32_t	number;					/* Message number */
	uint32_t	time;					/* Local time of fingerprinting */
	uint32_t	length;					/* Length (in bytes) of source */
	uchar		source;					/* SMB_HASH_SOURCE* (in low 5-bits) */
	uchar		flags;					/* indications of valid hashes and pre-processing */
	uint16_t	crc16;					/* CRC-16 of source */
	uint32_t	crc32;					/* CRC-32 of source */
	uchar		md5[MD5_DIGEST_SIZE];	/* MD5 digest of source */
	uchar		reserved[28];			/* sizeof(hash_t) = 64 */

} hash_t;

typedef struct _PACK {		/* Message base header (fixed portion) */

    uchar		id[LEN_HEADER_ID];	/* SMB<^Z> */
    uint16_t	version;        /* version number (initially 100h for 1.00) */
    uint16_t	length;         /* length including this struct */

} smbhdr_t;

typedef struct _PACK {		/* Message base status header */

	uint32_t	last_msg;		/* last message number */
	uint32_t	total_msgs; 	/* total messages */
	uint32_t	header_offset;	/* byte offset to first header record */
	uint32_t	max_crcs;		/* Maximum number of CRCs to keep in history */
    uint32_t	max_msgs;       /* Maximum number of message to keep in sub */
    uint16_t	max_age;        /* Maximum age of message to keep in sub (in days) */
	uint16_t	attr;			/* Attributes for this message base (SMB_HYPER,etc) */

} smbstatus_t;

enum smb_msg_type {
     SMB_MSG_TYPE_NORMAL		/* Classic message (for reading) */
	,SMB_MSG_TYPE_POLL			/* A poll question  */
	,SMB_MSG_TYPE_BALLOT		/* Voter response to poll or normal message */
	,SMB_MSG_TYPE_POLL_CLOSURE	/* Closure of an existing poll */
};

typedef struct _PACK {		/* Message header */

	/* 00 */ uchar		id[LEN_HEADER_ID];	/* SHD<^Z> */
    /* 04 */ uint16_t	type;				/* Message type (enum smb_msg_type) */
    /* 06 */ uint16_t	version;			/* Version of type (initially 100h for 1.00) */
    /* 08 */ uint16_t	length;				/* Total length of fixed record + all fields */
	/* 0a */ uint16_t	attr;				/* Attributes (bit field) (duped in SID) */
	/* 0c */ uint32_t	auxattr;			/* Auxiliary attributes (bit field) */
    /* 10 */ uint32_t	netattr;			/* Network attributes */
	/* 14 */ when_t		when_written;		/* Date/time/zone message was written */
	/* 1a */ when_t		when_imported;		/* Date/time/zone message was imported */
    /* 20 */ uint32_t	number;				/* Message number */
    /* 24 */ uint32_t	thread_back;		/* Message number for backwards threading (aka thread_orig) */
    /* 28 */ uint32_t	thread_next;		/* Next message in thread */
    /* 2c */ uint32_t	thread_first;		/* First reply to this message */
	/* 30 */ uint16_t	delivery_attempts;	/* Delivery attempt counter */
	/* 32 */ int16_t	votes;				/* Votes value (response to poll) or maximum votes per ballot (poll) */
	/* 34 */ uint32_t	thread_id;			/* Number of original message in thread (or 0 if unknown) */
	union {	/* 38-3f */
		struct {		// message
			uint8_t		priority;			/* enum smb_priority_t */
		};
		struct {		// file
			uint32_t	times_downloaded;	/* Total number of times downloaded (moved Mar-6-2012) */
			uint32_t	last_downloaded;	/* Date/time of last download (moved Mar-6-2012) */
		};
	};
    /* 40 */ uint32_t	offset;				/* Offset for buffer into data file (0 or mod 256) */
	/* 44 */ uint16_t	total_dfields;		/* Total number of data fields */

} msghdr_t;

#define thread_orig	thread_back	/* for backwards compatibility with older code */

typedef struct _PACK {		/* Data field */

	uint16_t	type;			/* Type of data field */
    uint32_t	offset;         /* Offset into buffer */ 
    uint32_t	length;         /* Length of data field */

} dfield_t;

typedef struct _PACK {		/* Header field */

	uint16_t	type;
	uint16_t	length; 		/* Length of buffer */

} hfield_t;

typedef struct _PACK {		/* FidoNet address (zone:net/node.point) */

	uint16_t	zone;
	uint16_t	net;
	uint16_t	node;
	uint16_t	point;

} fidoaddr_t;

#if defined(PRAGMA_PACK)
#pragma pack(pop)		/* original packing */
#endif

typedef uint16_t smb_net_type_t;

typedef struct {		/* Network (type and address) */

    smb_net_type_t	type;	// One of enum smb_net_type
	void*			addr;

} net_t;

								/* Valid bits in smbmsg_t.flags					*/
#define MSG_FLAG_HASHED	(1<<0)	/* Message has been hashed with smb_hashmsg()	*/

typedef struct {				/* Message */

	idxrec_t	idx;			/* Index */
	msghdr_t	hdr;			/* Header record (fixed portion) */
	char		*to,			/* To name */
				*to_ext,		/* To extension */
				*to_list,		/* Comma-separated list of recipients, RFC822-style */
				*from,			/* From name */
				*from_ext,		/* From extension */
				*from_org,		/* From organization */
				*from_ip,		/* From IP address (e.g. "192.168.1.2") */
				*from_host,		/* From host name */
				*from_prot,		/* From protocol (e.g. "Telnet", "NNTP", "HTTP", etc.) */
				*replyto,		/* Reply-to name */
				*replyto_ext,	/* Reply-to extension */
				*replyto_list,	/* Comma-separated list of mailboxes, RFC822-style */
				*cc_list,		/* Comma-separated list of additional recipients, RFC822-style */
				*id,			/* RFC822 Message-ID */
				*reply_id,		/* RFC822 Reply-ID */
				*forward_path,	/* SMTP forward-path (RCPT TO: argument) */
				*reverse_path,	/* SMTP reverse-path (MAIL FROM: argument) */
				*path,			/* USENET Path */
				*newsgroups,	/* USENET Newsgroups */
				*ftn_pid,		/* FTN PID */
				*ftn_tid,		/* FTN TID */
				*ftn_area,		/* FTN AREA */
				*ftn_flags,		/* FTN FLAGS */
				*ftn_charset,	/* FTN CHRS */
				*ftn_msgid,		/* FTN MSGID */
				*ftn_reply;		/* FTN REPLY */
	char*		summary;		/* Summary  */
	char*		subj;			/* Subject  */
	char*		tags;			/* Message tags (space-delimited) */
	char*		editor;			/* Message editor (if known) */
	char*		mime_version;	/* MIME Version (if applicable) */
	char*		content_type;	/* MIME Content-Type (if applicable) */
	char*		text_charset;	/* MIME text <charset>  (if applicable) - malloc'd */
	char*		text_subtype;	/* MIME text/<sub-type> (if applicable) - malloc'd */
	uint16_t	to_agent,		/* Type of agent message is to */
				from_agent, 	/* Type of agent message is from */
				replyto_agent;	/* Type of agent replies should be sent to */
	net_t		to_net, 		/* Destination network type and address */
                from_net,       /* Origin network address */
                replyto_net;    /* Network type and address for replies */
	uint16_t	total_hfields;	/* Total number of header fields */
	hfield_t	*hfield;		/* Header fields (fixed length portion) */
	void		**hfield_dat;	/* Header fields (variable length portion) */
	dfield_t	*dfield;		/* Data fields (fixed length portion) */
	int32_t		offset; 		/* Offset (number of records) into index */
	BOOL		forwarded;		/* Forwarded from agent to another */
	uint32_t	expiration; 	/* Message will expire on this day (if >0) */
	uint32_t	cost;			/* Cost to download/read */
	uint32_t	flags;			/* Various smblib run-time flags (see MSG_FLAG_*) */
	uint16_t	user_voted;		/* How the current user viewing this message, voted on it */
	uint32_t	upvotes;		/* Vote tally for this message */
	uint32_t	downvotes;		/* Vote tally for this message */
	uint32_t	total_votes;	/* Total votes for this message or poll */
	uint8_t		columns;		/* 0 means unknown or N/A */

} smbmsg_t;

typedef struct {				/* Message base */

    char		file[128];      /* Path and base filename (no extension) */
    FILE*		sdt_fp;			/* File pointer for data (.sdt) file */
    FILE*		shd_fp;			/* File pointer for header (.shd) file */
    FILE*		sid_fp;			/* File pointer for index (.sid) file */
    FILE*		sda_fp;			/* File pointer for data allocation (.sda) file */
    FILE*		sha_fp;			/* File pointer for header allocation (.sha) file */
	FILE*		hash_fp;		/* File pointer for hash (.hash) file */
	uint32_t	retry_time; 	/* Maximum number of seconds to retry opens/locks */
	uint32_t	retry_delay;	/* Time-slice yield (milliseconds) while retrying */
	smbstatus_t status; 		/* Status header record */
	BOOL		locked;			/* SMB header is locked */
	BOOL		continue_on_error;			/* Attempt recovery after some normaly fatal errors */
	char		last_error[MAX_PATH*2];		/* Last error message */

	/* Private member variables (not initialized by or used by smblib) */
	uint32_t	subnum;			/* Sub-board number */
	uint32_t	msgs;			/* Number of messages loaded (for user) */
	uint32_t	curmsg;			/* Current message number (for user, 0-based) */

} smb_t;

#endif /* Don't add anything after this #endif statement */
