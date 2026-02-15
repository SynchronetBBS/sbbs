/* Synchronet command shell/module constants and structure definitions */

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

#include <stdio.h>
#include "gen_defs.h"
#include "dirwrap.h"
#include "sockwrap.h"

#ifndef _CMDSHELL_H_
#define _CMDSHELL_H_

/******************************/
/* Instructions and Functions */
/******************************/
enum {

/* Single byte instructions */

	CS_IF_TRUE = 0          /* Same as IF_EQUAL */
	, CS_IF_FALSE            /* Same as IF_NOT_EQUAL */
	, CS_ELSE
	, CS_ENDIF
	, CS_CMD_HOME
	, CS_CMD_POP
	, CS_END_CMD
	, CS_RETURN
	, CS_GETKEY
	, CS_GETKEYE
	, CS_UNGETKEY
	, CS_UNGETSTR
	, CS_PRINTKEY
	, CS_PRINTSTR
	, CS_HANGUP
	, CS_SYNC
	, CS_ASYNC
	, CS_CHKSYSPASS
	, CS_LOGKEY
	, CS_LOGKEY_COMMA
	, CS_LOGSTR
	, CS_CLS
	, CS_CRLF
	, CS_PAUSE
	, CS_PAUSE_RESET
	, CS_GETLINES
	, CS_GETFILESPEC
	, CS_FINDUSER
	, CS_CLEAR_ABORT
	, CS_SELECT_SHELL
	, CS_SET_SHELL
	, CS_SELECT_EDITOR
	, CS_SET_EDITOR          /* 0x20 */
	, CS_INKEY
	, CS_INCHAR              /* Was RIOSYNC (now deprecated) 02/18/01 */
	, CS_GETTIMELEFT
	, CS_SAVELINE
	, CS_RESTORELINE
	, CS_IF_GREATER
	, CS_IF_GREATER_OR_EQUAL
	, CS_IF_LESS
	, CS_IF_LESS_OR_EQUAL
	, CS_DEFAULT             /* 0x2a */
	, CS_END_SWITCH
	, CS_END_CASE
	, CS_PUT_NODE
	, CS_GETCHAR
	, CS_ONE_MORE_BYTE = 0x2f

/* Two byte instructions */

	, CS_CMDKEY = 0x30
	, CS_NODE_ACTION
	, CS_GETSTR
	, CS_GETNAME
	, CS_GETSTRUPR
	, CS_SHIFT_STR
	, CS_COMPARE_KEY
	, CS_SETLOGIC
	, CS_SET_USER_LEVEL
	, CS_SET_USER_STRING
	, CS_GETLINE             /* 0x3a */
	, CS_NODE_STATUS
	, CS_CMDCHAR
	, CS_COMPARE_CHAR
	, CS_MULTINODE_CHAT
	, CS_TWO_MORE_BYTES = 0x3f

/* Three byte instructions */

	, CS_GOTO = 0x40
	, CS_CALL
	, CS_TOGGLE_NODE_MISC
	, CS_ADJUST_USER_CREDITS
	, CS_TOGGLE_USER_FLAG
	, CS_GETNUM
	, CS_COMPARE_NODE_MISC
	, CS_MSWAIT
	, CS_ADJUST_USER_MINUTES
	, CS_REVERT_TEXT
	, CS_THREE_MORE_BYTES = 0x4f

/* String arg instructions */

	, CS_MENU = 0x50
	, CS_PRINT
	, CS_PRINT_LOCAL
	, CS_PRINT_REMOTE
	, CS_PRINTFILE
	, CS_PRINTFILE_LOCAL
	, CS_PRINTFILE_REMOTE
	, CS_YES_NO
	, CS_NO_YES
	, CS_COMPARE_STR
	, CS_COMPARE_WORD        /* 0x5a */
	, CS_EXEC
	, CS_EXEC_INT
	, CS_EXEC_BIN
	, CS_EXEC_XTRN
	, CS_GETCMD
	, CS_LOG                 /* 0x60 */
	, CS_MNEMONICS
	, CS_SETSTR
	, CS_SET_MENU_DIR
	, CS_SET_MENU_FILE
	, CS_CMDSTR
	, CS_CHKFILE
	, CS_GET_TEMPLATE
	, CS_TRASHCAN
	, CS_CREATE_SIF
	, CS_READ_SIF            /* 0x6a */
	, CS_CMDKEYS
	, CS_COMPARE_KEYS
	, CS_STR_FUNCTION = 0x6f

/* Var length instructions */

	, CS_COMPARE_ARS = 0x70
	, CS_TOGGLE_USER_MISC
	, CS_COMPARE_USER_MISC
	, CS_REPLACE_TEXT
	, CS_TOGGLE_USER_CHAT
	, CS_COMPARE_USER_CHAT
	, CS_TOGGLE_USER_QWK
	, CS_COMPARE_USER_QWK
	, CS_SWITCH
	, CS_CASE
	, CS_USE_INT_VAR         /* 0x7a */

/* Network (TCP/IP) Functions */

	, CS_NET_FUNCTION = 0x7d

/* File I/O Functions */

	, CS_FIO_FUNCTION = 0x7e

/* Variable instruction sub-ops */

	, CS_VAR_INSTRUCTION = 0x7f

/* Functions */

	, CS_MAIL_READ = 0x80
	, CS_MAIL_READ_SENT
	, CS_MAIL_READ_ALL
	, CS_MAIL_SEND
	, CS_MAIL_SEND_BULK
	, CS_MAIL_SEND_FILE
	, CS_MAIL_SEND_FEEDBACK
	, CS_MAIL_SEND_NETMAIL
	, CS_MAIL_SEND_NETFILE
	, CS_LOGOFF
	, CS_LOGOFF_FAST
	, CS_AUTO_MESSAGE
	, CS_MSG_SET_AREA
	, CS_MSG_SELECT_AREA
	, CS_MSG_SHOW_GROUPS
	, CS_MSG_SHOW_SUBBOARDS
	, CS_MSG_GROUP_UP            /* 0x90 */
	, CS_MSG_GROUP_DOWN
	, CS_MSG_SUBBOARD_UP
	, CS_MSG_SUBBOARD_DOWN
	, CS_MSG_GET_SUB_NUM
	, CS_MSG_GET_GRP_NUM
	, CS_MSG_READ
	, CS_MSG_POST
	, CS_MSG_QWK
	, CS_MSG_PTRS_CFG
	, CS_MSG_PTRS_REINIT
	, CS_MSG_NEW_SCAN_CFG
	, CS_MSG_NEW_SCAN
	, CS_MSG_NEW_SCAN_ALL
	, CS_MSG_CONT_SCAN
	, CS_MSG_CONT_SCAN_ALL
	, CS_MSG_BROWSE_SCAN         /* 0xA0 */
	, CS_MSG_BROWSE_SCAN_ALL
	, CS_MSG_FIND_TEXT
	, CS_MSG_FIND_TEXT_ALL
	, CS_MSG_YOUR_SCAN_CFG
	, CS_MSG_YOUR_SCAN
	, CS_MSG_YOUR_SCAN_ALL
	, CS_MSG_NEW_SCAN_SUB
	, CS_MSG_SET_GROUP
	, CS_MSG_LIST
	, CS_MSG_UNUSED3
	, CS_MSG_UNUSED2
	, CS_MSG_UNUSED1
	, CS_FILE_SET_AREA
	, CS_FILE_SELECT_AREA
	, CS_FILE_SHOW_LIBRARIES
	, CS_FILE_SHOW_DIRECTORIES   /* 0xB0 */
	, CS_FILE_LIBRARY_UP
	, CS_FILE_LIBRARY_DOWN
	, CS_FILE_DIRECTORY_UP
	, CS_FILE_DIRECTORY_DOWN
	, CS_FILE_GET_DIR_NUM
	, CS_FILE_GET_LIB_NUM
	, CS_FILE_LIST
	, CS_FILE_LIST_EXTENDED
	, CS_FILE_VIEW
	, CS_FILE_UPLOAD
	, CS_FILE_UPLOAD_USER
	, CS_FILE_UPLOAD_SYSOP
	, CS_FILE_DOWNLOAD
	, CS_FILE_DOWNLOAD_USER
	, CS_FILE_DOWNLOAD_BATCH
	, CS_FILE_REMOVE             /* 0xC0 */
	, CS_FILE_BATCH_SECTION
	, CS_FILE_TEMP_SECTION
	, CS_FILE_NEW_SCAN_CFG
	, CS_FILE_NEW_SCAN
	, CS_FILE_NEW_SCAN_ALL
	, CS_FILE_FIND_TEXT
	, CS_FILE_FIND_TEXT_ALL
	, CS_FILE_FIND_NAME
	, CS_FILE_FIND_NAME_ALL
	, CS_FILE_PTRS_CFG
	, CS_FILE_BATCH_ADD
	, CS_FILE_BATCH_CLEAR
	, CS_FILE_SET_LIBRARY
	, CS_FILE_SEND               /* Like file_get, but no password needed */
	, CS_FILE_BATCH_ADD_LIST
	, CS_FILE_RECEIVE            /* 0xD0 */
	, CS_NODELIST_ALL
	, CS_NODELIST_USERS
	, CS_CHAT_SECTION
	, CS_USER_DEFAULTS
	, CS_USER_EDIT
	, CS_TEXT_FILE_SECTION
	, CS_INFO_SYSTEM
	, CS_INFO_SUBBOARD
	, CS_INFO_DIRECTORY
	, CS_INFO_USER
	, CS_INFO_VERSION
	, CS_INFO_XFER_POLICY
	, CS_XTRN_EXEC
	, CS_XTRN_SECTION
	, CS_USERLIST_SUB
	, CS_USERLIST_DIR            /* 0xE0 */
	, CS_USERLIST_ALL
	, CS_USERLIST_LOGONS
	, CS_PAGE_SYSOP
	, CS_PRIVATE_CHAT
	, CS_PRIVATE_MESSAGE
	, CS_MINUTE_BANK
	, CS_GURU_LOG
	, CS_ERROR_LOG
	, CS_SYSTEM_LOG
	, CS_SYSTEM_YLOG
	, CS_SYSTEM_STATS
	, CS_NODE_STATS
	, CS_SHOW_MEM
	, CS_CHANGE_USER
	, CS_ANSI_CAPTURE
	, CS_LIST_TEXT_FILE          /* 0xF0 */
	, CS_EDIT_TEXT_FILE
	, CS_FILE_SET_ALT_PATH
	, CS_FILE_RESORT_DIRECTORY
	, CS_FILE_GET
	, CS_FILE_PUT
	, CS_FILE_UPLOAD_BULK
	, CS_FILE_FIND_OLD
	, CS_FILE_FIND_OPEN
	, CS_FILE_FIND_OFFLINE
	, CS_FILE_FIND_OLD_UPLOADS
	, CS_INC_MAIN_CMDS
	, CS_INC_FILE_CMDS
	, CS_PRINTFILE_STR
	, CS_PAGE_GURU
	, CS_SPY                     /* 0xFF */

};

/* Variable instructions (sub-op-code) */

/* Preceded by CS_VAR_INSTRUCTION */

enum {

	SHOW_VARS                       /* Show all variables */
	, PRINT_VAR                      /* Print a single variable */
	, VAR_PRINTF                     /* Print a formated line of text */
	, VAR_PRINTF_LOCAL               /* Print a formated line of text to local display */
	, VAR_RESERVED_3
	, VAR_RESERVED_2
	, VAR_RESERVED_1
	, DEFINE_STR_VAR                 /* Define Local Variable */
	, DEFINE_INT_VAR
	, VAR_RESERVED_A4
	, VAR_RESERVED_A3
	, VAR_RESERVED_A2
	, VAR_RESERVED_A1
	, DEFINE_GLOBAL_STR_VAR          /* Define Global Variable */
	, DEFINE_GLOBAL_INT_VAR
	, VAR_RESERVED_B4
	, VAR_RESERVED_B3    /* 0x10 */
	, VAR_RESERVED_B2
	, VAR_RESERVED_B1
	, SET_STR_VAR                    /* Set string variable */
	, SET_INT_VAR                    /* Set integer variable */
	, VAR_RESERVED_C4
	, VAR_RESERVED_C3
	, VAR_RESERVED_C2
	, VAR_RESERVED_C1
	, COMPARE_STR_VAR                /* Compare string variable (static) */
	, COMPARE_INT_VAR                /* Compare integer variable (static) */
	, STRNCMP_VAR                    /* Compare n chars of str var (static) */
	, STRSTR_VAR                     /* Sub-string compare of str var (static) */
	, VAR_RESERVED_D2
	, VAR_RESERVED_D1
	, COMPARE_VARS                   /* Compare two variables */
	, STRNCMP_VARS       /* 0x20 	// Compare n chars of str vars (dynamic) */
	, STRSTR_VARS                    /* Sub-string compare between two str vars */
	, VAR_RESERVED_E2
	, VAR_RESERVED_E1
	, COPY_VAR                       /* Copy from one variable to another */
	, VAR_RESERVED_F4
	, VAR_RESERVED_F3
	, VAR_RESERVED_F2
	, VAR_RESERVED_F1
	, SWAP_VARS                      /* Swap two variables */
	, VAR_RESERVED_G4
	, VAR_RESERVED_G3
	, VAR_RESERVED_G2
	, VAR_RESERVED_G1
	, CAT_STR_VAR                    /* Concatenate string variable (static) */
	, CAT_STR_VARS                   /* Concatenate strint variable (dynamic) */
	, FORMAT_STR_VAR     /* 0x30 	// Format string variable */
	, TIME_STR                       /* Write formated date/time to string */
	, DATE_STR                       /* Write MM/DD/YY to string */
	, FORMAT_TIME_STR                /* Create custom date/time string */
	, SECOND_STR                     /* Create a string in format hh:mm:ss */
	, STRUPR_VAR                     /* Convert string to upper case */
	, STRLWR_VAR                     /* Convert string to lower case */
	, ADD_INT_VAR                    /* Add to int variable (static) */
	, ADD_INT_VARS                   /* Add to int variable (dynamic) */
	, VAR_RESERVED_I4
	, VAR_RESERVED_I3
	, VAR_RESERVED_I2
	, VAR_RESERVED_I1
	, SUB_INT_VAR                    /* Subtract from int variable (static) */
	, SUB_INT_VARS                   /* Subtract from int variable (dynamic) */
	, VAR_RESERVED_J4
	, VAR_RESERVED_J3    /* 0x40 */
	, VAR_RESERVED_J2
	, VAR_RESERVED_J1
	, MUL_INT_VAR                    /* Multiply int variable (static) */
	, MUL_INT_VARS                   /* Multiply int variable (dynamic) */
	, VAR_RESERVED_K4
	, VAR_RESERVED_K3
	, VAR_RESERVED_K2
	, VAR_RESERVED_K1
	, DIV_INT_VAR                    /* Divide int variable (static) */
	, DIV_INT_VARS                   /* Divide int variable (dynamic) */
	, MOD_INT_VAR
	, MOD_INT_VARS
	, VAR_RESERVED_L2
	, VAR_RESERVED_L1
	, AND_INT_VAR                    /* Bit-wise AND int variable (static) */
	, AND_INT_VARS       /* 0x50 	// Bit-wise AND int variable (dynamic) */
	, COMPARE_ANY_BITS
	, COMPARE_ALL_BITS
	, VAR_RESERVED_M2
	, VAR_RESERVED_M1
	, OR_INT_VAR                     /* Bit-wise OR int variable (static) */
	, OR_INT_VARS                    /* Bit-wise OR int variable (dynamic) */
	, VAR_RESERVED_N4
	, VAR_RESERVED_N3
	, VAR_RESERVED_N2
	, VAR_RESERVED_N1
	, NOT_INT_VAR                    /* Bit-wise NOT int variable (static) */
	, NOT_INT_VARS                   /* Bit-wise NOT int variable (dynamic) */
	, VAR_RESERVED_O4
	, VAR_RESERVED_O3
	, VAR_RESERVED_O2
	, VAR_RESERVED_O1    /* 0x60 */
	, XOR_INT_VAR                    /* XOR int variable (static) */
	, XOR_INT_VARS                   /* XOR int variable (dynamic) */
	, VAR_RESERVED_P4
	, VAR_RESERVED_P3
	, VAR_RESERVED_P2
	, VAR_RESERVED_P1
	, RANDOM_INT_VAR                 /* Set integer to random number */
	, TIME_INT_VAR                   /* Set integer to current time/date */
	, DATE_STR_TO_INT                /* Convert a date string to integer */
	, STRLEN_INT_VAR                 /* Set integer to length of str */
	, CRC16_TO_INT                   /* Get CRC-16 of str var */
	, CRC32_TO_INT                   /* Get CRC-32 of str var */
	, FLENGTH_TO_INT                 /* Put length of str var file into int */
	, CHARVAL_TO_INT                 /* Put character val of str var into int */
	, GETNUM_VAR                     /* Get number */
	, GETSTR_VAR         /* 0x70 	// Get string */
	, GETNAME_VAR                    /* Get string (upper/lower) */
	, GETSTRUPR_VAR                  /* Get string (upper) */
	, GETLINE_VAR                    /* Get string (input bar/line) */
	, SHIFT_STR_VAR                  /* Shift str in variable */
	, GETSTR_MODE                    /* Get string with various modes */
	, TRUNCSP_STR_VAR                /* Truncate space off end of str var */
	, CHKFILE_VAR
	, PRINTFILE_VAR_MODE             /* Printfile str var with mode */
	, PRINTTAIL_VAR_MODE             /* Print tail-end of str var with mode */
	, CHKSUM_TO_INT                  /* Get CHKSUM of str var */
	, STRIP_CTRL_STR_VAR             /* Strip ctrl chars from str var */
	, SEND_FILE_VIA                  /* Send file (static) via protocol */
	, SEND_FILE_VIA_VAR              /* Send file (dynamic) via protocol */
	, FTIME_TO_INT                   /* Put time of str var file into int */
	, RECEIVE_FILE_VIA               /* Receive file (static) via protocol */
	, RECEIVE_FILE_VIA_VAR /* 0x80	// Receive file (dynamic) via protocol */
	, TELNET_GATE_STR                /* Run telnet gateway to static address with mode */
	, TELNET_GATE_VAR                /* Run telnet gateway to variable address with mode */
	, COPY_FIRST_CHAR                /* Copy first char of str var to int var */
	, COMPARE_FIRST_CHAR             /* Compare first char of str var to static char */
	, COPY_CHAR                      /* Copy cmdkey to int var or str var */
	, SHIFT_TO_FIRST_CHAR            /* Shift str var to first occurance of static char */
	, SHIFT_TO_LAST_CHAR             /* Shift str var to last occurance of static char */
	, MATCHUSER                      /* Set int var to user number of user name (str var) */
	, EMAIL_SECTION                  /* E-mail section */
};

/* Preceded by CS_STR_FUNCTION */

enum {                              /* More string arg functions */
	CS_LOGIN                        /* Login/password prompt */
	, CS_LOAD_TEXT                   /* Load alternative TEXT.DAT */
};

/* Preceded by CS_ONE_MORE_BYTE */
enum {                              /* More single byte instructions */
	CS_ONLINE                       /* Online execution only */
	, CS_OFFLINE                     /* Offline execution allowed */
	, CS_NEWUSER                     /* New user procedure */
	, CS_LOGON                       /* Logon procedure */
	, CS_LOGOUT                      /* Logout procedure */
	, CS_EXIT                        /* Exit current module immediately */
	, CS_LOOP_BEGIN                  /* Looping anchor */
	, CS_CONTINUE_LOOP               /* "next" loop */
	, CS_BREAK_LOOP                  /* Stop executing loop */
	, CS_END_LOOP                    /* End of looping code block */
};

/* Preceded by CS_TWO_MORE_BYTES */
enum {                              /* More two byte instructions */
	CS_USER_EVENT                   /* External user event */
};

/* Preceded by CS_NET_FUNCTION */

enum {
	CS_SOCKET_OPEN                  /* Open a socket */
	, CS_SOCKET_CLOSE                /* Close a socket */
	, CS_SOCKET_CONNECT              /* Outbound connection */
	, CS_SOCKET_ACCEPT               /* Accept an incoming connection */
	, CS_SOCKET_NREAD                /* Get number of bytes in input buffer */
	, CS_SOCKET_PEEK                 /* Peek at input buffer */
	, CS_SOCKET_READ                 /* Read input buffer */
	, CS_SOCKET_WRITE                /* Write to socket */
	, CS_SOCKET_CHECK                /* Check connection */
	, CS_SOCKET_READLINE             /* Read a cr/lf delimited line from socket */
	, CS_SOCKET_UNUSED6
	, CS_SOCKET_UNUSED5
	, CS_SOCKET_UNUSED4
	, CS_SOCKET_UNUSED3
	, CS_SOCKET_UNUSED2
	, CS_SOCKET_UNUSED1

	, CS_FTP_LOGIN                   /* socket, username, password */
	, CS_FTP_LOGOUT
	, CS_FTP_PWD                     /* print working dir */
	, CS_FTP_CWD                     /* change working dir */
	, CS_FTP_DIR                     /* path */
	, CS_FTP_PUT                     /* path */
	, CS_FTP_GET                     /* path, offset */
	, CS_FTP_RENAME
	, CS_FTP_DELETE
	, CS_FTP_UNUSED7
	, CS_FTP_UNUSED6
	, CS_FTP_UNUSED5
	, CS_FTP_UNUSED4
	, CS_FTP_UNUSED3
	, CS_FTP_UNUSED2
	, CS_FTP_UNUSED1
};

/* Preceded by CS_FIO_FUNCTION */
enum {
	FIO_OPEN                        /* Open file (static filename) */
	, FIO_CLOSE                      /* Close file */
	, FIO_READ                       /* Read from file */
	, FIO_READ_VAR                   /* Read from file, variable # of bytes */
	, FIO_WRITE                      /* Write to file */
	, FIO_WRITE_VAR                  /* Write to file, variable # of bytes */
	, FIO_GET_LENGTH                 /* Get length */
	, FIO_EOF                        /* Set logic to TRUE if eof */
	, FIO_GET_POS                    /* Get current file position */
	, FIO_SEEK                       /* Seek within file */
	, FIO_SEEK_VAR                   /* Seek within file, variable offset */
	, FIO_LOCK                       /* Lock a region */
	, FIO_LOCK_VAR                   /* Lock a region, variable length */
	, FIO_UNLOCK                     /* Unlock a region */
	, FIO_UNLOCK_VAR                 /* Unlock a region, variable length */
	, FIO_SET_LENGTH                 /* Change size */
	, FIO_SET_LENGTH_VAR             /* Change size, variable length */
	, FIO_PRINTF                     /* Write formated string to file */
	, FIO_SET_ETX                    /* Set end-of-text character */
	, FIO_GET_TIME                   /* Gets the current date/time of file */
	, FIO_SET_TIME                   /* Sets the current date/time of file */
	, FIO_OPEN_VAR                   /* Open a file (dynamic filename) */
	, FIO_READ_LINE                  /* Read a single line from file */
	, FIO_FLUSH                      /* Flush buffered output to disk */
	, FIO_UNUSED8
	, FIO_UNUSED7
	, FIO_UNUSED6
	, FIO_UNUSED5
	, FIO_UNUSED4
	, FIO_UNUSED3
	, FIO_UNUSED2
	, FIO_UNUSED1
	, REMOVE_FILE                    /* Remove a file */
	, RENAME_FILE                    /* Rename a file */
	, COPY_FILE                      /* Copy a file to another file */
	, MOVE_FILE                      /* Move a file to another file */
	, GET_FILE_ATTRIB                /* Get file attributes */
	, SET_FILE_ATTRIB                /* Set file attributes */
	, MAKE_DIR                       /* Make directory */
	, CHANGE_DIR                     /* Change current directory */
	, REMOVE_DIR                     /* Remove directory */
	, OPEN_DIR                       /* Open a directory */
	, READ_DIR                       /* Read a directory entry */
	, REWIND_DIR                     /* Rewind an open directory */
	, CLOSE_DIR                      /* Close an open directory */
};

enum {
	USER_STRING_ALIAS
	, USER_STRING_REALNAME
	, USER_STRING_HANDLE
	, USER_STRING_COMPUTER
	, USER_STRING_NOTE
	, USER_STRING_ADDRESS
	, USER_STRING_LOCATION
	, USER_STRING_ZIPCODE
	, USER_STRING_PASSWORD
	, USER_STRING_BIRTHDAY
	, USER_STRING_PHONE
	, USER_STRING_MODEM
	, USER_STRING_COMMENT
	, USER_STRING_NETMAIL
	, USER_STRING_IPADDR

};

#define CS_ONE_BYTE     CS_IF_TRUE
#define CS_TWO_BYTE     CS_CMDKEY
#define CS_THREE_BYTE   CS_GOTO
#define CS_ASCIIZ       CS_MENU
#define CS_MISC         CS_COMPARE_ARS
#define CS_FUNCTIONS    CS_MAIL_READ
#define CS_ELSEORENDIF  0xff
#define CS_NEXTCASE     0xfe

#define CS_DIGIT        0xff
#define CS_EDIGIT       0xfe

/* Bits for csi_t.misc */
#define CS_IN_SWITCH    (1L << 0)     /* Inside active switch statement */
#define CS_OFFLINE_EXEC (1L << 1)     /* Offline execution */

/* Bits for csi_t.ftp_mode */
#define CS_FTP_ECHO_CMD (1L << 0)     /* Echo command lines to user (for debug) */
#define CS_FTP_ECHO_RSP (1L << 1)     /* Echo response lines to user */
#define CS_FTP_PASV     (1L << 2)     /* Use PASV mode transfers */
#define CS_FTP_ASCII    (1L << 3)     /* Use ASCII file transfers */
#define CS_FTP_HASH     (1L << 4)     /* Hash marks printing during xfers */

#define MAX_RETS        50  /* maximum nested call depth */
#define MAX_CMDRETS     50  /* maximum nested cmd depth */
#define MAX_LOOPDEPTH   50  /* maximum nested loop depth */
#define MAX_STRVARS     26
#define MAX_INTVARS     26
#define MAX_STRLEN      81
#define MAX_OPENDIRS    10  /* maximum concurrent open directories */
#define MAX_FOPENS      10  /* maximum concurrent open files */
#define MAX_SOCKETS     10  /* maximum concurrent open sockets */
#define MAX_SYSVARS     16  /* maximum system variable saves */

#define LOGIC_LESS      -1
#define LOGIC_EQUAL     0
#define LOGIC_GREATER   1
#define LOGIC_TRUE      LOGIC_EQUAL
#define LOGIC_FALSE     LOGIC_LESS

typedef struct {                    /* Command shell image */

	char* str,                      /* Current string */
	    **str_var;                  /* String variables */

	uchar *cs,                      /* Command shell image */
	      *ip,                      /* Instruction pointer */
	      cmd,                      /* Current command key */
	      etx,                      /* End-of-text character */
	      *ret[MAX_RETS],           /* Return address stack */
	      *cmdret[MAX_CMDRETS],     /* Command return address stack */
	      *loop_home[MAX_LOOPDEPTH];    /* Loop home address stack */

	int logic;                      /* Current logic */

	DIR* dir[MAX_OPENDIRS];         /* Each directory ptr */
	FILE* file[MAX_FOPENS];         /* Each file ptr */
	SOCKET socket[MAX_SOCKETS];     /* Open socket descriptors */
	int32_t socket_error;           /* Last socket error */

	int str_vars,                   /* Total number of string variables */
	    int_vars,                   /* Total number of integer variables */
	    files,                      /* Open files */
	    dirs,                       /* Open directories */
	    sockets,                    /* Open sockets */
	    rets,                       /* Returns on stack */
	    loops,                      /* Nested loop depth (loops on stack) */
	    cmdrets;                    /* Command returns on stack */

	int32_t ftp_mode,               /* FTP operation mode */
	        *int_var;               /* Integer variables */
	uint32_t
	*str_var_name,                  /* String variable names (CRC-32) */
	*int_var_name;                  /* Integer variable names (CRC-32) */
	int retval,                     /* Return value */
	    misc,                       /* Misc bits */
	    switch_val;                 /* Current switch value */

	size_t length;                  /* Length of image */

} csi_t;

#endif /* End of CMDSHELL.H */
