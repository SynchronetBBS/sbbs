# userdefs.inc

# Baja include file defining various Synchronet USER related constants

# $id: $

# @format.tab-size 8, @format.use-tabs true

!define LEN_ALIAS	25	# User alias
!define LEN_NAME        25      # User name
!define LEN_HANDLE       8      # User chat handle
!define LEN_NOTE        30      # User note
!define LEN_COMP        30      # User computer description
!define LEN_COMMENT     60      # User comment
!define LEN_NETMAIL     60      # NetMail forwarding address
!define LEN_PASS         8      # User password
!define LEN_PHONE       12      # User phone number
!define LEN_BIRTH        8      # Birthday in MM/DD/YY format
!define LEN_ADDRESS     30      # User address
!define LEN_LOCATION    30      # Location (City, State
!define LEN_ZIPCODE     10      # Zip/Postal code
!define LEN_MODEM        8      # User modem type description

#**************************************************************************
# This is a list of offsets into the USER.DAT file for different variables
# that are stored (for each user
#**************************************************************************
!define U_ALIAS 	0			# Offset to alias
!define U_NAME		U_ALIAS+LEN_ALIAS	# Offset to name
!define U_HANDLE	U_NAME+LEN_NAME
!define U_NOTE		U_HANDLE+LEN_HANDLE+2
!define U_COMP		U_NOTE+LEN_NOTE
!define U_COMMENT	U_COMP+LEN_COMP+2

!define U_NETMAIL	U_COMMENT+LEN_COMMENT+2

!define U_ADDRESS	U_NETMAIL+LEN_NETMAIL+2
!define U_LOCATION	U_ADDRESS+LEN_ADDRESS
!define U_ZIPCODE	U_LOCATION+LEN_LOCATION

!define U_PASS		U_ZIPCODE+LEN_ZIPCODE+2
!define U_PHONE 	U_PASS+8		# Offset to phone-number
!define U_BIRTH 	U_PHONE+12		# Offset to users birthday
!define U_MODEM 	U_BIRTH+8
!define U_LASTON	U_MODEM+8
!define U_FIRSTON	U_LASTON+8
!define U_EXPIRE	U_FIRSTON+8
!define U_PWMOD 	U_EXPIRE+8

!define U_LOGONS	U_PWMOD+8+2
!define U_LTODAY	U_LOGONS+5
!define U_TIMEON	U_LTODAY+5
!define U_TEXTRA	U_TIMEON+5
!define U_TTODAY	U_TEXTRA+5
!define U_TLAST 	U_TTODAY+5
!define U_POSTS 	U_TLAST+5
!define U_EMAILS	U_POSTS+5
!define U_FBACKS	U_EMAILS+5
!define U_ETODAY	U_FBACKS+5
!define U_PTODAY	U_ETODAY+5

!define U_ULB		U_PTODAY+5+2
!define U_ULS		U_ULB+10
!define U_DLB		U_ULS+5
!define U_DLS		U_DLB+10
!define U_CDT		U_DLS+5
!define U_MIN		U_CDT+10

!define U_LEVEL 	U_MIN+10+2		# Offset to Security Level
!define U_FLAGS1	U_LEVEL+2		# Offset to Flags
!define U_TL		U_FLAGS1+8		# Offset to unused field
!define U_FLAGS2	U_TL+2
!define U_EXEMPT	U_FLAGS2+8
!define U_REST		U_EXEMPT+8
!define U_ROWS		U_REST+8+2	# Number of Rows on user's monitor
!define U_SEX		U_ROWS+2	# Sex, Del, ANSI, color etc.
!define U_MISC		U_SEX+1 	# Miscellaneous flags in 8byte hex
!define U_OLDXEDIT	U_MISC+8	# External editor  Version 1 method
!define U_LEECH 	U_OLDXEDIT+2	# two hex digits - leech attempt count
!define U_CURSUB	U_LEECH+2	# Current sub  internal code
!define U_CURDIR	U_CURSUB+8	# Current dir  internal code
!define U_CMDSET	U_CURDIR+8	# unused
!define U_MAIN_CMD	U_CMDSET+2+2	# unused
!define U_XFER_CMD	U_MAIN_CMD+40	# unused
!define U_SCAN_CMD	U_XFER_CMD+40+2 # unused
!define U_MAIL_CMD	U_SCAN_CMD+40	# unused
!define U_FREECDT	U_MAIL_CMD+40+2
!define U_FLAGS3	U_FREECDT+10	# Flag set #3
!define U_FLAGS4	U_FLAGS3+8	# Flag set #4
!define U_XEDIT 	U_FLAGS4+8	# External editor  code
!define U_SHELL 	U_XEDIT+8	# Command shell  code
!define U_QWK		U_SHELL+8	# QWK settings
!define U_TMPEXT	U_QWK+8 	# QWK extension
!define U_CHAT		U_TMPEXT+3	# Chat settings
!define U_NS_TIME	U_CHAT+8	# New-file scan date/time
!define U_PROT		U_NS_TIME+8	# Default transfer protocol
!define U_UNUSED	U_PROT+1
!define U_LEN		U_UNUSED+28+2


!define UM_DELETED	.0	# Bit values for user.misc
!define UM_ANSI 	.1	# Supports ANSI terminal emulation
!define UM_COLOR	.2	# Send color codes
!define UM_RIP		.3	# Supports RIP terminal emulation
!define UM_UPAUSE	.4	# Pause on every screen full
!define UM_SPIN 	.5	# Spinning cursor - Same as K_SPIN
!define UM_INACTIVE	.6	# Inactive user slot
!define UM_EXPERT	.7	# Expert menu mode
!define UM_ANFSCAN	.8	# Auto New file scan
!define UM_CLRSCRN	.9	# Clear screen before each message
!define UM_QUIET	.10	# Quiet mode upon logon
!define UM_BATCHFLAG	.11	# File list allow batch dl flags
!define UM_NETMAIL	.12	# Forward e-mail to fidonet addr
!define UM_CURSUB	.13	# Remember current sub-board/dir
!define UM_ASK_NSCAN	.14	# Ask for newscanning upon logon
!define UM_NO_EXASCII	.15	# Don't send extended ASCII
!define UM_ASK_SSCAN	.16	# Ask for messages to you at logon
!define UM_AUTOTERM	.17	# Autodetect terminal type
!define UM_COLDKEYS	.18	# No hot-keys
!define UM_EXTDESC	.19	# Extended file descriptions
!define UM_AUTOHANG	.20	# Auto-hang-up after transfer
!define UM_WIP		.21	# Supports WIP terminal emulation
                                # Bits in user.qwk
!define QWK_FILES	.0	# Include new files list
!define QWK_EMAIL	.1	# Include unread e-mail
!define QWK_ALLMAIL	.2	# Include ALL e-mail
!define QWK_DELMAIL	.3	# Delete e-mail after download
!define QWK_BYSELF	.4	# Include messages from self
!define QWK_UNUSED	.5	# Currently unused
!define QWK_EXPCTLA	.6	# Expand ctrl-a codes to ascii
!define QWK_RETCTLA	.7	# Retain ctrl-a codes
!define QWK_ATTACH	.8	# Include file attachments
!define QWK_NOINDEX	.9	# Do not create index files in QWK
!define QWK_TZ		.10	# Include "@TZ" time zone in msgs
!define QWK_VIA 	.11	# Include "@VIA" seen-bys in msgs
!define QWK_NOCTRL	.12	# No extraneous control files
                                # Bits in user.chat
!define CHAT_ECHO	.0	# Multinode chat echo
!define CHAT_ACTION	.1	# Chat actions
!define CHAT_NOPAGE	.2	# Can't be paged
!define CHAT_NOACT	.3	# No activity alerts
!define CHAT_SPLITP	.4	# Split screen private chat

                                # For use with SET_USER_STRING function
!define USER_STRING_ALIAS	0
!define USER_STRING_REALNAME	1
!define USER_STRING_HANDLE	2
!define USER_STRING_COMPUTER	3
!define USER_STRING_NOTE	4
!define USER_STRING_ADDRESS	5
!define USER_STRING_LOCATION	6
!define USER_STRING_ZIPCODE	7
!define USER_STRING_PASSWORD	8
!define USER_STRING_BIRTHDAY	9
!define USER_STRING_PHONE	10
!define USER_STRING_MODEM	11
!define USER_STRING_COMMENT	12
!define USER_STRING_NETMAIL	13	# Requires v3 (03/02/00) or later

# End of USERDEFS.INC
