// Synchronet constants, macros, and structure definitions

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

#ifndef _SBBSDEFS_H
#define _SBBSDEFS_H

#include <time.h>

#include "smbdefs.h"
#include "nodedefs.h"   // node_t
#include "fidodefs.h"   // fmsghdr_t, fpkthdr_t, FIDO_*, etc.
#include "xpbeep.h"     // BEEP()
#include "str_list.h"   // str_list_t

/*************/
/* Constants */
/*************/

#define VERSION     "3.21"  // Version: Major.minor as 4-char string
#define REVISION    'd'     // Revision: lowercase letter
#define VERSION_INT ((DEC_CHAR_TO_INT(VERSION[0]) * 100) + (DEC_CHAR_TO_INT(VERSION[2]) * 10) + DEC_CHAR_TO_INT(VERSION[3]))  // Version as increasing 3-decimal-digit integer value
#define VERSION_NUM (((VERSION_INT) * 100) + (tolower(REVISION) - 'a')) // Version and revision as a 5-decimal-digit integer value
#define VERSION_HEX (((VERSION_INT / 100) * 0x10000)  + (((VERSION_INT % 100) / 10) * 0x1000) + ((VERSION_INT % 10) * 0x100) + (tolower(REVISION) - 'a'))

#define VERSION_NOTICE      "Synchronet BBS for " PLATFORM_DESC \
		"  Version " VERSION
#define COPYRIGHT_NOTICE    "Copyright 2026 Rob Swindell"

#define SBBSCTRL_DEFAULT    "/sbbs/ctrl"

#define Y2K_2DIGIT_WINDOW   70

#define MAX_FILENAME_LEN        64
#define MAX_FILEEXT_LEN         15
#define ILLEGAL_FILENAME_CHARS  "\\/|<>:\";,%?*"
#define SAFEST_FILENAME_CHARS   "-._0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

#define BIND_FAILURE_HELP   "!Another application or service may be using this port"
#define UNKNOWN_LOAD_ERROR  "Unknown load error - Library mismatch?"

#define STR_UNKNOWN_USER    "<unknown user>"
#define STR_NO_HOSTNAME     "<no name>"

#define JAVASCRIPT_MAX_BYTES        (16 * 1024 * 1024)
#define JAVASCRIPT_CONTEXT_STACK    (8 * 1024)
#define JAVASCRIPT_TIME_LIMIT       (24 * 60 * 600)         // in 100ms ticks
#define JAVASCRIPT_YIELD_INTERVAL   10000
#define JAVASCRIPT_GC_INTERVAL      100
#define JAVASCRIPT_LOAD_PATH        "load"
#define JAVASCRIPT_LOAD_PATH_LIST   "load_path_list"
#define JAVASCRIPT_OPTIONS          0x810   // JSOPTION_JIT | JSOPTION_COMPILE_N_GO

#define JSVAL_NULL_OR_VOID(val)     (JSVAL_IS_NULL(val) || JSVAL_IS_VOID(val))

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/************/
/* Maximums */
/************/

#define MAX_NODES       250

#define MAX_TEXTDAT_ITEM_LEN    2000

#define LEN_DIR     100     // Maximum length of directory paths
#define LEN_CMD     100     // Maximum length of command lines

#define LEN_CODE    16      // Maximum length of internal codes
#define LEN_QWKID   8       // Maximum length of QWK-ID
#define LEN_SIFNAME 8       // Maximum length of SIF filename
#define LEN_EXTCODE (LEN_CODE * 2)    // Code prefix + suffix

#define LEN_DATE        8
#define LEN_DATETIME    24

// Lengths of various strings
#define LEN_GSNAME  15      // Group/Lib short name
#define LEN_GLNAME  60      // Group/Lib long name
#define LEN_SSNAME  25      // Sub/Dir short name
#define LEN_SLNAME  60      // Sub/Dir long name

#define LEN_FLAGSTR 32      // Flag sets are 32-bits each

// User Questions (e.g. for new users)
#define UQ_ALIASES      (1 << 0)  // Ask for alias
#define UQ_LOCATION     (1 << 1)  // Ask for location
#define UQ_ADDRESS      (1 << 2)  // Ask for address
#define UQ_PHONE        (1 << 3)  // Ask for phone number
#define UQ_HANDLE       (1 << 4)  // Ask for chat handle / HAM callsign
#define UQ_DUPHAND      (1 << 5)  // Search for duplicate handles
#define UQ_SEX          (1 << 6)  // Ask for gender
#define UQ_BIRTH        (1 << 7)  // Ask for birth date
#define UQ_UNUSED1      (1 << 8)  // was UQ_COMP
#define UQ_UNUSED2      (1 << 9)  // was UQ_MC_COMP
#define UQ_REALNAME     (1 << 10) // Ask for real name
#define UQ_DUPREAL      (1 << 11) // Search for duplicate real names
#define UQ_COMPANY      (1 << 12) // Ask for company name
#define UQ_NOEXASC      (1 << 13) // Don't allow ex-ASCII in user text (same as K_NOEXASC)
#define UQ_CMDSHELL     (1 << 14) // Ask for command shell
#define UQ_XEDIT        (1 << 15) // Ask for external editor
#define UQ_NODEF        (1 << 16) // Don't ask for default settings
#define UQ_NOCOMMAS     (1 << 17) // Do not require commas in location
#define UQ_NONETMAIL    (1 << 18) // Don't ask for e-mail/netmail address
#define UQ_NOUPRLWR     (1 << 19) // Don't force upper/lower case strings
#define UQ_COLORTERM    (1 << 20) // Ask if new user has color terminal
#define UQ_DUPNETMAIL   (1 << 21) // Don't allow duplicate e-mail address
#define UQ_NOSPACEREQ   (1 << 22) // Don't require space in real names

#define DEFAULT_NEWUSER_QS (UQ_ALIASES | UQ_LOCATION | UQ_HANDLE | UQ_DUPHAND | UQ_SEX | UQ_BIRTH \
							| UQ_REALNAME)

// Different bits in sys_misc
#define SM_CLOSED       (1 << 0)  // System is closed to New Users
#define SM_SYSSTAT      (1 << 1)  // Sysops activity included in statistics
#define SM_NOSYSINFO    (1 << 2)  // Suppress system info display at logon
#define SM_PWEDIT       (1 << 3)  // Allow users to change their passwords
#define SM_RA_EMU       (1 << 4)  // Reverse R/A commands at msg read prompt
#define SM_ANON_EM      (1 << 5)  // Allow anonymous e-mail
#define SM_LISTLOC      (1 << 6)  // Use location of caller in user lists
#define SM_WILDCAT      (1 << 7)  // Expand Wildcat color codes in messages
#define SM_PCBOARD      (1 << 8)  // Expand PCBoard color codes in messages
#define SM_WWIV         (1 << 9)  // Expand WWIV color codes in messages
#define SM_CELERITY     (1 << 10) // Expand Celerity color codes in messages
#define SM_RENEGADE     (1 << 11) // Expand Renegade color codes in messages
#define SM_ECHO_PW      (1 << 12) // Echo passwords locally
#define SM_SYSPASSLOGIN (1 << 13) // Require system password for sysop login
#define SM_AUTO_DST     (1 << 14) // Automatic Daylight Savings Toggle (US)
#define SM_R_SYSOP      (1 << 15) // Allow remote sysop login/commands
#define SM_QUOTE_EM     (1 << 16) // Allow quoting of e-mail
#define SM_EURODATE     (1 << 17) // European date format (DD/MM/YY) - DEPRECATED
#define SM_MILITARY     (1 << 18) // Military (24hr) time format
#define SM_TIMEBANK     (1 << 19) // Allow time bank functions
#define SM_FILE_EM      (1 << 20) // Allow file attachments in E-mail
#define SM_SHRTPAGE     (1 << 21) // Short sysop page
#define SM_TIME_EXP     (1 << 22) // Set to expired values if out-of-time
#define SM_FASTMAIL     (1 << 23) // Fast e-mail storage mode
#define SM_NONODELIST   (1 << 24) // Suppress active node list during logon
#define SM_MOUSE_HOT    (1 << 25) // Mouse hotspot support
#define SM_FWDTONET     (1 << 26) // Allow forwarding of e-mail to netmail
#define SM_DELREADM     (1 << 27) // Delete read mail automatically
#define SM_NOCDTCVT     (1 << 28) // No credit to minute conversions allowed
#define SM_DELEMAIL     (1 << 29) // Physically remove deleted e-mail immed.
#define SM_USRVDELM     (1 << 30) // Users can see deleted msgs
#define SM_SYSVDELM     (1U << 31)// Sysops can see deleted msgs

#define SM_XATTR_SUPPORT (SM_WWIV | SM_WILDCAT | SM_PCBOARD | SM_CELERITY | SM_RENEGADE)

// Bit flags for scfg_t.login
#define LOGIN_USERNUM   (1 << 0)  // Allow logins by user number
#define LOGIN_REALNAME  (1 << 1)  // Allow logins by user's real name
#define LOGIN_PWPROMPT  (1 << 2)  // Always display password prompt, even for bad login-id

// Different bits in node_misc
#define NM_NO_NUM       (1 << 8)  // Don't allow logins by user number (deprecated)
#define NM_LOGON_R      (1 << 9)  // Allow logins by user real name (deprecated)
#define NM_LOGON_P      (1 << 10) // Secure logins (always ask for password) (deprecated)
#define NM_LOWPRIO      (1 << 15) // Always use low priority input
#define NM_7BITONLY     (1 << 16) // Except 7-bit input only (E71 terminals)
#define NM_NOPAUSESPIN  (1 << 18) // No spinning cursor at pause prompt
#define NM_CLOSENODEDAB (1 << 19) // Keep node.dab file closed (for Samba)

// Bit values for level_misc[x]
#define LEVEL_EXPTOLVL  (1 << 0)      // Expire to level_expireto[x]
#define LEVEL_EXPTOVAL  (1 << 1)      // Expire to val[level_expireto[x]]

// Bit values for prot[x].misc
#define PROT_DSZLOG     (1 << 0)      // Supports DSZ Log
#define PROT_NATIVE     (1 << 1)      // Native (not MS-DOS) executable
#define PROT_SOCKET     (1 << 2)      // Use socket I/O, not stdio on *nix

// Bit values in netmail_misc

#define NMAIL_ALLOW     (1 << 0)      // Allow NetMail
#define NMAIL_CRASH     (1 << 1)      // Default netmail to crash
#define NMAIL_HOLD      (1 << 2)      // Default netmail to hold
#define NMAIL_KILL      (1 << 3)      // Default netmail to kill after sent
#define NMAIL_ALIAS     (1 << 4)      // Use Aliases in NetMail
#define NMAIL_FILE      (1 << 5)      // Allow file attachments
#define NMAIL_DIRECT    (1 << 6)      // Default netmail to direct
#define NMAIL_CHSRCADDR (1 << 7)      // Allow sender to choose source address

// Bit values for subscan_t.cfg and sav_cfg
#define SUB_CFG_NSCAN   0x0005      // Auto-scan for new messages
#define SUB_CFG_SSCAN   0x0002      // Auto-scan for unread messages to you
#define SUB_CFG_YSCAN   0x0100      // Auto-scan for new messages to you only
                                    // (bits 8-15 default to off)

// Bit values for sub[x].misc
#define SUB_NOVOTING    (1 << 0)      // No voting allowed in this sub-board
#define SUB_TEMPLATE    (1 << 1)      // Use this sub as template for new subs (in this group)
#define SUB_MSGTAGS     (1 << 2)      // Allow messages to be tagged (ala hashtags)
#define SUB_QNET        (1 << 3)      // Sub-board is netted via QWK network
#define SUB_PNET        (1 << 4)      // Sub-board is netted via PostLink
#define SUB_FIDO        (1 << 5)      // Sub-board is netted via FidoNet
#define SUB_PRIV        (1 << 6)      // Allow private posts on sub
#define SUB_PONLY       (1 << 7)      // Private posts only
#define SUB_ANON        (1 << 8)      // Allow anonymous posts on sub
#define SUB_AONLY       (1 << 9)      // Anonymous only
#define SUB_NAME        (1 << 10)     // Must use real names
#define SUB_DEL         (1 << 11)     // Allow users to delete messages
#define SUB_DELLAST     (1 << 12)     // Allow users to delete last msg only
#define SUB_FORCED      (1 << 13)     // Sub-board is forced scanning
#define SUB_NOTAG       (1 << 14)     // Don't add tag or origin lines
#define SUB_TOUSER      (1 << 15)     // Prompt for to user on posts
#define SUB_ASCII       (1 << 16)     // ASCII characters only
#define SUB_QUOTE       (1 << 17)     // Allow online quoting
#define SUB_NSDEF       (1 << 18)     // New-Scan on by default
#define SUB_INET        (1 << 19)     // Sub-board is netted via Internet
#define SUB_FAST        (1 << 20)     // Fast storage mode
#define SUB_KILL        (1 << 21)     // Kill read messages automatically (NOT IMPLEMENTED)
#define SUB_KILLP       (1 << 22)     // Kill read pvt messages automatically (NOT IMPLEMENTED)
#define SUB_SYSPERM     (1 << 23)     // Sysop messages are permanent
#define SUB_GATE        (1 << 24)     // Gateway between Network types
#define SUB_LZH         (1 << 25)     // Use LZH compression for msgs
#define SUB_SSDEF       (1 << 26)     // Default ON for Scan for Your msgs
#define SUB_HYPER       (1 << 27)     // Hyper allocation
#define SUB_EDIT        (1 << 28)     // Users can edit message text after posting
#define SUB_EDITLAST    (1 << 29)     // Users can edit last message only
#define SUB_NOUSERSIG   (1 << 30)     // Suppress user signatures

// Bit values for lib[x].misc
#define LIB_DIRS    (1 << 0)          // Local directory (sub-directory of lib parent) access

// Bit values for dir[x].misc
#define DIR_FCHK        (1 << 0)      // Check for file existence
#define DIR_RATE        (1 << 1)      // Force uploads to be rated G,R, or X
#define DIR_MULT        (1 << 2)      // Ask for multi-disk numbering
#define DIR_DUPES       (1 << 3)      // Search this dir for upload dupes
#define DIR_FREE        (1 << 4)      // Free downloads
#define DIR_TFREE       (1 << 5)      // Time to download is free
#define DIR_CDTUL       (1 << 6)      // Credit Uploads
#define DIR_CDTDL       (1 << 7)      // Credit Downloads
#define DIR_ANON        (1 << 8)      // Anonymous uploads
#define DIR_AONLY       (1 << 9)      // Anonymous only
#define DIR_ULDATE      (1 << 10)     // Include upload date in listing
#define DIR_DIZ         (1 << 11)     // FILE_ID.DIZ and DESC.SDI support
#define DIR_NOSCAN      (1 << 12)     // Don't new-scan this directory
#define DIR_NOAUTO      (1 << 13)     // Don't auto-add this directory
#define DIR_ULTIME      (1 << 14)     // Deduct time during uploads
#define DIR_CDTMIN      (1 << 15)     // Give uploader minutes instead of cdt
#define DIR_SINCEDL     (1 << 16)     // Purge based on days since last dl
#define DIR_MOVENEW     (1 << 17)     // Files marked as new when moved
#define DIR_QUIET       (1 << 18)     // Do not notify uploader of downloads
#define DIR_NOSTAT      (1 << 19)     // Do not include transfers in system stats
#define DIR_FILES       (1 << 20)     // List/access files not in database (not used)
#define DIR_TEMPLATE    (1 << 21)     // Use this dir as template for new dirs (in this lib) - DEPRECATED
#define DIR_NOHASH      (1 << 22)     // Don't auto calculate/store file content hashes
#define DIR_FILETAGS    (1 << 23)     // Allow files to have user-specified tags

#define DEFAULT_DIR_OPTIONS (DIR_FCHK | DIR_DUPES | DIR_CDTUL | DIR_CDTDL | DIR_DIZ)

#define FM_SAFEST       (1 << 1)      // Allow safest filenames to be uploaded only
#define FM_SPACES       (1 << 2)      // Allow spaces in uploaded filenames
#define FM_EXASCII      (1 << 3)      // Allow extended-ASCII (or UTF-8) in uploaded filenames

// Bit values for cfg.msg_misc (upper 16-bits default to on)
#define MM_REALNAME (1 << 16)         // Allow receipt of e-mail using real names
#define MM_EMAILSIG (1 << 17)         // Include user signatures in e-mail msgs

// errormsg() codes
#define ERR_OPEN    "opening"       // opening a file
#define ERR_CLOSE   "closing"       // close a file
#define ERR_FDOPEN  "fdopen"        // associating a stream with fd
#define ERR_READ    "reading"       // reading from file
#define ERR_WRITE   "writing"       // writing to file
#define ERR_REMOVE  "removing"      // removing a file
#define ERR_ALLOC   "allocating"    // allocating memory
#define ERR_CHK     "checking"      // checking
#define ERR_LEN     "checking length"
#define ERR_EXEC    "executing"     // executing
#define ERR_MKDIR   "making directory"
#define ERR_CHDIR   "changing directory"
#define ERR_CREATE  "creating"      // creating
#define ERR_LOCK    "locking"       // locking
#define ERR_UNLOCK  "unlocking"     // unlocking
#define ERR_TIMEOUT "timeout"       // timeout waiting for resource
#define ERR_IOCTL   "sending IOCTL" // IOCTL error
#define ERR_SEEK    "seeking"       // SEEKing error

enum file_sort {                    // Values for dir[x].sort
	  FILE_SORT_NAME_A   = 0        // Sort by filename, ascending (case-insensitive)
	, FILE_SORT_NAME_D   = 1        // Sort by filename, descending (case-insensitive)
	, FILE_SORT_NAME_AC  = 4        // Sort by filename, ascending (case-sensitive)
	, FILE_SORT_NAME_DC  = 5        // Sort by filename, descending (case-sensitive)
	, FILE_SORT_DATE_A   = 2        // Sort by upload date, ascending
	, FILE_SORT_DATE_D   = 3        // Sort by upload date, descending
	, FILE_SORT_SIZE_A   = 6        // Sort by file size, ascending
	, FILE_SORT_SIZE_D   = 7        // Sort by file size, descending
	, FILE_SORT_NATURAL  = 8        // No sorting
};

// Values for grp[x].sort
enum area_sort {
	AREA_SORT_NONE,
	AREA_SORT_LNAME,
	AREA_SORT_SNAME,
	AREA_SORT_CODE,
	AREA_SORT_TYPES,
};

// Values for lib[x].vdir_name
enum vdir_name {
	VDIR_NAME_CODE,
	VDIR_NAME_SHORT,
	VDIR_NAME_LONG
};

enum {
	  clr_mnehigh
	, clr_mnelow
	, clr_mnecmd
	, clr_inputline
	, clr_err
	, clr_nodenum
	, clr_nodeuser
	, clr_nodestatus
	, clr_filename
	, clr_filecdt
	, clr_filedesc
	, clr_filelsthdrbox
	, clr_filelstline
	, clr_chatlocal
	, clr_chatremote
	, clr_multichat
	, clr_external
	, clr_votes_full
	, clr_votes_empty
	, clr_progress_full
	, clr_progress_empty
	, clr_userlow
	, clr_userhigh
	, NUM_COLORS
};

enum {                               // Values for xtrn_t.type
	  XTRN_NONE                      // No data file needed
	, XTRN_SBBS                      // Synchronet external
	, XTRN_WWIV                      // WWIV external
	, XTRN_DOOR_SYS                  // 52-line door.sys file
	, XTRN_RBBS                      // RBBS, QBBS, or Remote Access
	, XTRN_WILDCAT                   // Wildcat
	, XTRN_PCBOARD                   // PCBoard
	, XTRN_SPITFIRE                  // SpitFire
	, XTRN_GAP                       // 31-line doors.sys file
	, XTRN_SR                        // Solar Realms
	, XTRN_RBBS1                     // DORINFO1.DEF always
	, XTRN_TRIBBS                    // TRIBBS.SYS
	, XTRN_DOOR32                    // DOOR32.SYS
};

typedef enum {                       // Values for xtrn_t.event
	  EVENT_NONE                     // Only accessible by menu
	, EVENT_LOGON                    // Execute during logon sequence
	, EVENT_LOGOFF                   // Execute during logoff sequence
	, EVENT_NEWUSER                  // Execute during newuser app.
	, EVENT_BIRTHDAY                 // Execute on birthday
	, EVENT_POST                     // Execute after posting a message
	, EVENT_UPLOAD                   // Execute after uploading a file
	, EVENT_DOWNLOAD                 // Execute after downloading a file
	, EVENT_LOCAL_CHAT               // Execute upon local/sysop chat
} user_event_t;

// Misc bits for event_t.misc
#define EVENT_EXCL      (1 << 0)      // Exclusive
#define EVENT_FORCE     (1 << 1)      // Force users off-line for event
#define EVENT_INIT      (1 << 2)      // Always run event after init
#define EVENT_DISABLED  (1 << 3)      // Disabled

#define NODE_ANY        0           // special qhub/event_t.node value

// Bits in xtrn_t.misc
#define MULTIUSER       (1 << 0)      // allow multi simultaneous users
#define XTRN_ANSI       (1 << 1)      // LEGACY (not used)
#define XTRN_STDIO      (1 << 2)      // Intercept Standard I/O (aka IO_INTS)
#define MODUSERDAT      (1 << 3)      // Program can modify user data
#define WWIVCOLOR       (1 << 4)      // Program uses WWIV color codes
#define EVENTONLY       (1 << 5)      // Program executes as event only
#define STARTUPDIR      (1 << 6)      // Create drop file in start-up dir
#define REALNAME        (1 << 7)      // Use real name in drop file
#define SWAP            (1 << 8)      // Swap for this door
#define FREETIME        (1 << 9)      // Free time while in this door
#define QUICKBBS        (1 << 10)     // QuickBBS style editor
#define EXPANDLF        (1 << 11)     // Expand LF to CRLF editor (deprecated)
#define QUOTEALL        (1 << 12)     // Automatically quote all of msg
#define QUOTENONE       (1 << 13)     // Automatically quote none of msg
#define XTRN_NATIVE     (1 << 14)     // Native application (EX_NATIVE)
#define STRIPKLUDGE     (1 << 15)     // Strip FTN Kludge lines from msg
#define XTRN_CHKTIME    (1 << 16)     // Check time online (EX_CHKTIME)
#define XTRN_LWRCASE    (1 << 17)     // Use lowercase drop-file names
#define XTRN_SH         (1 << 18)     // Use command shell to execute
#define XTRN_PAUSE      (1 << 19)     // Force a screen pause on exit
#define XTRN_NOECHO     (1 << 20)     // Don't echo stdin to stdout
#define QUOTEWRAP       (1 << 21)     // Word-wrap quoted message text
#define SAVECOLUMNS     (1 << 22)     // Save/share current terminal width
#define XTRN_UTF8       (1 << 23)     // External program supports UTF-8
#define XTRN_TEMP_DIR   (1 << 24)     // Place drop files in temp dir
#define XTRN_UART       (1 << 25)     // Enable the virtual UART driver
#define XTRN_FOSSIL     (1 << 26)     // Enable the int14h/FOSSIL driver
#define XTRN_NODISPLAY  (1 << 27)     // Disable local screen/display
#define KEEP_CTRL_A     (1 << 28)     // Strip Ctrl-A quotes from quoted text
#define XTRN_CONIO      (1U << 31)    // Intercept Windows Console I/O (Drwy)

// Bits in user.qwk
#define QWK_FILES       (1 << 0)      // Include new files list
#define QWK_EMAIL       (1 << 1)      // Include unread e-mail
#define QWK_ALLMAIL     (1 << 2)      // Include ALL e-mail
#define QWK_DELMAIL     (1 << 3)      // Delete e-mail after download
#define QWK_BYSELF      (1 << 4)      // Include messages from self
#define QWK_UNUSED      (1 << 5)      // Currently unused
#define QWK_EXPCTLA     (1 << 6)      // Expand ctrl-a codes to ascii
#define QWK_RETCTLA     (1 << 7)      // Retain ctrl-a codes
#define QWK_ATTACH      (1 << 8)      // Include file attachments
#define QWK_NOINDEX     (1 << 9)      // Do not create index files in QWK
#define QWK_TZ          (1 << 10)     // Include "@TZ" time zone in msgs
#define QWK_VIA         (1 << 11)     // Include "@VIA" seen-bys in msgs
#define QWK_NOCTRL      (1 << 12)     // No extraneous control files
#define QWK_EXT         (1 << 13)     // QWK Extended (QWKE) format
#define QWK_MSGID       (1 << 14)     // Include "@MSGID" in msgs
#define QWK_HEADERS     (1 << 16)     // Include HEADERS.DAT file
#define QWK_VOTING      (1 << 17)     // Include VOTING.DAT
#define QWK_UTF8        (1 << 18)     // Include UTF-8 characters
#define QWK_WORDWRAP    (1 << 19)     // Word-wrap message text
#define QWK_MIME        (1 << 20)     // Include MIME-encoded message text

#define QWK_DEFAULT (QWK_FILES | QWK_ATTACH | QWK_EMAIL | QWK_DELMAIL)

#define QHUB_EXPCTLA    (1 << 0)      // Same as QM_EXPCTLA
#define QHUB_RETCTLA    (1 << 1)      // Same as QM_RETCTLA
#define QHUB_CTRL_A     (QHUB_EXPCTLA | QHUB_RETCTLA)
#define QHUB_STRIP      0
#define QHUB_EXT        (1 << 13)     // Use QWKE format
#define QHUB_NOKLUDGES  (1 << 14)     // Don't include @-kludges
#define QHUB_NOHEADERS  (1 << 16)     // Don't include HEADERS.DAT
#define QHUB_NOVOTING   (1 << 17)     // Don't include VOTING.DAT
#define QHUB_UTF8       (1 << 18)     // Include UTF-8 characters
#define QHUB_WORDWRAP   (1 << 19)     // Word-wrap message text
#define QHUB_MIME       (1 << 20)     // Include MIME-encoded message text
#define QHUB_NATIVE     (1 << 31)     // Native call-out script

// Bits in user.chat
#define CHAT_ECHO   (1 << 0)  // Multinode chat echo
#define CHAT_ACTION (1 << 1)  // Chat actions
#define CHAT_NOPAGE (1 << 2)  // Can't be paged
#define CHAT_NOACT  (1 << 3)  // No activity alerts
#define CHAT_SPLITP (1 << 4)  // Split screen private chat

#define CHAT_DEFAULT    CHAT_SPLITP

// Bits in user.mail
#define MAIL_LM_MODE    (LM_REVERSE)

#define INVALID_DIR (-1)    // Invalid directory value
#define INVALID_SUB (-1)    // Invalid sub-board value

#define KEY_BUFSIZE 1024    // Size of keyboard input buffer
#define LINE_BUFSIZE 512    // Size of line output buffer


#define EDIT_TABSIZE 4      // Tab size for internal message/line editor

// Terminal Console I/O Bits (console)
#define CON_PASSWORD    (1 << 1)  // Password input mode, echo *'s to remote user
#define CON_PAUSE       (1 << 4)  // Temporary pause over-ride (same as UPAUSE)
#define CON_RAW_IN      (1 << 8)  // Raw input mode - no editing capabilities
#define CON_RIGHTARROW  (1 << 9)  // Right arrow hit, exiting from getstr()
#define CON_ECHO_OFF    (1 << 10) // Output disabled
#define CON_UPARROW     (1 << 11) // Up arrow hit - move up one line
#define CON_DOWNARROW   (1 << 12) // Down arrow hit, exiting from getstr()
#define CON_NO_INACT    (1 << 13) // Console inactivity detection disabled
#define CON_BACKSPACE   (1 << 14) // Backspace hit, exiting from getstr()
#define CON_LEFTARROW   (1 << 15) // Left arrow hit, exiting from getstr()
#define CON_INSERT      (1 << 16) // Insert mode, for getstr()
#define CON_DELETELINE  (1 << 17) // Deleted line, exiting from getstr()
#define CON_NORM_FONT   (1 << 18) // Alt normal font set activated
#define CON_HIGH_FONT   (1 << 19) // Alt high-intensity font activated
#define CON_BLINK_FONT  (1 << 20) // Alt blink attribute font activated
#define CON_HBLINK_FONT (1 << 21) // Alt high-blink attribute font activated
#define CON_MOUSE_CLK_PASSTHRU  (1 << 24) // Pass-through unhandled mouse button-click reports
#define CON_MOUSE_REL_PASSTHRU  (1 << 25) // Pass-through unhandled mouse button-release reports
#define CON_MOUSE_SCROLL        (1 << 26) // Mouse-reports enabled for non-hotspots (e.g. scroll-wheel)
#define CON_CR_CLREOL           (1U << 31)// outchar('\r') clears to end-of-line first

// Number of milliseconds
#define DELAY_AUTOHG 1500   // Delay for auto-hangup (xfer)

#define SEC_LOGON   1800    // 30 minutes allowed to logon
#define SEC_BILLING   90    // under 2 minutes per billing call
#define SEC_OK         5    // Attempt to get an OK response from modem
#define SEC_ANSI       5    // Attempt to get a valid ANSI response
#define SEC_ANSWER    30    // Retries to get an answer code from modem
#define SEC_CID       10    // Ten second pause for caller ID
#define SEC_RING       6    // Maximum seconds between rings

#define LOOP_NODEDAB  200   // Retries on node.dab locking/unlocking

// String lengths
#define LEN_ALIAS       25  // User alias
#define LEN_NAME        25  // User name
#define LEN_LANG        8   // Language code
#define LEN_HANDLE      8   // User chat handle
#define LEN_NOTE        30  // User note
#define LEN_HOST        60  // User hostname
#define LEN_COMMENT     60  // User comment
#define LEN_NETMAIL     60  // NetMail forwarding address
#define LEN_PHONE       12  // User phone number
#define LEN_BIRTH        8  // Birthday in MM/DD/YY format
#define LEN_ADDRESS     30  // User address
#define LEN_LOCATION    30  // Location (City, State)
#define LEN_ZIPCODE     10  // Zip/Postal code
#define LEN_CONNECTION  8   // Connection description
#define LEN_FDESC       58  // File description (summary)
#define LEN_EXTDESC     5000    // Extended file description
#define LEN_TITLE       70  // Message title
#define LEN_PASS        40
#define MIN_PASS_LEN     4
#define RAND_PASS_LEN    8
#define LEN_IPADDR      45
#define LEN_CID         45  // Caller ID (phone number)
#define LEN_ARSTR       40  // Max length of Access Requirement string
#define LEN_CHATACTCMD   9  // Chat action command
#define LEN_CHATACTOUT  65  // Chat action output string
#define LEN_RAINBOW     40  // Rainbow attribute array length

#define SIF_MAXBUF  0x7000          // Maximum buffer size of SIF data

// NOTE: Do not change the values of the following block of defines!

#define DELETED     (1 << 0)      // Bit values for user.misc
#define ANSI        (1 << 1)      // Supports ANSI terminal emulation
#define COLOR       (1 << 2)      // Send color codes
#define RIP         (1 << 3)      // Supports RIP terminal emulation
#define UPAUSE      (1 << 4)      // Pause on every screen full
#define SPIN        (1 << 5)      // Spinning cursor - Same as K_SPIN
#define INACTIVE    (1 << 6)      // Inactive user slot
#define EXPERT      (1 << 7)      // Expert menu mode
#define ANFSCAN     (1 << 8)      // Auto New file scan
#define CLRSCRN     (1 << 9)      // Clear screen before each message
#define QUIET       (1 << 10)     // Quiet mode upon logon
#define BATCHFLAG   (1 << 11)     // File list allow batch dl flags
#define NETMAIL     (1 << 12)     // Forward e-mail to fidonet addr
#define CURSUB      (1 << 13)     // Remember current sub-board/dir
#define ASK_NSCAN   (1 << 14)     // Ask for newscanning upon logon
#define NO_EXASCII  (1 << 15)     // Don't send extended ASCII
#define ASK_SSCAN   (1 << 16)     // Ask for messages to you at logon
#define AUTOTERM    (1 << 17)     // Autodetect terminal type
#define COLDKEYS    (1 << 18)     // No hot-keys
#define EXTDESC     (1 << 19)     // Extended file descriptions
#define AUTOHANG    (1 << 20)     // Auto-hang-up after transfer
#define WIP_UNUSED  (1 << 21)     // Supports WIP terminal emulation
#define AUTOLOGON   (1 << 22)     // AutoLogon via IP
#define HTML_UNUSED (1 << 23)     // Using Zuul/HTML terminal
#define NOPAUSESPIN (1 << 24)     // No spinning cursor at pause prompt
#define PETSCII     (1 << 26)     // Commodore PET/CBM terminal
#define SWAP_DELETE (1 << 27)     // Swap Delete and Backspace keys
#define ICE_COLOR   (1 << 28)     // Bright background color support
#define UTF8        (1 << 29)     // UTF-8 terminal
#define MOUSE       (1U << 31)    // Mouse supported terminal

// TODO: Really, NO_EXASCII  and UTF8 are not terminal flags.
#define TERM_FLAGS      (ANSI | COLOR | RIP | SWAP_DELETE | ICE_COLOR | MOUSE | CHARSET_FLAGS)
// TODO: Picking these out gets tricky, PETSCII is both terminal and charset
#define CHARSET_FLAGS   (NO_EXASCII | PETSCII | UTF8)
#define CHARSET_ASCII   NO_EXASCII  // US-ASCII
#define CHARSET_PETSCII PETSCII     // CBM-ASCII
#define CHARSET_UTF8    UTF8
#define CHARSET_CP437   0

// Special terminal key mappings
#define TERM_KEY_HOME   CTRL_B
#define TERM_KEY_END    CTRL_E
#define TERM_KEY_UP     CTRL_CARET
#define TERM_KEY_DOWN   CTRL_J
#define TERM_KEY_LEFT   CTRL_CLOSE_BRACKET
#define TERM_KEY_RIGHT  CTRL_F
#define TERM_KEY_INSERT CTRL_V
#define TERM_KEY_DELETE DEL
#define TERM_KEY_ABORT  CTRL_C
#define TERM_KEY_PAGEUP CTRL_P
#define TERM_KEY_PAGEDN CTRL_N

#define TERM_COLS_AUTO      0
#define TERM_COLS_MIN       40
#define TERM_COLS_MAX       999
#define TERM_COLS_DEFAULT   80
#define TERM_ROWS_AUTO      0
#define TERM_ROWS_MIN       8   // Amstrad NC100 has an 8-line display
#define TERM_ROWS_MAX       999
#define TERM_ROWS_DEFAULT   24

// Online status (online)
#define ON_LOCAL    1       // Online locally
#define ON_REMOTE   2       // Online remotely

// Various SYSTEM parameters for sys_status
#define SS_TMPSYSOP     (1 << 2)  // Temporary Sysop Status
#define SS_USERON       (1 << 3)  // A User is logged on to the BBS
#define SS_LCHAT        (1 << 4)  // Local chat in progress
#define SS_NEW_MONTH    (1 << 8)  // Execute System Monthly Event
#define SS_NEW_DAY      (1 << 9)  // Execute System Daily Event
#define SS_NEW_WEEK     (1 << 10) // Execute System Weekly Event
#define SS_ABORT        (1 << 11) // Global abort input or output flag
#define SS_SYSPAGE      (1 << 12) // Paging sysop
#define SS_GURUCHAT     (1 << 14) // Guru chat in progress
#define SS_EVENT        (1 << 16) // Time shortened due to upcoming event
#define SS_PAUSEON      (1 << 17) // Pause on, overriding user default
#define SS_PAUSEOFF     (1 << 18) // Pause off, overriding user default
#define SS_IN_CTRLP     (1 << 19) // Inside ctrl-p send node message func
#define SS_NEWUSER      (1 << 20) // New User online
#define SS_NEST_PF      (1 << 22) // Nested in printfile function
#define SS_SPLITP       (1 << 24) // Split-screen private chat
#define SS_DATE_CHANGED (1 << 25) // Date changed while online
#define SS_RLOGIN       (1 << 26) // Current login via BSD RLogin
#define SS_FILEXFER     (1 << 27) // File transfer in progress, halt spy
#define SS_SSH          (1 << 28) // Current login via SSH
#define SS_MOFF         (1 << 29) // Disable automatic messages
#define SS_QWKLOGON     (1 << 30) // QWK logon
#define SS_FASTLOGON    (1U << 31)// Fast logon

#define SS_DAILY        (SS_NEW_DAY | SS_NEW_MONTH | SS_NEW_WEEK)

// Bits in 'mode' for getkey and getstr
#define K_NONE      0             // Use as a place holder for no mode flags
#define K_UPPER     (1 << 0)      // Converts all letters to upper case
#define K_UPRLWR    (1 << 1)      // Upper/Lower case automatically
#define K_NUMBER    (1 << 2)      // Allow numbers only
#define K_WORDWRAP  (1 << 3)      // Allows word wrap
#define K_MSG       (1 << 4)      // Allows ANSI, ^N ^A ^G
#define K_SPIN      (1 << 5)      // Spinning cursor (same as SPIN)
#define K_LINE      (1 << 6)      // Input line (inverse color)
#define K_EDIT      (1 << 7)      // Edit string passed
#define K_CHAT      (1 << 8)      // In chat multi-chat
#define K_NOCRLF    (1 << 9)      // Don't print CRLF after string input
#define K_ALPHA     (1 << 10)     // Only allow alphabetic characters
#define K_GETSTR    (1 << 11)     // getkey called from getstr()
#define K_LOWPRIO   (1 << 12)     // low priority input
#define K_NOEXASC   (1 << 13)     // No extended ASCII allowed
#define K_E71DETECT (1 << 14)     // Detect E-7-1 terminal type
#define K_AUTODEL   (1 << 15)     // Auto-delete text (used with K_EDIT)
#define K_COLD      (1 << 16)     // Possible cold key mode
#define K_NOECHO    (1 << 17)     // Don't echo input
#define K_TAB       (1 << 18)     // Treat TAB key as CR
#define K_LEFTEXIT  (1 << 19)     // Allow exit by backspacing/arrowing left
#define K_USEOFFSET (1 << 20)     // Use getstr_offset for cursor	position
#define K_NOSPIN    (1 << 21)     // Do not honor the user's spinning cursor
#define K_ANSI_CPR  (1 << 22)     // Expect ANSI Cursor Position Report
#define K_TRIM      (1 << 23)     // Trimmed white-space
#define K_CTRLKEYS  (1 << 24)     // No control-key handling/eating in inkey()
#define K_NUL       (1 << 25)     // Return NOINP on timeout instead of '\0'
#define K_UTF8      (1 << 26)     // Don't translate UTF-8 input into CP437
#define K_RIGHTEXIT (1 << 27)     // Allow exit by arrowing right
#define K_LINEWRAP  (1 << 29)     // Allow string input to wrap the terminal
#define K_EXTKEYS   (1 << 30)     // Like K_CTRLKEYS, but inkey() still performs extended (e.g. arrow) key translations

// Bits in 'mode' for putmsg and printfile
#define P_NONE      0             // No mode flags
#define P_NOABORT   (1 << 0)      // Disallows abortion of a message
#define P_SAVEATR   (1 << 1)      // Save the new current attributes after
                                  // msg has printed.
#define P_NOATCODES (1 << 2)      // Don't allow @ codes
#define P_OPENCLOSE (1 << 3)      // Open and close the file
#define P_NOPAUSE   (1 << 4)      // Disable screen pause
#define P_SEEK      (1 << 5)      // Support jumping around via Home/End/PgUp/Dn, etc.
#define P_NOCRLF    (1 << 6)      // Don't prepend a CRLF	in printfile()
#define P_WORDWRAP  (1 << 7)      // Word-wrap long lines for user's terminal
#define P_CPM_EOF   (1 << 8)      // Ignore Ctrl-Z chars (CPM End-of-File)
#define P_TRUNCATE  (1 << 9)      // Truncate (don't display) long lines
#define P_NOERROR   (1 << 10)     // Don't report error if file doesn't exist
#define P_PETSCII   (1 << 11)     // Message is native PETSCII
#define P_WRAP      (1 << 12)     // Wrap/split long-lines, ungracefully
#define P_UTF8      (1 << 13)     // Message is UTF-8
#define P_AUTO_UTF8 (1 << 14)     // Message may be UTF-8, auto-detect
#define P_NOXATTRS  (1 << 15)     // No "Extra Attribute Codes" supported
#define P_MARKUP    (1 << 16)     // Support StyleCodes/Rich/StructuredText
#define P_HIDEMARKS (1 << 17)     // Hide the mark-up characters
#define P_REMOTE    (1 << 18)     // Only print when online == ON_REMOTE
#define P_INDENT    (1 << 19)     // Indent lines to current cursor column
#define P_ATCODES   (1 << 20)     // Trusted @-codes in formatted string
#define P_MODS      (1 << 21)     // Display from mods/text dir, if file is there
#define P_CENTER    (1 << 22)     // Center the output based on widest line
#define P_80COLS    (1 << 23)     // Format output for 80-column display

#define P_XATTR_SHIFT 20
#define P_WILDCAT   (SM_WILDCAT  << P_XATTR_SHIFT)
#define P_PCBOARD   (SM_PCBOARD  << P_XATTR_SHIFT)
#define P_WWIV      (SM_WWIV     << P_XATTR_SHIFT)
#define P_CELERITY  (SM_CELERITY << P_XATTR_SHIFT)
#define P_RENEGADE  (SM_RENEGADE << P_XATTR_SHIFT)

// Bits in 'mode' for listfiles
#define FL_ULTIME   (1 << 0)      // List files by upload time
#define FL_DLTIME   (1 << 1)      // List files by download time - UNUSED
#define FL_NO_HDR   (1 << 2)      // Don't list directory header
#define FL_FIND     (1 << 3)      // Find text in description or filename
#define FL_EXT      (1 << 4)      // Displayed extended file information
#define FL_VIEW     (1 << 5)      // View ZIP/ARC/GIF etc. info

// Bits in the mode of writemsg and email()
#define WM_NONE     0             // No bit flags set
#define WM_EXTDESC  (1 << 0)      // Writing extended file description
#define WM_EMAIL    (1 << 1)      // Writing e-mail
#define WM_NETMAIL  (1 << 2)      // Writing NetMail
#define WM_ANON     (1 << 3)      // Writing anonymous message
#define WM_FILE     (1 << 4)      // Attaching a file to the message
#define WM_NOTOP    (1 << 5)      // Don't add top because we need top line
#define WM_QUOTE    (1 << 6)      // Quote file available
#define WM_QWKNET   (1 << 7)      // Writing QWK NetMail (25 char title)
#define WM_PRIVATE  (1 << 8)      // Private (for creating MSGINF file)
#define WM_SUBJ_RO  (1 << 9)      // Subject/title is read-only
#define WM_EDIT     (1 << 10)     // Editing existing message
#define WM_FORCEFWD (1 << 11)     // Force "yes" to ForwardMailQ for email
#define WM_NOFWD    (1 << 12)     // Don't forward email to netmail
#define WM_EXPANDLF (1 << 13)     // Insure CRLF-terminated lines
#define WM_STRIP_CR (1 << 14)     // Insure no carriage-returns in output

// Bits in the mode of loadposts()
#define LP_BYSELF   (1 << 0)      // Include messages sent by self
#define LP_OTHERS   (1 << 1)      // Include messages sent to others
#define LP_UNREAD   (1 << 2)      // Un-read messages only
#define LP_PRIVATE  (1 << 3)      // Include all private messages
#define LP_REP      (1 << 4)      // Packing REP packet
#define LP_POLLS    (1 << 5)      // Include polls
#define LP_VOTES    (1 << 6)      // Include votes
#define LP_NOMSGS   (1 << 7)      // Don't include regular messages

// Bits in the mode of loadmail()
#define LM_UNREAD   (1 << 0)      // Include un-read mail only
#define LM_INCDEL   (1 << 1)      // Include deleted mail
#define LM_NOSPAM   (1 << 2)      // Exclude SPAM
#define LM_SPAMONLY (1 << 3)      // Load SPAM only
#define LM_REVERSE  (1 << 4)      // Reverse the index order (newest-first)

enum {                            // readmail and delmailidx which types
	  MAIL_YOUR                   // mail sent to you
	, MAIL_SENT                   // mail you have sent
	, MAIL_ANY                    // mail sent to or from you
	, MAIL_ALL                    // all mail (ignores usernumber arg)
};

// Bits in the mode of external()
#define EX_SH       (1 << 0)      // Use command shell to load other process
#define EX_STDOUT   (1 << 1)      // Copy DOS output to remote
#define EX_OUTL     (1 << 2)      // Use _lputc() for local output (*legacy*)
#define EX_STDIN    (1 << 3)      // Trap int 16h keyboard input requests
#define EX_WWIV     WWIVCOLOR     // Expand WWIV color codes to ANSI sequence
#define EX_SWAP     (1 << 5)      // Swap out for this external (*legacy*)
#define EX_POPEN    (1 << 7)      // Leave COM port open	(*legacy*)
#define EX_OFFLINE  (1 << 8)      // Run this program offline
#define EX_BG       (1 << 10)     // Back-ground/detached process
#define EX_BIN      (1 << 11)     // Binary mode (no Unix LF to CRLF)
#define EX_NATIVE   XTRN_NATIVE     // Native (not MS-DOS) application
#define EX_CHKTIME  XTRN_CHKTIME    // Check time left
#define EX_NOECHO   XTRN_NOECHO     // Don't echo stdin to stdout
#define EX_STDIO    (EX_STDIN | EX_STDOUT)
#define EX_UART     XTRN_UART
#define EX_FOSSIL   XTRN_FOSSIL
#define EX_NODISPLAY XTRN_NODISPLAY
#define EX_NOLOG    (1 << 30)     // Don't log intercepted stdio
#define EX_CONIO    (1U << 31)    // Intercept Windows console I/O (doorway)
#define EX_UNSPECIFIED  -1

#if defined(__unix__)
#define EX_WILDCARD EX_SH       // Expand wildcards using 'sh' on Unix
#else
#define EX_WILDCARD 0
#endif

// Linux-DOSemu path/drive hackeroo
#define DOSEMU_NODE_DRIVE   "D:"
#define DOSEMU_XTRN_DRIVE   "E:"    // Parent of xtrn's startup-dir
#define DOSEMU_CTRL_DRIVE   "F:"
#define DOSEMU_DATA_DRIVE   "G:"
#define DOSEMU_EXEC_DRIVE   "H:"
#define DOSEMU_NODE_DIR     DOSEMU_NODE_DRIVE "\\"
#define DOSEMU_XTRN_DIR     DOSEMU_XTRN_DRIVE "\\"
#define DOSEMU_CTRL_DIR     DOSEMU_CTRL_DRIVE "\\"
#define DOSEMU_DATA_DIR     DOSEMU_DATA_DRIVE "\\"
#define DOSEMU_EXEC_DIR     DOSEMU_EXEC_DRIVE "\\"
#define DOSEMU_TEMP_DIR     DOSEMU_NODE_DRIVE "\\TEMP\\"
#define DOSEMU_TEXT_DIR     DOSEMU_CTRL_DRIVE "\\..\\TEXT\\"

// telnet_gate() mode bits
#define TG_ECHO         (1 << 0)  // Turn on telnet echo
#define TG_CRLF         (1 << 1)  // Expand sole CR to CRLF
#define TG_LINEMODE     (1 << 2)  // Send entire lines only
#define TG_NODESYNC     (1 << 3)  // Call Nodesync, get msgs, etc.
#define TG_CTRLKEYS     (1 << 4)  // Interpret ^P ^U ^T, etc locally
#define TG_PASSTHRU     (1 << 5)  // Pass-through telnet commands/responses
#define TG_RLOGIN       (1 << 6)  // Use BSD RLogin protocol
#define TG_NOCHKTIME    (1 << 7)  // Don't check time left while gated
#define TG_NOTERMTYPE   (1 << 8)  // Request client "DONT TERM_TYPE"
#define TG_SENDPASS     (1 << 9)  // Send password instead of real name (RLogin) - DEPRECATED	(it sent the password as the server user name)
#define TG_NOLF         (1 << 10) // Do not send line-feeds (opposite of TG_CRLF)
#define TG_RLOGINSWAP   (1 << 11) // Swap the RLogin alias/real-names
#define TG_RAW          (1 << 12) // Connecting to a raw TCP server
#define TG_EXPANDLF     (1 << 13) // Expand incoming LF to CRLF

enum {                           // Values for 'mode' in listfileinfo
	  FI_INFO                    // Just list file information
	, FI_REMOVE                  // Remove/Move/Edit file information
	, FI_DOWNLOAD                // Download files
	, FI_OLD                     // Search/Remove files not downloaded since
	, FI_OLDUL                   // Search/Remove files uploaded before
	, FI_OFFLINE                 // Search/Remove files not online
	, FI_USERXFER                // User-to-user transfer Download
};

enum XFER_TYPE {                 // Values for type in xfer_prot_select()
	  XFER_UPLOAD
	, XFER_DOWNLOAD
	, XFER_BATCH_UPLOAD
	, XFER_BATCH_DOWNLOAD
};

#define L_LOGON     1           // Logon List maintenance
#define LOL_SIZE    81          // Length of each logon list entry

// Bits in mode of scanposts() function
#define SCAN_CONT       (1 << 0)  // Continuous message scanning
#define SCAN_NEW        (1 << 1)  // New scanning
#define SCAN_BACK       (1 << 2)  // Scan the last message if no new
#define SCAN_TOYOU      (1 << 3)  // Scan for messages to you
#define SCAN_FIND       (1 << 4)  // Scan for text in messages
#define SCAN_UNREAD     (1 << 5)  // Display un-read messages only
#define SCAN_MSGSONLY   (1 << 6)  // Do not do a new file scan even if the user enabled Automatic New File Scan
#define SCAN_POLLS      (1 << 7)  // Scan for polls (only)
#define SCAN_INDEX      (1 << 8)  // List the msg index or exec listmsgs_mod

// Bits in misc of chan_t
#define CHAN_PW     (1 << 0)      // Can be password protected
#define CHAN_GURU   (1 << 1)      // Guru joins empty channel

enum {                           // Values of mode for userlist function
	  UL_ALL                     // List all users in userlist
	, UL_SUB                     // List all users with access to cursub
	, UL_DIR                     // List all users with access to curdir
};

/**********/
/* Macros */
/**********/

#define SYSOP_LEVEL     90
#define FLAG(x)         (uint)(1U << (x - 'A'))
#define WHERE           __LINE__, __FUNCTION__, getfname(__FILE__)
#define TM_MONTH(mm)    ((mm) + 1)
#define TM_YEAR(yy)     ((yy) % 100)
#define sbbs_beep(f, d)  BEEP(f, d)
#define mswait(x)       SLEEP(x)

/**************************************/
/* Text Attribute (color) Definitions */
/**************************************/
#define HIGH  0x08      // High intensity foreground text
#ifndef BLINK
#define BLINK 0x80      // Blinking text
#endif

#ifndef __COLORS
#define __COLORS

enum COLORS {
	BLACK,          // dark colors (HIGH bit unset)
	BLUE,
	GREEN,
	CYAN,
	RED,
	MAGENTA,
	BROWN,
	LIGHTGRAY,
	DARKGRAY,       // light colors (HIGH bit set)
	LIGHTBLUE,
	LIGHTGREEN,
	LIGHTCYAN,
	LIGHTRED,
	LIGHTMAGENTA,
	YELLOW,
	WHITE
};

#endif  // __COLORS

#define FG_UNKNOWN      0x100
#define BG_BLACK        0x200
#define BG_BRIGHT       0x400       // Not an IBM-CGA/ANSI.SYS compatible attribute
#define REVERSED        0x800
#define UNDERLINE       0x1000
#define CONCEALED       0x2000
#define BG_UNKNOWN      0x4000
// TODO: Do we need to keep this value compatible?
#define ANSI_NORMAL     (FG_UNKNOWN | BG_UNKNOWN)
#define BG_BLUE         (BLUE << 4)
#define BG_GREEN        (GREEN << 4)
#define BG_CYAN         (CYAN << 4)
#define BG_RED          (RED << 4)
#define BG_MAGENTA      (MAGENTA << 4)
#define BG_BROWN        (BROWN << 4)
#define BG_LIGHTGRAY    (LIGHTGRAY << 4)

/********************/
/* Type Definitions */
/********************/

typedef struct {                        // Users information
	int    number;                      // Number
	uint   uls,                         // Number of uploads
	       dls,                         // Number of downloads
	       posts,                       // Number of posts
	       emails,                      // Number of emails
	       fbacks,                      // Number of emails sent to sysop
	       etoday,                      // Emails today
	       ptoday,                      // Posts today
	       timeon,                      // Total time on
	       textra,                      // Extra time for today
	       logons,                      // Total logons
	       ttoday,                      // Time on today
	       tlast,                       // Time on last call
	       ltoday;                      // Logons today
	int    xedit,                       // External editor (1 based)
	       shell;                       // Command shell
	uint32_t dtoday;                    // Downloads today (files)
	uint64_t btoday;					// Downloads today (bytes)

	char alias[LEN_ALIAS + 1],          // Alias
	     name[LEN_NAME + 1],            // Name - Real
	     handle[LEN_HANDLE + 1],        // Chat handle
	     lang[LEN_LANG + 1],            // Language code (blank for default)
	     host[LEN_HOST + 1],            // Hostname
	     note[LEN_NOTE + 1],            // Public notice about this user
	     address[LEN_ADDRESS + 1],      // Street Address
	     location[LEN_LOCATION + 1],    // Location of user
	     zipcode[LEN_ZIPCODE + 1],      // Zip/Postal code
	     pass[LEN_PASS + 1],            // Password - not case sensitive
	     birth[LEN_BIRTH + 1],          // Birthday in MM/DD/YY format
	     phone[LEN_PHONE + 1],          // Phone number xxx-xxx-xxxx format
	     connection[LEN_CONNECTION + 1],// Connection type - 8 chars max
	     netmail[LEN_NETMAIL + 1],      // NetMail forwarding address
	     tmpext[MAX_FILEEXT_LEN + 1],   // QWK Packet / Temp file type
	     comment[LEN_COMMENT + 1],      // Private comment about user
	     cursub[LEN_EXTCODE + 1],       // Current sub-board internal code
	     curdir[LEN_EXTCODE + 1],       // Current directory internal code
	     curxtrn[LEN_CODE + 1],         // Current external program internal code
	     ipaddr[LEN_IPADDR + 1];        // Last known IP address

	uchar level,                        // Security level
	      gender,                       // Gender, e.g. 'M', 'F', ' ' (unknown)
	      prot,                         // Default transfer protocol
	      leech;                        // Leech attempt counter

	int rows,                           // Rows on terminal (0 = auto-detect)
	    cols;                           // Columns on terminal (0 = auto-detect)

	uint32_t misc,                      // Misc. bits - ANSI, Deleted etc.
	         qwk,                       // QWK settings
	         chat,                      // Chat settings
	         mail,                      // Mail setttings
	         flags1,                    // Flag set #1
	         flags2,                    // Flag set #2
	         flags3,                    // Flag set #3
	         flags4,                    // Flag set #4
	         exempt,                    // Exemption Flags
	         rest,                      // Restriction Flags
	         min;                       // Minutes
	uint64_t cdt;                       // Credits
	uint64_t freecdt;                   // Free credits (renewed daily)
	uint64_t ulb;                       // Total bytes uploaded
	uint64_t dlb;                       // Total bytes downloaded
	uint32_t dlcps;                     // Last download rate (in CPS)
	time32_t firston,                   // Date/Time first called
	         laston,                    // Last logoff date/time
	         expire,                    // Expiration date
	         pwmod,                     // Password last modified
	         ns_time,                   // Date/Time of last new file scan
	         logontime,                 // Time of current logon
	         deldate,                   // Date/Time account deleted
	         reset;                     // Date/Time of last daily (e.g. stats) reset
} user_t;

typedef struct {
	idxrec_t idx;                       // defined in smbdefs.h
	uint32_t num;                       // 1-based offset
	union {
		struct {
			uint32_t upvotes;
			uint32_t downvotes;
		};
		uint32_t votes[MSG_POLL_MAX_ANSWERS];
	};
	uint32_t total_votes;
} post_t;
typedef idxrec_t mail_t;                // defined in smbdefs.h
typedef fidoaddr_t faddr_t;             // defined in smbdefs.h
typedef smbfile_t file_t;               // defined in smbdefs.h

typedef struct {                        // System/Node Statistics
	uint logons;
	uint timeon;
	uint uls;
	uint64_t ulb;
	uint dls;
	uint64_t dlb;
	uint posts;
	uint email;
	uint fbacks;
	uint nusers;
} totals_t;

typedef struct {                        // System/Node Statistics
	time32_t date;                      // When stats were last rolled-over
	time32_t last;                      // When stats were last read from disk
	union {
		totals_t total;
		struct { // legacy names
			uint logons;
			uint timeon;
		};
	};
	union {
		totals_t today;
		struct { // legacy names
			uint ltoday;
			uint ttoday;
			uint uls;
			uint64_t ulb;
			uint dls;
			uint64_t dlb;
			uint ptoday;
			uint etoday;
			uint ftoday;
			uint nusers;
		};
	};
} stats_t;

#define LEN_CSTATS_RECORD 128
enum CSTATS_FIELD {
	  CSTATS_DATE
	, CSTATS_LOGONS
	, CSTATS_TIMEON
	, CSTATS_UPLOADS
	, CSTATS_UPLOADB
	, CSTATS_DNLOADS
	, CSTATS_DNLOADB
	, CSTATS_POSTS
	, CSTATS_EMAIL
	, CSTATS_FBACKS
	, CSTATS_NUSERS
};

typedef struct {                            // Sub-board scan information
	uint16_t cfg;                           // User's configuration
	uint32_t ptr;                           // New-message pointer
	uint32_t last;                          // Last read message number
	uint16_t sav_cfg;                       // Saved configuration
	uint32_t sav_ptr;                       // Saved New-message pointer
	uint32_t sav_last;                      // Saved Last read message number
} subscan_t;

#endif // Don't add anything after this #endif statement
