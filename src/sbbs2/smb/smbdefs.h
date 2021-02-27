/* SMBDEFS.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#ifndef _SMBDEFS_H
#define _SMBDEFS_H

#include <stdio.h>

/**********/
/* Macros */
/**********/

										/* Control characters */
#define TAB 				0x09		/* Horizontal tabulation	^I */
#define LF					0x0a		/* Line feed				^J */
#define FF					0x0c		/* Form feed				^L */
#define CR					0x0d		/* Carriage return			^M */
#define ESC 				0x1b		/* Escape					^[ */
#define SP					0x20		/* Space					   */

#define ulong				unsigned long
#define ushort				unsigned short
#define uchar				unsigned char
#define uint				unsigned int

/****************************************************************************/
/* Memory allocation macros for various compilers and environments			*/
/* MALLOC is used for allocations of 64k or less							*/
/* FREE is used to free buffers allocated with MALLOC						*/
/* LMALLOC is used for allocations of possibly larger than 64k				*/
/* LFREE is used to free buffers allocated with LMALLOC 					*/
/* REALLOC is used to re-size a previously MALLOCed or LMALLOCed buffer 	*/
/****************************************************************************/
#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
#	define HUGE16 huge
#	define FAR16 far
#	if defined(__TURBOC__)
#		define REALLOC(x,y) farrealloc(x,y)
#		define LMALLOC(x) farmalloc(x)
#		define MALLOC(x) farmalloc(x)
#		define LFREE(x) farfree(x)
#		define FREE(x) farfree(x)
#	elif defined(__WATCOMC__)
#		define REALLOC realloc
#		define LMALLOC(x) halloc(x,1)  /* far heap, but slow */
#		define MALLOC malloc		   /* far heap, but 64k max */
#		define LFREE hfree
#		define FREE free
#	else	/* Other 16-bit Compiler */
#		define REALLOC realloc
#		define LMALLOC malloc
#		define MALLOC malloc
#		define LFREE free
#		define FREE free
#	endif
#else		/* 32-bit Compiler or Small Memory Model */
#	define HUGE16
#	define FAR16
#	define REALLOC realloc
#	define LMALLOC malloc
#	define MALLOC malloc
#	define LFREE free
#	define FREE free
#endif


#define SDT_BLOCK_LEN		256 		/* Size of data blocks */
#define SHD_BLOCK_LEN		256 		/* Size of header blocks */

#define SMB_SELFPACK		0			/* Self-packing storage allocation */
#define SMB_FASTALLOC		1			/* Fast allocation */
#define SMB_HYPERALLOC		2			/* No allocation */

#define SMB_EMAIL			1			/* User numbers stored in Indexes */

										/* Time zone macros for when_t.zone */
#define DAYLIGHT			0x8000		/* Daylight savings is active */
#define US_ZONE 			0x4000		/* U.S. time zone */
#define WESTERN_ZONE		0x2000		/* Non-standard zone west of UT */
#define EASTERN_ZONE		0x1000		/* Non-standard zone east of UT */

										/* US Time Zones (standard) */
#define AST 				0x40F0		// Atlantic 			(-04:00)
#define EST 				0x412C		// Eastern				(-05:00)
#define CST 				0x4168		// Central				(-06:00)
#define MST 				0x41A4		// Mountain 			(-07:00)
#define PST 				0x41E0		// Pacific				(-08:00)
#define YST 				0x421C		// Yukon				(-09:00)
#define HST 				0x4258		// Hawaii/Alaska		(-10:00)
#define BST 				0x4294		// Bering				(-11:00)

										/* US Time Zones (daylight) */
#define ADT 				0xC0F0		// Atlantic 			(-03:00)
#define EDT 				0xC12C		// Eastern				(-04:00)
#define CDT 				0xC168		// Central				(-05:00)
#define MDT 				0xC1A4		// Mountain 			(-06:00)
#define PDT 				0xC1E0		// Pacific				(-07:00)
#define YDT 				0xC21C		// Yukon				(-08:00)
#define HDT 				0xC258		// Hawaii/Alaska		(-09:00)
#define BDT 				0xC294		// Bering				(-10:00)

										/* Non-standard Time Zones */
#define MID 				0x2294		// Midway				(-11:00)
#define VAN 				0x21E0		// Vancouver			(-08:00)
#define EDM 				0x21A4		// Edmonton 			(-07:00)
#define WIN 				0x2168		// Winnipeg 			(-06:00)
#define BOG 				0x212C		// Bogota				(-05:00)
#define CAR 				0x20F0		// Caracas				(-04:00)
#define RIO 				0x20B4		// Rio de Janeiro		(-03:00)
#define FER 				0x2078		// Fernando de Noronha	(-02:00)
#define AZO 				0x203C		// Azores				(-01:00)
#define LON 				0x1000		// London				(+00:00)
#define BER 				0x103C		// Berlin				(+01:00)
#define ATH 				0x1078		// Athens				(+02:00)
#define MOS 				0x10B4		// Moscow				(+03:00)
#define DUB 				0x10F0		// Dubai				(+04:00)
#define KAB 				0x110E		// Kabul				(+04:30)
#define KAR 				0x112C		// Karachi				(+05:00)
#define BOM 				0x114A		// Bombay				(+05:30)
#define KAT 				0x1159		// Kathmandu			(+05:45)
#define DHA 				0x1168		// Dhaka				(+06:00)
#define BAN 				0x11A4		// Bangkok				(+07:00)
#define HON 				0x11E0		// Hong Kong			(+08:00)
#define TOK 				0x121C		// Tokyo				(+09:00)
#define SYD 				0x1258		// Sydney				(+10:00)
#define NOU 				0x1294		// Noumea				(+11:00)
#define WEL 				0x12D0		// Wellington			(+12:00)

										/* Valid hfield_t.types */
#define SENDER				0x00
#define SENDERAGENT 		0x01
#define SENDERNETTYPE		0x02
#define SENDERNETADDR		0x03
#define SENDEREXT			0x04
#define SENDERPOS			0x05
#define SENDERORG			0x06

#define AUTHOR				0x10
#define AUTHORAGENT 		0x11
#define AUTHORNETTYPE		0x12
#define AUTHORNETADDR		0x13
#define AUTHOREXT			0x14
#define AUTHORPOS			0x15
#define AUTHORORG			0x16

#define REPLYTO 			0x20
#define REPLYTOAGENT		0x21
#define REPLYTONETTYPE		0x22
#define REPLYTONETADDR		0x23
#define REPLYTOEXT			0x24
#define REPLYTOPOS			0x25
#define REPLYTOORG			0x26

#define RECIPIENT			0x30
#define RECIPIENTAGENT		0x31
#define RECIPIENTNETTYPE	0x32
#define RECIPIENTNETADDR	0x33
#define RECIPIENTEXT		0x34
#define RECIPIENTPOS		0x35
#define RECIPIENTORG		0x36

#define FORWARDTO			0x40
#define FORWARDTOAGENT		0x41
#define FORWARDTONETTYPE	0x42
#define FORWARDTONETADDR	0x43
#define FORWARDTOEXT		0x44
#define FORWARDTOPOS		0x45
#define FORWARDTOORG		0x46

#define FORWARDED			0x48

#define RECEIVEDBY			0x50
#define RECEIVEDBYAGENT 	0x51
#define RECEIVEDBYNETTYPE	0x52
#define RECEIVEDBYNETADDR	0x53
#define RECEIVEDBYEXT		0x54
#define RECEIVEDBYPOS		0x55
#define RECEIVEDBYORG		0x56

#define RECEIVED			0x58

#define SUBJECT 			0x60
#define SUMMARY 			0x61
#ifndef COMMENT
#define COMMENT 			0x62
#endif
#define CARBONCOPY			0x63
#define GROUP				0x64
#define EXPIRATION			0x65
#define PRIORITY			0x66

#define FILEATTACH			0x70
#define DESTFILE			0x71
#define FILEATTACHLIST		0x72
#define DESTFILELIST		0x73
#define FILEREQUEST 		0x74
#define FILEPASSWORD		0x75
#define FILEREQUESTLIST 	0x76
#define FILEPASSWORDLIST	0x77

#define IMAGEATTACH 		0x80
#define ANIMATTACH			0x81
#define FONTATTACH			0x82
#define SOUNDATTACH 		0x83
#define PRESENTATTACH		0x84
#define VIDEOATTACH 		0x85
#define APPDATAATTACH		0x86

#define IMAGETRIGGER		0x90
#define ANIMTRIGGER 		0x91
#define FONTTRIGGER 		0x92
#define SOUNDTRIGGER		0x93
#define PRESENTTRIGGER		0x94
#define VIDEOTRIGGER		0x95
#define APPDATATRIGGER		0x96

#define FIDOCTRL			0xa0
#define FIDOAREA			0xa1
#define FIDOSEENBY			0xa2
#define FIDOPATH			0xa3
#define FIDOMSGID			0xa4
#define FIDOREPLYID 		0xa5
#define FIDOPID 			0xa6
#define FIDOFLAGS			0xa7

#define RFC822HEADER		0xb0
#define RFC822MSGID 		0xb1
#define RFC822REPLYID		0xb2

#define UNKNOWN 			0xf1
#define UNKNOWNASCII		0xf2
#define UNUSED				0xff

										/* Valid dfield_t.types */
#define TEXT_BODY			0x00
#define TEXT_SOUL			0x01
#define TEXT_TAIL			0x02
#define TEXT_WING			0x03
#define IMAGEEMBED			0x20
#define ANIMEMBED			0x21
#define FONTEMBED			0x22
#define SOUNDEMBED			0x23
#define PRESENTEMBED		0x24
#define VIDEOEMBED			0x25
#define APPDATAEMBED		0x26
#define UNUSED				0xff


										/* Message attributes */
#define MSG_PRIVATE 		(1<<0)
#define MSG_READ			(1<<1)
#define MSG_PERMANENT		(1<<2)
#define MSG_LOCKED			(1<<3)
#define MSG_DELETE			(1<<4)
#define MSG_ANONYMOUS		(1<<5)
#define MSG_KILLREAD		(1<<6)
#define MSG_MODERATED		(1<<7)
#define MSG_VALIDATED		(1<<8)
#define MSG_REPLIED			(1<<9)		// User replied to this message

										/* Auxillary header attributes */
#define MSG_FILEREQUEST 	(1<<0)		// File request
#define MSG_FILEATTACH		(1<<1)		// File(s) attached to Msg
#define MSG_TRUNCFILE		(1<<2)		// Truncate file(s) when sent
#define MSG_KILLFILE		(1<<3)		// Delete file(s) when sent
#define MSG_RECEIPTREQ		(1<<4)		// Return receipt requested
#define MSG_CONFIRMREQ		(1<<5)		// Confirmation receipt requested
#define MSG_NODISP			(1<<6)		// Msg may not be displayed to user

										/* Message network attributes */
#define MSG_LOCAL			(1<<0)		// Msg created locally
#define MSG_INTRANSIT		(1<<1)		// Msg is in-transit
#define MSG_SENT			(1<<2)		// Sent to remote
#define MSG_KILLSENT		(1<<3)		// Kill when sent
#define MSG_ARCHIVESENT 	(1<<4)		// Archive when sent
#define MSG_HOLD			(1<<5)		// Hold for pick-up
#define MSG_CRASH			(1<<6)		// Crash
#define MSG_IMMEDIATE		(1<<7)		// Send Msg now, ignore restrictions
#define MSG_DIRECT			(1<<8)		// Send directly to destination
#define MSG_GATE			(1<<9)		// Send via gateway
#define MSG_ORPHAN			(1<<10) 	// Unknown destination
#define MSG_FPU 			(1<<11) 	// Force pickup
#define MSG_TYPELOCAL		(1<<12) 	// Msg is for local use only
#define MSG_TYPEECHO		(1<<13) 	// Msg is for conference distribution
#define MSG_TYPENET 		(1<<14) 	// Msg is direct network mail


enum {
     NET_NONE
    ,NET_UNKNOWN
    ,NET_FIDO
    ,NET_POSTLINK
    ,NET_QWK
	,NET_INTERNET
	,NET_WWIV
	,NET_MHS

/* Add new ones here */

    ,NET_TYPES
    };

enum {
     AGENT_PERSON
    ,AGENT_PROCESS

/* Add new ones here */

    ,AGENT_TYPES
    };

enum {
     XLAT_NONE              // No translation/End of translation list
    ,XLAT_ENCRYPT           // Encrypted data
    ,XLAT_ESCAPED           // 7-bit ASCII escaping for ctrl and 8-bit data
    ,XLAT_HUFFMAN           // Static and adaptive Huffman coding compression
    ,XLAT_LZW               // Limpel/Ziv/Welch compression
    ,XLAT_MLZ78             // Modified LZ78 compression
    ,XLAT_RLE               // Run length encoding compression
    ,XLAT_IMPLODE           // Implode compression (PkZIP)
    ,XLAT_SHRINK            // Shrink compression (PkZIP)
	,XLAT_LZH				// LHarc (LHA) Dynamic Huffman coding

/* Add new ones here */

    ,XLAT_TYPES
    };


/************/
/* Typedefs */
/************/

#ifdef _WIN32	/* necessary for compatibility with SBBS v2 */
#pragma pack(push)
#pragma pack(1)
#endif

typedef struct {			// Time with time-zone

	ulong	time;			// Local time (unix format)
	short	zone;			// Time zone

	} when_t;

typedef struct {			// Index record

	ushort	to; 			// 16-bit CRC of recipient name (lower case)
	ushort	from;			// 16-bit CRC of sender name (lower case)
	ushort	subj;			// 16-bit CRC of subject (lower case, w/o RE:)
	ushort	attr;			// attributes (read, permanent, etc.)
	ulong	offset; 		// offset into header file
	ulong	number; 		// number of message (1 based)
	ulong	time;			// time/date message was imported/posted

	} idxrec_t;

typedef struct {			// Message base header (fixed portion)

    uchar   id[4];          // text or binary unique hdr ID
    ushort  version;        // version number (initially 100h for 1.00)
    ushort  length;         // length including this struct

	} smbhdr_t;

typedef struct {			// Message base status header

	ulong	last_msg;		// last message number
	ulong	total_msgs; 	// total messages
	ulong	header_offset;	// byte offset to first header record
	ulong	max_crcs;		// Maximum number of CRCs to keep in history
    ulong   max_msgs;       // Maximum number of message to keep in sub
    ushort  max_age;        // Maximum age of message to keep in sub (in days)
	ushort	attr;			// Attributes for this message base (SMB_HYPER,etc)

	} smbstatus_t;

typedef struct {			// Message header

	uchar	id[4];			// SHD<^Z>
    ushort  type;           // Message type (normally 0)
    ushort  version;        // Version of type (initially 100h for 1.00)
    ushort  length;         // Total length of fixed record + all fields
	ushort	attr;			// Attributes (bit field) (duped in SID)
	ulong	auxattr;		// Auxillary attributes (bit field)
    ulong   netattr;        // Network attributes
	when_t	when_written;	// Time message was written (unix format)
	when_t	when_imported;	// Time message was imported
    ulong   number;         // Message number
    ulong   thread_orig;    // Original message number in thread
    ulong   thread_next;    // Next message in thread
    ulong   thread_first;   // First reply to this message
	uchar	reserved[16];	// Reserved for future use
    ulong   offset;         // Offset for buffer into data file (0 or mod 256)
	ushort	total_dfields;	// Total number of data fields

	} msghdr_t;

typedef struct {			// Data field

	ushort	type;			// Type of data field
    ulong   offset;         // Offset into buffer 
    ulong   length;         // Length of data field

    } dfield_t;

typedef struct {			// Header field

	ushort	type;
	ushort	length; 		// Length of buffer

	} hfield_t;

typedef struct {			// FidoNet address (zone:net/node.point)

	ushort	zone;
	ushort	net;
	ushort	node;
	ushort	point;

    } fidoaddr_t;

typedef struct {			// Network (type and address)

    ushort  type;
	void	*addr;

	} net_t;

typedef struct {				// Message

	idxrec_t	idx;			// Index
	msghdr_t	hdr;			// Header record (fixed portion)
	char		*to,			// To name
				*to_ext,		// To extension
				*from,			// From name
				*from_ext,		// From extension
				*replyto,		// Reply-to name
				*replyto_ext,	// Reply-to extension */
				*subj;			// Subject
	ushort		to_agent,		// Type of agent message is to
				from_agent, 	// Type of agent message is from
				replyto_agent;	// Type of agent replies should be sent to
	net_t		to_net, 		// Destination network type and address
                from_net,       // Origin network address
                replyto_net;    // Network type and address for replies
	ushort		total_hfields;	// Total number of header fields
	hfield_t	*hfield;		// Header fields (fixed length portion)
	void		**hfield_dat;	// Header fields (variable length portion)
	dfield_t	*dfield;		// Data fields (fixed length portion)
	ulong		offset; 		// Offset (number of records) into index
	uchar		forwarded;		// Forwarded from agent to another
	when_t		expiration; 	// Message will exipre on this day (if >0)

	} smbmsg_t;

typedef struct {			// Message base

    char    file[128];      // Path and base filename (no extension)
    FILE    *sdt_fp;        // File pointer for data (.sdt) file
    FILE    *shd_fp;        // File pointer for header (.shd) file
    FILE    *sid_fp;        // File pointer for index (.sid) file
    FILE    *sda_fp;        // File pointer for data allocation (.sda) file
    FILE    *sha_fp;        // File pointer for header allocation (.sha) file
	ulong	retry_time; 	// Maximum number of seconds to retry opens/locks
	smbstatus_t status; 	// Status header record
	char	shd_buf[SHD_BLOCK_LEN]; 	// File I/O buffer for header file

    } smb_t;

#ifdef _WIN32
#pragma pack(pop)		/* original packing */
#endif

#endif /* Don't add anything after this #endif statement */
