																	/* Values for MsgBase.status */
var SMB_SUCCESS			=0;		/* Successful result/return code */
var SMB_DUPE_MSG		=1;		/* Duplicate message detected by smb_addcrc() */
var SMB_FAILURE			=-1;	/* Generic error (discouraged) */
var SMB_ERR_NOT_OPEN	=-100;	/* Message base not open */
var SMB_ERR_HDR_LEN		=-101;	/* Invalid message header length (>64k) */
var SMB_ERR_HDR_OFFSET	=-102;	/* Invalid message header offset */
var SMB_ERR_HDR_ID		=-103;	/* Invalid header ID */
var SMB_ERR_HDR_VER		=-104;	/* Unsupported version */
var SMB_ERR_HDR_FIELD	=-105;	/* Missing header field */
var SMB_ERR_NOT_FOUND	=-110;	/* Item not found */
var SMB_ERR_DAT_OFFSET	=-120;	/* Invalid data offset (>2GB) */
var SMB_ERR_DAT_LEN		=-121;	/* Invalid data length (>2GB) */
var SMB_ERR_OPEN		=-200;	/* File open error */
var SMB_ERR_SEEK		=-201;	/* File seek/setpos error */
var SMB_ERR_LOCK		=-202;	/* File lock error */
var SMB_ERR_READ		=-203;	/* File read error */
var SMB_ERR_WRITE		=-204;	/* File write error */
var SMB_ERR_TIMEOUT		=-205;	/* File operation timed-out */
var SMB_ERR_FILE_LEN	=-206;	/* File length invalid */
var SMB_ERR_DELETE		=-207;	/* File deletion error */
var SMB_ERR_UNLOCK		=-208;	/* File unlock error */
var SMB_ERR_MEM			=-300;	/* Memory allocation error */
									
									/* Message Types */
var MSG_TYPE_NORMAL			= 0;	/* Classic message (for reading) */
var MSG_TYPE_POLL			= 1;	/* A poll question  */
var MSG_TYPE_BALLOT			= 2;	/* Voter response to poll or normal message */
var MSG_TYPE_POLL_CLOSURE	= 3;	/* Closure of an existing poll */

									/* Message attributes */
var MSG_PRIVATE 		=(1<<0);
var MSG_READ			=(1<<1);
var MSG_PERMANENT		=(1<<2);
var MSG_LOCKED			=(1<<3);
var MSG_DELETE			=(1<<4);
var MSG_ANONYMOUS		=(1<<5);
var MSG_KILLREAD		=(1<<6);
var MSG_MODERATED		=(1<<7);
var MSG_VALIDATED		=(1<<8);
var MSG_REPLIED			=(1<<9);	// User replied to this message
var MSG_NOREPLY			=(1<<10);	// No replies (or bounces) should be sent to the sender
var MSG_UPVOTE			=(1<<11);	// This message is an upvote
var MSG_DOWNVOTE		=(1<<12);	// This message is a downvote */
var MSG_POLL			=(1<<13);	// This message is a poll
var MSG_SPAM			=(1<<14);	// This message has been flagged as SPAM

var MSG_VOTE			=(MSG_UPVOTE|MSG_DOWNVOTE);	/* This message is a poll-vote */
var MSG_POLL_CLOSURE	=(MSG_POLL|MSG_VOTE);		/* This message is a poll-closure */
var MSG_POLL_VOTE_MASK	=MSG_POLL_CLOSURE;

var MSG_POLL_MAX_ANSWERS	=16;


									 /* Auxiliary header attributes */
var MSG_FILEREQUEST 	=(1<<0);	// File request
var MSG_FILEATTACH		=(1<<1);	// File(s) attached to Msg
var MSG_TRUNCFILE		=(1<<2);	// Truncate file(s) when sent
var MSG_KILLFILE		=(1<<3);	// Delete file(s) when sent
var MSG_RECEIPTREQ		=(1<<4);	// Return receipt requested
var MSG_CONFIRMREQ		=(1<<5);	// Confirmation receipt requested
var MSG_NODISP			=(1<<6);	// Msg may not be displayed to user
var POLL_CLOSED			=(1<<24);	// Closed to voting 
var POLL_RESULTS_MASK	=(3<<30);	// 4 possible values:
var POLL_RESULTS_SECRET	=(3<<30);	// No one but pollster can see results
var POLL_RESULTS_CLOSED	=(2<<30);	// No one but pollster can see results until poll is closed
var POLL_RESULTS_OPEN	=(1<<30);	// Results are visible to everyone always
var POLL_RESULTS_VOTERS	=(0<<30);	// Voters can see results right away, everyone else when closed
var POLL_RESULTS_SHIFT	=30;


								/* Message network attributes */
var MSG_LOCAL			=(1<<0);// Msg created locally
var MSG_INTRANSIT		=(1<<1);// Msg is in-transit
var MSG_SENT			=(1<<2);// Sent to remote
var MSG_KILLSENT		=(1<<3);// Kill when sent
var MSG_ARCHIVESENT 	=(1<<4);// Archive when sent
var MSG_HOLD			=(1<<5);// Hold for pick-up
var MSG_CRASH			=(1<<6);// Crash
var MSG_IMMEDIATE		=(1<<7);// Send Msg now, ignore restrictions
var MSG_DIRECT			=(1<<8);// Send directly to destination
var MSG_GATE			=(1<<9);// Send via gateway
var MSG_ORPHAN			=(1<<10);// Unknown destination
var MSG_FPU 			=(1<<11);// Force pickup
var MSG_TYPELOCAL		=(1<<12);// Msg is for local use only
var MSG_TYPEECHO		=(1<<13);// Msg is for conference distribution
var MSG_TYPENET 		=(1<<14);// Msg is direct network mail

								/* Net types */
var NET_NONE			=0;		// Local message
var NET_UNKNOWN			=1;		// Networked, but unknown type
var NET_FIDO			=2;		// FidoNet
var NET_POSTLINK		=3;		// PostLink
var NET_QWK				=4;		// QWK
var NET_INTERNET		=5;		// NNTP
var NET_WWIV			=6;		// WWIV
var NET_MHS				=7;		// MHS

								/* Agent types */
var AGENT_PERSON		=0;		/* Human */
var AGENT_PROCESS		=1;		/* Unknown process type */
var AGENT_SMBUTIL		=2;		/* Imported via Synchronet SMBUTIL */
var AGENT_SMTPSYSMSG	=3;		/* Synchronet SMTP server system message */

								/* Message hfield types */
var SMB_COMMENT 		= 0x62; /* Appear in message text, before body */
var SMB_POLL_ANSWER		= 0xe0;	/* One poll answer (the subject is the question) */

								/* "flags" bits for directory() */
var GLOB_MARK		=(1<<1);	/* Append a slash to each name.  */
var GLOB_NOSORT		=(1<<2);	/* Don't sort the names.  */
var GLOB_APPEND		=(1<<5);	/* Append to results of a previous call.  */
var GLOB_NOESCAPE   =(1<<6);	/* Backslashes don't quote metacharacters.  */
var GLOB_PERIOD     =(1<<7); 	/* Leading `.' can be matched by metachars.  */
var GLOB_ONLYDIR    =(1<<13);	/* Match only directories.  */

								/********************************************/
								/* Values for which in bbs.read_mail()		*/
								/********************************************/
var MAIL_YOUR			=0;		/* mail sent to you							*/
var MAIL_SENT			=1;		/* mail you have sent						*/
var MAIL_ANY			=2;		/* mail sent to or from you					*/
var MAIL_ALL			=3;		/* all mail (ignores usernumber arg)		*/
								/********************************************/
