// $Id: smbdefs.js,v 1.11 2019/08/26 08:57:14 rswindell Exp $
// Synchronet Message Base constant definitions (from smbdefs.h and smblib.h)								
								
/* Values for MsgBase.status (SMBLIB function return values) */
const SMB_SUCCESS			= 0;		// Successful result/return code
const SMB_DUPE_MSG			= 1;		// Duplicate message detected by smb_addcrc()
const SMB_FAILURE			= -1;		// Generic error (discouraged)
const SMB_ERR_NOT_OPEN		= -100;		// Message base not open
const SMB_ERR_HDR_LEN		= -101;		// Invalid message header length (>64k)
const SMB_ERR_HDR_OFFSET	= -102;		// Invalid message header offset
const SMB_ERR_HDR_ID		= -103;		// Invalid header ID
const SMB_ERR_HDR_VER		= -104;		// Unsupported version
const SMB_ERR_HDR_FIELD		= -105;		// Missing header field
const SMB_ERR_NOT_FOUND		= -110;		// Item not found
const SMB_ERR_DAT_OFFSET	= -120;		// Invalid data offset (>2GB)
const SMB_ERR_DAT_LEN		= -121;		// Invalid data length (>2GB)
const SMB_ERR_OPEN			= -200;		// File open error
const SMB_ERR_SEEK			= -201;		// File seek/setpos error
const SMB_ERR_LOCK			= -202;		// File lock error
const SMB_ERR_READ			= -203;		// File read error
const SMB_ERR_WRITE			= -204;		// File write error
const SMB_ERR_TIMEOUT		= -205;		// File operation timed-out
const SMB_ERR_FILE_LEN		= -206;		// File length invalid
const SMB_ERR_DELETE		= -207;		// File deletion error
const SMB_ERR_UNLOCK		= -208;		// File unlock error
const SMB_ERR_MEM			= -300;		// Memory allocation error

/* MsgBase.attributes bit flags */
const SMB_EMAIL				= 1;		// User numbers stored in Indexes
const SMB_HYPERALLOC		= 2;		// No allocation (also storage value for smb_addmsghdr)
const SMB_NOHASH			= 4;		// Do not calculate or store hashes
									
/* Message Types */
const MSG_TYPE_NORMAL		= 0;		// Classic message (for reading)
const MSG_TYPE_POLL			= 1;		// A poll question 
const MSG_TYPE_BALLOT		= 2;		// Voter response to poll or normal message
const MSG_TYPE_POLL_CLOSURE	= 3;		// Closure of an existing poll

/* Message attributes */
const MSG_PRIVATE 			= (1<<0);
const MSG_READ				= (1<<1);
const MSG_PERMANENT			= (1<<2);
const MSG_LOCKED			= (1<<3);
const MSG_DELETE			= (1<<4);
const MSG_ANONYMOUS			= (1<<5);
const MSG_KILLREAD			= (1<<6);
const MSG_MODERATED			= (1<<7);
const MSG_VALIDATED			= (1<<8);
const MSG_REPLIED			= (1<<9);	// User replied to this message
const MSG_NOREPLY			= (1<<10);	// No replies (or bounces) should be sent to the sender
const MSG_UPVOTE			= (1<<11);	// This message is an upvote
const MSG_DOWNVOTE			= (1<<12);	// This message is a downvote
const MSG_POLL				= (1<<13);	// This message is a poll
const MSG_SPAM				= (1<<14);	// This message has been flagged as SPAM

const MSG_VOTE				= (MSG_UPVOTE|MSG_DOWNVOTE);// This message is a poll-vote
const MSG_POLL_CLOSURE		= (MSG_POLL|MSG_VOTE);		// This message is a poll-closure
const MSG_POLL_VOTE_MASK	= MSG_POLL_CLOSURE;

const MSG_POLL_MAX_ANSWERS	= 16;


/* Auxiliary header attributes */
const MSG_FILEREQUEST 		= (1<<0);	// File request
const MSG_FILEATTACH		= (1<<1);	// File(s) attached to Msg
const MSG_MIMEATTACH		= (1<<2);	// Truncate file(s) when sent
const MSG_KILLFILE			= (1<<3);	// Delete file(s) when sent
const MSG_RECEIPTREQ		= (1<<4);	// Return receipt requested
const MSG_CONFIRMREQ		= (1<<5);	// Confirmation receipt requested
const MSG_NODISP			= (1<<6);	// Msg may not be displayed to user
const MSG_HFIELDS_UTF8		= (1<<13);	// Message header fields are UTF-8 encoded
const POLL_CLOSED			= (1<<24);	// Closed to voting 
const POLL_RESULTS_MASK		= (3<<30);	// 4 possible values:
const POLL_RESULTS_SECRET	= (3<<30);	// No one but pollster can see results
const POLL_RESULTS_CLOSED	= (2<<30);	// No one but pollster can see results until poll is closed
const POLL_RESULTS_OPEN		= (1<<30);	// Results are visible to everyone always
const POLL_RESULTS_VOTERS	= (0<<30);	// Voters can see results right away, everyone else when closed
const POLL_RESULTS_SHIFT	= 30;


/* Message network attributes */
const MSG_LOCAL				= (1<<0);	// Msg created locally
const MSG_INTRANSIT			= (1<<1);	// Msg is in-transit
const MSG_SENT				= (1<<2);	// Sent to remote
const MSG_KILLSENT			= (1<<3);	// Kill when sent
const MSG_ARCHIVESENT 		= (1<<4);	// Archive when sent
const MSG_HOLD				= (1<<5);	// Hold for pick-up
const MSG_CRASH				= (1<<6);	// Crash
const MSG_IMMEDIATE			= (1<<7);	// Send Msg now, ignore restrictions
const MSG_DIRECT			= (1<<8);	// Send directly to destination

/* Net types */										
const NET_NONE				= 0;		// Local message
const NET_UNKNOWN			= 1;		// Networked, but unknown type
const NET_FIDO				= 2;		// FidoNet
const NET_POSTLINK			= 3;		// PostLink
const NET_QWK				= 4;		// QWK
const NET_INTERNET			= 5;		// NNTP
const NET_WWIV				= 6;		// WWIV
const NET_MHS				= 7;		// MHS

/* Agent types */										
const AGENT_PERSON			= 0;		// Human
const AGENT_PROCESS			= 1;		// Unknown process type
const AGENT_SMBUTIL			= 2;		// Imported via Synchronet SMBUTIL
const AGENT_SMTPSYSMSG		= 3;		// Synchronet SMTP server system message

/* Message Priority */
const SMB_PRIORITY_UNSPECIFIED 	= 0;
const SMB_PRIORITY_HIGHEST		= 1;
const SMB_PRIORITY_HIGH			= 2;
const SMB_PRIORITY_NORMAL		= 3;
const SMB_PRIORITY_LOW			= 4;
const SMB_PRIORITY_LOWEST		= 5;

/* Message hfield types */										
const SMB_SUMMARY			= 0x61; 	// Not currently used for messages								
const SMB_COMMENT 			= 0x62; 	// Appear in message text, before body
const SMB_TAGS				= 0x69; 	// Tags (ala hashtags) for a message
const SMB_TAG_DELIMITER		= ' ';		// Tags are space-separated
const SMB_POLL_ANSWER		= 0xe0;		// One poll answer (the subject is the question)

const FIDOCTRL     			= 0xa0;
const FIDOSEENBY   			= 0xa2;
const FIDOPATH     			= 0xa3;
const RFC822HEADER 			= 0xb0;
const SMTPRECEIVED 			= 0xd3;
