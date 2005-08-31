# sbbsdefs.inc

# Baja include file defining various Synchronet related constants

# $id: $

# @format.tab-size 8, @format.use-tabs true

!include nodedefs.inc
!include userdefs.inc
!include errno.inc

				# System toggle options (_SYS_MISC)
!define SM_CLOSED	.0	# System is clsoed to New Users
!define SM_SYSSTAT	.1	# Sysops activity included in statistics
!define SM_NOBEEP	.2	# No beep sound locally
!define SM_PWEDIT	.3	# Allow users to change their passwords
!define SM_RA_EMU	.4	# Reverse R/A read message/mail command keys
!define SM_ANON_EM	.5	# Allow anonymous e-mail
!define SM_LISTLOC	.6	# Use location of caller in user lists
!define SM_WILDCAT	.7	# Expand Wildcat color codes in messages
!define SM_PCBOARD	.8	# Expand PCBoard color codes in messages
!define SM_WWIV 	.9	# Expand WWIV color codes in messages
!define SM_CELERITY	.10	# Expand Celerity color codes in messages
!define SM_RENEGADE	.11	# Expand Renegade color codes in messages
!define SM_ECHO_PW	.12	# Echo passwords locally
!define SM_REQ_PW	.13	# Require passwords locally
!define SM_L_SYSOP	.14	# Allow local sysop logon/commands
!define SM_R_SYSOP	.15	# Allow remote sysop logon/commands
!define SM_QUOTE_EM	.16	# Allow quoting of e-mail
!define SM_EURODATE	.17	# Europian date format (DD/MM/YY
!define SM_MILITARY	.18	# Military time format
!define SM_TIMEBANK	.19	# Allow time bank functions
!define SM_FILE_EM	.20	# Allow file attachments in E-mail
!define SM_SHRTPAGE	.21	# Short sysop page
!define SM_TIME_EXP	.22	# Set to expired values if out-of-time
!define SM_FASTMAIL	.23	# Fast e-mail storage mode
!define SM_QVALKEYS	.24	# Quick validation keys enabled
!define SM_ERRALARM	.25	# Error beeps on
!define SM_FWDTONET	.26	# Allow forwarding of e-mail to netmail
!define SM_DELREADM	.27	# Delete read mail automatically
!define SM_NOCDTCVT	.28	# No credit to minute conversions allowed

				# Node toggle options (_NODE_MISC)
!define NM_ANSALARM	.0	# Alarm locally on answer
!define NM_WFCSCRN	.1	# Wait for call screen
!define NM_WFCMSGS	.2	# Include total messages/files on WFC
!define NM_LCL_EDIT	.3	# Use local editor to create messages
!define NM_EMSOVL	.4	# Use expanded memory of overlays
!define NM_WINOS2	.5	# Use Windows/OS2 time slice API call
!define NM_INT28	.6	# Make int 28 DOS idle calls
!define NM_NODV 	.7	# Don't detect and use DESQview API
!define NM_NO_NUM	.8	# Don't allow logons by user number
!define NM_LOGON_R	.9	# Allow logons by user real name
!define NM_LOGON_P	.10	# Secure logons (always ask for password
!define NM_NO_LKBRD	.11	# No local keyboard (at all
!define NM_SYSPW	.12	# Protect WFC keys and Alt keys with SY:
!define NM_NO_INACT	.13	# No local inactivity alert/logoff
!define NM_NOBEEP	.14	# Don't beep locally
!define NM_LOWPRIO	.15	# Always use low priority input
!define NM_7BITONLY	.16	# Except 7-bit input only (E71 terminals
!define NM_NOPAUSESPIN	.17	# No spinning cursor at pause prompt

				# New user question toggles (_UQ)
!define UQ_ALIASES      .0      # Ask for alias
!define UQ_LOCATION     .1      # Ask for location
!define UQ_ADDRESS      .2      # Ask for address
!define UQ_PHONE        .3      # Ask for phone number
!define UQ_HANDLE       .4      # Ask for chat handle
!define UQ_DUPHAND      .5      # Search for duplicate handles
!define UQ_SEX          .6      # Ask for sex
!define UQ_BIRTH        .7      # Ask for birth date
!define UQ_COMP         .8      # Ask for computer type
!define UQ_MC_COMP      .9      # Multiple choice computer type
!define UQ_REALNAME     .10     # Ask for real name
!define UQ_DUPREAL      .11     # Search for duplicate real names
!define UQ_COMPANY      .12     # Ask for company name
!define UQ_NOEXASC      .13     # Don't allow ex-ASCII in user text
!define UQ_CMDSHELL     .14     # Ask for command shell
!define UQ_XEDIT        .15     # Ask for external editor
!define UQ_NODEF        .16     # Don't ask for default settings
!define UQ_NOCOMMAS     .17     # Do not require commas in location


				# FidoNet netmail toggles (_NETMAIL_MISC)
!define NMAIL_ALLOW	.0	# Allow NetMail
!define NMAIL_CRASH	.1	# Default netmail to crash
!define NMAIL_HOLD	.2	# Default netmail to hold
!define NMAIL_KILL	.3	# Default netmail to kill after sent
!define NMAIL_ALIAS	.4	# Use Aliases in NetMail
!define NMAIL_FILE	.5	# Allow file attachments
!define NMAIL_DIRECT	.6	# Default netmail to direct

				# Console I/O status (_CONSOLE)
!define CON_R_ECHO	.0	# Echo remotely
!define CON_R_ECHOX	.1	# Echo X's to remote user
!define CON_R_INPUT	.2	# Accept input remotely
!define CON_L_ECHO	.3	# Echo locally
!define CON_L_ECHOX	.4	# Echo X's locally
!define CON_L_INPUT	.5	# Accept input locally
!define CON_RAW_IN	.8	# Raw input mode - no editing capabilities
!define CON_ECHO_OFF	.10	# Remote & Local echo disabled for ML/MF
!define CON_UPARROW	.11	# Up arrow hit - move up one line
!define CON_NO_INACT	.13	# Disable user inactivity detection

				# String lengths

!define LEN_FDESC	58	# File description
!define LEN_TITLE	70	# Message title
!define LEN_CID 	25	# Caller ID (phone number
!define LEN_ARSTR	40	# Max length of Access Requirement string
!define LEN_CHATACTCMD	 9	# Chat action command
!define LEN_CHATACTOUT	65	# Chat action output string


				# Online status (0=offline) (_ONLINE)
!define ON_LOCAL	1	# Online locally
!define ON_REMOTE	2	# Online remotely

				# System status bits (_SYS_STATUS)
!define SS_LOGOPEN	.0	# Node's Log file is open
!define SS_INITIAL	.1	# The bbs data has been initialized.
!define SS_TMPSYSOP	.2	# Temporary Sysop Status
!define SS_USERON	.3	# A User is logged on to the BBS
!define SS_LCHAT	.4	# Local chat in progress
!define SS_CAP		.5	# Capture is on
!define SS_ANSCAP	.6	# Capture ANSI codes too
!define SS_FINPUT	.7	# Using file for input
!define SS_COMISR	.8	# Com port ISR is installed
!define SS_DAILY	.9	# Execute System Daily Event on logoff
!define SS_INUEDIT	.10	# Inside Alt-Useredit section
!define SS_ABORT	.11	# Global abort input or output flag
!define SS_SYSPAGE	.12	# Paging sysop
!define SS_SYSALERT	.13	# Notify sysop when users hangs up
!define SS_GURUCHAT	.14	# Guru chat in progress
!define SS_NODEDAB	.15	# NODE.DAB operations are okay
!define SS_EVENT	.16	# Time shortened due to upcoming event
!define SS_PAUSEON	.17	# Pause on, overriding user default
!define SS_PAUSEOFF	.18	# Pause off, overriding user default
!define SS_IN_CTRLP	.19	# Inside ctrl-p send node message func
!define SS_NEWUSER	.20	# New User online
!define SS_MDMDEBUG	.21	# Modem debug output
!define SS_NEST_PF	.22	# Nested in printfile function
!define SS_DCDHIGH	.23	# Assume DCD is high always
!define SS_SPLITP	.24	# Split-screen private chat
!define SS_RLOGIN	.26	# RLogin session (not telnet)

				# Bits in 'mode' for GETSTR function
!define K_UPPER 	.0	# Converts all letters to upper case
!define K_UPRLWR	.1	# Upper/Lower case automatically
!define K_NUMBER	.2	# Allow numbers only
!define K_WRAP		.3	# Allows word wrap
!define K_MSG		.4	# Allows ANSI, ^N ^A ^G
!define K_SPIN		.5	# Spinning cursor (same as SPIN
!define K_LINE		.6	# Input line (inverse color
!define K_EDIT		.7	# Edit string passed
!define K_CHAT		.8	# In chat multi-chat
!define K_NOCRLF	.9	# Don't print CRLF after string input
!define K_ALPHA 	.10	# Only allow alphabetic characters
!define K_GETSTR	.11	# getkey called from getstr
!define K_LOWPRIO	.12	# low priority input
!define K_NOEXASC	.13	# No extended ASCII allowed
!define K_E71DETECT	.14	# Detect E-7-1 terminal type
!define K_AUTODEL	.15	# Auto-delete text (used with K_EDIT
!define K_COLD		.16	# Possible cold key mode
!define K_NOECHO	.17	# Don't echo input
!define K_TAB		.18	# Treat TAB as CR

				# Bits in 'mode' for PRINTFILE function
!define P_NOABORT	.0	# Disallows abortion of a message
!define P_SAVEATR	.1	# Save the new current attributres after
				# msg has printed.
!define P_NOATCODES	.2	# Don't allow @ codes
!define P_OPENCLOSE	.3	# Open and close the file
!define P_NOPAUSE	.4	# Disable screen pause
!define P_HTML		.5	# Message is HTML
!define P_NOCRLF	.6	# Don't prepend a CRLF

				# Values for USER_EVENT function
!define EVENT_NONE      0       # Only accessible by menu
!define EVENT_LOGON	1	# Execute during logon sequence
!define EVENT_NEWUSER	2	# Execute during newuser app.
!define EVENT_BIRTHDAY	3	# Execute on birthday
!define EVENT_LOGOFF	4	# Execute during normal logoff
!define EVENT_POST		5	# Execute after message posted
!define EVENT_UPLOAD	6	# Execute after file uploaded
!define EVENT_DOWNLOAD	7	# Execute after file downloaded

!define TG_ECHO		.0	# Turn on telnet echo
!define TG_CRLF		.1	# Expand sole CR to CRLF
!define TG_LINEMODE	.2	# Send entire lines only
!define TG_NODESYNC	.3	# Call Nodesync, get msgs, etc.		
!define TG_CTRLKEYS	.4	# Interpret ^P ^U ^T, etc locally
!define TG_PASSTHRU	.5	# Pass-through telnet commands/responses
!define TG_RLOGIN	.6	# Use BSD RLogin protocol
!define TG_NOCHKTIME	.7	# Don't check time left online
!define TG_NOTERMTYPE	.8	# Request client "DONT TERM_TYPE"

				# Bits for _FTP_MODE
!define FTP_ECHO_CMD	.0	# Echo commands to user (debug)
!define FTP_ECHO_RSP	.1	# Echo reply/response lines to user
!define FTP_PASV	.2	# Use passive transfer mode
!define FTP_ASCII	.3	# Use ASCII transfer mode
!define FTP_HASH	.4	# Print hash marks during transfers

!define BBS_OPT_KEEP_ALIVE	.0	# Send keep-alives					
!define BBS_OPT_XTRN_MINIMIZED	.1	# Run externals minimized			
!define BBS_OPT_AUTO_LOGON	.2	# Auto-logon via IP				
!define BBS_OPT_DEBUG_TELNET	.3	# Debug telnet commands			
!define BBS_OPT_SYSOP_AVAILABLE	.4	# Available for chat				
!define BBS_OPT_ALLOW_RLOGIN	.5	# Allow logins via BSD RLogin		
!define BBS_OPT_USE_2ND_RLOGIN	.6	# Use 2nd username in BSD RLogin	
!define BBS_OPT_NO_QWK_EVENTS	.7	# Don't run QWK-related events		
!define BBS_OPT_NO_HOST_LOOKUP	.11
!define BBS_OPT_LOCAL_TIMEZONE	.30	# Don't force UCT/GMT				
!define BBS_OPT_MUTE		.31	# Mute sounds						

################################
# System Variable Declarations #
################################

				# String variables
!global _USERON.ALIAS
!global _USERON.NAME
!global _USERON.HANDLE
!global _USERON.COMP
!global _USERON.NOTE
!global _USERON.ADDRESS
!global _USERON.LOCATION
!global _USERON.ZIPCODE
!global _USERON.PASS
!global _USERON.BIRTH
!global _USERON.PHONE
!global _USERON.MODEM
!global _USERON.NETMAIL
!global _USERON.TMPEXT
!global _USERON.COMMENT
!global _CONNECTION
!global _CAP_FNAME
!global _CID
!global _COMSPEC
!global _QUESTION

				# Integer variables
!global _USERON.NUMBER
!global _USERON.ULS
!global _USERON.DLS
!global _USERON.POSTS
!global _USERON.EMAILS
!global _USERON.FBACKS
!global _USERON.ETODAY
!global _USERON.PTODAY
!global _USERON.TIMEON
!global _USERON.TEXTRA
!global _USERON.LOGONS
!global _USERON.TTODAY
!global _USERON.TLAST
!global _USERON.LTODAY
!global _USERON.XEDIT
!global _USERON.SHELL
!global _USERON.LEVEL
!global _USERON.SEX
!global _USERON.ROWS
!global _USERON.PROT
!global _USERON.LEECH
!global _USERON.MISC
!global _USERON.QWK
!global _USERON.CHAT
!global _USERON.FLAGS1
!global _USERON.FLAGS2
!global _USERON.FLAGS3
!global _USERON.FLAGS4
!global _USERON.EXEMPT
!global _USERON.REST
!global _USERON.ULB
!global _USERON.DLB
!global _USERON.CDT
!global _USERON.MIN
!global _USERON.FREECDT
!global _USERON.FIRSTON
!global _USERON.LASTON
!global _USERON.EXPIRE
!global _USERON.PWMOD
!global _USERON.NS_TIME
!global _CUR_RATE
!global _CUR_CPS
!global _DTE_RATE
!global _LNCNTR
!global _TOS
!global _ROWS
!global _AUTOTERM
!global _CONSOLE
!global _ANSWERTIME
!global _LOGONTIME
!global _NS_TIME
!global _LAST_NS_TIME
!global _ONLINE
!global _SYS_STATUS
!global _SYS_MISC
!global _SYS_PSNUM
!global _SYS_TIMEZONE
!global _SYS_PWDAYS
!global _SYS_DELDAYS
!global _SYS_AUTODEL
!global _SYS_NODES
!global _SYS_EXP_WARN
!global _SYS_LASTNODE
!global _SYS_AUTONODE
!global _NODE_SCRNLEN
!global _NODE_SCRNBLANK
!global _NODE_MISC
!global _NODE_VALUSER
!global _NODE_IVT
!global _NODE_SWAP
!global _NODE_MINBPS
!global _NODE_NUM
!global _NEW_LEVEL
!global _NEW_FLAGS1
!global _NEW_FLAGS2
!global _NEW_FLAGS3
!global _NEW_FLAGS4
!global _NEW_EXEMPT
!global _NEW_REST
!global _NEW_CDT
!global _NEW_MIN
!global _NEW_SHELL
!global _NEW_MISC
!global _NEW_EXPIRE
!global _NEW_PROT
!global _EXPIRED_LEVEL
!global _EXPIRED_FLAGS1
!global _EXPIRED_FLAGS2
!global _EXPIRED_FLAGS3
!global _EXPIRED_FLAGS4
!global _EXPIRED_EXEMPT
!global _EXPIRED_REST
!global _MIN_DSPACE
!global _CDT_MIN_VALUE
!global _CDT_PER_DOLLAR
!global _LEECH_PCT
!global _LEECH_SEC
!global _NETMAIL_COST
!global _NETMAIL_MISC
!global _INETMAIL_COST
!global _INETMAIL_MISC
!global _LOGON_ULB
!global _LOGON_DLB
!global _LOGON_ULS
!global _LOGON_DLS
!global _LOGON_POSTS
!global _LOGON_EMAILS
!global _LOGON_FBACKS
!global _POSTS_READ
!global _LOGFILE
!global _NODEFILE
!global _NODE_EXT
!global _TIMELEFT
!global _MAX_MINUTES
!global _MAX_QWKMSGS
!global _UQ
!global _ERRORLEVEL
!global _WORDWRAP
!global _FTP_MODE
!global _CLIENT_SOCKET
!global _SOCKET_ERROR
!global _STARTUP_OPTIONS

# End of SBBSDEFS.INC
